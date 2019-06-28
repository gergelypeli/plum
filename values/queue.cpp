#include "../plum.h"


void fix_index_overflow(Register r, Register i, Cx *cx) {
    Label ok;
    cx->op(ADDQ, i, Address(r, CIRCULARRAY_FRONT_OFFSET));
    cx->op(CMPQ, i, Address(r, CIRCULARRAY_RESERVATION_OFFSET));
    cx->op(JL, ok);

    //cx->err("Fixing index overflow.");
    cx->op(SUBQ, i, Address(r, CIRCULARRAY_RESERVATION_OFFSET));
        
    cx->code_label(ok);
}


void fix_index_underflow(Register r, Register i, Cx *cx) {
    Label ok;
    cx->op(ADDQ, i, Address(r, CIRCULARRAY_FRONT_OFFSET));
    cx->op(CMPQ, i, 0);
    cx->op(JGE, ok);
        
    //cx->err("Fixing index underflow.");
    cx->op(ADDQ, i, Address(r, CIRCULARRAY_RESERVATION_OFFSET));
        
    cx->code_label(ok);
}


void compile_queue_alloc(Label label, TypeSpec elem_ts, Cx *cx) {
    // R10 - reservation
    int elem_size = ContainerType::get_elem_size(elem_ts);
    Label finalizer_label = elem_ts.prefix(circularray_type).get_finalizer_label(cx);
    
    cx->code_label_local(label, elem_ts.prefix(queue_type).symbolize("alloc"));
    cx->prologue();
    
    container_alloc(CIRCULARRAY_HEADER_SIZE, elem_size, CIRCULARRAY_RESERVATION_OFFSET, finalizer_label, cx);

    cx->op(MOVQ, Address(RAX, CIRCULARRAY_LENGTH_OFFSET), 0);
    cx->op(MOVQ, Address(RAX, CIRCULARRAY_FRONT_OFFSET), 0);
    
    cx->epilogue();
}


void compile_queue_realloc(Label label, TypeSpec elem_ts, Cx *cx) {
    // RAX - array, R10 - new reservation
    int elem_size = ContainerType::get_elem_size(elem_ts);

    cx->code_label_local(label, elem_ts.prefix(queue_type).symbolize("realloc"));
    cx->prologue();
    
    container_realloc(CIRCULARRAY_HEADER_SIZE, elem_size, CIRCULARRAY_RESERVATION_OFFSET, cx);
    
    cx->epilogue();
}


void compile_queue_grow(Label label, TypeSpec elem_ts, Cx *cx) {
    // RAX - array, R10 - new reservation
    // RCX, RSI, RDI - clob
    // Double the reservation until it's enough (can be relaxed to 1.5 times, but not less)
    Label realloc_label = cx->once->compile(compile_queue_realloc, elem_ts);
    int elem_size = ContainerType::get_elem_size(elem_ts);

    cx->code_label_local(label, elem_ts.prefix(queue_type).symbolize("grow"));
    cx->prologue();
    
    cx->runtime->log("grow_circularray");
    cx->op(PUSHQ, RCX);
    cx->op(PUSHQ, RSI);
    cx->op(PUSHQ, RDI);
    cx->op(PUSHQ, Address(RAX, CIRCULARRAY_RESERVATION_OFFSET));
    
    container_grow(CIRCULARRAY_RESERVATION_OFFSET, CIRCULARRAY_MINIMUM_RESERVATION, realloc_label, cx);

    cx->op(POPQ, R10);  // old reservation
    
    Label high, end;
    cx->op(MOVQ, RCX, R10);
    cx->op(SHRQ, RCX, 1);
    cx->op(CMPQ, Address(RAX, CIRCULARRAY_FRONT_OFFSET), RCX);
    cx->op(JAE, high);

    // The front is low, so it's better to unfold the folded part. This requires that
    // the growth rate was at least 1.5 times.
    
    cx->runtime->log("Unfolding queue circularray.");
    
    cx->op(LEA, RSI, Address(RAX, CIRCULARRAY_ELEMS_OFFSET));
    
    cx->op(MOVQ, RDI, R10);
    cx->op(IMUL3Q, RDI, RDI, elem_size);
    cx->op(LEA, RDI, Address(RAX, RDI, CIRCULARRAY_ELEMS_OFFSET));
    
    cx->op(MOVQ, RCX, Address(RAX, CIRCULARRAY_FRONT_OFFSET));
    cx->op(IMUL3Q, RCX, RCX, elem_size);
    
    cx->op(REPMOVSB);
    cx->op(JMP, end);
    
    cx->code_label(high);
    
    // The front is high, so it's better to move the unfolded part to the end of the
    // new reservation. This also requires 1.5 growth rate so we can copy forward.

    cx->runtime->log("Stretching queue circularray.");
    
    cx->op(MOVQ, RSI, Address(RAX, CIRCULARRAY_FRONT_OFFSET));
    cx->op(IMUL3Q, RSI, RSI, elem_size);
    cx->op(LEA, RSI, Address(RAX, RSI, CIRCULARRAY_ELEMS_OFFSET));

    cx->op(MOVQ, RDI, Address(RAX, CIRCULARRAY_FRONT_OFFSET));
    cx->op(SUBQ, RDI, R10);
    cx->op(ADDQ, RDI, Address(RAX, CIRCULARRAY_RESERVATION_OFFSET));
    cx->op(MOVQ, Address(RAX, CIRCULARRAY_FRONT_OFFSET), RDI);  // must update front index
    cx->op(IMUL3Q, RDI, RDI, elem_size);
    cx->op(LEA, RDI, Address(RAX, RDI, CIRCULARRAY_ELEMS_OFFSET));
    
    cx->op(MOVQ, RCX, Address(RAX, CIRCULARRAY_RESERVATION_OFFSET));
    cx->op(SUBQ, RCX, Address(RAX, CIRCULARRAY_FRONT_OFFSET));
    cx->op(IMUL3Q, RCX, RCX, elem_size);
    
    cx->op(REPMOVSB);
    
    cx->code_label(end);
    cx->op(POPQ, RDI);
    cx->op(POPQ, RSI);
    cx->op(POPQ, RCX);
    
    cx->epilogue();
}


