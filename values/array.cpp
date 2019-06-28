#include "../plum.h"


void compile_array_alloc(Label label, TypeSpec elem_ts, Cx *cx) {
    // R10 - reservation
    int elem_size = ContainerType::get_elem_size(elem_ts);
    Label finalizer_label = elem_ts.prefix(linearray_type).get_finalizer_label(cx);
    
    cx->code_label_local(label, elem_ts.prefix(array_type).symbolize("alloc"));
    cx->prologue();
    
    container_alloc(LINEARRAY_HEADER_SIZE, elem_size, LINEARRAY_RESERVATION_OFFSET, finalizer_label, cx);

    cx->op(MOVQ, Address(RAX, LINEARRAY_LENGTH_OFFSET), 0);
    
    cx->epilogue();
}


void compile_array_realloc(Label label, TypeSpec elem_ts, Cx *cx) {
    // RAX - array, R10 - new reservation
    int elem_size = ContainerType::get_elem_size(elem_ts);

    cx->code_label_local(label, elem_ts.prefix(array_type).symbolize("realloc"));
    cx->prologue();

    container_realloc(LINEARRAY_HEADER_SIZE, elem_size, LINEARRAY_RESERVATION_OFFSET, cx);

    cx->epilogue();
}


void compile_array_grow(Label label, TypeSpec elem_ts, Cx *cx) {
    // RAX - array, R10 - new reservation
    // Double the reservation until it's enough (can be relaxed to 1.5 times, but not less)
    Label realloc_label = cx->once->compile(compile_array_realloc, elem_ts);

    cx->code_label_local(label, elem_ts.prefix(array_type).symbolize("grow"));
    cx->prologue();
    
    container_grow(LINEARRAY_RESERVATION_OFFSET, LINEARRAY_MINIMUM_RESERVATION, realloc_label, cx);
    
    cx->epilogue();
}


void compile_array_clone(Label label, TypeSpec elem_ts, Cx *cx) {
    // RAX - Linearray Ref
    // Return a cloned Ref
    Label loop, end;
    Label alloc_label = cx->once->compile(compile_array_alloc, elem_ts);
    int elem_size = ContainerType::get_elem_size(elem_ts);
    TypeSpec heap_ts = elem_ts.prefix(linearray_type);
    
    cx->code_label_local(label, elem_ts.prefix(array_type).symbolize("clone"));
    cx->prologue();
    cx->runtime->log("XXX array clone");
    
    cx->op(PUSHQ, RAX);
    cx->op(MOVQ, R10, Address(RAX, LINEARRAY_RESERVATION_OFFSET));
    cx->op(CALL, alloc_label);  // clobbers all
    
    cx->op(POPQ, RBX);  // orig
    cx->op(MOVQ, RCX, Address(RBX, LINEARRAY_LENGTH_OFFSET));
    cx->op(MOVQ, Address(RAX, LINEARRAY_LENGTH_OFFSET), RCX);

    // And this is the general version
    cx->op(CMPQ, RCX, 0);
    cx->op(JE, end);

    cx->op(LEA, RSI, Address(RBX, LINEARRAY_ELEMS_OFFSET));
    cx->op(LEA, RDI, Address(RAX, LINEARRAY_ELEMS_OFFSET));
    
    cx->code_label(loop);
    elem_ts.create(Storage(MEMORY, Address(RSI, 0)), Storage(MEMORY, Address(RDI, 0)), cx);
    cx->op(ADDQ, RSI, elem_size);
    cx->op(ADDQ, RDI, elem_size);
    cx->op(DECQ, RCX);
    cx->op(JNE, loop);

    heap_ts.decref(RBX, cx);
    
    cx->code_label(end);
    cx->epilogue();
}



ArrayLengthValue::ArrayLengthValue(Value *l, TypeMatch &match)
    :ContainerLengthValue(l, match, match[1].prefix(linearray_type), LINEARRAY_LENGTH_OFFSET) {
}



