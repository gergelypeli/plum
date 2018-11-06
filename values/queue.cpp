
int queue_elem_size(TypeSpec elem_ts) {
    return elem_ts.measure_elem();
}


void fix_R10_index_overflow(Register r, X64 *x64) {
    Label ok;
    x64->op(ADDQ, R10, Address(r, CIRCULARRAY_FRONT_OFFSET));
    x64->op(CMPQ, R10, Address(r, CIRCULARRAY_RESERVATION_OFFSET));
    x64->op(JL, ok);

    //x64->err("Fixing index overflow.");
    x64->op(SUBQ, R10, Address(r, CIRCULARRAY_RESERVATION_OFFSET));
        
    x64->code_label(ok);
}


void fix_R10_index_underflow(Register r, X64 *x64) {
    Label ok;
    x64->op(ADDQ, R10, Address(r, CIRCULARRAY_FRONT_OFFSET));
    x64->op(CMPQ, R10, 0);
    x64->op(JGE, ok);
        
    //x64->err("Fixing index underflow.");
    x64->op(ADDQ, R10, Address(r, CIRCULARRAY_RESERVATION_OFFSET));
        
    x64->code_label(ok);
}


void compile_queue_alloc(Label label, TypeSpec elem_ts, X64 *x64) {
    // R10 - reservation
    int elem_size = queue_elem_size(elem_ts);
    Label finalizer_label = elem_ts.prefix(circularray_type).get_finalizer_label(x64);
    
    x64->code_label_local(label, "x_queue_alloc");
    
    container_alloc(CIRCULARRAY_HEADER_SIZE, elem_size, CIRCULARRAY_RESERVATION_OFFSET, finalizer_label, x64);

    x64->op(MOVQ, Address(RAX, CIRCULARRAY_LENGTH_OFFSET), 0);
    x64->op(MOVQ, Address(RAX, CIRCULARRAY_FRONT_OFFSET), 0);
    
    x64->op(RET);
}


void compile_queue_realloc(Label label, TypeSpec elem_ts, X64 *x64) {
    // RAX - array, R10 - new reservation
    int elem_size = queue_elem_size(elem_ts);

    x64->code_label_local(label, "x_queue_realloc");
    //x64->log("realloc_array");
    
    container_realloc(CIRCULARRAY_HEADER_SIZE, elem_size, CIRCULARRAY_RESERVATION_OFFSET, x64);
    
    x64->op(RET);
}


void compile_queue_grow(Label label, TypeSpec elem_ts, X64 *x64) {
    // RAX - array, R10 - new reservation
    // RCX, RSI, RDI - clob
    // Double the reservation until it's enough (can be relaxed to 1.5 times, but not less)
    Label realloc_label = x64->once->compile(compile_queue_realloc, elem_ts);
    int elem_size = queue_elem_size(elem_ts);

    x64->code_label_local(label, "x_queue_grow");
    x64->runtime->log("grow_circularray");
    x64->op(PUSHQ, RCX);
    x64->op(PUSHQ, RSI);
    x64->op(PUSHQ, RDI);
    x64->op(PUSHQ, Address(RAX, CIRCULARRAY_RESERVATION_OFFSET));
    
    container_grow(CIRCULARRAY_RESERVATION_OFFSET, CIRCULARRAY_MINIMUM_RESERVATION, realloc_label, x64);

    x64->op(POPQ, R10);  // old reservation
    
    Label high, end;
    x64->op(MOVQ, RCX, R10);
    x64->op(SHRQ, RCX, 1);
    x64->op(CMPQ, Address(RAX, CIRCULARRAY_FRONT_OFFSET), RCX);
    x64->op(JAE, high);

    // The front is low, so it's better to unfold the folded part. This requires that
    // the growth rate was at least 1.5 times.
    
    x64->runtime->log("Unfolding queue circularray.");
    
    x64->op(LEA, RSI, Address(RAX, CIRCULARRAY_ELEMS_OFFSET));
    
    x64->op(MOVQ, RDI, R10);
    x64->op(IMUL3Q, RDI, RDI, elem_size);
    x64->op(LEA, RDI, Address(RAX, RDI, CIRCULARRAY_ELEMS_OFFSET));
    
    x64->op(MOVQ, RCX, Address(RAX, CIRCULARRAY_FRONT_OFFSET));
    x64->op(IMUL3Q, RCX, RCX, elem_size);
    
    x64->op(REPMOVSB);
    x64->op(JMP, end);
    
    x64->code_label(high);
    
    // The front is high, so it's better to move the unfolded part to the end of the
    // new reservation. This also requires 1.5 growth rate so we can copy forward.

    x64->runtime->log("Stretching queue circularray.");
    
    x64->op(MOVQ, RSI, Address(RAX, CIRCULARRAY_FRONT_OFFSET));
    x64->op(IMUL3Q, RSI, RSI, elem_size);
    x64->op(LEA, RSI, Address(RAX, RSI, CIRCULARRAY_ELEMS_OFFSET));

    x64->op(MOVQ, RDI, Address(RAX, CIRCULARRAY_FRONT_OFFSET));
    x64->op(SUBQ, RDI, R10);
    x64->op(ADDQ, RDI, Address(RAX, CIRCULARRAY_RESERVATION_OFFSET));
    x64->op(MOVQ, Address(RAX, CIRCULARRAY_FRONT_OFFSET), RDI);  // must update front index
    x64->op(IMUL3Q, RDI, RDI, elem_size);
    x64->op(LEA, RDI, Address(RAX, RDI, CIRCULARRAY_ELEMS_OFFSET));
    
    x64->op(MOVQ, RCX, Address(RAX, CIRCULARRAY_RESERVATION_OFFSET));
    x64->op(SUBQ, RCX, Address(RAX, CIRCULARRAY_FRONT_OFFSET));
    x64->op(IMUL3Q, RCX, RCX, elem_size);
    
    x64->op(REPMOVSB);
    
    x64->code_label(end);
    x64->op(POPQ, RDI);
    x64->op(POPQ, RSI);
    x64->op(POPQ, RCX);
    x64->op(RET);
}


