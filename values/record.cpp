

class RecordOperationValue: public GenericOperationValue {
public:
    RecordOperationValue(OperationType o, Value *p, TypeMatch &match)
        :GenericOperationValue(o, match[0].rvalue(), is_comparison(o) ? BOOLEAN_TS : match[0], p) {
    }
};
