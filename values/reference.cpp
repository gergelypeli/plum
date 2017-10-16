
class ReferenceOperationValue: public GenericOperationValue {
public:
    ReferenceOperationValue(OperationType o, Value *l, TypeMatch &match)
        :GenericOperationValue(o, op_arg_ts(o, match), op_ret_ts(o, match), l) {
    }

    virtual Storage equal(X64 *x64, BitSetOp op) {
        subcompile(x64);
        
        switch (ls.where * rs.where) {
        case REGISTER_REGISTER:
            x64->decref(ls.reg);
            x64->decref(rs.reg);
            x64->op(CMPQ, ls.reg, rs.reg);
            return Storage(FLAGS, op);
        case REGISTER_MEMORY:
            x64->decref(ls.reg);
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


class NullReferenceValue: public Value {
public:
    Register reg;

    NullReferenceValue(TypeSpec ts)
        :Value(ts) {
    }
    
    Regs precompile(Regs preferred) {
        reg = preferred.get_any();
        return Regs().add(reg);
    }
    
    Storage compile(X64 *x64) {
        x64->op(MOVQ, reg, 0);
        return Storage(REGISTER, reg);
    }
};
