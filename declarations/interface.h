class InterfaceType: public Type, public Inheritable {
public:
    std::vector<Function *> member_functions;
    std::vector<Declaration *> member_procedures;
    std::vector<Associable *> member_associables;

    InterfaceType(std::string name, Metatypes param_metatypes);

    virtual DataScope *make_inner_scope();
    virtual bool complete_type();
    virtual void get_heritage(std::vector<Associable *> &assocs, std::vector<Function *> &funcs);
    virtual Allocation measure_identity(TypeMatch tm);
    virtual void create(TypeMatch tm, Storage s, Storage t, X64 *x64);
    virtual void destroy(TypeMatch tm, Storage s, X64 *x64);
    virtual StorageWhere where(TypeMatch tm, AsWhat as_what);
    virtual Value *lookup_inner(TypeMatch tm, std::string n, Value *v, Scope *s);
};

class Implementation: public Associable {
public:
    Implementation(std::string name, PivotRequirement pr, TypeSpec ifts, InheritAs ia);
    Implementation(std::string p, Associable *oi, TypeMatch explicit_tm);

    virtual bool is_abstract();
    virtual Associable *make_shadow(std::string prefix, TypeMatch explicit_tm);
    virtual void set_outer_scope(Scope *os);
    virtual Value *make_value(Value *orig, TypeMatch match);
    virtual void streamify(TypeMatch tm, X64 *x64);
    virtual Value *matched(Value *pivot, Scope *scope, TypeMatch &match);
    virtual void allocate();
    virtual void relocate(Allocation explicit_offset);
    virtual void override_virtual_entry(int vi, VirtualEntry *ve);
    virtual void compile_vt(TypeMatch tm, X64 *x64);
    virtual void init_vt(TypeMatch tm, Address self_addr, X64 *x64);
    virtual void compile_act(TypeMatch tm, X64 *x64);
};

class RawStreamifiableImplementation: public Implementation {
public:
    Once::FunctionCompiler compile_streamification;

    RawStreamifiableImplementation(std::string name, Once::FunctionCompiler cs);

    virtual void check_full_implementation();
    virtual void streamify(TypeMatch tm, X64 *x64);
};

class StringRawStreamifiableImplementation: public RawStreamifiableImplementation {
public:
    StringRawStreamifiableImplementation(std::string name);

    static void compile_raw_streamification(Label label, X64 *x64);
};

class CharacterRawStreamifiableImplementation: public RawStreamifiableImplementation {
public:
    CharacterRawStreamifiableImplementation(std::string name);

    static void compile_raw_streamification(Label label, X64 *x64);
};
