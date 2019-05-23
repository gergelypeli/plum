
class OptionType: public Type {
public:
    OptionType(std::string n);

    static int get_flag_size(TypeSpec some_ts);
    virtual Allocation measure(TypeMatch tm);
    virtual void store(TypeMatch tm, Storage s, Storage t, X64 *x64);
    virtual void create(TypeMatch tm, Storage s, Storage t, X64 *x64);
    virtual void destroy(TypeMatch tm, Storage s, X64 *x64);
    virtual StorageWhere where(TypeMatch tm, AsWhat as_what);
    virtual void equal(TypeMatch tm, Storage s, Storage t, X64 *x64);
    virtual void compare(TypeMatch tm, Storage s, Storage t, X64 *x64);
    virtual void streamify(TypeMatch tm, X64 *x64);
    static void compile_streamification(Label label, TypeSpec some_ts, X64 *x64);
    virtual Value *lookup_initializer(TypeMatch tm, std::string n, Scope *scope);
    virtual Value *lookup_matcher(TypeMatch tm, std::string n, Value *pivot, Scope *s);
    virtual void type_info(TypeMatch tm, X64 *x64);
};

class UnionType: public Type {
public:
    // TODO: allow default initialization if the first type is Unit?
    std::vector<std::string> tags;
    std::vector<TypeSpec> tss;

    UnionType(std::string n);

    virtual bool complete_type();
    static int get_flag_size();
    virtual Allocation measure(TypeMatch tm);
    virtual void store(TypeMatch tm, Storage s, Storage t, X64 *x64);
    virtual void create(TypeMatch tm, Storage s, Storage t, X64 *x64);
    virtual void destroy(TypeMatch tm, Storage s, X64 *x64);
    virtual StorageWhere where(TypeMatch tm, AsWhat as_what);
    virtual void equal(TypeMatch tm, Storage s, Storage t, X64 *x64);
    virtual void compare(TypeMatch tm, Storage s, Storage t, X64 *x64);
    virtual void streamify(TypeMatch tm, X64 *x64);
    static void compile_streamification(Label label, TypeSpec union_ts, X64 *x64);
    virtual Value *lookup_initializer(TypeMatch tm, std::string n, Scope *scope);
    virtual Value *lookup_matcher(TypeMatch tm, std::string n, Value *pivot, Scope *s);
    virtual void type_info(TypeMatch tm, X64 *x64);
};
