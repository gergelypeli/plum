class FloatType: public Type {
public:
    FloatType(std::string n, MetaType *mt = NULL);

    virtual Allocation measure(TypeMatch tm);
    virtual void store(TypeMatch tm, Storage s, Storage t, X64 *x64);
    virtual void create(TypeMatch tm, Storage s, Storage t, X64 *x64);
    virtual void destroy(TypeMatch tm, Storage s, X64 *x64);
    virtual void equal(TypeMatch tm, Storage s, Storage t, X64 *x64);
    virtual void compare(TypeMatch tm, Storage s, Storage t, X64 *x64);
    virtual StorageWhere where(TypeMatch tm, AsWhat as_what);
    virtual Storage optimal_value_storage(TypeMatch tm, Regs preferred);
    virtual void streamify(TypeMatch tm, X64 *x64);
    virtual Value *lookup_initializer(TypeMatch tm, std::string name, Scope *scope);
    virtual void type_info(TypeMatch tm, X64 *x64);
};
