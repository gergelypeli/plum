class CodeScopeValue: public Value {
public:
    std::unique_ptr<Value> value;
    CodeScope *code_scope;
    Storage save_storage;

    CodeScopeValue(Value *v, CodeScope *s, TypeSpec ts);

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile_body(Cx *cx);
    virtual Storage compile(Cx *cx);
    virtual CodeScope *unwind(Cx *cx);
    virtual void escape_statement_variables();
};

class RetroScopeValue: public CodeScopeValue, public Deferrable {
public:
    RetroScopeValue(Value *v, CodeScope *s, TypeSpec ts);
    
    virtual void deferred_compile(Label label, Cx *cx);
    virtual Storage compile(Cx *cx);
};

class DataBlockValue: public Value {
public:
    std::vector<std::unique_ptr<Value>> statements;

    DataBlockValue();
    
    virtual bool check_statement(Expr *expr, Scope *scope);
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);      
    virtual bool check_tuple(Expr *expr, Scope *scope);
    virtual bool define_data();
    virtual bool define_code();
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
};

class CodeBlockValue: public Value {
public:
    std::vector<std::unique_ptr<Value>> statements;
    TypeSpec last_statement_context;
    
    CodeBlockValue(TypeSpec lsc);
    
    virtual bool add_statement(Value *value, bool result = false);
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
};

class TupleBlockValue: public Value {
public:
    std::vector<std::unique_ptr<Value>> statements;
    TSs context_tss;
    std::vector<Storage> pushed_storages;

    TupleBlockValue(TypeSpec tuple_ts);

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
    virtual CodeScope *unwind(Cx *cx);
};

class DeclarationValue: public Value {
public:
    std::string name;
    Declaration *decl;
    Variable *var;
    std::unique_ptr<Value> value;
    TypeSpec *context;

    DeclarationValue(std::string n, TypeSpec *c = NULL);

    virtual std::string get_name();
    virtual Declaration *get_decl();
    virtual Variable *get_var();
    virtual void fix_bare(TypeSpec var_ts, Scope *scope);
    virtual void fix_bare_retro(TypeSpec var_ts, Scope *scope);
    virtual bool associate(Declaration *decl, std::string name);
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual bool define_data();
    virtual bool define_code();
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
    virtual void escape_statement_variables();
};

class RetroArgumentValue: public Value {
public:
    std::unique_ptr<Value> value;
    RetroArgumentScope *retro_argument_scope;

    RetroArgumentValue(Value *v, RetroArgumentScope *ras);

    virtual bool check();
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
};

class CreateValue: public GenericOperationValue {
public:
    CreateValue(Value *l, TypeMatch &tm);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual void use(Value *r);
    virtual Declaration *get_decl();
    virtual void escape_statement_variables();
};
