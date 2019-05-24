TypeSpec container_elem_ts(TypeSpec ts, Type *container_type = NULL);
void container_alloc(int header_size, int elem_size, int reservation_offset, Label finalizer_label, X64 *x64);
void container_realloc(int header_size, int elem_size, int reservation_offset, X64 *x64);
void container_grow(int reservation_offset, int min_reservation, Label realloc_label, X64 *x64);
void container_preappend2(int reservation_offset, int length_offset, Label grow_label, Storage ref_storage, X64 *x64);
void container_cow(Label clone_label, Storage ref_storage, X64 *x64);

class ContainerLengthValue: public GenericValue {
public:
    Register reg;
    TypeSpec heap_ts;
    int length_offset;
    
    ContainerLengthValue(Value *l, TypeMatch &match, TypeSpec hts, int lo);
    
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};

class ContainerIndexValue: public GenericOperationValue, public Raiser, public ContainedLvalue {
public:
    TypeSpec heap_ts;
    TypeSpec elem_ts;
    int length_offset;
    int elems_offset;
    Storage value_storage;
    bool may_borrow_heap;
    
    ContainerIndexValue(OperationType o, Value *pivot, TypeMatch &match, TypeSpec hts, int lo, int eo);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual void fix_index(Register r, Register i, X64 *x64);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};

class ContainerEmptyValue: public GenericValue {
public:
    TypeSpec elem_ts;
    Once::TypedFunctionCompiler compile_alloc;
    
    ContainerEmptyValue(TypeSpec ts, Once::TypedFunctionCompiler ca);
    
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};

class ContainerReservedValue: public GenericValue {
public:
    TypeSpec elem_ts;
    Once::TypedFunctionCompiler compile_alloc;
    
    ContainerReservedValue(TypeSpec ts, Once::TypedFunctionCompiler ca);
    
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};

class ContainerAllValue: public Value {
public:
    std::unique_ptr<Value> fill_value;
    std::unique_ptr<Value> length_value;
    TypeSpec elem_ts;
    int length_offset;
    int elems_offset;
    Once::TypedFunctionCompiler compile_alloc;
    
    ContainerAllValue(TypeSpec ts, int lo, int eo, Once::TypedFunctionCompiler ca);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};

class ContainerInitializerValue: public Value {
public:
    TypeSpec elem_ts;
    std::vector<std::unique_ptr<Value>> elems;
    int length_offset;
    int elems_offset;
    Once::TypedFunctionCompiler compile_alloc;

    ContainerInitializerValue(TypeSpec ets, TypeSpec ts, int lo, int eo, Once::TypedFunctionCompiler ca);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};

class ContainerEmptiableValue: public GenericValue, public Raiser {
public:
    ContainerEmptiableValue(TypeSpec arg_ts, TypeSpec res_ts, Value *pivot);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
};

class ContainerPushValue: public GenericValue {
public:
    TypeSpec elem_ts;
    int reservation_offset;
    int length_offset;
    int elems_offset;
    Once::TypedFunctionCompiler compile_clone, compile_grow;

    ContainerPushValue(Value *l, TypeMatch &match, int ro, int lo, int eo, Once::TypedFunctionCompiler cc, Once::TypedFunctionCompiler cg);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual void fix_index(Register r, Register i, X64 *x64);
    virtual Storage compile(X64 *x64);
};

class ContainerPopValue: public ContainerEmptiableValue {
public:
    TypeSpec elem_ts;
    TypeSpec heap_ts;
    int length_offset;
    int elems_offset;
    Once::TypedFunctionCompiler compile_clone;

    ContainerPopValue(Value *l, TypeMatch &match, TypeSpec hts, int lo, int eo, Once::TypedFunctionCompiler cc);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual void fix_index(Register r, Register i, X64 *x64);
    virtual Storage compile(X64 *x64);
};

// Iteration
class ContainerIterValue: public SimpleRecordValue {
public:
    ContainerIterValue(TypeSpec t, Value *l);
    
    virtual Storage compile(X64 *x64);
};

// Array iterator next methods
class ContainerNextValue: public GenericValue, public Raiser {
public:
    Regs clob;
    int length_offset;
    bool is_down;
    TypeSpec elem_ts;

    ContainerNextValue(TypeSpec ts, TypeSpec ets, Value *l, int lo, bool d);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage postprocess(Register r, Register i, X64 *x64);
    virtual Storage compile(X64 *x64);
};
