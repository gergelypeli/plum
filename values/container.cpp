#include "../plum.h"


TypeSpec container_elem_ts(TypeSpec ts, Type *container_type) {
    // FIXME: this won't work for ptr
    TypeSpec x = ts.rvalue();
    
    if (x[0] == ref_type)
        x = x.unprefix(ref_type);  // FIXME: until Rbtree exists
        
    x = x.unprefix(container_type);
    
    return x;
}


void container_alloc(int header_size, int elem_size, int reservation_offset, Label finalizer_label, Cx *cx) {
    // R10 - reservation size
    cx->op(PUSHQ, R10);
    
    cx->op(IMUL3Q, R10, R10, elem_size);
    cx->op(ADDQ, R10, header_size);
    cx->op(LEA, R11, Address(finalizer_label, 0));
    
    cx->op(PUSHQ, R10);
    cx->op(PUSHQ, R11);
    cx->runtime->heap_alloc();  // clobbers all
    cx->op(ADDQ, RSP, 2 * ADDRESS_SIZE);
    
    cx->op(POPQ, Address(RAX, reservation_offset));
}


void container_realloc(int header_size, int elem_size, int reservation_offset, Cx *cx) {
    // RAX - array, R10 - new reservation
    cx->op(MOVQ, Address(RAX, reservation_offset), R10);
    cx->op(IMUL3Q, R10, R10, elem_size);
    cx->op(ADDQ, R10, header_size);
    
    cx->op(PUSHQ, RAX);
    cx->op(PUSHQ, R10);
    cx->runtime->heap_realloc();  // clobbers all
    cx->op(ADDQ, RSP, 2 * ADDRESS_SIZE);
}


void container_grow(int reservation_offset, int min_reservation, Label realloc_label, Cx *cx) {
    // RAX - array, R10 - new reservation
    // Double the reservation until it's enough (can be relaxed to 1.5 times, but not less)

    Label check, ok;
    cx->op(XCHGQ, R10, Address(RAX, reservation_offset));  // save desired value
    cx->op(CMPQ, R10, min_reservation);
    cx->op(JAE, check);
    cx->op(MOVQ, R10, min_reservation);
    
    cx->code_label(check);
    cx->op(CMPQ, R10, Address(RAX, reservation_offset));
    cx->op(JAE, ok);
    cx->op(SHLQ, R10, 1);
    cx->op(JMP, check);
    
    cx->code_label(ok);
    cx->op(CALL, realloc_label);  // clobbers all
}


void container_preappend2(int reservation_offset, int length_offset, Label grow_label, Storage ref_storage, Cx *cx) {
    // R10 - new addition. Returns the Ref in RAX.
    Label ok;

    cx->runtime->load_lvalue(RAX, R11, ref_storage);

    cx->op(ADDQ, R10, Address(RAX, length_offset));
    cx->op(CMPQ, R10, Address(RAX, reservation_offset));
    cx->op(JBE, ok);

    cx->op(CALL, grow_label);  // clobbers all

    cx->runtime->store_lvalue(RAX, R11, ref_storage);
    
    cx->code_label(ok);
}


void container_cow(Label clone_label, Storage ref_storage, Cx *cx) {
    // No runtime arguments. Returns the borrowed Ref in RAX
    Label end;
    
    cx->runtime->load_lvalue(RAX, R11, ref_storage);

    cx->runtime->oneref(RAX);
    cx->op(JE, end);
    
    // Cloning decreases the refcount of the passed RAX. Since we just loaded it
    // from memory, it effectively decreases the original's refcount. After the clone we get
    // a refcounted copy, which we immediately store back to the same location.
    // So the original lost one reference, and the copy has exactly one.
    // The RAX returned is a borrowed reference, and needs no decref later.
    cx->op(CALL, clone_label);  // clobbers all

    cx->runtime->store_lvalue(RAX, R11, ref_storage);
    
    cx->code_label(end);
}




ContainerLengthValue::ContainerLengthValue(Value *l, TypeMatch &match, TypeSpec hts, int lo)
    :GenericValue(NO_TS, INTEGER_TS, l) {
    reg = NOREG;
    heap_ts = hts;
    length_offset = lo;
}

