#include "../plum.h"


StringRegexpMatcherValue::StringRegexpMatcherValue(Value *l, TypeMatch &match)
    :GenericValue(STRING_TS, STRING_ARRAY_TS, l) {
}

bool StringRegexpMatcherValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    if (!check_raise(match_unmatched_exception_type, scope))
        return false;
    
    return GenericValue::check(args, kwargs, scope);
}

Regs StringRegexpMatcherValue::precompile(Regs preferred) {
    Regs clob = right->precompile_tail();
    left->precompile(~clob);
    
    return Regs::all();
}

Storage StringRegexpMatcherValue::compile(Cx *cx) {
    compile_and_store_both(cx, Storage(STACK), Storage(STACK));
    Label ok;
    auto arg_regs = cx->abi_arg_regs();
    auto res_regs = cx->abi_res_regs();

    cx->op(MOVQ, arg_regs[0], Address(RSP, ADDRESS_SIZE));
    cx->op(MOVQ, arg_regs[1], Address(RSP, 0));
    
    // This uses SIMD instructions on X64, so SysV stack alignment must be ensured
    cx->runtime->call_sysv(cx->runtime->sysv_string_regexp_match_label);

    right->ts.store(Storage(STACK), Storage(), cx);
    left->ts.store(Storage(STACK), Storage(), cx);
    
    cx->op(CMPQ, res_regs[0], 0);
    cx->op(JNE, ok);
    
    raise("UNMATCHED", cx);
    
    cx->code_label(ok);

    return Storage(REGISTER, res_regs[0]);
}



SliceEmptyValue::SliceEmptyValue(TypeMatch &match)
    :GenericValue(NO_TS, match[1].prefix(slice_type), NULL) {
}

Regs SliceEmptyValue::precompile(Regs preferred) {
    return Regs();
}

Storage SliceEmptyValue::compile(Cx *cx) {
    cx->op(LEA, R10, Address(cx->runtime->empty_array_label, 0));
    TypeSpec heap_ts = ts.reprefix(slice_type, linearray_type);
    heap_ts.incref(R10, cx);
    
    cx->op(PUSHQ, 0);  // length
    cx->op(PUSHQ, 0);  // front
    cx->op(PUSHQ, R10);  // ptr
    
    return Storage(STACK);
}



SliceAllValue::SliceAllValue(TypeMatch &match)
    :GenericValue(match[1].prefix(array_type), match[1].prefix(slice_type), NULL) {
}

Regs SliceAllValue::precompile(Regs preferred) {
    return right->precompile_tail();
}

Storage SliceAllValue::compile(Cx *cx) {
    TypeSpec heap_ts = ts.reprefix(slice_type, linearray_type);
    Storage rs = right->compile(cx);
    Register r;
    
    switch (rs.where) {
    case REGISTER:
        r = rs.reg;
        break;
    case STACK:
        cx->op(POPQ, R10);
        r = R10;
        break;
    case MEMORY:
        cx->op(MOVQ, R10, rs.address);
        heap_ts.incref(R10, cx);
        r = R10;
        break;
    default:
        throw INTERNAL_ERROR;
    }
    
    cx->op(PUSHQ, Address(r, LINEARRAY_LENGTH_OFFSET));  // length
    cx->op(PUSHQ, 0);  // front
    cx->op(PUSHQ, r);  // ptr
    
    return Storage(STACK);
}




ArraySliceValue::ArraySliceValue(Value *pivot, TypeMatch &match)
    :Value(match[1].prefix(slice_type)) {
    
    array_value.reset(pivot);
    heap_ts = match[1].prefix(linearray_type);
}

bool ArraySliceValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    if (!check_raise(lookup_exception_type, scope))
        return false;

    ArgInfos infos = {
        { "front", &INTEGER_TS, scope, &front_value },
        { "length", &INTEGER_TS, scope, &length_value }
    };
    
    if (!check_arguments(args, kwargs, infos))
        return false;

    return true;
}