class QueueLengthValue: public ContainerLengthValue {
public:
    QueueLengthValue(Value *l, TypeMatch &match)
        :ContainerLengthValue(l, match, match[1].prefix(circularray_type), CIRCULARRAY_LENGTH_OFFSET) {
    }
};


class QueueIndexValue: public ContainerIndexValue {
public:
    QueueIndexValue(OperationType o, Value *pivot, TypeMatch &match)
        :ContainerIndexValue(o, pivot, match, match[1].prefix(circularray_type), CIRCULARRAY_ELEMS_OFFSET) {
    }

    virtual void fix_R10_index(Register r, X64 *x64) {
        fix_R10_index_overflow(r, x64);
    }
};


class QueueEmptyValue: public ContainerEmptyValue {
public:
    QueueEmptyValue(TypeSpec ts)
        :ContainerEmptyValue(ts, compile_queue_alloc) {
    }
};


class QueueReservedValue: public ContainerReservedValue {
public:
    QueueReservedValue(TypeSpec ts)
        :ContainerReservedValue(ts, compile_queue_alloc) {
    }
};


class QueueInitializerValue: public ContainerInitializerValue {
public:
    QueueInitializerValue(TypeSpec ts)
        :ContainerInitializerValue(ts.unprefix(queue_type), ts, CIRCULARRAY_LENGTH_OFFSET, CIRCULARRAY_ELEMS_OFFSET, compile_queue_alloc) {
    }
};


class QueuePushValue: public ContainerPushValue {
public:
    QueuePushValue(Value *l, TypeMatch &match)
        :ContainerPushValue(l, match, CIRCULARRAY_RESERVATION_OFFSET, CIRCULARRAY_LENGTH_OFFSET, CIRCULARRAY_ELEMS_OFFSET, compile_queue_grow) {
    }

    virtual void fix_R10_index(Register r, X64 *x64) {
        fix_R10_index_overflow(r, x64);
    }
};


class QueuePopValue: public ContainerPopValue {
public:
    QueuePopValue(Value *l, TypeMatch &match)
        :ContainerPopValue(l, match, match[1].prefix(circularray_type), CIRCULARRAY_LENGTH_OFFSET, CIRCULARRAY_ELEMS_OFFSET) {
    }

    virtual void fix_R10_index(Register r, X64 *x64) {
        fix_R10_index_overflow(r, x64);
    }
};


class QueueUnshiftValue: public QueuePushValue {
public:
    QueueUnshiftValue(Value *l, TypeMatch &match)
        :QueuePushValue(l, match) {
    }

