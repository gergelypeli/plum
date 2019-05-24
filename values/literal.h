// This class must only return 32-bit constants as CONSTANT storage class,
// because that is what we can treat as immediate value in instructions.
// If a greater value is specified, it must be loaded into a register, and
// returned as REGISTER.

class BasicValue: public Value {
public:
    Register reg;
    int64 number;

    BasicValue(TypeSpec ts, int64 n);
    
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};

class FloatValue: public Value {
public:
    double number;

    FloatValue(TypeSpec ts, double n);
    
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};

class UnicodeCharacterValue: public Value {
public:
    std::unique_ptr<Value> value;

    UnicodeCharacterValue();
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};

class StringLiteralValue: public Value {
public:
    std::ustring utext;
    
    StringLiteralValue(std::ustring ut);
    
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};

class StringTemplateValue: public Value {
public:
    std::vector<std::ustring> fragments;

    StringTemplateValue(std::vector<std::ustring> f);
    
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};

class TreenumerationMatcherValue: public GenericValue, public Raiser {
public:
    int index;
    
    TreenumerationMatcherValue(int i, Value *p);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};

class TreenumerationAnyMatcherValue: public GenericValue {
public:
    TreenumerationAnyMatcherValue(Value *p);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};