Regs ArraySliceValue::precompile(Regs preferred) {
    Regs clob = length_value->precompile_tail();
    clob = clob | front_value->precompile_tail();
    clob = clob | array_value->precompile_tail();
    
    return clob | Regs(RAX, RCX, RDX);
}

Storage ArraySliceValue::compile(Cx *cx) {
    array_value->compile_and_store(cx, Storage(STACK));
    front_value->compile_and_store(cx, Storage(STACK));
    length_value->compile_and_store(cx, Storage(STACK));
    
    cx->op(POPQ, RCX);  // length
    cx->op(POPQ, R10);  // front
    cx->op(POPQ, RAX);  // ptr
    cx->op(MOVQ, RDX, Address(RAX, LINEARRAY_LENGTH_OFFSET));
    
    Label ok, nok;
    cx->op(CMPQ, R10, RDX);
    cx->op(JAE, nok);
    
    cx->op(SUBQ, RDX, R10);
    cx->op(CMPQ, RCX, RDX);
    cx->op(JBE, ok);
    
    cx->code_label(nok);

    // all popped
    heap_ts.decref(RAX, cx);
    raise("NOT_FOUND", cx);
    
    cx->code_label(ok);
    cx->op(PUSHQ, RCX);
    cx->op(PUSHQ, R10);
    cx->op(PUSHQ, RAX);  // inherit reference
    
    return Storage(STACK);
}



SliceSliceValue::SliceSliceValue(Value *pivot, TypeMatch &match)
    :Value(match[0]) {
    
    slice_value.reset(pivot);
}

bool SliceSliceValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    if (!check_raise(lookup_exception_type, scope))
        return false;

    ArgInfos infos = {
        { "front", &INTEGER_TS, scope, &front_value },
        { "length", &INTEGER_TS, scope, &length_value }
    };
    
    if (!check_arguments(args, kwargs, infos))
        return false;

    return true;
}

Regs SliceSliceValue::precompile(Regs preferred) {
    Regs clob = length_value->precompile_tail();
    clob = clob | front_value->precompile_tail();
    clob = clob | slice_value->precompile_tail();
    
    return clob | Regs(RAX, RCX, RDX);
}

Storage SliceSliceValue::compile(Cx *cx) {
    slice_value->compile_and_store(cx, Storage(STACK));
    front_value->compile_and_store(cx, Storage(STACK));
    length_value->compile_and_store(cx, Storage(STACK));
    
    cx->op(POPQ, RCX);  // length
    cx->op(POPQ, R10);  // front
    cx->op(MOVQ, RDX, Address(RSP, ADDRESS_SIZE + INTEGER_SIZE));  // old length
    
    Label ok, nok;
    cx->op(CMPQ, R10, RDX);
    cx->op(JAE, nok);
    
    cx->op(SUBQ, RDX, R10);
    cx->op(CMPQ, RCX, RDX);
    cx->op(JBE, ok);
    
    cx->code_label(nok);
    
    drop_and_raise(ts, Storage(STACK), "NOT_FOUND", cx);
    
    cx->code_label(ok);
    cx->op(ADDQ, Address(RSP, ADDRESS_SIZE), R10);  // adjust front
    cx->op(MOVQ, Address(RSP, ADDRESS_SIZE + INTEGER_SIZE), RCX);  // set length
    
    return Storage(STACK);
}



SliceIndexValue::SliceIndexValue(Value *pivot, TypeMatch &match)
    :GenericValue(INTEGER_TS, match[1].lvalue(), pivot) {
    elem_ts = match[1];
    heap_ts = elem_ts.prefix(linearray_type);
}

bool SliceIndexValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    if (!check_raise(lookup_exception_type, scope))
        return false;

    //if (!check_reference(scope))
    //    return false;
    
    return GenericValue::check(args, kwargs, scope);
}

Regs SliceIndexValue::precompile(Regs preferred) {
    Regs clob = right->precompile_tail();
    clob = clob | left->precompile(~clob);

    clob = clob | precompile_contained_lvalue();
        
    return clob | Regs(RAX, RBX);
}