    virtual void fix_R10_index(Register r, X64 *x64) {
        // Compute the new front, and use it for the element index
        x64->op(MOVQ, R10, -1);
        fix_R10_index_underflow(r, x64);
        x64->op(MOVQ, Address(r, CIRCULARRAY_FRONT_OFFSET), R10);
    }
};


class QueueShiftValue: public QueuePopValue {
public:
    QueueShiftValue(Value *l, TypeMatch &match)
        :QueuePopValue(l, match) {
    }

    virtual void fix_R10_index(Register r, X64 *x64) {
        // Compute the new front, and use the old one for the element index
        x64->op(MOVQ, R10, 1);
        fix_R10_index_overflow(r, x64);
        x64->op(XCHGQ, R10, Address(r, CIRCULARRAY_FRONT_OFFSET));
    }
};

/*
class QueueAutogrowValue: public ContainerAutogrowValue {
public:
    QueueAutogrowValue(Value *l, TypeMatch &match)
        :ContainerAutogrowValue(l, match) {
    }
    
    virtual Storage compile(X64 *x64) {
        return subcompile(CIRCULARRAY_RESERVATION_OFFSET, CIRCULARRAY_LENGTH_OFFSET, compile_queue_grow, x64);
    }
};
*/

// Iteration

class QueueElemIterValue: public ContainerIterValue {
public:
    QueueElemIterValue(Value *l, TypeMatch &match)
        :ContainerIterValue(typesubst(SAME_QUEUEELEMITER_TS, match), l) {
    }
};


class QueueIndexIterValue: public ContainerIterValue {
public:
    QueueIndexIterValue(Value *l, TypeMatch &match)
        :ContainerIterValue(typesubst(SAME_QUEUEINDEXITER_TS, match), l) {
    }
};


class QueueItemIterValue: public ContainerIterValue {
public:
    QueueItemIterValue(Value *l, TypeMatch &match)
        :ContainerIterValue(typesubst(SAME_QUEUEITEMITER_TS, match), l) {
    }
};


class QueueNextElemValue: public ContainerNextValue {
public:
    QueueNextElemValue(Value *l, TypeMatch &match)
        :ContainerNextValue(match[1], match[1], l, CIRCULARRAY_LENGTH_OFFSET, false) {
    }

    virtual Storage compile(X64 *x64) {
        int elem_size = queue_elem_size(elem_ts);

        Storage r = ContainerNextValue::compile(x64);

        fix_R10_index_overflow(r.reg, x64);
        
        x64->op(IMUL3Q, R10, R10, elem_size);
        x64->op(LEA, r.reg, Address(r.reg, R10, CIRCULARRAY_ELEMS_OFFSET));
        
        return Storage(MEMORY, Address(r.reg, 0));
    }
};


class QueueNextIndexValue: public ContainerNextValue {
public:
    QueueNextIndexValue(Value *l, TypeMatch &match)
        :ContainerNextValue(INTEGER_TS, match[1], l, CIRCULARRAY_LENGTH_OFFSET, false) {
    }
    
    virtual Storage compile(X64 *x64) {
        Storage r = ContainerNextValue::compile(x64);
        
        x64->op(MOVQ, r.reg, R10);
        
        return Storage(REGISTER, r.reg);
    }
};


class QueueNextItemValue: public ContainerNextValue {
public:
    QueueNextItemValue(Value *l, TypeMatch &match)
        :ContainerNextValue(typesubst(INTEGER_SAME_ITEM_TS, match), match[1], l, CIRCULARRAY_LENGTH_OFFSET, false) {
    }

    virtual Storage compile(X64 *x64) {
        int elem_size = queue_elem_size(elem_ts);
        int item_stack_size = ts.measure_stack();

        Storage r = ContainerNextValue::compile(x64);
        
        x64->op(SUBQ, RSP, item_stack_size);
        x64->op(MOVQ, Address(RSP, 0), R10);

        fix_R10_index_overflow(r.reg, x64);

        x64->op(IMUL3Q, R10, R10, elem_size);
        x64->op(LEA, r.reg, Address(r.reg, R10, CIRCULARRAY_ELEMS_OFFSET));
        
        Storage s = Storage(MEMORY, Address(r.reg, 0));
        Storage t = Storage(MEMORY, Address(RSP, INTEGER_SIZE));
        elem_ts.create(s, t, x64);
        
        return Storage(STACK);
    }
};

