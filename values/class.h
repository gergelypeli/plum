Storage preinitialize_class(TypeSpec class_ts, Cx *cx);

class ClassPreinitializerValue: public Value {
public:
    ClassPreinitializerValue(TypeSpec rts);
    
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
};

class ClassPostinitializerValue: public Value {
public:
    std::unique_ptr<Value> pivot;

    ClassPostinitializerValue(TypeSpec mts, Value *p);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
};

class ClassWrapperInitializerValue: public Value {
public:
    std::unique_ptr<Value> object, value;
    
    ClassWrapperInitializerValue(Value *o, Value *v);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
};

// TODO: rename to AbstractMatcher, and validate name sooner!
class ClassMatcherValue: public Value, public Raiser {
public:
    std::string name;
    std::unique_ptr<Value> value;
    
    ClassMatcherValue(std::string n, Value *v);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
};
