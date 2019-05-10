

void fix_index_overflow(Register r, Register i, X64 *x64) {
    Label ok;
    x64->op(ADDQ, i, Address(r, CIRCULARRAY_FRONT_OFFSET));
    x64->op(CMPQ, i, Address(r, CIRCULARRAY_RESERVATION_OFFSET));
    x64->op(JL, ok);

    //x64->err("Fixing index overflow.");
    x64->op(SUBQ, i, Address(r, CIRCULARRAY_RESERVATION_OFFSET));
        
    x64->code_label(ok);
}


void fix_index_underflow(Register r, Register i, X64 *x64) {
    Label ok;
    x64->op(ADDQ, i, Address(r, CIRCULARRAY_FRONT_OFFSET));
    x64->op(CMPQ, i, 0);
    x64->op(JGE, ok);
        
    //x64->err("Fixing index underflow.");
    x64->op(ADDQ, i, Address(r, CIRCULARRAY_RESERVATION_OFFSET));
        
    x64->code_label(ok);
}


void compile_queue_alloc(Label label, TypeSpec elem_ts, X64 *x64) {
    // R10 - reservation
    int elem_size = ContainerType::get_elem_size(elem_ts);
    Label finalizer_label = elem_ts.prefix(circularray_type).get_finalizer_label(x64);
    
    x64->code_label_local(label, elem_ts.prefix(queue_type).symbolize("alloc"));
    
    container_alloc(CIRCULARRAY_HEADER_SIZE, elem_size, CIRCULARRAY_RESERVATION_OFFSET, finalizer_label, x64);

    x64->op(MOVQ, Address(RAX, CIRCULARRAY_LENGTH_OFFSET), 0);
    x64->op(MOVQ, Address(RAX, CIRCULARRAY_FRONT_OFFSET), 0);
    
    x64->op(RET);
}


void compile_queue_realloc(Label label, TypeSpec elem_ts, X64 *x64) {
    // RAX - array, R10 - new reservation
    int elem_size = ContainerType::get_elem_size(elem_ts);

    x64->code_label_local(label, elem_ts.prefix(queue_type).symbolize("realloc"));
    //x64->log("realloc_array");
    
    container_realloc(CIRCULARRAY_HEADER_SIZE, elem_size, CIRCULARRAY_RESERVATION_OFFSET, x64);
    
    x64->op(RET);
}


