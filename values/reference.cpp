
class ReferenceOperationValue: public GenericOperationValue {
public:
    ReferenceOperationValue(OperationType o, Value *l, TypeMatch &match)
        :GenericOperationValue(o, op_arg_ts(o, match), op_ret_ts(o, match), l) {
    }
};


class WeakreferenceOperationValue: public ReferenceOperationValue {
public:
    WeakreferenceOperationValue(OperationType o, Value *l, TypeMatch &match)
        :ReferenceOperationValue(o, l, match) {
    }
};


class ReferenceWeakenValue: public Value {
public:
    std::unique_ptr<Value> value;
    
    ReferenceWeakenValue(Value *v, TypeMatch &tm)
        :Value(NO_TS) {
        if (v) {
            // When used as an automatic conversion
            value.reset(v);
            ts = value->ts.rvalue().reprefix(ref_type, weakref_type);
        }
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        // When used as the :weak control
        if (!check_arguments(args, kwargs, {{ "value", NULL, scope, &value }}))
            return false;
            
        ts = value->ts.rvalue().reprefix(ref_type, weakref_type);
        return true;
    }
    
    virtual Regs precompile(Regs preferred) {
        return value->precompile(preferred);
    }

    virtual Storage compile(X64 *x64) {
        Storage s = value->compile(x64);
        
        switch (s.where) {
        case REGISTER:
            x64->runtime->incweakref(s.reg);
            x64->runtime->decref(s.reg);
            return s;
        case STACK:
            x64->op(MOVQ, RBX, Address(RSP, 0));
            x64->runtime->incweakref(RBX);
            x64->runtime->decref(RBX);
            return s;
        case MEMORY:
            return s;
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class WeakAnchorageValue: public GenericValue {
public:
    WeakAnchorageValue(TypeSpec rts)
        :GenericValue(rts.unprefix(ref_type).reprefix(weakanchorage_type, weakref_type), rts, NULL) {
    }
    
    virtual Regs precompile(Regs preferred) {
        Regs clob = right->precompile(preferred);
        return clob | RAX | RCX | RDX;
    }

    virtual Storage compile(X64 *x64) {
        Label callback_label = x64->once->compile(compile_callback);
        Label finalizer_label = ts.unprefix(ref_type).get_finalizer_label(x64);
        
        x64->op(MOVQ, RAX, ADDRESS_SIZE * 2);
        //std::cerr << "XXX Allocating " << heap_size << " on the heap.\n";
        x64->op(LEA, RBX, Address(finalizer_label, 0));
        x64->runtime->alloc_RAX_RBX();

        x64->op(PUSHQ, RAX);
        
        right->compile_and_store(x64, Storage(STACK));
        
        x64->op(MOVQ, RAX, Address(RSP, 0));  // the object address
        x64->op(LEA, RBX, Address(callback_label, 0));
        x64->op(MOVQ, RCX, Address(RSP, 8));  // the anchorage address as the payload1
        x64->op(MOVQ, RDX, 0);
        x64->op(CALL, x64->runtime->alloc_fcb_label);
        
        x64->op(POPQ, RCX);  // object address
        x64->op(POPQ, RDX);  // anchorage address
        
        x64->op(MOVQ, Address(RDX, 0), RCX);
        x64->op(MOVQ, Address(RDX, 8), RAX);
        
        return Storage(REGISTER, RDX);
    }
    
    static void compile_callback(Label label, X64 *x64) {
        x64->code_label_local(label, "weakanchorage_callback");
        
        x64->runtime->log("WeakAnchorage callback.");
        
        x64->op(MOVQ, RBX, Address(RCX, 0));
        x64->runtime->decweakref(RBX);
        
        x64->op(MOVQ, Address(RCX, 0), 0);
        x64->op(MOVQ, Address(RCX, 8), 0);  // clear FCB address for the finalizer
        
        x64->op(CALL, x64->runtime->free_fcb_label);
        x64->op(RET);
    }
};


class WeakAnchorageDeadMatcherValue: public GenericValue, public Raiser {
public:
    WeakAnchorageDeadMatcherValue(Value *p, TypeMatch &match)
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
        x64->runtime->decweakref(RBX);  // for the anchorage
        x64->op(CMPQ, Address(RBX, 0), 0);
        x64->op(JE, ok);

        // popped        
        raise("UNMATCHED", x64);
                
        x64->code_label(ok);
        return Storage();
    }
};


class WeakAnchorageLiveMatcherValue: public GenericValue, public Raiser {
public:
    WeakAnchorageLiveMatcherValue(Value *p, TypeMatch &match)
        :GenericValue(NO_TS, match[1].prefix(weakref_type), p) {
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
        x64->runtime->decweakref(RBX);  // for the anchorage
        x64->op(CMPQ, Address(RBX, 0), 0);
        x64->op(JNE, ok);
        
        // popped
        raise("UNMATCHED", x64);
                
        x64->code_label(ok);
        x64->op(MOVQ, RBX, Address(RBX, 0));
        x64->runtime->incweakref(RBX);  // for the object
        x64->op(PUSHQ, RBX);

        return Storage(STACK);
    }
};