Storage SliceIndexValue::compile(Cx *cx) {
    int elem_size = ContainerType::get_elem_size(elem_ts);

    // TODO: MEMORY pivot can be much more optimal
    left->compile_and_store(cx, Storage(STACK));
    right->compile_and_store(cx, Storage(STACK));
    
    cx->op(POPQ, RAX);  // index
    cx->op(POPQ, RBX);  // ptr
    cx->op(POPQ, R10);  // front
    cx->op(POPQ, R11);  // length

    Label ok;
    //defer_decref(RBX, cx);
    
    cx->op(CMPQ, RAX, R11);
    cx->op(JB, ok);

    // all popped
    raise("NOT_FOUND", cx);
    
    cx->code_label(ok);
    cx->op(ADDQ, RAX, R10);
    
    Address addr = cx->runtime->make_address(RBX, RAX, elem_size, LINEARRAY_ELEMS_OFFSET);

    return compile_contained_lvalue(addr, RBX, ts, cx);
}




SliceFindValue::SliceFindValue(Value *l, TypeMatch &match)
    :GenericValue(match[1], INTEGER_TS, l) {
    elem_ts = match[1];
    slice_ts = match[0];
}

bool SliceFindValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    if (!check_raise(lookup_exception_type, scope))
        return false;

    return GenericValue::check(args, kwargs, scope);
}

Regs SliceFindValue::precompile(Regs preferred) {
    Regs clob = right->precompile_tail();
    clob = clob | left->precompile(~clob);
    
    return clob | Regs(RAX, RCX, RDX) | COMPARE_CLOB;
}

Storage SliceFindValue::compile(Cx *cx) {
    left->compile_and_store(cx, Storage(STACK));
    right->compile_and_store(cx, Storage(STACK));
    
    Label loop, check, found;
    int elem_size = ContainerType::get_elem_size(elem_ts);
    int stack_size = elem_ts.measure_stack();

    cx->op(MOVQ, RAX, Address(RSP, stack_size));

    cx->op(MOVQ, RCX, 0);
    cx->op(MOVQ, RDX, Address(RSP, stack_size + ADDRESS_SIZE));  // front index
    cx->op(IMUL3Q, RDX, RDX, elem_size);
    cx->op(JMP, check);
    
    cx->code_label(loop);
    elem_ts.compare(Storage(MEMORY, Address(RAX, RDX, LINEARRAY_ELEMS_OFFSET)), Storage(MEMORY, Address(RSP, 0)), cx);
    cx->op(JE, found);

    cx->op(INCQ, RCX);
    cx->op(ADDQ, RDX, elem_size);
    
    cx->code_label(check);
    cx->op(CMPQ, RCX, Address(RSP, stack_size + ADDRESS_SIZE + INTEGER_SIZE));  // length
    cx->op(JB, loop);

    drop_two_and_raise(elem_ts, Storage(STACK), slice_ts, Storage(STACK), "NOT_FOUND", cx);

    cx->code_label(found);
    elem_ts.store(Storage(STACK), Storage(), cx);
    slice_ts.store(Storage(STACK), Storage(), cx);
    
    return Storage(REGISTER, RCX);
}


// Iteration
// TODO: too many similarities with container iteration!

SliceIterValue::SliceIterValue(TypeSpec t, Value *l)
    :SimpleRecordValue(t, l) {
}

Storage SliceIterValue::compile(Cx *cx) {
    cx->op(PUSHQ, 0);

    left->compile_and_store(cx, Storage(STACK));
    
    return Storage(STACK);
}



SliceElemIterValue::SliceElemIterValue(Value *l, TypeMatch &match)
    :SliceIterValue(typesubst(SAME_SLICEELEMITER_TS, match), l) {
}



SliceIndexIterValue::SliceIndexIterValue(Value *l, TypeMatch &match)
    :SliceIterValue(typesubst(SAME_SLICEINDEXITER_TS, match), l) {
}