ArrayIndexValue::ArrayIndexValue(Value *pivot, TypeMatch &match)
    :ContainerIndexValue(pivot, match, match[1].prefix(linearray_type), LINEARRAY_LENGTH_OFFSET, LINEARRAY_ELEMS_OFFSET) {
}



ArrayEmptyValue::ArrayEmptyValue(TypeSpec ts)
    :ContainerEmptyValue(ts, compile_array_alloc) {
}



ArrayReservedValue::ArrayReservedValue(TypeSpec ts)
    :ContainerReservedValue(ts, compile_array_alloc) {
}



ArrayAllValue::ArrayAllValue(TypeSpec ts)
    :ContainerAllValue(ts, LINEARRAY_LENGTH_OFFSET, LINEARRAY_ELEMS_OFFSET, compile_array_alloc) {
}



ArrayInitializerValue::ArrayInitializerValue(TypeSpec ts)
    :ContainerInitializerValue(ts.unprefix(array_type), ts, LINEARRAY_LENGTH_OFFSET, LINEARRAY_ELEMS_OFFSET, compile_array_alloc) {
}



ArrayPushValue::ArrayPushValue(Value *l, TypeMatch &match)
    :ContainerPushValue(l, match, LINEARRAY_RESERVATION_OFFSET, LINEARRAY_LENGTH_OFFSET, LINEARRAY_ELEMS_OFFSET, compile_array_clone, compile_array_grow) {
}



ArrayPopValue::ArrayPopValue(Value *l, TypeMatch &match)
    :ContainerPopValue(l, match, match[1].prefix(linearray_type), LINEARRAY_LENGTH_OFFSET, LINEARRAY_ELEMS_OFFSET, compile_array_clone) {
}

/*

ArrayAutogrowValue::ArrayAutogrowValue(Value *l, TypeMatch &match)
    :ContainerAutogrowValue(l, match) {
}

Storage compile(Cx *cx) {
    return subcompile(LINEARRAY_RESERVATION_OFFSET, LINEARRAY_LENGTH_OFFSET, compile_array_grow, cx);
}
*/


TypeSpec elem_ts;

ArrayConcatenationValue::ArrayConcatenationValue(Value *l, TypeMatch &match)
    :GenericValue(match[0], match[0], l) {
    elem_ts = match[1];
}

Regs ArrayConcatenationValue::precompile(Regs preferred) {
    right->precompile_tail();
    left->precompile_tail();
    
    return Regs::all();
}

Storage ArrayConcatenationValue::compile(Cx *cx) {
    Label l = cx->once->compile(compile_array_concatenation, elem_ts);

    compile_and_store_both(cx, Storage(STACK), Storage(STACK));

    cx->op(CALL, l);  // result array ref in RAX
    
    right->ts.store(rs, Storage(), cx);
    left->ts.store(ls, Storage(), cx);
    
    return Storage(REGISTER, RAX);
}

