void compile_array_alloc(Label label, TypeSpec elem_ts, Cx *cx);
void compile_array_realloc(Label label, TypeSpec elem_ts, Cx *cx);
void compile_array_grow(Label label, TypeSpec elem_ts, Cx *cx);
void compile_array_clone(Label label, TypeSpec elem_ts, Cx *cx);

class ArrayLengthValue: public ContainerLengthValue {
public:
    ArrayLengthValue(Value *l, TypeMatch &match);
};

class ArrayIndexValue: public ContainerIndexValue {
public:
    ArrayIndexValue(Value *pivot, TypeMatch &match);
};

class ArrayEmptyValue: public ContainerEmptyValue {
public:
    ArrayEmptyValue(TypeSpec ts);
};

class ArrayReservedValue: public ContainerReservedValue {
public:
    ArrayReservedValue(TypeSpec ts);
};

class ArrayAllValue: public ContainerAllValue {
public:
    ArrayAllValue(TypeSpec ts);
};

class ArrayInitializerValue: public ContainerInitializerValue {
public:
    ArrayInitializerValue(TypeSpec ts);
};

class ArrayPushValue: public ContainerPushValue {
public:
    ArrayPushValue(Value *l, TypeMatch &match);
};

class ArrayPopValue: public ContainerPopValue {
public:
    ArrayPopValue(Value *l, TypeMatch &match);
};

class ArrayConcatenationValue: public GenericValue {
public:
    TypeSpec elem_ts;
    
    ArrayConcatenationValue(Value *l, TypeMatch &match);
    
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
    static void compile_array_concatenation(Label label, TypeSpec elem_ts, Cx *cx);
};

class ArrayExtendValue: public GenericValue {
public:
    TypeSpec elem_ts;
    TypeSpec heap_ts;
    
    ArrayExtendValue(Value *l, TypeMatch &match);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
};

class ArrayReallocValue: public GenericOperationValue {
public:
    TypeSpec elem_ts;
    
    ArrayReallocValue(OperationType o, Value *l, TypeMatch &match);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
};

class ArraySortValue: public GenericValue {
public:
    TypeSpec elem_ts;
    
    ArraySortValue(Value *l, TypeMatch &match);
    
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
    static void compile_compar(Label label, TypeSpec elem_ts, Cx *cx);
};

class ArrayRemoveValue: public GenericValue {
public:
    TypeSpec elem_ts;
    
    ArrayRemoveValue(Value *l, TypeMatch &match);
    
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
    static void compile_remove(Label label, TypeSpec elem_ts, Cx *cx);
};

class ArrayRefillValue: public Value {
public:
    std::unique_ptr<Value> array_value;
    std::unique_ptr<Value> fill_value;
    std::unique_ptr<Value> length_value;
    TypeSpec elem_ts;

    ArrayRefillValue(Value *l, TypeMatch &match);

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
};

// Iteration
class ArrayElemIterValue: public ContainerIterValue {
public:
    ArrayElemIterValue(Value *l, TypeMatch &match);
};

class ArrayIndexIterValue: public ContainerIterValue {
public:
    ArrayIndexIterValue(Value *l, TypeMatch &match);
};

class ArrayItemIterValue: public ContainerIterValue {
public:
    ArrayItemIterValue(Value *l, TypeMatch &match);
};

class ArrayNextElemValue: public ContainerNextValue, public ContainedLvalue {
public:
    ArrayNextElemValue(Value *l, TypeMatch &match);

    virtual Regs precompile(Regs preferred);
    virtual Storage postprocess(Register r, Register i, Cx *cx);
};

class ArrayNextIndexValue: public ContainerNextValue {
public:
    ArrayNextIndexValue(Value *l, TypeMatch &match);
    
    virtual Storage postprocess(Register r, Register i, Cx *cx);
};

class ArrayNextItemValue: public ContainerNextValue {
public:
    ArrayNextItemValue(Value *l, TypeMatch &match);
    
    virtual Storage postprocess(Register r, Register i, Cx *cx);
};
