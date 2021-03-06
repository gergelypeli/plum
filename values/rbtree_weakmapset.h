// Helper functions
void nosytree_fcb_action(Label action_label, TypeSpec elem_ts, Cx *cx);
void compile_nosytree_callback(Label label, TypeSpec elem_ts, Cx *cx);
void nosy_postadd(TypeSpec elem_ts, Storage ref_storage, Cx *cx);
void nosy_postremove(TypeSpec elem_ts, Storage ref_storage, Cx *cx);

// Nosytree basics
void compile_nosytree_finalizer(Label label, TypeSpec elem_ts, Cx *cx);
void alloc_nosytree(TypeSpec elem_ts, Cx *cx);
void compile_nosytree_clone(Label label, TypeSpec elem_ts, Cx *cx);

// Internally used access to the Rbtree Ref inside the Nosytree Ref
class NosytreeMemberValue: public Value, public GenericLvalue {
public:
    TypeSpec elem_ts;
    std::unique_ptr<Value> pivot;
    Regs clob;

    NosytreeMemberValue(Value *p, TypeSpec ets, TypeSpec member_ts);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual void need_rvalue();
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
};

// Wraps a newly created Rbtree in a Nosytree
class WeaktreeValue: public Value {
public:
    TypeSpec elem_ts;
    std::unique_ptr<Value> member_value;

    WeaktreeValue(Value *pivot, TypeSpec ets, TypeSpec cts);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
};

// Common operations on Rbtree-s with some nosy elements
class NosyRbtreeLengthValue: public RbtreeLengthValue {
public:
    NosyRbtreeLengthValue(Value *l, TypeSpec elem_ts);
};

class NosyRbtreeAddValue: public RbtreeAddValue {
public:
    NosyRbtreeAddValue(Value *l, TypeSpec elem_ts);
    
    virtual Regs precompile(Regs preferred);
    virtual Storage postprocess(Storage ps, Cx *cx);
};

class NosyRbtreeAddItemValue: public RbtreeAddItemValue {
public:
    NosyRbtreeAddItemValue(Value *l, TypeSpec elem_ts);
    
    virtual Regs precompile(Regs preferred);
    virtual Storage postprocess(Storage ps, Cx *cx);
};

class NosyRbtreeRemoveValue: public RbtreeRemoveValue {
public:
    NosyRbtreeRemoveValue(Value *l, TypeSpec elem_ts);
    
    virtual Regs precompile(Regs preferred);
    virtual Storage postprocess(Storage ps, Cx *cx);
};

class NosyRbtreeHasValue: public RbtreeHasValue {
public:
    NosyRbtreeHasValue(Value *l, TypeSpec elem_ts);
};

class NosyRbtreeIndexValue: public RbtreeIndexValue {
public:
    NosyRbtreeIndexValue(Value *l, TypeSpec elem_ts);
};