void ArrayConcatenationValue::compile_array_concatenation(Label label, TypeSpec elem_ts, Cx *cx) {
    // RAX - result, R10 - first, R11 - second
    cx->code_label_local(label, elem_ts.prefix(array_type).symbolize("concatenation"));
    cx->prologue();

    Label alloc_array = cx->once->compile(compile_array_alloc, elem_ts);
    int elem_size = ContainerType::get_elem_size(elem_ts);
    
    cx->op(MOVQ, R10, Address(RSP, RIP_SIZE + ADDRESS_SIZE + REFERENCE_SIZE));
    cx->op(MOVQ, R11, Address(RSP, RIP_SIZE + ADDRESS_SIZE));
    
    cx->op(MOVQ, R10, Address(R10, LINEARRAY_LENGTH_OFFSET));
    cx->op(ADDQ, R10, Address(R11, LINEARRAY_LENGTH_OFFSET));  // total length
    
    cx->op(CALL, alloc_array);  // clobbers all
    
    cx->op(LEA, RDI, Address(RAX, LINEARRAY_ELEMS_OFFSET));
    
    // copy left array
    cx->op(MOVQ, R10, Address(RSP, RIP_SIZE + ADDRESS_SIZE + REFERENCE_SIZE));  // restored
    cx->op(LEA, RSI, Address(R10, LINEARRAY_ELEMS_OFFSET));
    cx->op(MOVQ, RCX, Address(R10, LINEARRAY_LENGTH_OFFSET));
    cx->op(ADDQ, Address(RAX, LINEARRAY_LENGTH_OFFSET), RCX);
    
    // This is the raw bytes version
    //cx->op(IMUL3Q, RCX, RCX, elem_size);
    //cx->op(REPMOVSB);
    
    // And this is the general version
    Label left_loop, left_end;
    cx->op(CMPQ, RCX, 0);
    cx->op(JE, left_end);
    
    cx->code_label(left_loop);
    elem_ts.create(Storage(MEMORY, Address(RSI, 0)), Storage(MEMORY, Address(RDI, 0)), cx);
    cx->op(ADDQ, RSI, elem_size);
    cx->op(ADDQ, RDI, elem_size);
    cx->op(DECQ, RCX);
    cx->op(JNE, left_loop);
    cx->code_label(left_end);

    // copy right array
    cx->op(MOVQ, R11, Address(RSP, RIP_SIZE + ADDRESS_SIZE));  // restored
    cx->op(LEA, RSI, Address(R11, LINEARRAY_ELEMS_OFFSET));
    cx->op(MOVQ, RCX, Address(R11, LINEARRAY_LENGTH_OFFSET));
    cx->op(ADDQ, Address(RAX, LINEARRAY_LENGTH_OFFSET), RCX);
    
    // This is the raw bytes version
    //cx->op(IMUL3Q, RCX, RCX, elem_size);
    //cx->op(REPMOVSB);

    // And this is the general version
    Label right_loop, right_end;
    cx->op(CMPQ, RCX, 0);
    cx->op(JE, right_end);
    
    cx->code_label(right_loop);
    elem_ts.create(Storage(MEMORY, Address(RSI, 0)), Storage(MEMORY, Address(RDI, 0)), cx);
    cx->op(ADDQ, RSI, elem_size);
    cx->op(ADDQ, RDI, elem_size);
    cx->op(DECQ, RCX);
    cx->op(JNE, right_loop);
    cx->code_label(right_end);
    
    cx->epilogue();  // new array in RAX
}




ArrayExtendValue::ArrayExtendValue(Value *l, TypeMatch &match)
    :GenericValue(match[0].rvalue(), match[0].rvalue(), l) {
    elem_ts = match[1];
    heap_ts = elem_ts.prefix(linearray_type);
}

bool ArrayExtendValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    //check_alias(scope);
    
    return GenericValue::check(args, kwargs, scope);
}

Regs ArrayExtendValue::precompile(Regs preferred) {
    Regs rclob = right->precompile_tail();
    left->precompile(preferred & ~rclob);
    
    return Regs::all();
}

Storage ArrayExtendValue::compile(Cx *cx) {
    // TODO: This is just a concatenation and an assignment, could be more optimal.
    Label l = cx->once->compile(ArrayConcatenationValue::compile_array_concatenation, elem_ts);

    ls = left->compile_lvalue(cx);

    cx->op(SUBQ, RSP, ADDRESS_SIZE);  // placeholder for borrowed left side Ref
    right->compile_and_store(cx, Storage(STACK));

    Storage als = ls.access(ADDRESS_SIZE * 2);
    cx->runtime->load_lvalue(R10, R11, als);
    cx->op(MOVQ, Address(RSP, ADDRESS_SIZE), R10);  // borrow left side Ref

    cx->op(CALL, l);  // result array ref in RAX

    // Now mimic a ref assignment
    cx->runtime->exchange_lvalue(RAX, R11, als);
    cx->runtime->decref(RAX);
    
    right->ts.store(Storage(STACK), Storage(), cx);
    cx->op(ADDQ, RSP, ADDRESS_SIZE);
    
    return ls;
}




