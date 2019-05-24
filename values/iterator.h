class SimpleRecordValue: public GenericValue {
public:
    SimpleRecordValue(TypeSpec ret_ts, Value *pivot);
    
    virtual Regs precompile(Regs preferred);
};

// Counters
class CountupNextValue: public GenericValue, public Raiser {
public:
    Regs clob;

    CountupNextValue(Value *l, TypeMatch &match);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual void advance(Address addr, X64 *x64);
    virtual Storage compile(X64 *x64);
};

class CountdownNextValue: public CountupNextValue {
public:
    CountdownNextValue(Value *l, TypeMatch &match);
    
    virtual void advance(Address addr, X64 *x64);
};

class CountupValue: public SimpleRecordValue {
public:
    CountupValue(Value *l, TypeMatch &match);
    
    virtual Storage compile(X64 *x64);
};

class CountdownValue: public SimpleRecordValue {
public:
    CountdownValue(Value *l, TypeMatch &match);
    
    virtual Storage compile(X64 *x64);
};
