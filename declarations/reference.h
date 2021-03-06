
class ReferenceType: public Type {
public:
    ReferenceType(std::string name);

    virtual Allocation measure(TypeMatch tm);
    virtual void store(TypeMatch tm, Storage s, Storage t, Cx *cx);
    virtual void create(TypeMatch tm, Storage s, Storage t, Cx *cx);
    virtual void destroy(TypeMatch tm, Storage s, Cx *cx);
    virtual void equal(TypeMatch tm, Storage s, Storage t, Cx *cx);
    virtual void compare(TypeMatch tm, Storage s, Storage t, Cx *cx);
    virtual void streamify(TypeMatch tm, Cx *cx);
    virtual StorageWhere where(TypeMatch tm, AsWhat as_what);
    virtual Storage optimal_value_storage(TypeMatch tm, Regs preferred);
    virtual Value *lookup_inner(TypeMatch tm, std::string n, Value *v, Scope *s);
    virtual Value *lookup_initializer(TypeMatch tm, std::string name, Scope *scope);
    virtual Value *lookup_matcher(TypeMatch tm, std::string name, Value *pivot, Scope *scope);
    virtual devector<VirtualEntry *> get_virtual_table(TypeMatch tm);
    virtual Label get_virtual_table_label(TypeMatch tm, Cx *cx);
    virtual Label get_interface_table_label(TypeMatch tm, Cx *cx);
    virtual Value *autoconv(TypeMatch tm, Type *target, Value *orig, TypeSpec &ifts);
    virtual void type_info(TypeMatch tm, Cx *cx);
};

class PointerType: public ReferenceType {
public:
    PointerType(std::string name);
};
