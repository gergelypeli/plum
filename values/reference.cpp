
class ReferenceOperationValue: public GenericOperationValue {
public:
    ReferenceOperationValue(OperationType o, Value *l, TypeMatch &match)
        :GenericOperationValue(o, op_arg_ts(o, match), op_ret_ts(o, match), l) {
    }

    virtual void decref(Register r, X64 *x64) {
        x64->decref(r);
    }

    virtual Storage equal(X64 *x64, BitSetOp op) {
        subcompile(x64);
        
        switch (ls.where * rs.where) {
        case REGISTER_REGISTER:
            decref(ls.reg, x64);
            decref(rs.reg, x64);
            x64->op(CMPQ, ls.reg, rs.reg);
            return Storage(FLAGS, op);
        case REGISTER_MEMORY:
            decref(ls.reg, x64);
            x64->op(CMPQ, ls.reg, rs.address);
            return Storage(FLAGS, op);
        case MEMORY_MEMORY:
            x64->op(MOVQ, RBX, ls.address);
            x64->op(CMPQ, RBX, rs.address);
            return Storage(FLAGS, op);
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Storage compile(X64 *x64) {
        switch (operation) {
        case EQUAL:
            return equal(x64, SETE);
        case NOT_EQUAL:
            return equal(x64, SETNE);
        default:
            return GenericOperationValue::compile(x64);
        }
    }
};


class WeakreferenceOperationValue: public ReferenceOperationValue {
public:
    WeakreferenceOperationValue(OperationType o, Value *l, TypeMatch &match)
        :ReferenceOperationValue(o, l, match) {
    }
    
    virtual void decref(Register r, X64 *x64) {
        x64->decweakref(r);
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
            x64->incweakref(s.reg);
            x64->decref(s.reg);
            return s;
        case STACK:
            x64->op(MOVQ, RBX, Address(RSP, 0));
            x64->incweakref(RBX);
            x64->decref(RBX);
            return s;
        case MEMORY:
            return s;
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class WeaktrampolineValue: public GenericValue {
public:
    WeaktrampolineValue(TypeSpec rts)
        :GenericValue(rts.unprefix(ref_type).reprefix(weaktrampoline_type, weakref_type), rts, NULL) {
    }
    
    virtual Regs precompile(Regs preferred) {
        Regs clob = right->precompile(preferred);
        return clob.add(RAX).add(RCX).add(RDX);
    }

    virtual Storage compile(X64 *x64) {
        Label callback_label = x64->once->compile(compile_callback);
        Label finalizer_label = ts.unprefix(ref_type).get_finalizer_label(x64);
        
        x64->op(MOVQ, RAX, ADDRESS_SIZE);
        //std::cerr << "XXX Allocating " << heap_size << " on the heap.\n";
        x64->op(LEARIP, RBX, finalizer_label);
        x64->alloc_RAX_RBX();

        x64->op(PUSHQ, RAX);
        
        right->compile_and_store(x64, Storage(REGISTER, RAX));
        x64->decweakref(RAX);
        
        x64->op(LEARIP, RBX, callback_label);
        x64->op(MOVQ, RCX, Address(RSP, 0));  // the trampoline address as the payload1
        x64->op(MOVQ, RDX, 0);
        x64->op(CALL, x64->alloc_fcb_label);
        
        x64->op(MOVQ, RBX, RAX);
        x64->op(POPQ, RAX);
        x64->op(MOVQ, Address(RAX, 0), RBX);
        
        return Storage(REGISTER, RAX);
    }
    
    static void compile_callback(Label label, X64 *x64) {
        x64->code_label_local(label, "weaktrampoline_callback");
        
        x64->log("Weak trampoline called back.");
        
        x64->op(MOVQ, Address(RCX, 0), 0);  // clear FCB address for the finalizer
        
        x64->op(PUSHQ, RAX);
        x64->op(MOVQ, RAX, RBX);
        x64->op(CALL, x64->free_fcb_label);
        x64->op(POPQ, RAX);
        
        x64->op(RET);
    }
};
