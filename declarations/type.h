#define EQUAL_CLOB Regs()
#define COMPARE_CLOB Regs()
#define STREAMIFY_CLOB Regs::all()

typedef std::vector<MetaType *> Metatypes;

class Type: public Identifier {
public:
    std::string prefix;
    Metatypes param_metatypes;
    MetaType *meta_type;
    std::unique_ptr<DataScope> inner_scope;  // Won't be visible from the outer scope

    Type(std::string n, Metatypes pmts, MetaType *ut);

    virtual unsigned get_parameter_count();
    virtual void set_outer_scope(Scope *os);
    virtual TypeSpec make_pivot_ts();
    virtual DataScope *make_inner_scope();
    virtual DataScope *get_inner_scope();
    virtual void outer_scope_entered();
    virtual void outer_scope_left();
    virtual Value *matched(Value *pivot, Scope *scope, TypeMatch &match);
    virtual Value *match(std::string name, Value *pivot, Scope *scope);
    virtual void allocate();
    virtual StorageWhere where(TypeMatch tm, AsWhat as_what);
    virtual Allocation measure(TypeMatch tm);
    virtual Allocation measure_identity(TypeMatch tm);
    virtual Storage optimal_value_storage(TypeMatch tm, Regs preferred);
    virtual void store(TypeMatch tm, Storage s, Storage t, Cx *cx);
    virtual void create(TypeMatch tm, Storage s, Storage t, Cx *cx);
    virtual void destroy(TypeMatch tm, Storage s, Cx *cx);
    // Allowed to clobber EQUAL_CLOB
    // Returns result in ZF (set iff equal)
    virtual void equal(TypeMatch tm, Storage s, Storage t, Cx *cx);
    // Allowed to clobber COMPARE_CLOB
    // Returns result in R10B (-1/0/+1), and the flags (below&less/equal/above&greater)
    virtual void compare(TypeMatch tm, Storage s, Storage t, Cx *cx);
    // NOTE: allowed to clobber STREAMIFY_CLOB, because it is mostly called
    // from interpolation, which is in Void context, so not much is lost. But
    // nested streamifications must take care!
    virtual void streamify(TypeMatch tm, Cx *cx);
    virtual Value *lookup_initializer(TypeMatch tm, std::string n, Scope *scope);
    virtual Value *lookup_matcher(TypeMatch tm, std::string n, Value *pivot, Scope *scope);
    virtual Value *lookup_inner(TypeMatch tm, std::string n, Value *v, Scope *s);
    virtual devector<VirtualEntry *> get_virtual_table(TypeMatch tm);
    virtual Label get_virtual_table_label(TypeMatch tm, Cx *cx);
    virtual Label get_interface_table_label(TypeMatch tm, Cx *cx);
    virtual Label get_finalizer_label(TypeMatch tm, Cx *cx);
    virtual Value *autoconv_scope(Scope *scope, TypeMatch tm, Type *target, Value *orig, TypeSpec &ifts);
    virtual Value *autoconv(TypeMatch tm, Type *target, Value *orig, TypeSpec &ifts);
    virtual void init_vt(TypeMatch tm, Address addr, Cx *cx);
    virtual void incref(TypeMatch tm, Register r, Cx *cx);
    virtual void decref(TypeMatch tm, Register r, Cx *cx);
    virtual bool complete_type();
    virtual void debug_inner_scopes(TypeMatch tm, Cx *cx);
    virtual void type_info(TypeMatch tm, Cx *cx);
};

class MetaType: public Type {
public:
    // Plain Type-s get their parameters in the form of pre-evaluated type names, and
    // produce variables by declaring them to those types.
    // MetaTypes get their parameters in the form of keyword arguments to be evaluated
    // later, and produce types by declaring them to the resulting type.
    std::vector<MetaType *> super_types;
    typedef Value *(*TypeDefinitionFactory)();
    TypeDefinitionFactory factory;

    MetaType(std::string n, std::vector<MetaType *> sts, TypeDefinitionFactory f);

    virtual TypeSpec make_pivot_ts();
    virtual Value *match(std::string name, Value *pivot, Scope *scope);
    virtual void store(TypeMatch tm, Storage s, Storage t, Cx *cx);
    virtual bool is_typedefinition(std::string n);
    virtual bool has_super(MetaType *mt);
};

class TupleMetaType: public MetaType {
public:
    TupleMetaType(std::string n, std::vector<MetaType *> sts, TypeDefinitionFactory f);

    virtual TypeSpec make_pivot_ts();
    virtual Value *lookup_initializer(TypeMatch tm, std::string n, Scope *s);
};

class AnyType: public Type {
public:
    AnyType(std::string name, Metatypes param_metatypes, MetaType *mt);
};

class SameType: public Type {
public:
    SameType(std::string name, Metatypes param_metatypes, MetaType *mt);

    virtual StorageWhere where(TypeMatch tm, AsWhat as_what);
    virtual Allocation measure(TypeMatch tm);
    virtual void store(TypeMatch tm, Storage s, Storage t, Cx *cx);
};

class AttributeType: public Type {
public:
    AttributeType(std::string n, MetaType *arg_metatype = value_metatype);

