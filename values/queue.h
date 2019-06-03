void fix_index_overflow(Register r, Register i, X64 *x64);
void fix_index_underflow(Register r, Register i, X64 *x64);
void compile_queue_alloc(Label label, TypeSpec elem_ts, X64 *x64);
void compile_queue_realloc(Label label, TypeSpec elem_ts, X64 *x64);
void compile_queue_grow(Label label, TypeSpec elem_ts, X64 *x64);
void compile_queue_clone(Label label, TypeSpec elem_ts, X64 *x64);

class QueueLengthValue: public ContainerLengthValue {
public:
    QueueLengthValue(Value *l, TypeMatch &match);
};

class QueueIndexValue: public ContainerIndexValue {
public:
    QueueIndexValue(Value *pivot, TypeMatch &match);
    
    virtual void fix_index(Register r, Register i, X64 *x64);
};

class QueueEmptyValue: public ContainerEmptyValue {
public:
    QueueEmptyValue(TypeSpec ts);
};

class QueueReservedValue: public ContainerReservedValue {
public:
    QueueReservedValue(TypeSpec ts);
};

class QueueInitializerValue: public ContainerInitializerValue {
public:
    QueueInitializerValue(TypeSpec ts);
};

class QueuePushValue: public ContainerPushValue {
public:
    QueuePushValue(Value *l, TypeMatch &match);
    
    virtual void fix_index(Register r, Register i, X64 *x64);
};

class QueuePopValue: public ContainerPopValue {
public:
    QueuePopValue(Value *l, TypeMatch &match);
    
    virtual void fix_index(Register r, Register i, X64 *x64);
};

class QueueUnshiftValue: public QueuePushValue {
public:
    QueueUnshiftValue(Value *l, TypeMatch &match);
    
    virtual void fix_index(Register r, Register i, X64 *x64);
};

class QueueShiftValue: public QueuePopValue {
public:
    QueueShiftValue(Value *l, TypeMatch &match);
    
    virtual void fix_index(Register r, Register i, X64 *x64);
};

// Iteration
class QueueElemIterValue: public ContainerIterValue {
public:
    QueueElemIterValue(Value *l, TypeMatch &match);
};

class QueueIndexIterValue: public ContainerIterValue {
public:
    QueueIndexIterValue(Value *l, TypeMatch &match);
};

class QueueItemIterValue: public ContainerIterValue {
public:
    QueueItemIterValue(Value *l, TypeMatch &match);
};

class QueueNextElemValue: public ContainerNextValue, public ContainedLvalue {
public:
    QueueNextElemValue(Value *l, TypeMatch &match);
    
    virtual Regs precompile(Regs preferred);
    virtual Storage postprocess(Register r, Register i, X64 *x64);
};

class QueueNextIndexValue: public ContainerNextValue {
public:
    QueueNextIndexValue(Value *l, TypeMatch &match);
    
    virtual Storage postprocess(Register r, Register i, X64 *x64);
};

class QueueNextItemValue: public ContainerNextValue {
public:
    QueueNextItemValue(Value *l, TypeMatch &match);
    
    virtual Storage postprocess(Register r, Register i, X64 *x64);
};