void compile_queue_clone(Label label, TypeSpec elem_ts, Cx *cx) {
    // RAX - Circularray Ref
    // Return a cloned Ref
    Label loop, end, linear, linear2, loop2;
    Label alloc_label = cx->once->compile(compile_queue_alloc, elem_ts);
    int elem_size = ContainerType::get_elem_size(elem_ts);
    TypeSpec heap_ts = elem_ts.prefix(circularray_type);
    
    cx->code_label_local(label, elem_ts.prefix(queue_type).symbolize("clone"));
    cx->prologue();
    cx->runtime->log("XXX queue clone");
    
    cx->op(PUSHQ, RAX);
    cx->op(MOVQ, R10, Address(RAX, CIRCULARRAY_RESERVATION_OFFSET));
    cx->op(CALL, alloc_label);  // clobbers all
    
    cx->op(POPQ, RBX);  // orig
    cx->op(MOVQ, RCX, Address(RBX, CIRCULARRAY_LENGTH_OFFSET));
    cx->op(MOVQ, Address(RAX, CIRCULARRAY_LENGTH_OFFSET), RCX);

    cx->op(CMPQ, RCX, 0);
    cx->op(JE, end);

    cx->op(MOVQ, RDX, Address(RBX, CIRCULARRAY_FRONT_OFFSET));
    cx->op(MOVQ, Address(RAX, CIRCULARRAY_FRONT_OFFSET), RDX);
    cx->op(IMUL3Q, R10, RDX, elem_size);

    cx->op(LEA, RSI, Address(RBX, R10, CIRCULARRAY_ELEMS_OFFSET));
    cx->op(LEA, RDI, Address(RAX, R10, CIRCULARRAY_ELEMS_OFFSET));
    cx->op(ADDQ, RDX, RCX);  // theoretical end index
    cx->op(SUBQ, RDX, Address(RBX, CIRCULARRAY_RESERVATION_OFFSET));  // wrapped length
    cx->op(JLE, linear);
    
    cx->op(SUBQ, RCX, RDX);  // trim first loop (remains nonempty)
    
    cx->code_label(linear);
    cx->code_label(loop);
    elem_ts.create(Storage(MEMORY, Address(RSI, 0)), Storage(MEMORY, Address(RDI, 0)), cx);
    cx->op(ADDQ, RSI, elem_size);
    cx->op(ADDQ, RDI, elem_size);
    cx->op(DECQ, RCX);
    cx->op(JNE, loop);

    cx->op(CMPQ, RDX, 0);
    cx->op(JLE, linear2);
    
    cx->op(LEA, RSI, Address(RBX, CIRCULARRAY_ELEMS_OFFSET));
    cx->op(LEA, RDI, Address(RAX, CIRCULARRAY_ELEMS_OFFSET));
    cx->op(MOVQ, RCX, RDX);
    
    cx->code_label(loop2);
    elem_ts.create(Storage(MEMORY, Address(RSI, 0)), Storage(MEMORY, Address(RDI, 0)), cx);
    cx->op(ADDQ, RSI, elem_size);
    cx->op(ADDQ, RDI, elem_size);
    cx->op(DECQ, RCX);
    cx->op(JNE, loop2);
    
    cx->code_label(linear2);
    heap_ts.decref(RBX, cx);
    
    cx->code_label(end);
    cx->epilogue();
}




QueueLengthValue::QueueLengthValue(Value *l, TypeMatch &match)
    :ContainerLengthValue(l, match, match[1].prefix(circularray_type), CIRCULARRAY_LENGTH_OFFSET) {
}



QueueIndexValue::QueueIndexValue(Value *pivot, TypeMatch &match)
    :ContainerIndexValue(pivot, match, match[1].prefix(circularray_type), CIRCULARRAY_LENGTH_OFFSET, CIRCULARRAY_ELEMS_OFFSET) {
}

void QueueIndexValue::fix_index(Register r, Register i, Cx *cx) {
    fix_index_overflow(r, i, cx);
}



