
class StringRegexpMatcherValue: public GenericValue, public Raiser {
public:
    StringRegexpMatcherValue(Value *l, TypeMatch &match);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
};

class SliceEmptyValue: public GenericValue {
public:
    SliceEmptyValue(TypeMatch &match);
    
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
};

class SliceAllValue: public GenericValue {
public:
    SliceAllValue(TypeMatch &match);
    
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
};

class ArraySliceValue: public Value, public Raiser {
public:
    std::unique_ptr<Value> array_value;
    std::unique_ptr<Value> front_value;
    std::unique_ptr<Value> length_value;
    Register reg;
    TypeSpec heap_ts;

    ArraySliceValue(Value *pivot, TypeMatch &match);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
};

class SliceSliceValue: public Value, public Raiser {
public:
    std::unique_ptr<Value> slice_value;
    std::unique_ptr<Value> front_value;
    std::unique_ptr<Value> length_value;
    Register reg;

    SliceSliceValue(Value *pivot, TypeMatch &match);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
};

class SliceIndexValue: public GenericValue, public Raiser, public ContainedLvalue {
public:
    TypeSpec elem_ts;
    TypeSpec heap_ts;
    Storage value_storage;

    SliceIndexValue(Value *pivot, TypeMatch &match);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
};

class SliceFindValue: public GenericValue, public Raiser {
public:
    TypeSpec slice_ts;
    TypeSpec elem_ts;

    SliceFindValue(Value *l, TypeMatch &match);

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
};

// Iteration
// TODO: too many similarities with container iteration!
class SliceIterValue: public SimpleRecordValue {
public:
    SliceIterValue(TypeSpec t, Value *l);
    
    virtual Storage compile(Cx *cx);
};

class SliceElemIterValue: public SliceIterValue {
public:
    SliceElemIterValue(Value *l, TypeMatch &match);
};

class SliceIndexIterValue: public SliceIterValue {
public:
    SliceIndexIterValue(Value *l, TypeMatch &match);
};

class SliceItemIterValue: public SliceIterValue {
public:
    SliceItemIterValue(Value *l, TypeMatch &match);
};

class SliceNextValue: public GenericValue, public Raiser {
public:
    Regs clob;
    bool is_down;
    TypeSpec elem_ts;
    int elem_size;

    SliceNextValue(TypeSpec ts, TypeSpec ets, Value *l, bool d);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage postprocess(Register r, Register i, Cx *cx);
    virtual Storage compile(Cx *cx);
};

class SliceNextElemValue: public SliceNextValue, public ContainedLvalue {
public:
    SliceNextElemValue(Value *l, TypeMatch &match);
    
    virtual Regs precompile(Regs preferred);
    virtual Storage postprocess(Register r, Register i, Cx *cx);
};

class SliceNextIndexValue: public SliceNextValue {
public:
    SliceNextIndexValue(Value *l, TypeMatch &match);
    
    virtual Storage postprocess(Register r, Register i, Cx *cx);
};

class SliceNextItemValue: public SliceNextValue {
public:
    SliceNextItemValue(Value *l, TypeMatch &match);
    
    virtual Storage postprocess(Register r, Register i, Cx *cx);
};
