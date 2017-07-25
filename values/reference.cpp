
class ReferenceOperationValue: public GenericOperationValue {
public:
    ReferenceOperationValue(GenericOperation o, TypeSpec t, Value *l)
        :GenericOperationValue(o, t.rvalue(), is_comparison(o) ? BOOLEAN_TS : t, l) {
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
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Storage assign(X64 *x64) {
        subcompile(x64);

        switch (ls.where * rs.where) {
        case MEMORY_REGISTER:
            x64->incref(rs.reg);
            x64->op(XCHGQ, rs.reg, ls.address);
            x64->decref(rs.reg);
            return ls;
        case MEMORY_MEMORY:
            x64->op(MOVQ, reg, rs.address);
            x64->incref(reg);
            x64->op(XCHGQ, reg, ls.address);
            x64->decref(reg);
            return ls;
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
        case ASSIGN:
            return assign(x64);
        default:
            throw INTERNAL_ERROR;
        }
    }
};

