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
    virtual void fix_arg(Declaration *d, Cx *cx);
    virtual Storage compile(Cx *cx);
    virtual CodeScope *unwind(Cx *cx);
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
    struct PushedInfo {
        TypeSpec ts;
        Storage orig_storage;
        Storage storage;
        unsigned size;
        bool is_fixed;
        bool is_borrowed;
    };
    
    Function *function;
    std::unique_ptr<Value> pivot;
    std::vector<std::unique_ptr<Value>> values;
    Register reg;
    unsigned res_total;
    std::vector<PushedInfo> pushed_infos;
    TypeSpec pivot_ts;
    std::vector<TypeSpec> arg_tss;
    std::vector<TypeSpec> res_tss;
    std::vector<std::string> arg_names;
    bool has_code_arg;
    bool may_borrow_stackvars;
    Associable *static_role;

    FunctionCallValue(Function *f, Value *p, TypeMatch &m);
    
    virtual void be_static(Associable *sr);
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual void call_static(Cx *cx, unsigned passed_size);
    virtual void call_virtual(Cx *cx, unsigned passed_size);
    virtual Regs precompile(Regs preferred);
    virtual void push_arg(TypeSpec arg_ts, Value *arg_value, Cx *cx);
    virtual void pop_arg(Cx *cx);
    virtual Storage ret_pivot(Cx *cx);
    virtual Storage compile(Cx *cx);
    virtual CodeScope *unwind(Cx *cx);
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
    virtual Storage compile(Cx *cx);
    virtual CodeScope *unwind(Cx *cx);
};
