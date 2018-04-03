
class FloatOperationValue: public GenericOperationValue {
public:
    FloatOperationValue(OperationType o, Value *p, TypeMatch &match)
        :GenericOperationValue(o, op_arg_ts(o, match), op_ret_ts(o, match), p) {
    }
    
    virtual Storage compile(X64 *x64) {
        return GenericOperationValue::compile(x64);
    }
};