ArrayReallocValue::ArrayReallocValue(OperationType o, Value *l, TypeMatch &match)
    :GenericOperationValue(o, INTEGER_OVALUE_TS, l->ts, l) {
    elem_ts = match[1];
}

bool ArrayReallocValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    //check_alias(scope);
    
    return GenericOperationValue::check(args, kwargs, scope);
}

Regs ArrayReallocValue::precompile(Regs preferred) {
    GenericOperationValue::precompile(preferred);
    return Regs::all();
}

Storage ArrayReallocValue::compile(Cx *cx) {
    Label realloc_array = cx->once->compile(compile_array_realloc, elem_ts);

    ls = left->compile_lvalue(cx);

    if (right) {
        right->compile_and_store(cx, Storage(STACK));
        cx->op(POPQ, R10);
    }

    Storage als = ls.access(0);
    cx->runtime->load_lvalue(RAX, R11, als);
    
    if (!right)
        cx->op(MOVQ, R10, Address(RAX, LINEARRAY_LENGTH_OFFSET));  // shrink to fit
        
    cx->op(CALL, realloc_array);  // clobbers all

    // Although realloc may change the address of the container, it is technically
    // the same object with the same refcount, so we can just store it.
    cx->runtime->store_lvalue(RAX, R11, als);
    
    return ls;
}




ArraySortValue::ArraySortValue(Value *l, TypeMatch &match)
    :GenericValue(NO_TS, VOID_TS, l) {
    elem_ts = match[1];
}

Regs ArraySortValue::precompile(Regs preferred) {
    return left->precompile_tail() | Regs(RAX, RCX, RDX, RSI, RDI) | COMPARE_CLOB;
}

Storage ArraySortValue::compile(Cx *cx) {
    auto arg_regs = cx->abi_arg_regs();
    int elem_size = ContainerType::get_elem_size(elem_ts);
    Label compar = cx->once->compile(compile_compar, elem_ts);
    Label done;

    left->compile_and_store(cx, Storage(STACK));
    
    // base, nmemb, size, compar
    cx->op(MOVQ, R10, Address(RSP, 0));
    cx->op(LEA, arg_regs[0], Address(R10, LINEARRAY_ELEMS_OFFSET));
    cx->op(MOVQ, arg_regs[1], Address(R10, LINEARRAY_LENGTH_OFFSET));
    cx->op(MOVQ, arg_regs[2], elem_size);
    cx->op(LEA, arg_regs[3], Address(compar, 0));

    cx->runtime->call_sysv(cx->runtime->sysv_sort_label);
    
    left->ts.store(Storage(STACK), Storage(), cx);
    
    return Storage();
}

void ArraySortValue::compile_compar(Label label, TypeSpec elem_ts, Cx *cx) {
    // Generate a SysV function to wrap our compare function.
    // The two arguments contain the pointers to the array elements.
    auto arg_regs = cx->abi_arg_regs();
    auto res_regs = cx->abi_res_regs();

    cx->code_label_local(label, elem_ts.prefix(array_type).symbolize("compar"));
    
    cx->runtime->callback_prologue();
    
    Storage a(MEMORY, Address(arg_regs[0], 0));
    Storage b(MEMORY, Address(arg_regs[1], 0));

    elem_ts.compare(a, b, cx);
    
    cx->op(MOVSXBQ, res_regs[0], R10B);

    cx->runtime->callback_epilogue();
}




ArrayRemoveValue::ArrayRemoveValue(Value *l, TypeMatch &match)
    :GenericValue(INTEGER_TS, match[0], l) {
    elem_ts = match[1];
}

Regs ArrayRemoveValue::precompile(Regs preferred) {
    return left->precompile_tail() | right->precompile_tail() | Regs::all();  // SysV
}

Storage ArrayRemoveValue::compile(Cx *cx) {
    Label remove_label = cx->once->compile(compile_remove, elem_ts);

    left->compile_and_store(cx, Storage(STACK));
    right->compile_and_store(cx, Storage(STACK));
    
    cx->op(CALL, remove_label);
    
    cx->op(POPQ, R10);
    
    return Storage(STACK);
}

