enum FunctionType {
    GENERIC_FUNCTION, LVALUE_FUNCTION, INITIALIZER_FUNCTION, FINALIZER_FUNCTION
};

enum FunctionProt {
    NATIVE_FUNCTION, SYSV_FUNCTION
};

//VirtualEntry *make_method_virtual_entry(Function *f);
class Function: public Identifier, public Methodlike {
public:
    std::vector<TypeSpec> arg_tss;
    std::vector<std::string> arg_names;
    std::vector<TypeSpec> res_tss;
    TreenumerationType *exception_type;
    FunctionScope *fn_scope;
    int virtual_index;
    FunctionType type;
    FunctionProt prot;
    Associable *associated;
    Function *implemented_function;
    Label label;
    int low_pc;
    int high_pc;

    Function(std::string n, PivotRequirement pr, FunctionType ft, std::vector<TypeSpec> ats, std::vector<std::string> ans, std::vector<TypeSpec> rts, TreenumerationType *et, FunctionScope *fs);

    virtual Function *clone_abstract(std::string prefix);
    virtual bool is_abstract();
    virtual void set_outer_scope(Scope *os);
    virtual Value *matched(Value *cpivot, Scope *scope, TypeMatch &match);
    virtual void get_parameters(TypeSpec &pts, TSs &rtss, TSs &atss, Ss &anames, TypeSpec ts, TypeMatch tm);
    virtual void allocate();
    virtual Label get_label(X64 *x64);
    virtual bool does_implement(TypeMatch tm, Function *iff, TypeMatch iftm);
    virtual void set_associated(Associable *a);
    virtual Label get_method_label(X64 *x64);
    virtual std::string get_method_name();
    virtual void set_pc_range(int lo, int hi);
    virtual void debug(TypeMatch tm, X64 *x64);
};

class SysvFunction: public Function, public Deferrable {
public:
    std::string import_name;

    SysvFunction(std::string in, std::string n, PivotRequirement pr, FunctionType ft, std::vector<TypeSpec> ats, std::vector<std::string> ans, std::vector<TypeSpec> rts, TreenumerationType *et = NULL, FunctionScope *fs = NULL);

    virtual Label get_label(X64 *x64);
    virtual void deferred_compile(Label label, X64 *x64);
    virtual void debug(TypeMatch tm, X64 *x64);
};

class ImportedFloatFunction: public Identifier {
public:
    std::string import_name;
    TypeSpec arg_ts;
    TypeSpec res_ts;

    ImportedFloatFunction(std::string in, std::string n, PivotRequirement pr, TypeSpec ats, TypeSpec rts);

    virtual Value *matched(Value *cpivot, Scope *scope, TypeMatch &match);
    virtual Label get_label(X64 *x64);
};
