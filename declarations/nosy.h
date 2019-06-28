// This is a hack type to cooperate closely with Weakref and Weak* containers.
// It contains a raw pointer, so it can disguise as a Ptr within Weak*Map, and
// comparisons would work with the input Ptr-s.
class NosyValueType: public PointerType {
public:
    NosyValueType(std::string name);

    virtual Allocation measure(TypeMatch tm);
    virtual void store(TypeMatch tm, Storage s, Storage t, Cx *cx);
    virtual void create(TypeMatch tm, Storage s, Storage t, Cx *cx);
    virtual void destroy(TypeMatch tm, Storage s, Cx *cx);
};

class NosytreeType: public ContainerType {
public:
    NosytreeType(std::string name);

    virtual void type_info(TypeMatch tm, Cx *cx);
};

class NosyrefType: public ContainerType {
public:
    NosyrefType(std::string name);

    virtual void type_info(TypeMatch tm, Cx *cx);
};

class WeakrefType: public RecordType {
public:
    WeakrefType(std::string n);

    virtual Value *lookup_initializer(TypeMatch tm, std::string n, Scope *s);
    virtual Value *lookup_matcher(TypeMatch tm, std::string n, Value *p, Scope *s);
};
