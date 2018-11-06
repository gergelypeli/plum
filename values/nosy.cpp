
static void alloc_nosycontainer(TypeSpec member_ts, X64 *x64) {
    TypeSpec ts = member_ts.prefix(nosycontainer_type);
    Label finalizer_label = ts.get_finalizer_label(x64);
    int member_size = member_ts.measure_stack();  // type parameter
    
    x64->op(PUSHQ, member_size);
    x64->op(LEA, R10, Address(finalizer_label, 0));
    x64->op(PUSHQ, R10);
    x64->runtime->heap_alloc();  // clobbers all
    x64->op(ADDQ, RSP, 2 * ADDRESS_SIZE);

    // Ref to NosyContainer in RAX
}


class NosyContainerMemberValue: public Value {
public:
    TypeSpec heap_ts;
    std::unique_ptr<Value> pivot;
    Unborrow *unborrow;

    NosyContainerMemberValue(Value *p, TypeSpec member_ts, Scope *scope)
        :Value(member_ts) {
        pivot.reset(p);
        heap_ts = member_ts.prefix(nosycontainer_type);
        
        unborrow = new Unborrow(heap_ts);
        scope->add(unborrow);
    }

    virtual Regs precompile(Regs preferred) {
        return pivot->precompile(preferred) | Regs(RAX);
    }

