
class RecordType: public Type, public PartialInitializable {
public:
    std::vector<Variable *> member_variables;
    std::vector<TypeSpec> member_tss;  // rvalues, for the initializer arguments
    std::vector<std::string> member_names;
    Function *streamify_function;
    bool is_single;

    RecordType(std::string n, Metatypes param_metatypes);

    virtual bool complete_type();
    virtual Allocation measure(TypeMatch tm);
    virtual void store(TypeMatch tm, Storage s, Storage t, Cx *cx);
    virtual void create(TypeMatch tm, Storage s, Storage t, Cx *cx);
    virtual void destroy(TypeMatch tm, Storage s, Cx *cx);
    virtual StorageWhere where(TypeMatch tm, AsWhat as_what);
    virtual Storage optimal_value_storage(TypeMatch tm, Regs preferred);
    virtual unsigned comparable_member_count();
    virtual void equal(TypeMatch tm, Storage s, Storage t, Cx *cx);
    virtual void compare(TypeMatch tm, Storage s, Storage t, Cx *cx);
    virtual void streamify(TypeMatch tm, Cx *cx);
    virtual Value *lookup_initializer(TypeMatch tm, std::string n, Scope *scope);
    virtual std::vector<TypeSpec> get_member_tss(TypeMatch &match);
    virtual std::vector<std::string> get_partial_initializable_names();
    virtual void type_info(TypeMatch tm, Cx *cx);
};

class StringType: public RecordType {
public:
    StringType(std::string n);

    virtual void equal(TypeMatch tm, Storage s, Storage t, Cx *cx);
    static void compile_stringeq(Label label, Cx *cx);
    virtual void compare(TypeMatch tm, Storage s, Storage t, Cx *cx);
    static void compile_stringcmp(Label label, Cx *cx);
    virtual void streamify(TypeMatch tm, Cx *cx);
    static void compile_esc_streamification(Label label, Cx *cx);
    virtual Value *lookup_initializer(TypeMatch tm, std::string n, Scope *scope);
    virtual Value *lookup_matcher(TypeMatch tm, std::string n, Value *pivot, Scope *scope);
};

class SliceType: public RecordType {
public:
    SliceType(std::string n);

    virtual Value *lookup_initializer(TypeMatch tm, std::string n, Scope *scope);
};

class ItemType: public RecordType {
public:
    ItemType(std::string n);

    virtual unsigned comparable_member_count();
};