Regs ContainerLengthValue::precompile(Regs preferred) {
    Regs clob = left->precompile(preferred);
    
    if (!clob.has_gpr())
        clob = clob | RAX;
    
    reg = clob.get_gpr();
        
    return clob;
}

Storage ContainerLengthValue::compile(Cx *cx) {
    ls = left->compile(cx);

    switch (ls.where) {
    case REGISTER:
        heap_ts.decref(ls.reg, cx);  // FIXME: use after decref
        cx->op(MOVQ, ls.reg, Address(ls.reg, length_offset));
        return Storage(REGISTER, ls.reg);
    case STACK:
        cx->op(POPQ, RBX);
        cx->op(MOVQ, reg, Address(RBX, length_offset));
        heap_ts.decref(RBX, cx);
        return Storage(REGISTER, reg);
    case MEMORY:
        cx->op(MOVQ, reg, ls.address);
        cx->op(MOVQ, reg, Address(reg, length_offset));
        return Storage(REGISTER, reg);
    default:
        throw INTERNAL_ERROR;
    }
}




ContainerIndexValue::ContainerIndexValue(Value *pivot, TypeMatch &match, TypeSpec hts, int lo, int eo)
    :GenericValue(INTEGER_TS, match[1].lvalue(), pivot) {
    heap_ts = hts;
    elem_ts = match[1];
    length_offset = lo;
    elems_offset = eo;
    may_borrow_heap = false;
}

bool ContainerIndexValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    if (!check_raise(lookup_exception_type, scope))
        return false;

    //if (!check_reference(scope))
    //    return false;

    return GenericValue::check(args, kwargs, scope);
}

void ContainerIndexValue::fix_index(Register r, Register i, Cx *cx) {
}

Regs ContainerIndexValue::precompile(Regs preferred) {
    may_borrow_heap = (bool)(preferred & Regs::heapvars());

    rclob = right ? right->precompile_tail() : Regs();
    
    Regs lclob = left->precompile(preferred & ~rclob);
    clob = lclob | rclob;
    
    //clob = clob | Regs(RAX) | Regs(RBX);
    clob = clob | precompile_contained_lvalue();
    clob.reserve_gpr(3);

    return clob;
}

Storage ContainerIndexValue::compile(Cx *cx) {
    int elem_size = ContainerType::get_elem_size(elem_ts);

    ls = left->compile(cx);
    
    if (ls.regs() & rclob) {
        ls = left->ts.store(ls, Storage(STACK), cx);
    }

    cx->unwind->push(this);
    rs = right->compile(cx);
    cx->unwind->pop(this);

    rs = right->compile(cx);

    //Register r = (ls.where == REGISTER ? ls.reg : ls.where == MEMORY ? auxls.reg : throw INTERNAL_ERROR);
    //Register i = (rs.where == REGISTER ? rs.reg : r == RAX ? RBX : RAX);
    Register r, i;

    switch (rs.where) {
    case CONSTANT:
        i = (clob & ~ls.regs()).get_gpr();
        cx->op(MOVQ, i, rs.value);
        break;
    case REGISTER:
        i = rs.reg;
        break;
    case MEMORY:
        i = (clob & ~ls.regs()).get_gpr();
        cx->op(MOVQ, i, rs.address);
        break;
    default:
        throw INTERNAL_ERROR;
    }

    bool container_borrowed = false;

    switch (ls.where) {
    case REGISTER:
        r = ls.reg;
        break;
    case STACK:
        r = (clob & ~Regs(i)).get_gpr();
        cx->op(POPQ, r);
        break;
    case MEMORY:
        r = (clob & ~Regs(i)).get_gpr();
        cx->op(MOVQ, r, ls.address);  // r may be the base of ls.address
        
        if (may_borrow_heap)
            container_borrowed = true;
        else
            heap_ts.incref(r, cx);
            
        break;
    default:
        throw INTERNAL_ERROR;
    }

    //defer_decref(r, cx);

    Label ok;
    cx->op(CMPQ, i, Address(r, length_offset));  // needs logical index
    cx->op(JB, ok);

    if (!container_borrowed)
        heap_ts.decref(r, cx);
    raise("NOT_FOUND", cx);

    cx->code_label(ok);
    fix_index(r, i, cx);  // turns logical index into physical
    Address addr = cx->runtime->make_address(r, i, elem_size, elems_offset);
    
    return compile_contained_lvalue(addr, container_borrowed ? NOREG : r, ts, cx);
}