void compile_queue_grow(Label label, TypeSpec elem_ts, X64 *x64) {
    // RAX - array, R10 - new reservation
    // RCX, RSI, RDI - clob
    // Double the reservation until it's enough (can be relaxed to 1.5 times, but not less)
    Label realloc_label = x64->once->compile(compile_queue_realloc, elem_ts);
    int elem_size = ContainerType::get_elem_size(elem_ts);

    x64->code_label_local(label, elem_ts.prefix(queue_type).symbolize("grow"));
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


void compile_queue_clone(Label label, TypeSpec elem_ts, X64 *x64) {
    // RAX - Circularray Ref
    // Return a cloned Ref
    Label loop, end, linear, linear2, loop2;
    Label alloc_label = x64->once->compile(compile_queue_alloc, elem_ts);
    int elem_size = ContainerType::get_elem_size(elem_ts);
    TypeSpec heap_ts = elem_ts.prefix(circularray_type);
    
    x64->code_label_local(label, elem_ts.prefix(queue_type).symbolize("clone"));
    x64->runtime->log("XXX queue clone");
    
    x64->op(PUSHQ, RAX);
    x64->op(MOVQ, R10, Address(RAX, CIRCULARRAY_RESERVATION_OFFSET));
    x64->op(CALL, alloc_label);  // clobbers all
    
    x64->op(POPQ, RBX);  // orig
    x64->op(MOVQ, RCX, Address(RBX, CIRCULARRAY_LENGTH_OFFSET));
    x64->op(MOVQ, Address(RAX, CIRCULARRAY_LENGTH_OFFSET), RCX);

    x64->op(CMPQ, RCX, 0);
    x64->op(JE, end);

    x64->op(MOVQ, RDX, Address(RBX, CIRCULARRAY_FRONT_OFFSET));
    x64->op(MOVQ, Address(RAX, CIRCULARRAY_FRONT_OFFSET), RDX);
    x64->op(IMUL3Q, R10, RDX, elem_size);

    x64->op(LEA, RSI, Address(RBX, R10, CIRCULARRAY_ELEMS_OFFSET));
    x64->op(LEA, RDI, Address(RAX, R10, CIRCULARRAY_ELEMS_OFFSET));
    x64->op(ADDQ, RDX, RCX);  // theoretical end index
    x64->op(SUBQ, RDX, Address(RBX, CIRCULARRAY_RESERVATION_OFFSET));  // wrapped length
    x64->op(JLE, linear);
    
    x64->op(SUBQ, RCX, RDX);  // trim first loop (remains nonempty)
    
    x64->code_label(linear);
    x64->code_label(loop);
    elem_ts.create(Storage(MEMORY, Address(RSI, 0)), Storage(MEMORY, Address(RDI, 0)), x64);
    x64->op(ADDQ, RSI, elem_size);
    x64->op(ADDQ, RDI, elem_size);
    x64->op(DECQ, RCX);
    x64->op(JNE, loop);

    x64->op(CMPQ, RDX, 0);
    x64->op(JLE, linear2);
    
    x64->op(LEA, RSI, Address(RBX, CIRCULARRAY_ELEMS_OFFSET));
    x64->op(LEA, RDI, Address(RAX, CIRCULARRAY_ELEMS_OFFSET));
    x64->op(MOVQ, RCX, RDX);
    
    x64->code_label(loop2);
    elem_ts.create(Storage(MEMORY, Address(RSI, 0)), Storage(MEMORY, Address(RDI, 0)), x64);
    x64->op(ADDQ, RSI, elem_size);
    x64->op(ADDQ, RDI, elem_size);
    x64->op(DECQ, RCX);
    x64->op(JNE, loop2);
    
    x64->code_label(linear2);
    heap_ts.decref(RBX, x64);
    
    x64->code_label(end);
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
        :ContainerIndexValue(o, pivot, match, match[1].prefix(circularray_type), CIRCULARRAY_LENGTH_OFFSET, CIRCULARRAY_ELEMS_OFFSET) {
    }

    virtual void fix_index(Register r, Register i, X64 *x64) {
        fix_index_overflow(r, i, x64);
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
        :ContainerPushValue(l, match, CIRCULARRAY_RESERVATION_OFFSET, CIRCULARRAY_LENGTH_OFFSET, CIRCULARRAY_ELEMS_OFFSET, compile_queue_clone, compile_queue_grow) {
    }

    virtual void fix_index(Register r, Register i, X64 *x64) {
        fix_index_overflow(r, i, x64);
    }
};


class QueuePopValue: public ContainerPopValue {
public:
    QueuePopValue(Value *l, TypeMatch &match)
        :ContainerPopValue(l, match, match[1].prefix(circularray_type), CIRCULARRAY_LENGTH_OFFSET, CIRCULARRAY_ELEMS_OFFSET, compile_queue_clone) {
    }

    virtual void fix_index(Register r, Register i, X64 *x64) {
        fix_index_overflow(r, i, x64);
    }
};


class QueueUnshiftValue: public QueuePushValue {
public:
    QueueUnshiftValue(Value *l, TypeMatch &match)
        :QueuePushValue(l, match) {
    }

    virtual void fix_index(Register r, Register i, X64 *x64) {
        // Compute the new front, and use it for the element index
        x64->op(MOVQ, i, -1);
        fix_index_underflow(r, i, x64);
        x64->op(MOVQ, Address(r, CIRCULARRAY_FRONT_OFFSET), i);
    }
};


class QueueShiftValue: public QueuePopValue {
public:
    QueueShiftValue(Value *l, TypeMatch &match)
        :QueuePopValue(l, match) {
    }

    virtual void fix_index(Register r, Register i, X64 *x64) {
        // Compute the new front, and use the old one for the element index
        x64->op(MOVQ, i, 1);
        fix_index_overflow(r, i, x64);
        x64->op(XCHGQ, i, Address(r, CIRCULARRAY_FRONT_OFFSET));
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
        :ContainerNextValue(match[1].prefix(tuple1_type), match[1], l, CIRCULARRAY_LENGTH_OFFSET, false) {
    }

    virtual Storage postprocess(Register r, Register i, X64 *x64) {
        int elem_size = ContainerType::get_elem_size(elem_ts);

        fix_index_overflow(r, i, x64);
        
        Address addr = x64->runtime->make_address(r, i, elem_size, CIRCULARRAY_ELEMS_OFFSET);
        
        return Storage(MEMORY, addr);
    }
};


class QueueNextIndexValue: public ContainerNextValue {
public:
    QueueNextIndexValue(Value *l, TypeMatch &match)
        :ContainerNextValue(INTEGER_TS.prefix(tuple1_type), match[1], l, CIRCULARRAY_LENGTH_OFFSET, false) {
    }
    
    virtual Storage postprocess(Register r, Register i, X64 *x64) {
        return Storage(REGISTER, i);
    }
};


class QueueNextItemValue: public ContainerNextValue {
public:
    QueueNextItemValue(Value *l, TypeMatch &match)
        :ContainerNextValue(typesubst(INTEGER_SAME_TUPLE2_TS, match), match[1], l, CIRCULARRAY_LENGTH_OFFSET, false) {
    }

    virtual Storage postprocess(Register r, Register i, X64 *x64) {
        int elem_size = ContainerType::get_elem_size(elem_ts);
        //int item_stack_size = ts.measure_stack();

        x64->op(PUSHQ, i);
        x64->op(SUBQ, RSP, elem_ts.measure_stack());

        fix_index_overflow(r, i, x64);

        Address addr = x64->runtime->make_address(r, i, elem_size, CIRCULARRAY_ELEMS_OFFSET);
        
        Storage s = Storage(MEMORY, addr);
        Storage t = Storage(MEMORY, Address(RSP, 0));
        elem_ts.create(s, t, x64);
        
        return Storage(STACK);
    }
};