    virtual StorageWhere where(TypeMatch tm, AsWhat as_what);
    virtual Storage optimal_value_storage(TypeMatch tm, Regs preferred);
    virtual Allocation measure(TypeMatch tm);
    virtual void store(TypeMatch tm, Storage s, Storage t, Cx *cx);
    virtual void create(TypeMatch tm, Storage s, Storage t, Cx *cx);
    virtual void destroy(TypeMatch tm, Storage s, Cx *cx);
    virtual void equal(TypeMatch tm, Storage s, Storage t, Cx *cx);
    virtual void compare(TypeMatch tm, Storage s, Storage t, Cx *cx);
    virtual void streamify(TypeMatch tm, Cx *cx);
    virtual Value *lookup_initializer(TypeMatch tm, std::string n, Scope *s);
    virtual Value *lookup_matcher(TypeMatch tm, std::string n, Value *pivot, Scope *scope);
    virtual devector<VirtualEntry *> get_virtual_table(TypeMatch tm);
    virtual Label get_virtual_table_label(TypeMatch tm, Cx *cx);
    virtual Value *lookup_inner(TypeMatch tm, std::string n, Value *v, Scope *s);
    virtual void type_info(TypeMatch tm, Cx *cx);
};

class DvalueType: public Type {
public:
    DvalueType(std::string n);

    virtual StorageWhere where(TypeMatch tm, AsWhat as_what);
    virtual Allocation measure(TypeMatch tm);
    virtual void type_info(TypeMatch tm, Cx *cx);
};

class CodeType: public Type {
public:
    CodeType(std::string n);

    virtual StorageWhere where(TypeMatch tm, AsWhat as_what);
    virtual Allocation measure(TypeMatch tm);
    virtual void store(TypeMatch tm, Storage s, Storage t, Cx *cx);
    virtual void type_info(TypeMatch tm, Cx *cx);
};

class TupleType: public Type {
public:
    typedef std::vector<std::string> Keywords;
    
    static std::map<Keywords, std::unique_ptr<TupleType>> cache;
    static TupleType *get(Keywords kws);
    
    Keywords keywords;

    TupleType(std::vector<std::string> kws);

    virtual TypeSpec make_pivot_ts();
    virtual StorageWhere where(TypeMatch tm, AsWhat as_what);
    virtual Allocation measure(TypeMatch tm);
    virtual void store(TypeMatch tm, Storage s, Storage t, Cx *cx);
    virtual void create(TypeMatch tm, Storage s, Storage t, Cx *cx);
    virtual void destroy(TypeMatch tm, Storage s, Cx *cx);
};


class PartialType: public Type {
public:
    PartialType(std::string name);

    virtual StorageWhere where(TypeMatch tm, AsWhat as_what);
    virtual Allocation measure(TypeMatch tm);
    virtual void store(TypeMatch tm, Storage s, Storage t, Cx *cx);
    virtual void create(TypeMatch tm, Storage s, Storage t, Cx *cx);
    virtual Value *lookup_inner(TypeMatch tm, std::string n, Value *v, Scope *s);
    virtual void type_info(TypeMatch tm, Cx *cx);
};

class UninitializedType: public Type {
public:
    UninitializedType(std::string name);

    virtual StorageWhere where(TypeMatch tm, AsWhat as_what);
};

class InitializableType: public Type {
public:
    InitializableType(std::string name);

    virtual StorageWhere where(TypeMatch tm, AsWhat as_what);
    virtual Allocation measure(TypeMatch tm);
    virtual void store(TypeMatch tm, Storage s, Storage t, Cx *cx);
    virtual void create(TypeMatch tm, Storage s, Storage t, Cx *cx);
    virtual Value *lookup_inner(TypeMatch tm, std::string n, Value *v, Scope *s);
};

class MultiType: public Type {
public:
    MultiType(std::string name);

    virtual StorageWhere where(TypeMatch tm, AsWhat as_what);
};

class VoidType: public Type {
public:
    VoidType(std::string name);

    virtual StorageWhere where(TypeMatch tm, AsWhat as_what);
    virtual void store(TypeMatch tm, Storage s, Storage t, Cx *cx);
    virtual void type_info(TypeMatch tm, Cx *cx);
};

class UnitType: public Type {
public:
    UnitType(std::string name);

    virtual StorageWhere where(TypeMatch tm, AsWhat as_what);
    virtual Allocation measure(TypeMatch tm);
    virtual void store(TypeMatch tm, Storage s, Storage t, Cx *cx);
    virtual void create(TypeMatch tm, Storage s, Storage t, Cx *cx);
    virtual void destroy(TypeMatch tm, Storage s, Cx *cx);
    virtual void streamify(TypeMatch tm, Cx *cx);
    virtual void type_info(TypeMatch tm, Cx *cx);
};

class ColonType: public UnitType {
public:
    ColonType(std::string name);

    virtual TypeSpec make_pivot_ts();
};

// For expressions that don't return
class WhateverType: public Type {
public:
    WhateverType(std::string name);

    virtual StorageWhere where(TypeMatch tm, AsWhat as_what);
    virtual Allocation measure(TypeMatch tm);
    virtual void store(TypeMatch tm, Storage s, Storage t, Cx *cx);
    virtual void destroy(TypeMatch tm, Storage s, Cx *cx);
    virtual void type_info(TypeMatch tm, Cx *cx);
};

class StringtemplateType: public Type {
public:
    StringtemplateType(std::string name);
};

// A dummy type, only to be used as Bare Uninitialized for bare declarations,
// whose type will be derived from the right hand side of their initialization.
class BareType: public Type {
public:
    BareType(std::string name);
};
