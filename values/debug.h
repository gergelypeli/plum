class FrameNameValue: public Value, public Raiser {
public:
    std::unique_ptr<Value> value;

    FrameNameValue(Value *, TypeMatch);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};

class FrameStuffValue: public Value, public Raiser {
public:
    std::unique_ptr<Value> value;

    FrameStuffValue(Value *, TypeMatch);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};

class DoubleStackValue: public Value {
public:
    DoubleStackValue(Value *, TypeMatch);
    
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};

class DieValue: public Value {
public:
    DieValue(Value *p, TypeMatch tm);
    
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};