QueueEmptyValue::QueueEmptyValue(TypeSpec ts)
    :ContainerEmptyValue(ts, compile_queue_alloc) {
}



QueueReservedValue::QueueReservedValue(TypeSpec ts)
    :ContainerReservedValue(ts, compile_queue_alloc) {
}



QueueInitializerValue::QueueInitializerValue(TypeSpec ts)
    :ContainerInitializerValue(ts.unprefix(queue_type), ts, CIRCULARRAY_LENGTH_OFFSET, CIRCULARRAY_ELEMS_OFFSET, compile_queue_alloc) {
}



QueuePushValue::QueuePushValue(Value *l, TypeMatch &match)
    :ContainerPushValue(l, match, CIRCULARRAY_RESERVATION_OFFSET, CIRCULARRAY_LENGTH_OFFSET, CIRCULARRAY_ELEMS_OFFSET, compile_queue_clone, compile_queue_grow) {
}

void QueuePushValue::fix_index(Register r, Register i, Cx *cx) {
    fix_index_overflow(r, i, cx);
}



QueuePopValue::QueuePopValue(Value *l, TypeMatch &match)
    :ContainerPopValue(l, match, match[1].prefix(circularray_type), CIRCULARRAY_LENGTH_OFFSET, CIRCULARRAY_ELEMS_OFFSET, compile_queue_clone) {
}

void QueuePopValue::fix_index(Register r, Register i, Cx *cx) {
    fix_index_overflow(r, i, cx);
}



QueueUnshiftValue::QueueUnshiftValue(Value *l, TypeMatch &match)
    :QueuePushValue(l, match) {
}

void QueueUnshiftValue::fix_index(Register r, Register i, Cx *cx) {
    // Compute the new front, and use it for the element index
    cx->op(MOVQ, i, -1);
    fix_index_underflow(r, i, cx);
    cx->op(MOVQ, Address(r, CIRCULARRAY_FRONT_OFFSET), i);
}



QueueShiftValue::QueueShiftValue(Value *l, TypeMatch &match)
    :QueuePopValue(l, match) {
}

void QueueShiftValue::fix_index(Register r, Register i, Cx *cx) {
    // Compute the new front, and use the old one for the element index
    cx->op(MOVQ, i, 1);
    fix_index_overflow(r, i, cx);
    cx->op(XCHGQ, i, Address(r, CIRCULARRAY_FRONT_OFFSET));
}


// Iteration

QueueElemIterValue::QueueElemIterValue(Value *l, TypeMatch &match)
    :ContainerIterValue(typesubst(SAME_QUEUEELEMITER_TS, match), l) {
}



QueueIndexIterValue::QueueIndexIterValue(Value *l, TypeMatch &match)
    :ContainerIterValue(typesubst(SAME_QUEUEINDEXITER_TS, match), l) {
}



QueueItemIterValue::QueueItemIterValue(Value *l, TypeMatch &match)
    :ContainerIterValue(typesubst(SAME_QUEUEITEMITER_TS, match), l) {
}



QueueNextElemValue::QueueNextElemValue(Value *l, TypeMatch &match)
    :ContainerNextValue(typesubst(SAME_LVALUE_TUPLE1_TS, match), match[1], l, CIRCULARRAY_LENGTH_OFFSET, false) {
}

Regs QueueNextElemValue::precompile(Regs preferred) {
    return ContainerNextValue::precompile(preferred) | precompile_contained_lvalue();
}

Storage QueueNextElemValue::postprocess(Register r, Register i, Cx *cx) {
    int elem_size = ContainerType::get_elem_size(elem_ts);

    fix_index_overflow(r, i, cx);
    
    Address addr = cx->runtime->make_address(r, i, elem_size, CIRCULARRAY_ELEMS_OFFSET);
    
    return compile_contained_lvalue(addr, NOREG, ts, cx);
}



QueueNextIndexValue::QueueNextIndexValue(Value *l, TypeMatch &match)
    :ContainerNextValue(INTEGER_TUPLE1_TS, match[1], l, CIRCULARRAY_LENGTH_OFFSET, false) {
}

Storage QueueNextIndexValue::postprocess(Register r, Register i, Cx *cx) {
    return Storage(REGISTER, i);
}



QueueNextItemValue::QueueNextItemValue(Value *l, TypeMatch &match)
    :ContainerNextValue(typesubst(INTEGER_SAME_LVALUE_TUPLE2_TS, match), match[1], l, CIRCULARRAY_LENGTH_OFFSET, false) {
}

Storage QueueNextItemValue::postprocess(Register r, Register i, Cx *cx) {
    int elem_size = ContainerType::get_elem_size(elem_ts);
    //int item_stack_size = ts.measure_stack();

    cx->op(PUSHQ, i);
    
    fix_index_overflow(r, i, cx);

    Address addr = cx->runtime->make_address(r, i, elem_size, CIRCULARRAY_ELEMS_OFFSET);
    
    cx->op(PUSHQ, 0);
    cx->op(LEA, R10, addr);
    cx->op(PUSHQ, R10);
    
    return Storage(STACK);
}
