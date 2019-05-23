class IdentityType: public Type, public Inheritable {
public:
    std::vector<Allocable *> member_allocables;
    std::vector<std::string> member_names;
    std::vector<Function *> member_functions;
    std::vector<Associable *> member_associables;
    bool is_abstract;
    Function *finalizer_function;

    IdentityType(std::string n, Metatypes pmts, MetaType *mt, bool ia);

    virtual TypeSpec make_pivot_ts();
    virtual DataScope *make_inner_scope();
    virtual bool complete_type();
    virtual Associable *get_base_role();
    virtual Associable *get_main_role();
    virtual void get_heritage(std::vector<Associable *> &assocs, std::vector<Function *> &funcs);
    virtual Allocation measure(TypeMatch tm);
    virtual Allocation measure_identity(TypeMatch tm);
    virtual devector<VirtualEntry *> allocate_vt();
    virtual void allocate();
    virtual void override_virtual_entry(int vi, VirtualEntry *ve);
    virtual devector<VirtualEntry *> get_virtual_table(TypeMatch tm);
    virtual void incref(TypeMatch tm, Register r, X64 *x64);
    virtual void decref(TypeMatch tm, Register r, X64 *x64);
    virtual void streamify(TypeMatch tm, X64 *x64);
    virtual Value *lookup_matcher(TypeMatch tm, std::string n, Value *v, Scope *s);
    virtual Value *lookup_inner(TypeMatch tm, std::string n, Value *v, Scope *s);
};

class AbstractType: public IdentityType {
public:
    AbstractType(std::string name, Metatypes param_metatypes);

    virtual devector<VirtualEntry *> allocate_vt();
    virtual Label get_interface_table_label(TypeMatch tm, X64 *x64);
    static void compile_ift(Label label, TypeSpec ts, X64 *x64);
    virtual void type_info(TypeMatch tm, X64 *x64);
};

class ClassType: public IdentityType, public Autoconvertible, public PartialInitializable {
public:
    AutoconvVirtualEntry *autoconv_ve;
    FfwdVirtualEntry *fastforward_ve;

    ClassType(std::string name, Metatypes param_metatypes);

    virtual std::vector<std::string> get_partial_initializable_names();
    virtual void destroy(TypeMatch tm, Storage s, X64 *x64);
    virtual Value *lookup_initializer(TypeMatch tm, std::string name, Scope *scope);
    virtual devector<VirtualEntry *> allocate_vt();
    virtual std::vector<AutoconvEntry> get_autoconv_table(TypeMatch tm);
    virtual Label get_autoconv_table_label(TypeMatch tm, X64 *x64);
    static void compile_act(Label label, TypeSpec ts, X64 *x64);
    virtual Label get_virtual_table_label(TypeMatch tm, X64 *x64);
    virtual Label get_finalizer_label(TypeMatch tm, X64 *x64);
    static void compile_vt(Label label, TypeSpec ts, X64 *x64);
    static void compile_finalizer(Label label, TypeSpec ts, X64 *x64);
    virtual void init_vt(TypeMatch tm, Address self_addr, X64 *x64);
    virtual Value *lookup_inner(TypeMatch tm, std::string n, Value *v, Scope *s);
    virtual void type_info(TypeMatch tm, X64 *x64);
};

class Role: public Associable, public Autoconvertible {
public:
    devector<VirtualEntry *> vt;
    Label vt_label;
    Label act_label;

    Role(std::string n, PivotRequirement pr, TypeSpec ts, InheritAs ia, bool ama);
    Role(std::string p, Associable *original, TypeMatch etm);

    virtual bool is_abstract();
    virtual Associable *make_shadow(std::string prefix, TypeMatch explicit_tm);
    virtual Value *matched(Value *cpivot, Scope *scope, TypeMatch &match);
    virtual Value *make_value(Value *orig, TypeMatch tm);
    virtual devector<VirtualEntry *> get_virtual_table_fragment();
    virtual void override_virtual_entry(int vi, VirtualEntry *ve);
    virtual bool check_provisioning(std::string override_name, Associable *provider_associable);
    virtual void allocate();
    virtual void relocate(Allocation explicit_offset);
    virtual void destroy(TypeMatch tm, Storage s, X64 *x64);
    virtual void compile_vt(TypeMatch tm, X64 *x64);
    virtual void init_vt(TypeMatch tm, Address self_addr, X64 *x64);
    virtual std::vector<AutoconvEntry> get_autoconv_table(TypeMatch tm);
    virtual Label get_autoconv_table_label(TypeMatch tm, X64 *x64);
    void compile_act(TypeMatch tm, X64 *x64);
};
