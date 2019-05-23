
class TypeSpec;
typedef std::array<TypeSpec,4> TypeMatch;
class X64;

enum AsWhat {
    AS_VALUE, AS_VARIABLE, AS_ARGUMENT, AS_LVALUE_ARGUMENT
};


class TypeSpec: public std::vector<Type *> {
public:
    TypeSpec();
    TypeSpec(iterator tsi);
    TypeSpec(std::initializer_list<Type *> il):std::vector<Type *>(il) {}
    TypeSpec(Type *t, TypeSpec &tm1, TypeSpec &tm2);

    std::string symbolize(std::string suffix = "");
    TypeMatch match();    
    Allocation measure();
    Allocation measure_identity();
    int measure_raw();
    int measure_stack();
    int measure_where(StorageWhere where);
    Storage optimal_value_storage(Regs preferred);
    devector<VirtualEntry *> get_virtual_table();
    Label get_virtual_table_label(X64 *x64);
    Label get_interface_table_label(X64 *x64);
    Label get_finalizer_label(X64 *x64);
    Value *autoconv(Type *target, Value *orig, TypeSpec &ifts);
    StorageWhere where(AsWhat as_what);
    TypeSpec prefix(Type *t);
    TypeSpec unprefix(Type *t = NULL);
    TypeSpec reprefix(Type *s, Type *t);
    TypeSpec rvalue();
    TypeSpec lvalue();
    bool is_tuple();
    void pack_tuple(std::vector<TypeSpec> &tss);
    void unpack_tuple(std::vector<TypeSpec> &tss);
    bool has_meta(MetaType *mt);
    bool is_meta();
    bool is_hyper();
    Storage store(Storage s, Storage t, X64 *x64);
    Storage create(Storage s, Storage t, X64 *x64);
    void destroy(Storage s, X64 *x64);
    void equal(Storage s, Storage t, X64 *x64);
    void compare(Storage s, Storage t, X64 *x64);
    void streamify(X64 *x64);
    Value *lookup_initializer(std::string name, Scope *scope);
    Value *lookup_matcher(std::string name, Value *pivot, Scope *scope);
    Value *lookup_inner(std::string name, Value *pivot, Scope *scope);
    void init_vt(Address self_addr, X64 *x64);
    void incref(Register r, X64 *x64);
    void decref(Register r, X64 *x64);
    void type_info(X64 *x64);
};

typedef TypeSpec::iterator TypeSpecIter;
typedef std::vector<TypeSpec> TSs;
typedef std::vector<std::string> Ss;
std::ostream &operator<<(std::ostream &os, const TypeSpec &ts);
std::ostream &operator<<(std::ostream &os, const TSs &tss);
std::ostream &operator<<(std::ostream &os, const TypeMatch &tm);

