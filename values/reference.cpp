
class ReferenceOperationValue: public GenericOperationValue {
public:
    ReferenceOperationValue(OperationType o, Value *l, TypeMatch &match)
        :GenericOperationValue(o, op_arg_ts(o, match), op_ret_ts(o, match), l) {
    }
};


class PointerOperationValue: public ReferenceOperationValue {
public:
    PointerOperationValue(OperationType o, Value *l, TypeMatch &match)
        :ReferenceOperationValue(o, l, match) {
    }
};


class NosyObjectValue: public GenericValue {
public:
    TypeSpec heap_ts;

    NosyObjectValue(TypeSpec rts)
        :GenericValue(rts.unprefix(ref_type).reprefix(nosyobject_type, ptr_type), rts, NULL) {
        heap_ts = rts.unprefix(ref_type);
    }
    
    virtual Regs precompile(Regs preferred) {
        right->precompile(preferred);
        return Regs::all();
    }

    virtual Storage compile(X64 *x64) {
        Label callback_label = x64->once->compile(compile_callback);
        Label finalizer_label = ts.unprefix(ref_type).get_finalizer_label(x64);
        
        right->compile_and_store(x64, Storage(STACK));  // object address
        
        x64->op(LEA, R10, Address(callback_label, 0));
        x64->op(PUSHQ, R10);  // callback address
        
        x64->op(PUSHQ, NOSYOBJECT_SIZE);
        x64->op(LEA, R10, Address(finalizer_label, 0));
        x64->op(PUSHQ, R10);
        x64->runtime->heap_alloc();  // clobbers all
        x64->op(ADDQ, RSP, 2 * ADDRESS_SIZE);
        
        x64->op(PUSHQ, RAX);  // nosy address as payload1
        
        x64->op(PUSHQ, 0);  // payload2
        
        x64->op(CALL, x64->runtime->fcb_alloc_label);  // clobbers all
        
        x64->op(POPQ, RDX);
        x64->op(POPQ, RDX);  // nosy address
        x64->op(POPQ, RCX);
        x64->op(POPQ, RCX);  // object address

        heap_ts.decref(RCX, x64);  // FIXME: use after decref
        
        x64->op(MOVQ, Address(RDX, NOSYOBJECT_RAW_OFFSET), RCX);
        x64->op(MOVQ, Address(RDX, NOSYOBJECT_FCB_OFFSET), RAX);
        
        return Storage(REGISTER, RDX);
    }
    
    static void compile_callback(Label label, X64 *x64) {
        x64->code_label_local(label, "nosyobject_callback");
        
        x64->runtime->log("NosyObject callback.");

        x64->op(MOVQ, RCX, Address(RSP, ADDRESS_SIZE * 2));  // payload1 arg (nosy address)
        x64->op(MOVQ, Address(RCX, NOSYOBJECT_RAW_OFFSET), 0);
        x64->op(MOVQ, Address(RCX, NOSYOBJECT_FCB_OFFSET), 0);  // clear FCB address for the finalizer
        
        x64->op(PUSHQ, Address(RSP, ADDRESS_SIZE * 3));  // fcb arg
        x64->op(CALL, x64->runtime->fcb_free_label);  // clobbers all
        x64->op(ADDQ, RSP, ADDRESS_SIZE);
        
        x64->op(RET);
    }
};


class NosyObjectDeadMatcherValue: public GenericValue, public Raiser {
public:
    TypeSpec nosy_heap_ts;

    NosyObjectDeadMatcherValue(Value *p, TypeMatch &match)
        :GenericValue(NO_TS, VOID_TS, p) {
        nosy_heap_ts = match[0];
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
        Label ok;
        left->compile_and_store(x64, Storage(STACK));
        
        x64->op(POPQ, R10);
        x64->op(MOVQ, R11, Address(R10, NOSYOBJECT_RAW_OFFSET));
        nosy_heap_ts.decref(R10, x64);
        x64->op(CMPQ, R11, 0);
        x64->op(JE, ok);

        // popped
        raise("UNMATCHED", x64);
                
        x64->code_label(ok);
        return Storage();
    }
};


class NosyObjectLiveMatcherValue: public GenericValue, public Raiser {
public:
    TypeSpec nosy_heap_ts;
    TypeSpec param_heap_ts;
    
    NosyObjectLiveMatcherValue(Value *p, TypeMatch &match)
        :GenericValue(NO_TS, match[1].prefix(ptr_type), p) {
        nosy_heap_ts = match[0];
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
        Label ok;
        left->compile_and_store(x64, Storage(STACK));
        
        x64->op(POPQ, R10);
        x64->op(MOVQ, R11, Address(R10, NOSYOBJECT_RAW_OFFSET));
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


class NosyContainerValue: public Value {
public:
    std::unique_ptr<Value> member_value;

    NosyContainerValue(Value *pivot, TypeSpec rts)
        :Value(rts) {
        member_value.reset(pivot);
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        return member_value->check(args, kwargs, scope);
    }
    
    virtual Regs precompile(Regs preferred) {
        member_value->precompile(preferred);
        return Regs::all();
    }

    virtual Storage compile(X64 *x64) {
        Label finalizer_label = ts.unprefix(ref_type).get_finalizer_label(x64);
        
        member_value->compile_and_store(x64, Storage(STACK));  // object address
        
        x64->op(PUSHQ, NOSYCONTAINER_SIZE);
        x64->op(LEA, R10, Address(finalizer_label, 0));
        x64->op(PUSHQ, R10);
        x64->runtime->heap_alloc();  // clobbers all
        x64->op(ADDQ, RSP, 2 * ADDRESS_SIZE);

        x64->op(POPQ, Address(RAX, NOSYCONTAINER_MEMBER_OFFSET));  // create member variable
        
        return Storage(REGISTER, RAX);
    }
};


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
