

class RecordOperationValue: public GenericOperationValue {
public:
    RecordOperationValue(OperationType o, Value *p, TypeMatch &match)
        :GenericOperationValue(o, match[0].rvalue(), is_comparison(o) ? BOOLEAN_TS : match[0], p) {
    }
    
    virtual Storage assign(X64 *x64) {
        subcompile(x64);

        ts.store(rs, ls, x64);
        
        return ls;
    }

    virtual Storage compile(X64 *x64) {
        switch (operation) {
        case ASSIGN:
            return assign(x64);
        default:
            throw INTERNAL_ERROR;
        }
    }
};