ContainerEmptyValue::ContainerEmptyValue(TypeSpec ts, Once::TypedFunctionCompiler ca)
    :GenericValue(NO_TS, ts, NULL) {
    elem_ts = container_elem_ts(ts);
    compile_alloc = ca;
}

Regs ContainerEmptyValue::precompile(Regs preferred) {
    return Regs::all();
}

Storage ContainerEmptyValue::compile(Cx *cx) {
    Label alloc_label = cx->once->compile(compile_alloc, elem_ts);

    cx->op(MOVQ, R10, 0);
    cx->op(CALL, alloc_label);  // clobbers all
    
    return Storage(REGISTER, RAX);
}




ContainerReservedValue::ContainerReservedValue(TypeSpec ts, Once::TypedFunctionCompiler ca)
    :GenericValue(INTEGER_TS, ts, NULL) {
    elem_ts = container_elem_ts(ts);
    compile_alloc = ca;
}

Regs ContainerReservedValue::precompile(Regs preferred) {
    right->precompile(preferred);
    return Regs::all();
}

Storage ContainerReservedValue::compile(Cx *cx) {
    Label alloc_label = cx->once->compile(compile_alloc, elem_ts);

    right->compile_and_store(cx, Storage(REGISTER, R10));  // FIXME: this may be illegal

    cx->op(CALL, alloc_label);  // clobbers all
    
    return Storage(REGISTER, RAX);
}




ContainerAllValue::ContainerAllValue(TypeSpec ts, int lo, int eo, Once::TypedFunctionCompiler ca)
    :Value(ts) {
    elem_ts = container_elem_ts(ts);
    length_offset = lo;
    elems_offset = eo;
    compile_alloc = ca;
}

bool ContainerAllValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    ArgInfos infos = {
        { "fill", &elem_ts, scope, &fill_value },
        { "length", &INTEGER_TS, scope, &length_value }
    };
    
    if (!check_arguments(args, kwargs, infos))
        return false;

    return true;
}

Regs ContainerAllValue::precompile(Regs preferred) {
    Regs clob = length_value->precompile_tail();
    clob = clob | fill_value->precompile_tail();
    
    return Regs::all();
}

Storage ContainerAllValue::compile(Cx *cx) {
    Label alloc_label = cx->once->compile(compile_alloc, elem_ts);
    int elem_size = ContainerType::get_elem_size(elem_ts);

    fill_value->compile_and_store(cx, Storage(STACK));
    length_value->compile_and_store(cx, Storage(STACK));
    
    cx->op(MOVQ, R10, Address(RSP, 0));
    cx->op(CALL, alloc_label);  // clobbers all
    cx->op(POPQ, Address(RAX, length_offset));
    
    Label loop, check;
    cx->op(MOVQ, RCX, 0);
    cx->op(JMP, check);
    
    cx->code_label(loop);
    cx->op(IMUL3Q, RDX, RCX, elem_size);
    
    elem_ts.create(Storage(MEMORY, Address(RSP, 0)), Storage(MEMORY, Address(RAX, RDX, elems_offset)), cx);
    cx->op(INCQ, RCX);
    
    cx->code_label(check);
    cx->op(CMPQ, RCX, Address(RAX, length_offset));
    cx->op(JB, loop);
    
    elem_ts.store(Storage(STACK), Storage(), cx);
    
    return Storage(REGISTER, RAX);
}




ContainerInitializerValue::ContainerInitializerValue(TypeSpec ets, TypeSpec ts, int lo, int eo, Once::TypedFunctionCompiler ca)
    :Value(ts) {
    elem_ts = ets;
    length_offset = lo;
    elems_offset = eo;
    compile_alloc = ca;
}

