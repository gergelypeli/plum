void compile_rbtree_alloc(Label label, TypeSpec elem_ts, Cx *cx);
void compile_rbtree_realloc(Label label, TypeSpec elem_ts, Cx *cx);
void compile_rbtree_grow(Label label, TypeSpec elem_ts, Cx *cx);
void rbtree_preappend2(TypeSpec elem_ts, Storage ref_storage, Cx *cx);
void compile_rbtree_clone(Label label, TypeSpec elem_ts, Cx *cx);

// Initializers
class RbtreeEmptyValue: public GenericValue {
public:
    TypeSpec elem_ts;

    RbtreeEmptyValue(TypeSpec ets, TypeSpec rts);
    
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
};

class RbtreeReservedValue: public GenericValue {
public:
    TypeSpec elem_ts;

    RbtreeReservedValue(TypeSpec ets, TypeSpec rts);
    
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
};

class RbtreeInitializerValue: public ContainerInitializerValue {
public:
    RbtreeInitializerValue(TypeSpec ets, TypeSpec rts);
    
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
};

// Methods
class RbtreeLengthValue: public GenericValue {
public:
    Register reg;
    TypeSpec heap_ts;

    RbtreeLengthValue(Value *l, TypeSpec ets);
    
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
};

class RbtreeAddValue: public Value {
public:
    TypeSpec elem_ts, elem_arg_ts;
    std::unique_ptr<Value> pivot, elem;

    RbtreeAddValue(Value *l, TypeSpec ets, TypeSpec eats);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage postprocess(Storage s, Cx *cx);
    virtual Storage compile(Cx *cx);
};

class RbtreeAddItemValue: public Value {
public:
    TypeSpec key_ts, value_ts, key_arg_ts, value_arg_ts;
    TypeSpec elem_ts;
    std::unique_ptr<Value> pivot, key, value;

    RbtreeAddItemValue(Value *l, TypeSpec kts, TypeSpec vts, TypeSpec kats, TypeSpec vats);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage postprocess(Storage s, Cx *cx);
    virtual Storage compile(Cx *cx);
};

class RbtreeRemoveValue: public Value {
public:
    TypeSpec elem_ts, key_arg_ts;
    std::unique_ptr<Value> pivot, key;

    RbtreeRemoveValue(Value *l, TypeSpec ets, TypeSpec kats);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage postprocess(Storage s, Cx *cx);
    virtual Storage compile(Cx *cx);
};

class RbtreeHasValue: public Value {
public:
    TypeSpec elem_ts, key_arg_ts;
    std::unique_ptr<Value> pivot, key, value;

    RbtreeHasValue(Value *l, TypeSpec ets, TypeSpec kats);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
};

class RbtreeIndexValue: public Value, public Raiser, public ContainedLvalue {
public:
    TypeSpec key_ts, elem_ts, key_arg_ts, heap_ts;
    std::unique_ptr<Value> pivot, key;
    Storage value_storage;
    bool may_borrow_heap;

    RbtreeIndexValue(Value *l, TypeSpec kts, TypeSpec ets, TypeSpec kats, TypeSpec vrts);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
};

// Iteration
class RbtreeElemByAgeIterValue: public SimpleRecordValue {
public:
    RbtreeElemByAgeIterValue(Value *l, TypeSpec iter_ts);
    
    virtual Storage compile(Cx *cx);
};

class RbtreeNextElemByAgeValue: public GenericValue, public Raiser {
public:
    Regs clob;
    bool is_down;

    RbtreeNextElemByAgeValue(Value *l, TypeSpec ts);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage postprocess(Register r, Register i, Cx *cx);
    virtual Storage compile(Cx *cx);
};

class RbtreeElemByOrderIterValue: public ContainerIterValue {
public:
    RbtreeElemByOrderIterValue(Value *l, TypeSpec iter_ts);
};

class RbtreeNextElemByOrderValue: public GenericValue, public Raiser {
public:
    Regs clob;

    RbtreeNextElemByOrderValue(Value *l, TypeSpec ts);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage postprocess(Register r, Register i, Cx *cx);
    virtual Storage compile(Cx *cx);
};
