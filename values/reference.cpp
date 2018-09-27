
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
    NosyObjectValue(TypeSpec rts)
        :GenericValue(rts.unprefix(ref_type).reprefix(nosyobject_type, ptr_type), rts, NULL) {
    }
    
    virtual Regs precompile(Regs preferred) {
        Regs clob = right->precompile(preferred);
        return clob | RAX | RCX | RDX;
    }

    virtual Storage compile(X64 *x64) {
        Label callback_label = x64->once->compile(compile_callback);
        Label finalizer_label = ts.unprefix(ref_type).get_finalizer_label(x64);
        
        right->compile_and_store(x64, Storage(STACK));  // object address
        
        x64->op(LEA, RBX, Address(callback_label, 0));
        x64->op(PUSHQ, RBX);  // callback address
        
        x64->op(PUSHQ, NOSYOBJECT_SIZE);
        x64->op(LEA, RBX, Address(finalizer_label, 0));
        x64->op(PUSHQ, RBX);
        x64->runtime->heap_alloc();
        x64->op(ADDQ, RSP, 2 * ADDRESS_SIZE);
        x64->op(PUSHQ, RAX);  // nosy address as payload1
        
        x64->op(PUSHQ, 0);  // payload2
        
        x64->op(CALL, x64->runtime->fcb_alloc_label);  // RAX - fcb address
        
        x64->op(POPQ, RDX);
        x64->op(POPQ, RDX);  // nosy address
        x64->op(POPQ, RCX);
        x64->op(POPQ, RCX);  // object address

        x64->runtime->decref(RCX);  // FIXME: this shouldn't finalize
        
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
        x64->op(CALL, x64->runtime->fcb_free_label);
        x64->op(ADDQ, RSP, ADDRESS_SIZE);
        
        x64->op(RET);
    }
};


class NosyObjectDeadMatcherValue: public GenericValue, public Raiser {
public:
    NosyObjectDeadMatcherValue(Value *p, TypeMatch &match)
        :GenericValue(NO_TS, VOID_TS, p) {
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
        
        x64->op(POPQ, RBX);
        x64->runtime->decref(RBX);
        x64->op(CMPQ, Address(RBX, NOSYOBJECT_RAW_OFFSET), 0);
        x64->op(JE, ok);

        // popped        
        raise("UNMATCHED", x64);
                
        x64->code_label(ok);
        return Storage();
    }
};


class NosyObjectLiveMatcherValue: public GenericValue, public Raiser {
public:
    NosyObjectLiveMatcherValue(Value *p, TypeMatch &match)
        :GenericValue(NO_TS, match[1].prefix(ptr_type), p) {
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
        
        x64->op(POPQ, RBX);
        x64->runtime->decref(RBX);
        x64->op(CMPQ, Address(RBX, NOSYOBJECT_RAW_OFFSET), 0);
        x64->op(JNE, ok);
        
        // popped
        raise("UNMATCHED", x64);
                
        x64->code_label(ok);
        x64->op(MOVQ, RBX, Address(RBX, NOSYOBJECT_RAW_OFFSET));
        x64->runtime->incref(RBX);
        x64->op(PUSHQ, RBX);
        
        return Storage(STACK);
    }
};
