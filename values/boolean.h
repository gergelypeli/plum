class BooleanOperationValue: public OptimizedOperationValue {
public:
    BooleanOperationValue(OperationType o, Value *p, TypeMatch &match);
};

class BooleanNotValue: public Value {
public:
    std::unique_ptr<Value> value;

    BooleanNotValue(Value *p, TypeMatch &match);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};

class BooleanBinaryValue: public Value {
public:
    std::unique_ptr<Value> left, right;
    Register reg;
    bool need_true;

    BooleanBinaryValue(Value *p, TypeMatch &match, bool nt);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};

class BooleanOrValue: public BooleanBinaryValue {
public:
    BooleanOrValue(Value *p, TypeMatch &match);
};

class BooleanAndValue: public BooleanBinaryValue {
public:
    BooleanAndValue(Value *p, TypeMatch &match);
};
