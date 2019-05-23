
class ContainerType: public Type {
public:
    ContainerType(std::string name, Metatypes param_metatypes);

    virtual Allocation measure(TypeMatch tm);
    virtual void incref(TypeMatch tm, Register r, X64 *x64);
    virtual void decref(TypeMatch tm, Register r, X64 *x64);
    virtual void streamify(TypeMatch tm, X64 *x64);
    static unsigned get_elem_size(TypeSpec elem_ts);
};

// Linearray based
class LinearrayType: public ContainerType {
public:
    LinearrayType(std::string name);

    virtual Label get_finalizer_label(TypeMatch tm, X64 *x64);
    static void compile_finalizer(Label label, TypeSpec ts, X64 *x64);
    virtual void type_info(TypeMatch tm, X64 *x64);
};

class ArrayType: public RecordType {
public:
    ArrayType(std::string name);

    virtual Value *lookup_initializer(TypeMatch tm, std::string name, Scope *scope);
    virtual void streamify(TypeMatch tm, X64 *x64);
    static void compile_streamification(Label label, TypeSpec elem_ts, X64 *x64);
};

// Circularray based
class CircularrayType: public ContainerType {
public:
    CircularrayType(std::string name);

    virtual Label get_finalizer_label(TypeMatch tm, X64 *x64);
    static void compile_finalizer(Label label, TypeSpec ts, X64 *x64);
    virtual void type_info(TypeMatch tm, X64 *x64);
};

class QueueType: public RecordType {
public:
    QueueType(std::string name);

    virtual Value *lookup_initializer(TypeMatch tm, std::string name, Scope *scope);
};

// Rbtree based
class RbtreeType: public ContainerType {
public:
    RbtreeType(std::string name);

    virtual Label get_finalizer_label(TypeMatch tm, X64 *x64);
    static void compile_finalizer(Label label, TypeSpec ts, X64 *x64);
    static unsigned get_rbnode_size(TypeSpec elem_ts);
    virtual void type_info(TypeMatch tm, X64 *x64);
};

class TreelikeType: public RecordType {
public:
    TypeSpec elem_ts;

    TreelikeType(std::string name, Metatypes param_metatypes, TypeSpec ets);

    virtual Value *lookup_initializer(TypeMatch tm, std::string name, Scope *scope);
    virtual void streamify(TypeMatch tm, X64 *x64);
    static void compile_streamification(Label label, TypeSpec elem_ts, X64 *x64);
};

class SetType: public TreelikeType {
public:
    SetType(std::string name);  // Must not use TS constants in constructor
};

class MapType: public TreelikeType {
public:
    MapType(std::string name);  // Must not use TS constants in constructor
};

// Nosy container based
// Base class for Weak* containers, containing a Ref to a Nosycontainer, which contains a
// container wrapper (likely a Set or Map).
class WeaktreeType: public RecordType {
public:
    TypeSpec elem_ts;

    WeaktreeType(std::string n, Metatypes param_metatypes, TypeSpec ets);

    virtual Value *lookup_initializer(TypeMatch tm, std::string n, Scope *s);
};

class WeakSetType: public WeaktreeType {
public:
    WeakSetType(std::string name);  // Must not use TS constants in constructor
};

class WeakIndexMapType: public WeaktreeType {
public:
    WeakIndexMapType(std::string name);  // Must not use TS constants in constructor
};

class WeakValueMapType: public WeaktreeType {
public:
    WeakValueMapType(std::string name);  // Must not use TS constants in constructor
};