bool ContainerInitializerValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    if (kwargs.size() != 0) {
        std::cerr << "Whacky container initializer keyword argument!\n";
        return false;
    }
    
    for (auto &arg : args) {
        Value *v = typize(arg.get(), scope, &elem_ts);
        if (!v)
            return false;
            
        TypeMatch match;
        
        if (!typematch(elem_ts, v, match)) {
            std::cerr << "Container element is not " << elem_ts << ", but " << v->ts << "!\n";
            return false;
        }
        
        elems.push_back(std::unique_ptr<Value>(v));
    }
    
    return true;
}

Regs ContainerInitializerValue::precompile(Regs preferred) {
    Regs clob;
    
    for (auto &elem : elems)
        clob = clob | elem->precompile_tail();
        
    return Regs::all();
}

Storage ContainerInitializerValue::compile(Cx *cx) {
    Label alloc_label = cx->once->compile(compile_alloc, elem_ts);
    int elem_size = ContainerType::get_elem_size(elem_ts);
    int stack_size = elem_ts.measure_stack();

    cx->op(MOVQ, R10, elems.size());
    cx->op(CALL, alloc_label);  // clobbers all
    cx->op(MOVQ, Address(RAX, length_offset), elems.size());
    cx->op(PUSHQ, RAX);
    
    unsigned offset = 0;
    
    for (auto &elem : elems) {
        elem->compile_and_store(cx, Storage(STACK));
        cx->op(MOVQ, RAX, Address(RSP, stack_size));
        Storage t(MEMORY, Address(RAX, elems_offset + offset));
        
        elem_ts.create(Storage(STACK), t, cx);
        offset += elem_size;
    }
    
    return Storage(STACK);
}



ContainerEmptiableValue::ContainerEmptiableValue(TypeSpec arg_ts, TypeSpec res_ts, Value *pivot)
    :GenericValue(arg_ts, res_ts, pivot) {
}

bool ContainerEmptiableValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    if (!check_raise(container_empty_exception_type, scope))
        return false;
    
    return GenericValue::check(args, kwargs, scope);
}




ContainerPushValue::ContainerPushValue(Value *l, TypeMatch &match, int ro, int lo, int eo, Once::TypedFunctionCompiler cc, Once::TypedFunctionCompiler cg)
    :GenericValue(match[1], match[0], l) {
    elem_ts = match[1];
    reservation_offset = ro;
    length_offset = lo;
    elems_offset = eo;
    compile_clone = cc;
    compile_grow = cg;
}

bool ContainerPushValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    //check_alias(scope);
    
    return GenericValue::check(args, kwargs, scope);
}

Regs ContainerPushValue::precompile(Regs preferred) {
    Regs clob = right->precompile_tail();
    clob = clob | left->precompile(preferred & ~clob);
    
    return Regs::all();
}

void ContainerPushValue::fix_index(Register r, Register i, Cx *cx) {
}

Storage ContainerPushValue::compile(Cx *cx) {
    int elem_size = ContainerType::get_elem_size(elem_ts);
    Label clone_label = cx->once->compile(compile_clone, elem_ts);
    Label grow_label = cx->once->compile(compile_grow, elem_ts);

    ls = left->compile_lvalue(cx);
    right->compile_and_store(cx, Storage(STACK));
    Storage als = ls.access(right->ts.measure_stack());

    container_cow(clone_label, als, cx);  // leaves borrowed Ref in RAX

    cx->op(MOVQ, R10, 1);
    container_preappend2(reservation_offset, length_offset, grow_label, als, cx);
    // RAX - Ref
    
    cx->op(MOVQ, R10, Address(RAX, length_offset));
    cx->op(INCQ, Address(RAX, length_offset));

    // R10 contains the index of the newly created element
    fix_index(RAX, R10, cx);
    
    cx->op(IMUL3Q, R10, R10, elem_size);
    cx->op(ADDQ, RAX, R10);
    
    elem_ts.create(Storage(STACK), Storage(MEMORY, Address(RAX, elems_offset)), cx);
    
    return ls;
}




