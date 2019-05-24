
class RoleCreator {
protected:
    virtual bool extend_main_role(Associable *base_role, Associable *main_role);
    virtual Associable *add_head_role(Scope *is, std::string name, TypeSpec main_ts, TypeSpec base_ts, InheritAs ia, bool isa);
};

class TypeDefinitionValue: public Value {
public:
    TypeDefinitionValue();
    
    virtual Regs precompile(Regs);
    virtual Storage compile(X64 *);
    virtual TypeSpec typize_typespec(Expr *expr, Scope *scope, MetaType *meta_type);
};

class IntegerDefinitionValue: public TypeDefinitionValue {
public:
    int size;
    bool is_not_signed;
    std::unique_ptr<Value> bs, iu;

    IntegerDefinitionValue();
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Declaration *declare(std::string name, Scope *scope);
};

class EnumerationDefinitionValue: public TypeDefinitionValue {
public:
    std::vector<std::string> keywords;

    EnumerationDefinitionValue();
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Declaration *declare(std::string name, Scope *scope);
};

class TreenumerationDefinitionValue: public TypeDefinitionValue {
public:
    std::vector<std::string> keywords;
    std::vector<unsigned> parents;

    TreenumerationDefinitionValue();
    
    virtual unsigned add_keyword(std::string kw, unsigned parent);
    virtual bool parse_level(Args &args, unsigned parent);
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Declaration *declare(std::string name, Scope *scope);
};

class RoleDefinitionValue: public TypeDefinitionValue, public RoleCreator {
public:
    TypeSpec implemented_ts, inherited_ts, required_ts;
    InheritAs inherit_as;
    bool is_concrete;
    bool is_autoconv;

    RoleDefinitionValue(Value *pivot, TypeMatch &tm, InheritAs ia, bool isa);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Declaration *declare(std::string name, Scope *scope);
};

class PlainRoleDefinitionValue: public RoleDefinitionValue {
public:
    PlainRoleDefinitionValue(Value *pivot, TypeMatch &tm);
};

class AutoRoleDefinitionValue: public RoleDefinitionValue {
public:
    AutoRoleDefinitionValue(Value *pivot, TypeMatch &tm);
};

class RequireRoleDefinitionValue: public RoleDefinitionValue {
public:
    RequireRoleDefinitionValue(Value *pivot, TypeMatch &tm);
};

class ProvideDefinitionValue: public TypeDefinitionValue {
public:
    Associable *provider_associable;

    ProvideDefinitionValue(Value *pivot, TypeMatch &tm);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Declaration *declare(std::string name, Scope *scope);
};

class ImportDefinitionValue: public TypeDefinitionValue {
public:
    ImportScope *import_scope;

    ImportDefinitionValue();
    
    virtual bool check_identifier(Expr *e);
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Declaration *declare(std::string name, Scope *scope);
};

class GlobalDefinitionValue: public TypeDefinitionValue {
public:
    TypeSpec main_ts, class_ts;
    GlobalVariable *gvar;

    GlobalDefinitionValue(Value *null, TypeMatch &tm);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual bool define_data();
    virtual Declaration *declare(std::string name, Scope *scope);
};

class ScopedTypeDefinitionValue: public TypeDefinitionValue {
public:
    Type *defined_type;
    Args meta_args;
    Args data_args;
    std::unique_ptr<DataBlockValue> block_value;

    ScopedTypeDefinitionValue();
    
    void defer(std::unique_ptr<Expr> e);
    void defer_as(std::unique_ptr<Expr> as_expr);
    virtual Type *define(Type *dt, Scope *s);
    virtual bool define_data_prehook();
    virtual bool define_data();
    virtual bool define_code();
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};

class RecordDefinitionValue: public ScopedTypeDefinitionValue {
public:
    RecordDefinitionValue();

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Declaration *declare(std::string name, Scope *scope);
};

class UnionDefinitionValue: public ScopedTypeDefinitionValue {
public:
    UnionDefinitionValue();
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Declaration *declare(std::string name, Scope *scope);
};

class AbstractDefinitionValue: public ScopedTypeDefinitionValue {
public:
    std::unique_ptr<Expr> base_expr;

    AbstractDefinitionValue();
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual bool define_data_prehook();
    virtual Declaration *declare(std::string name, Scope *scope);
};

class ClassDefinitionValue: public ScopedTypeDefinitionValue, public RoleCreator {
public:
    std::unique_ptr<Expr> base_expr;
    std::unique_ptr<Expr> main_expr;

    ClassDefinitionValue();
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Declaration *declare(std::string name, Scope *scope);
    virtual bool define_data_prehook();
};

// TODO: allow users define Interfaces?
class InterfaceDefinitionValue: public ScopedTypeDefinitionValue {
public:
    std::unique_ptr<Expr> base_expr;

    InterfaceDefinitionValue();

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual bool define_data_prehook();
    virtual Declaration *declare(std::string name, Scope *scope);
};

class ImplementationDefinitionValue: public TypeDefinitionValue {
public:
    TypeSpec interface_ts;

    ImplementationDefinitionValue(Value *pivot, TypeMatch &tm);

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Declaration *declare(std::string name, Scope *scope);
};

// TODO: this is not strictly a type definition
class FunctorDefinitionValue: public TypeDefinitionValue, public Deferrable {
public:
    ClassType *class_type;
    std::vector<Value *> with_values;
    std::vector<Variable *> with_vars;
    Value *fdv;
    TypeMatch match;

    FunctorDefinitionValue(Value *p, TypeMatch tm);

    virtual bool check_with(Expr *e, CodeScope *cs, DataScope *is);
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual void deferred_compile(Label label, X64 *x64);
    virtual Storage compile(X64 *x64);
};
