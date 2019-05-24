// The value of a :Function control
class FunctionDefinitionValue: public Value {
public:
    std::vector<std::unique_ptr<Value>> results;
    std::unique_ptr<DataBlockValue> head;
    std::unique_ptr<Value> body;
    std::unique_ptr<Value> exception_type_value;
    FunctionScope *fn_scope;
    std::unique_ptr<Expr> deferred_body_expr;
    Variable *self_var;
    TypeMatch match;
    Scope *outer_scope;
    FunctionType type;
    std::string import_name;
    Function *function;  // If declared with a name, which is always, for now

    FunctionDefinitionValue(Value *r, TypeMatch &tm);

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual bool define_code();
    virtual Regs precompile(Regs);
    virtual void fix_arg(Declaration *d, X64 *x64);
    virtual Storage compile(X64 *x64);
    virtual CodeScope *unwind(X64 *x64);
    virtual Declaration *declare(std::string name, Scope *scope);
};

class InitializerDefinitionValue: public FunctionDefinitionValue {
public:
    InitializerDefinitionValue(Value *r, TypeMatch &match);
};

class FinalizerDefinitionValue: public FunctionDefinitionValue {
public:
    FinalizerDefinitionValue(Value *r, TypeMatch &match);
};

class ProcedureDefinitionValue: public FunctionDefinitionValue {
public:
    ProcedureDefinitionValue(Value *r, TypeMatch &match);
};

// The value of calling a function
class FunctionCallValue: public Value, public Raiser {
public:
    Function *function;
    std::unique_ptr<Value> pivot;
    std::vector<std::unique_ptr<Value>> values;
    Register reg;
    unsigned res_total;
    std::vector<TypeSpec> pushed_tss;
    std::vector<Storage> pushed_storages;
    std::vector<unsigned> pushed_sizes;
    std::vector<Storage> pushed_stackfixes;
    TypeSpec pivot_ts;
    Storage pivot_alias_storage;
    std::vector<TypeSpec> arg_tss;
    std::vector<TypeSpec> res_tss;
    std::vector<std::string> arg_names;
    bool has_code_arg;
    Associable *static_role;

    FunctionCallValue(Function *f, Value *p, TypeMatch &m);
    
    virtual void be_static(Associable *sr);
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual void call_static(X64 *x64, unsigned passed_size);
    virtual void call_virtual(X64 *x64, unsigned passed_size);
    virtual Regs precompile(Regs preferred);
    virtual void push_arg(TypeSpec arg_ts, Value *arg_value, X64 *x64);
    virtual void pop_arg(X64 *x64);
    virtual Storage compile(X64 *x64);
    virtual CodeScope *unwind(X64 *x64);
};

class FunctionReturnValue: public Value {
public:
    std::vector<Variable *> result_vars;
    std::vector<std::unique_ptr<Value>> values;
    Declaration *dummy;
    std::vector<Storage> var_storages;
    TypeMatch match;
    FunctionScope *fn_scope;

    FunctionReturnValue(OperationType o, Value *v, TypeMatch &m);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs);
    virtual Storage compile(X64 *x64);
    virtual CodeScope *unwind(X64 *x64);
};