SliceItemIterValue::SliceItemIterValue(Value *l, TypeMatch &match)
    :SliceIterValue(typesubst(SAME_SLICEITEMITER_TS, match), l) {
}




SliceNextValue::SliceNextValue(TypeSpec ts, TypeSpec ets, Value *l, bool d)
    :GenericValue(NO_TS, ts, l) {
    is_down = d;
    elem_ts = ets;
}

bool SliceNextValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    if (!check_arguments(args, kwargs, {}))
        return false;

    if (!check_raise(iterator_done_exception_type, scope))
        return false;
    
    return true;
}

Regs SliceNextValue::precompile(Regs preferred) {
    clob = left->precompile(preferred);

    clob.reserve_gpr(4);

    return clob;
}

Storage SliceNextValue::postprocess(Register r, Register i, Cx *cx) {
    throw INTERNAL_ERROR;
}

Storage SliceNextValue::compile(Cx *cx) {
    elem_size = ContainerType::get_elem_size(elem_ts);
    ls = left->compile(cx);  // iterator
    Register r = (clob & ~ls.regs()).get_gpr();
    Register i = (clob & ~ls.regs() & ~Regs(r)).get_gpr();
    Label ok;

    int LENGTH_OFFSET = REFERENCE_SIZE + INTEGER_SIZE;
    int VALUE_OFFSET = REFERENCE_SIZE + 2 * INTEGER_SIZE;
    
    switch (ls.where) {
    case MEMORY:
        cx->op(MOVQ, i, ls.address + VALUE_OFFSET);
        cx->op(MOVQ, r, ls.address); // array ptr
        cx->op(CMPQ, i, ls.address + LENGTH_OFFSET);
        cx->op(JNE, ok);
        
        raise("ITERATOR_DONE", cx);
        
        cx->code_label(ok);
        cx->op(is_down ? DECQ : INCQ, ls.address + VALUE_OFFSET);
        
        return postprocess(r, i, cx);
    default:
        throw INTERNAL_ERROR;
    }
}



SliceNextElemValue::SliceNextElemValue(Value *l, TypeMatch &match)
    :SliceNextValue(typesubst(SAME_LVALUE_TUPLE1_TS, match), match[1], l, false) {
}

Regs SliceNextElemValue::precompile(Regs preferred) {
    return SliceNextValue::precompile(preferred) | precompile_contained_lvalue();
}

Storage SliceNextElemValue::postprocess(Register r, Register i, Cx *cx) {
    int FRONT_OFFSET = REFERENCE_SIZE;
    
    cx->op(ADDQ, i, ls.address + FRONT_OFFSET);
    
    Address addr = cx->runtime->make_address(r, i, elem_size, LINEARRAY_ELEMS_OFFSET);
    
    return compile_contained_lvalue(addr, NOREG, ts, cx);
}



SliceNextIndexValue::SliceNextIndexValue(Value *l, TypeMatch &match)
    :SliceNextValue(INTEGER_TUPLE1_TS, match[1], l, false) {
}

Storage SliceNextIndexValue::postprocess(Register r, Register i, Cx *cx) {
    return Storage(REGISTER, i);
}



SliceNextItemValue::SliceNextItemValue(Value *l, TypeMatch &match)
    :SliceNextValue(typesubst(INTEGER_SAME_LVALUE_TUPLE2_TS, match), match[1], l, false) {
}

Storage SliceNextItemValue::postprocess(Register r, Register i, Cx *cx) {
    int FRONT_OFFSET = REFERENCE_SIZE;
    //int item_stack_size = ts.measure_stack();

    cx->op(PUSHQ, i);

    cx->op(ADDQ, i, ls.address + FRONT_OFFSET);
    Address addr = cx->runtime->make_address(r, i, elem_size, LINEARRAY_ELEMS_OFFSET);

    cx->op(PUSHQ, 0);
    cx->op(LEA, R10, addr);
    cx->op(PUSHQ, R10);
    
    return Storage(STACK);
}
