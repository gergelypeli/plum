class EqualityValue: public Value {
public:
    bool no;
    std::unique_ptr<Value> value;

    EqualityValue(bool n, Value *v);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};

class ComparisonValue: public Value {
public:
    ConditionCode cc;
    std::unique_ptr<Value> value;

    ComparisonValue(ConditionCode c, Value *v);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};

class BulkEqualityMatcherValue: public Value, public Raiser {
public:
    std::unique_ptr<Value> pivot_value;
    std::vector<std::unique_ptr<Value>> values;

    BulkEqualityMatcherValue(Value *p);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};