ContainerPopValue::ContainerPopValue(Value *l, TypeMatch &match, TypeSpec hts, int lo, int eo, Once::TypedFunctionCompiler cc)
    :ContainerEmptiableValue(NO_TS, match[1], l) {
    elem_ts = match[1];
    heap_ts = hts;
    length_offset = lo;
    elems_offset = eo;
    compile_clone = cc;
}

bool ContainerPopValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    //check_alias(scope);
    
    return ContainerEmptiableValue::check(args, kwargs, scope);
}

Regs ContainerPopValue::precompile(Regs preferred) {
    Regs clob = left->precompile_tail();
    return clob | RAX | RCX;
}

void ContainerPopValue::fix_index(Register r, Register i, Cx *cx) {
}

Storage ContainerPopValue::compile(Cx *cx) {
    int elem_size = ContainerType::get_elem_size(elem_ts);
    Label clone_label = cx->once->compile(compile_clone, elem_ts);
    Label ok;
    
    ls = left->compile_lvalue(cx);
    Storage als = ls.access(0);

    container_cow(clone_label, als, cx);  // leaves borrowed Ref in RAX
    
    // Get rid of pivot
    left->ts.store(ls, Storage(), cx);
    
    cx->op(CMPQ, Address(RAX, length_offset), 0);
    cx->op(JNE, ok);

    // popped pivot
    raise("CONTAINER_EMPTY", cx);
    
    cx->code_label(ok);
    cx->op(DECQ, Address(RAX, length_offset));
    cx->op(MOVQ, R10, Address(RAX, length_offset));

    // R10 contains the index of the newly removed element
    fix_index(RAX, R10, cx);

    cx->op(IMUL3Q, RCX, R10, elem_size);
    
    // TODO: optimize this move!
    elem_ts.store(Storage(MEMORY, Address(RAX, RCX, elems_offset)), Storage(STACK), cx);
    elem_ts.destroy(Storage(MEMORY, Address(RAX, RCX, elems_offset)), cx);
    
    return Storage(STACK);
}


// Iteration

ContainerIterValue::ContainerIterValue(TypeSpec t, Value *l)
    :SimpleRecordValue(t, l) {
}

Storage ContainerIterValue::compile(Cx *cx) {
    cx->op(PUSHQ, 0);

    left->compile_and_store(cx, Storage(STACK));
    
    return Storage(STACK);
}


// Array iterator next methods

ContainerNextValue::ContainerNextValue(TypeSpec ts, TypeSpec ets, Value *l, int lo, bool d)
    :GenericValue(NO_TS, ts, l) {
    length_offset = lo;
    is_down = d;
    elem_ts = ets;
}

bool ContainerNextValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    if (!check_arguments(args, kwargs, {}))
        return false;

    if (!check_raise(iterator_done_exception_type, scope))
        return false;
    
    return true;
}

Regs ContainerNextValue::precompile(Regs preferred) {
    clob = left->precompile(preferred);

    clob.reserve_gpr(4);
    
    return clob;
}

Storage ContainerNextValue::postprocess(Register r, Register i, Cx *cx) {
    throw INTERNAL_ERROR;
}

Storage ContainerNextValue::compile(Cx *cx) {
    ls = left->compile(cx);  // iterator
    Register r = (clob & ~ls.regs()).get_gpr();
    Register i = (clob & ~ls.regs() & ~Regs(r)).get_gpr();
    Label ok;
    
    switch (ls.where) {
    case MEMORY:
        //std::cerr << "Compiling itemiter with reg=" << reg << " ls=" << ls << "\n";
        cx->op(MOVQ, i, ls.address + REFERENCE_SIZE);  // value
        cx->op(MOVQ, r, ls.address); // array reference without incref
        cx->op(CMPQ, i, Address(r, length_offset));
        cx->op(JNE, ok);

        // MEMORY arg
        raise("ITERATOR_DONE", cx);
        
        cx->code_label(ok);
        cx->op(is_down ? DECQ : INCQ, ls.address + REFERENCE_SIZE);

        return postprocess(r, i, cx);  // borrowed reference, with index
    default:
        throw INTERNAL_ERROR;
    }
}