void ArrayRemoveValue::compile_remove(Label label, TypeSpec elem_ts, Cx *cx) {
    auto arg_regs = cx->abi_arg_regs();
    int elem_size = ContainerType::get_elem_size(elem_ts);

    cx->code_label(label);
    cx->prologue();
    Label ok, loop, check;
    
    cx->op(MOVQ, RAX, Address(RSP, RIP_SIZE + ADDRESS_SIZE + INTEGER_SIZE));
    cx->op(MOVQ, RCX, Address(RAX, LINEARRAY_LENGTH_OFFSET));
    cx->op(CMPQ, RCX, Address(RSP, RIP_SIZE + ADDRESS_SIZE));
    cx->op(JAE, ok);
    
    cx->runtime->die("Array remove length out of bounds!");  // FIXME: raise instead
    
    // Destroy the first elements
    cx->code_label(ok);
    cx->op(MOVQ, RCX, 0);
    cx->op(JMP, check);
    
    cx->code_label(loop);
    cx->op(IMUL3Q, RDX, RCX, elem_size);
    
    elem_ts.destroy(Storage(MEMORY, Address(RAX, RDX, LINEARRAY_ELEMS_OFFSET)), cx);
    cx->op(INCQ, RCX);
    
    cx->code_label(check);
    cx->op(CMPQ, RCX, Address(RSP, RIP_SIZE + ADDRESS_SIZE));
    cx->op(JB, loop);

    cx->op(SUBQ, Address(RAX, LINEARRAY_LENGTH_OFFSET), RCX);

    // call memmove(dest, src, n)
    cx->op(MOVQ, R10, RAX);  // borrowed array ref
    cx->op(MOVQ, R11, RCX);  // remove count
    
    cx->op(LEA, arg_regs[0], Address(R10, LINEARRAY_ELEMS_OFFSET));  // dest
    
    cx->op(IMUL3Q, arg_regs[1], R11, elem_size);
    cx->op(ADDQ, arg_regs[1], arg_regs[0]);  // src
    
    cx->op(MOVQ, arg_regs[2], Address(R10, LINEARRAY_LENGTH_OFFSET));
    cx->op(IMUL3Q, arg_regs[2], arg_regs[2], elem_size);  // n
    
    cx->runtime->call_sysv(cx->runtime->sysv_memmove_label);

    cx->epilogue();
}




ArrayRefillValue::ArrayRefillValue(Value *l, TypeMatch &match)
    :Value(match[0]) {
    array_value.reset(l);
    elem_ts = match[1];
}

bool ArrayRefillValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    //check_alias(scope);
    
    ArgInfos infos = {
        { "fill", &elem_ts, scope, &fill_value },
        { "length", &INTEGER_TS, scope, &length_value }
    };
    
    if (!check_arguments(args, kwargs, infos))
        return false;

    return true;
}

Regs ArrayRefillValue::precompile(Regs preferred) {
    Regs clob = length_value->precompile_tail();
    clob = clob | fill_value->precompile_tail();
    clob = clob | array_value->precompile(preferred & ~clob);

    return Regs::all();
}

