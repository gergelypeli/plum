class TypeValue: public Value {
public:
    Type *type;
    MetaType *meta_type;
    Metatypes param_metatypes;
    TypeSpec represented_ts;

    TypeValue(Type *t, MetaType *mt, Metatypes pmts);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
    virtual Declaration *declare(std::string name, Scope *scope);
};

// Describes a type, that happens to be a tuple
class TupleTypeValue: public TypeValue {
public:
    TupleTypeValue();
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Declaration *declare(std::string name, Scope *scope);
};

// Describes a tuple, that happens to contain types
class TypeTupleValue: public Value {
public:
    std::vector<std::unique_ptr<Value>> values;
    std::vector<TypeSpec> represented_tss;
    static TypeSpec metas(std::vector<std::unique_ptr<Value>> &vs);

    TypeTupleValue(std::vector<std::unique_ptr<Value>> vs);

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Storage compile(Cx *cx);
};

class VoidConversionValue: public Value {
public:
    std::unique_ptr<Value> orig;

    VoidConversionValue(Value *o);
    
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
    virtual void escape_statement_variables();
};

class ImplementationConversionValue: public Value, public GenericLvalue {
public:
    TypeMatch match;
    Implementation *implementation;
    std::unique_ptr<Value> orig;

    ImplementationConversionValue(Implementation *imt, Value *o, TypeMatch tm);
    
    virtual void streamify(Cx *cx);
    virtual Value *lookup_inner(std::string name, Scope *scope);
    virtual void need_rvalue();
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
};
