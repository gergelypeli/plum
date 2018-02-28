
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
            ts = value->ts.rvalue().reprefix(reference_type, weakreference_type);
        }
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        // When used as the :weak control
        if (!check_arguments(args, kwargs, {{ "value", NULL, scope, &value }}))
            return false;
            
        ts = value->ts.rvalue().reprefix(reference_type, weakreference_type);
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