    virtual Storage compile(X64 *x64) {
        Storage s = pivot->compile(x64);
        
        switch (s.where) {
        case REGISTER:
            x64->op(MOVQ, unborrow->get_address(), s.reg);
            return Storage(MEMORY, Address(s.reg, NOSYCONTAINER_MEMBER_OFFSET));
        case MEMORY:
            x64->op(MOVQ, RAX, s.address);
            heap_ts.incref(RAX, x64);
            x64->op(MOVQ, unborrow->get_address(), RAX);
            return Storage(MEMORY, Address(RAX, NOSYCONTAINER_MEMBER_OFFSET));
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class WeakContainerValue: public Value {
public:
    TypeSpec member_ts;
    std::unique_ptr<Value> member_value;

    WeakContainerValue(Value *pivot, TypeSpec mts, TypeSpec cts)
        :Value(cts) {
        member_ts = mts;
        member_value.reset(pivot);
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        return member_value->check(args, kwargs, scope);  // for the Rbtree initializers
    }
    
    virtual Regs precompile(Regs preferred) {
        member_value->precompile(preferred);
        return Regs::all();
    }

    virtual Storage compile(X64 *x64) {
        member_value->compile_and_store(x64, Storage(STACK));

        alloc_nosycontainer(member_ts, x64);

        member_ts.create(Storage(STACK), Storage(MEMORY, Address(RAX, NOSYCONTAINER_MEMBER_OFFSET)), x64);
        
        return Storage(REGISTER, RAX);
    }
};


class WeakrefToValue: public Value {
public:
    std::unique_ptr<Value> target;
    TypeSpec heap_ts;

    WeakrefToValue(TypeSpec hts)
        :Value(hts.prefix(weakref_type)) {
        heap_ts = hts;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        TypeSpec arg_ts = heap_ts.prefix(ptr_type);
        
        return check_arguments(args, kwargs, {
            { "target", &arg_ts, scope, &target }
        });
    }
    
    virtual Regs precompile(Regs preferred) {
        target->precompile(preferred);
        return Regs::all();
    }

    virtual Storage compile(X64 *x64) {
        Label callback_label = x64->once->compile(compile_callback);
        
        target->compile_and_store(x64, Storage(STACK));  // object address
        
        x64->op(LEA, R10, Address(callback_label, 0));
        x64->op(PUSHQ, R10);  // callback address

        alloc_nosycontainer(heap_ts.prefix(nosyvalue_type), x64);
        x64->op(PUSHQ, RAX);  // nosy container address as payload1
        
        x64->op(PUSHQ, 0);  // payload2
        
        x64->op(CALL, x64->runtime->fcb_alloc_label);  // clobbers all
        
        x64->op(POPQ, RBX);
        x64->op(POPQ, RBX);  // nosy address
        x64->op(POPQ, RCX);
        x64->op(POPQ, RCX);  // object address

        heap_ts.decref(RCX, x64);  // FIXME: use after decref
        
        x64->op(MOVQ, Address(RBX, NOSYCONTAINER_MEMBER_OFFSET + NOSYVALUE_RAW_OFFSET), RCX);
        x64->op(MOVQ, Address(RBX, NOSYCONTAINER_MEMBER_OFFSET + NOSYVALUE_FCB_OFFSET), RAX);
        
        return Storage(REGISTER, RBX);
    }
    
    static void compile_callback(Label label, X64 *x64) {
        x64->code_label_local(label, "weakref_callback");
        x64->runtime->log("Weakref callback.");

        x64->op(MOVQ, RCX, Address(RSP, ADDRESS_SIZE * 2));  // payload1 arg (nosy address)
        x64->op(MOVQ, Address(RCX, NOSYCONTAINER_MEMBER_OFFSET + NOSYVALUE_RAW_OFFSET), 0);
        x64->op(MOVQ, Address(RCX, NOSYCONTAINER_MEMBER_OFFSET + NOSYVALUE_FCB_OFFSET), 0);  // clear FCB address for the finalizer
        
        x64->op(PUSHQ, Address(RSP, ADDRESS_SIZE * 3));  // fcb arg
        x64->op(CALL, x64->runtime->fcb_free_label);  // clobbers all
        x64->op(ADDQ, RSP, ADDRESS_SIZE);
        
        x64->op(RET);
    }
};


class WeakrefDeadMatcherValue: public GenericValue, public Raiser {
public:
    TypeSpec param_heap_ts;

    WeakrefDeadMatcherValue(Value *p, TypeMatch &match)
        :GenericValue(NO_TS, VOID_TS, p) {
        param_heap_ts = match[1];
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!check_raise(match_unmatched_exception_type, scope))
            return false;
        
        return GenericValue::check(args, kwargs, scope);
    }

    virtual Regs precompile(Regs preferred) {
        return left->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        TypeSpec nosy_heap_ts = param_heap_ts.prefix(nosyvalue_type).prefix(nosycontainer_type);
        Label ok;
        left->compile_and_store(x64, Storage(STACK));
        
        x64->op(POPQ, R10);
        x64->op(MOVQ, R11, Address(R10, NOSYCONTAINER_MEMBER_OFFSET + NOSYVALUE_RAW_OFFSET));
        nosy_heap_ts.decref(R10, x64);
        x64->op(CMPQ, R11, 0);
        x64->op(JE, ok);

        // popped
        raise("UNMATCHED", x64);
                
        x64->code_label(ok);
        return Storage();
    }
};


class WeakrefLiveMatcherValue: public GenericValue, public Raiser {
public:
    TypeSpec param_heap_ts;
    
    WeakrefLiveMatcherValue(Value *p, TypeMatch &match)
        :GenericValue(NO_TS, match[1].prefix(ptr_type), p) {
        param_heap_ts = match[1];
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!check_raise(match_unmatched_exception_type, scope))
            return false;
        
        return GenericValue::check(args, kwargs, scope);
    }

    virtual Regs precompile(Regs preferred) {
        return left->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        TypeSpec nosy_heap_ts = param_heap_ts.prefix(nosyvalue_type).prefix(nosycontainer_type);
        Label ok;
        left->compile_and_store(x64, Storage(STACK));
        
        x64->op(POPQ, R10);
        x64->op(MOVQ, R11, Address(R10, NOSYCONTAINER_MEMBER_OFFSET + NOSYVALUE_RAW_OFFSET));
        nosy_heap_ts.decref(R10, x64);
        x64->op(CMPQ, R11, 0);
        x64->op(JNE, ok);
        
        // popped
        raise("UNMATCHED", x64);
                
        x64->code_label(ok);
        param_heap_ts.incref(R11, x64);
        x64->op(PUSHQ, R11);
        
        return Storage(STACK);
    }
};
