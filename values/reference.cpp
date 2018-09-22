
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
        
        x64->op(MOVQ, RAX, NOSYOBJECT_SIZE);
        //std::cerr << "XXX Allocating " << heap_size << " on the heap.\n";
        x64->op(LEA, RBX, Address(finalizer_label, 0));
        x64->runtime->alloc_RAX_RBX();

        x64->op(PUSHQ, RAX);
        
        right->compile_and_store(x64, Storage(STACK));
        
        x64->op(MOVQ, RAX, Address(RSP, 0));  // the object address
        x64->op(LEA, RBX, Address(callback_label, 0));
        x64->op(MOVQ, RCX, Address(RSP, 8));  // the nosy address as the payload1
        x64->op(MOVQ, RDX, 0);
        x64->op(CALL, x64->runtime->alloc_fcb_label);
        
        x64->op(POPQ, RCX);  // object address
        x64->op(POPQ, RDX);  // nosy address

        x64->runtime->decref(RCX);
        
        x64->op(MOVQ, Address(RDX, NOSYOBJECT_RAW_OFFSET), RCX);
        x64->op(MOVQ, Address(RDX, NOSYOBJECT_FCB_OFFSET), RAX);
        
        return Storage(REGISTER, RDX);
    }
    
    static void compile_callback(Label label, X64 *x64) {
        x64->code_label_local(label, "nosyobject_callback");
        
        x64->runtime->log("NosyObject callback.");
        
        x64->op(MOVQ, Address(RCX, NOSYOBJECT_RAW_OFFSET), 0);
        x64->op(MOVQ, Address(RCX, NOSYOBJECT_FCB_OFFSET), 0);  // clear FCB address for the finalizer
        
        x64->op(CALL, x64->runtime->free_fcb_label);
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