Storage ArrayRefillValue::compile(Cx *cx) {
    Label ok, loop, check;
    int elem_size = ContainerType::get_elem_size(elem_ts);
    Label realloc_array = cx->once->compile(compile_array_realloc, elem_ts);

    Storage as = array_value->compile_lvalue(cx);
    fill_value->compile_and_store(cx, Storage(STACK));
    length_value->compile_and_store(cx, Storage(STACK));
    
    // borrow array ref
    Storage aas = as.access(fill_value->ts.measure_stack() + INTEGER_TS.measure_stack());
    cx->runtime->load_lvalue(RAX, R11, aas);
    
    cx->op(MOVQ, R10, Address(RSP, 0));  // extra length (keep on stack)
    cx->op(ADDQ, R10, Address(RAX, LINEARRAY_LENGTH_OFFSET));
    cx->op(CMPQ, R10, Address(RAX, LINEARRAY_RESERVATION_OFFSET));
    cx->op(JBE, ok);
    
    // Need to reallocate
    cx->op(CALL, realloc_array);  // clobbers all

    cx->runtime->store_lvalue(RAX, R11, aas);
    
    cx->code_label(ok);
    cx->op(MOVQ, RDX, Address(RAX, LINEARRAY_LENGTH_OFFSET));
    cx->op(IMUL3Q, RDX, RDX, elem_size);
    cx->op(POPQ, RCX);  // pop extra length, leave fill value on stack
    cx->op(ADDQ, Address(RAX, LINEARRAY_LENGTH_OFFSET), RCX);
    cx->op(JMP, check);
    
    cx->code_label(loop);
    // Use MEMORY for the create source, so it won't be popped
    elem_ts.create(Storage(MEMORY, Address(RSP, 0)), Storage(MEMORY, Address(RAX, RDX, LINEARRAY_ELEMS_OFFSET)), cx);
    cx->op(ADDQ, RDX, elem_size);
    cx->op(DECQ, RCX);
    
    cx->code_label(check);
    cx->op(CMPQ, RCX, 0);
    cx->op(JNE, loop);
    
    // drop fill value
    elem_ts.store(Storage(STACK), Storage(), cx);
    
    return as;
}


// Iteration


ArrayElemIterValue::ArrayElemIterValue(Value *l, TypeMatch &match)
    :ContainerIterValue(typesubst(SAME_ARRAYELEMITER_TS, match), l) {
}



ArrayIndexIterValue::ArrayIndexIterValue(Value *l, TypeMatch &match)
    :ContainerIterValue(typesubst(SAME_ARRAYINDEXITER_TS, match), l) {
}



ArrayItemIterValue::ArrayItemIterValue(Value *l, TypeMatch &match)
    :ContainerIterValue(typesubst(SAME_ARRAYITEMITER_TS, match), l) {
}



ArrayNextElemValue::ArrayNextElemValue(Value *l, TypeMatch &match)
    :ContainerNextValue(typesubst(SAME_LVALUE_TUPLE1_TS, match), match[1], l, LINEARRAY_LENGTH_OFFSET, false) {
}

Regs ArrayNextElemValue::precompile(Regs preferred) {
    return ContainerNextValue::precompile(preferred) | precompile_contained_lvalue();
}

Storage ArrayNextElemValue::postprocess(Register r, Register i, Cx *cx) {
    int elem_size = ContainerType::get_elem_size(elem_ts);
    
    Address addr = cx->runtime->make_address(r, i, elem_size, LINEARRAY_ELEMS_OFFSET);

    return compile_contained_lvalue(addr, NOREG, ts, cx);
}



ArrayNextIndexValue::ArrayNextIndexValue(Value *l, TypeMatch &match)
    :ContainerNextValue(INTEGER_TUPLE1_TS, match[1], l, LINEARRAY_LENGTH_OFFSET, false) {
}

Storage ArrayNextIndexValue::postprocess(Register r, Register i, Cx *cx) {
    return Storage(REGISTER, i);
}



ArrayNextItemValue::ArrayNextItemValue(Value *l, TypeMatch &match)
    :ContainerNextValue(typesubst(INTEGER_SAME_LVALUE_TUPLE2_TS, match), match[1], l, LINEARRAY_LENGTH_OFFSET, false) {
}

Storage ArrayNextItemValue::postprocess(Register r, Register i, Cx *cx) {
    int elem_size = ContainerType::get_elem_size(elem_ts);
    //int item_stack_size = ts.measure_stack();

    cx->op(PUSHQ, i);
    
    Address addr = cx->runtime->make_address(r, i, elem_size, LINEARRAY_ELEMS_OFFSET);
    
    cx->op(PUSHQ, 0);
    cx->op(LEA, R10, addr);
    cx->op(PUSHQ, R10);
    
    return Storage(STACK);
}
