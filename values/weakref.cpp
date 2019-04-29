
// Weakref

void compile_nosyref_callback(Label label, X64 *x64) {
    x64->code_label_local(label, "nosyref_callback");
    x64->runtime->log("Nosyref callback.");

    x64->op(MOVQ, RCX, Address(RSP, ADDRESS_SIZE * 2));  // payload1 arg (nosyref address)
    x64->op(MOVQ, Address(RCX, NOSYREF_MEMBER_OFFSET + NOSYVALUE_RAW_OFFSET), 0);
        
    x64->op(RET);
}


void compile_nosyref_finalizer(Label label, X64 *x64) {
    x64->code_label_local(label, "nosyref_finalizer");
    x64->runtime->log("Nosyref finalized.");

    x64->op(MOVQ, RAX, Address(RSP, ADDRESS_SIZE));  // pointer arg
    
    Label callback_label = x64->once->compile(compile_nosyref_callback);
    Label ok;

    // This object may have died already
    x64->op(MOVQ, R10, Address(RAX, NOSYREF_MEMBER_OFFSET + NOSYVALUE_RAW_OFFSET));
    x64->op(CMPQ, R10, 0);
    x64->op(JE, ok);
    
    x64->op(PUSHQ, R10);  // object
    x64->op(LEA, R10, Address(callback_label, 0));
    x64->op(PUSHQ, R10);  // callback
    x64->op(PUSHQ, RAX);  // payload1
    x64->op(PUSHQ, 0);    // payload2
    
    x64->op(CALL, x64->runtime->fcb_free_label);  // clobbers all

    x64->op(ADDQ, RSP, 4 * ADDRESS_SIZE);
    
    x64->code_label(ok);

    // Member needs no actual finalization

    x64->op(RET);
}


void alloc_nosyref(X64 *x64) {
    Label finalizer_label = x64->once->compile(compile_nosyref_finalizer);
    
    x64->op(PUSHQ, NOSYVALUE_SIZE);
    x64->op(LEA, R10, Address(finalizer_label, 0));
    x64->op(PUSHQ, R10);
    x64->runtime->heap_alloc();  // clobbers all
    x64->op(ADDQ, RSP, 2 * ADDRESS_SIZE);

    // Ref to Nosyref in RAX
}


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
        Label callback_label = x64->once->compile(compile_nosyref_callback);
        
        target->compile_and_store(x64, Storage(STACK));  // object address
        
        x64->op(LEA, R10, Address(callback_label, 0));
        x64->op(PUSHQ, R10);  // callback address

        alloc_nosyref(x64);
        x64->op(PUSHQ, RAX);  // nosy container address as payload1
        
        x64->op(PUSHQ, 0);  // payload2
        
        x64->op(CALL, x64->runtime->fcb_alloc_label);  // clobbers all, returns nothing
        
        x64->op(POPQ, RBX);
        x64->op(POPQ, RBX);  // nosyref address
        x64->op(POPQ, RCX);
        x64->op(POPQ, RCX);  // object address

        x64->op(MOVQ, Address(RBX, NOSYREF_MEMBER_OFFSET + NOSYVALUE_RAW_OFFSET), RCX);

        heap_ts.decref(RCX, x64);
        
        return Storage(REGISTER, RBX);
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
        TypeSpec nosy_heap_ts = param_heap_ts.prefix(nosyvalue_type).prefix(nosyref_type);
        Label ok;
        left->compile_and_store(x64, Storage(STACK));
        
        x64->op(POPQ, R10);
        x64->op(MOVQ, R11, Address(R10, NOSYREF_MEMBER_OFFSET + NOSYVALUE_RAW_OFFSET));
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
        TypeSpec nosy_heap_ts = param_heap_ts.prefix(nosyvalue_type).prefix(nosyref_type);
        Label ok;
        left->compile_and_store(x64, Storage(STACK));
        
        x64->op(POPQ, R10);
        x64->op(MOVQ, R11, Address(R10, NOSYREF_MEMBER_OFFSET + NOSYVALUE_RAW_OFFSET));
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
