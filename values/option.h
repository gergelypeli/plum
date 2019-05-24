class OptionNoneValue: public Value {
public:
    OptionNoneValue(TypeSpec ts);
    
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};

class OptionSomeValue: public Value {
public:
    std::unique_ptr<Value> some;
    int flag_size;

    OptionSomeValue(TypeSpec ts);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};

class OptionOperationValue: public GenericOperationValue {
public:
    OptionOperationValue(OperationType o, Value *p, TypeMatch &match);
};

class OptionNoneMatcherValue: public GenericValue, public Raiser {
public:
    OptionNoneMatcherValue(Value *p, TypeMatch &match);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};

class OptionSomeMatcherValue: public GenericValue, public Raiser {
public:
    int flag_size;

    OptionSomeMatcherValue(Value *p, TypeMatch &match);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};

class UnionValue: public Value {
public:
    TypeSpec arg_ts;
    int tag_index;
    std::unique_ptr<Value> value;

    UnionValue(TypeSpec ts, TypeSpec ats, int ti);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};

class UnionMatcherValue: public GenericValue, public Raiser {
public:
    TypeSpec union_ts;
    int tag_index;

    UnionMatcherValue(Value *p, TypeSpec uts, TypeSpec rts, int ti);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};
