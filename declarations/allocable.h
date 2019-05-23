

class Allocable: public Identifier {
public:
    TypeSpec alloc_ts;
    Allocation offset;
    StorageWhere where;
    
    Allocable(std::string name, PivotRequirement pr, TypeSpec ats);
    
    virtual bool is_abstract();
    virtual TypeSpec get_typespec(TypeMatch tm);
    virtual Scope *get_allocation_scope();
    virtual void allocate();
    virtual Regs borrowing_requirements();
    virtual int get_offset(TypeMatch tm);
    virtual Storage get_storage(TypeMatch tm, Storage s);
    virtual Storage get_local_storage();
    virtual void destroy(TypeMatch tm, Storage s, X64 *x64);
};


class Variable: public Allocable {
public:
    AsWhat as_what;
    
    Variable(std::string name, TypeSpec vts);
    
    virtual void set_outer_scope(Scope *os);
    virtual TypeSpec get_typespec(TypeMatch tm);
    virtual Value *matched(Value *cpivot, Scope *scope, TypeMatch &match);
    virtual void allocate();
    virtual void finalize(X64 *x64);
    virtual void create(TypeMatch tm, Storage s, Storage t, X64 *x64);
    virtual void store(TypeMatch tm, Storage s, Storage t, X64 *x64);
    virtual void destroy(TypeMatch tm, Storage s, X64 *x64);
    virtual void equal(TypeMatch tm, Storage s, Storage t, X64 *x64);
    virtual void compare(TypeMatch tm, Storage s, Storage t, X64 *x64);
    virtual void debug(TypeMatch tm, X64 *x64);
};


class SelfVariable: public Variable {
public:
    std::unique_ptr<SelfInfo> self_info;
    
    SelfVariable(std::string name, TypeSpec vts);
    
    virtual Value *matched(Value *cpivot, Scope *scope, TypeMatch &match);
};


class PartialVariable: public Variable {
public:
    std::unique_ptr<PartialInfo> partial_info;

    PartialVariable(std::string name, TypeSpec vts);
    
    virtual Value *matched(Value *cpivot, Scope *scope, TypeMatch &match);
};


class GlobalVariable: public Variable {
public:
    TypeSpec class_ts;  // TODO: allow overriding this
    Function *initializer_function;
    
    GlobalVariable(std::string n, TypeSpec mts, TypeSpec cts);
    
    virtual void set_initializer_function(Function *f);
    virtual TypeSpec get_class_ts();
    virtual bool is_called(std::string n);
    virtual bool is_abstract();
    virtual Scope *get_allocation_scope();
    virtual void allocate();
    virtual Label compile_initializer(X64 *x64);
    virtual Label compile_finalizer(X64 *x64);
    virtual void debug(TypeMatch tm, X64 *x64);
};


// This is a mutant global variable, used with Unit subtypes that won't be passed
// as pivot arguments, useful for grouping built-in functions in a namespace.
class GlobalNamespace: public GlobalVariable {
public:
    GlobalNamespace(std::string n, TypeSpec mts);
    
    virtual Regs borrowing_requirements();
    virtual void allocate();
    virtual Storage get_local_storage();
    virtual Label compile_initializer(X64 *x64);
    virtual Label compile_finalizer(X64 *x64);
    virtual void debug(TypeMatch tm, X64 *x64);
};

// TODO: remove
class RetroVariable: public Variable {
public:
    RetroVariable(std::string name, TypeSpec vts);
    
    virtual void finalize(X64 *x64);
    virtual void allocate();
};


class Evaluable: public Allocable {
public:
    std::vector<Variable *> dvalue_variables;
    
    Evaluable(std::string name, TypeSpec vts);
    
    virtual void set_outer_scope(Scope *os);
    virtual std::vector<Variable *> get_dvalue_variables();
    virtual Value *matched(Value *cpivot, Scope *scope, TypeMatch &match);
    virtual void allocate();
    virtual void debug(TypeMatch tm, X64 *x64);
};
