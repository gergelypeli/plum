
class Provision: public Identifier {
public:
    Associable *provider_associable;
    
    Provision(std::string n, Associable *pa);
};


class Inheritable {
public:
    virtual void get_heritage(std::vector<Associable *> &assocs, std::vector<Function *> &funcs);
    virtual void override_virtual_entry(int vi, VirtualEntry *ve);
};


const std::string MAIN_ROLE_NAME = "@";
const std::string BASE_ROLE_NAME = "";
const std::string QUALIFIER_NAME = ".";


class Associable: public Allocable, public Inheritable {
public:
    std::string prefix;
    Inheritable *parent;
    InheritAs inherit_as;
    bool am_autoconv;
    
    int virtual_index;  // of the entry that stores the data offset to the role
    Associable *original_associable;
    Associable *provider_associable;
    std::vector<std::unique_ptr<Associable>> shadow_associables;
    std::vector<Function *> functions;
    std::set<std::string> associated_names;
    TypeMatch explicit_tm;

    Associable(std::string n, PivotRequirement pr, TypeSpec ts, InheritAs ia, bool ama);
    Associable(std::string p, Associable *original, TypeMatch etm);
    
    static std::string mkname(std::string prefix, Associable *original);
    virtual void set_parent(Inheritable *p);
    virtual Associable *make_shadow(std::string prefix, TypeMatch explicit_tm);
    virtual Value *make_value(Value *orig, TypeMatch tm);
    virtual devector<VirtualEntry *> get_virtual_table_fragment();
    virtual void inherit();
    virtual void get_heritage(std::vector<Associable *> &assocs, std::vector<Function *> &funcs);
    virtual void provision(Associable *pa);
    virtual int get_offset(TypeMatch tm);
    virtual Allocation subst_offset(TypeMatch tm);
    virtual bool has_base_role();
    virtual bool has_main_role();
    virtual Associable *get_head_role();
    virtual void set_head_role(Associable *a);
    virtual void insert_head_role(Associable *a);
    virtual void set_outer_scope(Scope *os);
    virtual void outer_scope_left();
    virtual void check_full_implementation();
    virtual Associable *lookup_associable(std::string n);
    virtual bool check_provisioning(std::string override_name, Associable *provider_associable);
    virtual bool check_associated(Declaration *d);
    virtual bool is_requiring();
    virtual bool is_in_requiring();
    virtual bool is_autoconv();
    virtual bool is_baseconv();
    virtual bool is_mainconv();
    virtual Value *autoconv(TypeMatch tm, Type *target, Value *orig, TypeSpec &ifts);
    virtual Associable *autoconv_streamifiable(TypeMatch match);
    virtual void streamify(TypeMatch tm, Cx *cx);
    virtual void relocate(Allocation explicit_offset);
    virtual void compile_vt(TypeMatch tm, Cx *cx);
    virtual void init_vt(TypeMatch tm, Address self_addr, Cx *cx);
    virtual void compile_act(TypeMatch tm, Cx *cx);
};


class DataVirtualEntry: public VirtualEntry {
public:
    Associable *associable;
    
    DataVirtualEntry(Associable *a);
    
    virtual Label get_virtual_entry_label(TypeMatch tm, Cx *cx);
    virtual std::ostream &out_virtual_entry(std::ostream &os, TypeMatch tm);
};


void dump_associable(Associable *a, int indent);
