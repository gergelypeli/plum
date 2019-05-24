
class ReferenceOperationValue: public GenericOperationValue {
public:
    ReferenceOperationValue(OperationType o, Value *l, TypeMatch &match);
};

class PointerOperationValue: public ReferenceOperationValue {
public:
    PointerOperationValue(OperationType o, Value *l, TypeMatch &match);
};
