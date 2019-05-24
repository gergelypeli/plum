class LvalueTupleValue: public Value {
public:
    std::vector<std::unique_ptr<Value>> values;
    std::vector<TypeSpec> tss;
    std::vector<Storage> storages;

    LvalueTupleValue();

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual std::vector<Storage> get_storages();
    virtual Declaration *get_declaration(int i);
    virtual TypeSpec fix_bare(int i, TypeSpec implicit_ts, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
    virtual CodeScope *unwind(X64 *x64);
};

class UnpackingValue: public Value {
public:
    std::unique_ptr<LvalueTupleValue> left;
    std::unique_ptr<Value> right;
    std::vector<TypeSpec> left_tss, right_tss;
    int left_total;
    std::vector<Declaration *> declarations;
    Scope *scope;

    UnpackingValue(Value *l);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
    virtual CodeScope *unwind(X64 *x64);
    virtual void escape_statement_variables();
};

class ScalarConversionValue: public Value {
public:
    std::unique_ptr<Value> orig;
    std::vector<TypeSpec> tss;
    
    ScalarConversionValue(Value *p);
    
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};
