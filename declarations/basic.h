class BasicType: public Type {
public:
    unsigned size;
    int os;
    bool is_unsigned;

    BasicType(std::string n, unsigned s, bool iu, MetaType *mt = NULL);

    virtual Allocation measure(TypeMatch tm);
    virtual void store(TypeMatch tm, Storage s, Storage t, Cx *cx);
    virtual void create(TypeMatch tm, Storage s, Storage t, Cx *cx);
    virtual void destroy(TypeMatch tm, Storage s, Cx *cx);
    virtual void equal(TypeMatch tm, Storage s, Storage t, Cx *cx);
    virtual void compare(TypeMatch tm, Storage s, Storage t, Cx *cx);
    virtual StorageWhere where(TypeMatch tm, AsWhat as_what);
    virtual Storage optimal_value_storage(TypeMatch tm, Regs preferred);
    virtual bool get_unsigned();
};

class IntegerType: public BasicType {
public:
    IntegerType(std::string n, unsigned s, bool iu);

    virtual void streamify(TypeMatch tm, Cx *cx);
    virtual void type_info(TypeMatch tm, Cx *cx);
};

class BooleanType: public BasicType {
public:
    BooleanType(std::string n, unsigned s);

    virtual void streamify(TypeMatch tm, Cx *cx);
    virtual Value *lookup_initializer(TypeMatch tm, std::string name, Scope *scope);
    virtual void type_info(TypeMatch tm, Cx *cx);
};

class CharacterType: public BasicType {
public:
    CharacterType(std::string n, unsigned s);

    virtual void streamify(TypeMatch tm, Cx *cx);
    static void insert_pre_streamification(Cx *cx);
    static void compile_ascii_table(Label label, Cx *cx);
    static void compile_esc_streamification(Label label, Cx *cx);
    static void compile_str_streamification(Label label, Cx *cx);
    virtual Value *lookup_initializer(TypeMatch tm, std::string name, Scope *scope);
    virtual void type_info(TypeMatch tm, Cx *cx);
};

class EnumerationType: public BasicType {
public:
    std::vector<std::string> keywords;

    EnumerationType(std::string n, std::vector<std::string> kw, MetaType *mt = NULL);

    virtual void streamify(TypeMatch tm, Cx *cx);
    static void compile_streamification(Label label, Cx *cx);
    virtual Value *lookup_initializer(TypeMatch tm, std::string n, Scope *scope);
    virtual Label get_stringifications_label(Cx *cx);
    static void compile_stringifications(Label label, TypeSpec ts, Cx *cx);
    unsigned get_keyword_index(std::string kw);
    virtual void type_info(TypeMatch tm, Cx *cx);
};

class TreenumerationType: public EnumerationType {
public:
    std::vector<unsigned> parents;

    TreenumerationType(std::string n, std::vector<std::string> kw, std::vector<unsigned> ps);

    virtual Value *lookup_initializer(TypeMatch tm, std::string n, Scope *scope);
    virtual Value *lookup_matcher(TypeMatch tm, std::string n, Value *pivot, Scope *scope);
    virtual Label get_parents_label(Cx *cx);
    static void compile_parents(Label label, TypeSpec ts, Cx *cx);
};
