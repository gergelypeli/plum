#include "../plum.h"

#include "rbtree_registers.h"


// NOTE: node indexes stored in root, vacant, *.{left,right,prev,next} are tree-relative
// offsets, so if RSI points to the tree, then RSI + RAX points to the node. The NIL node
// value may be 0, which is an invalid offset, since the tree itself has a nonempty header.

void compile_rbtree_alloc(Label label, TypeSpec elem_ts, X64 *x64) {
    // R10 - reservation
    int rbnode_size = RbtreeType::get_rbnode_size(elem_ts);
    Label finalizer_label = elem_ts.prefix(rbtree_type).get_finalizer_label(x64);
    
    x64->code_label_local(label, elem_ts.prefix(rbtree_type).symbolize("alloc"));
    
    container_alloc(RBTREE_HEADER_SIZE, rbnode_size, RBTREE_RESERVATION_OFFSET, finalizer_label, x64);

    x64->op(MOVQ, Address(RAX, RBTREE_LENGTH_OFFSET), 0);
    x64->op(MOVQ, Address(RAX, RBTREE_ROOT_OFFSET), RBNODE_NIL);
    x64->op(MOVQ, Address(RAX, RBTREE_VACANT_OFFSET), RBNODE_NIL);
    x64->op(MOVQ, Address(RAX, RBTREE_FIRST_OFFSET), RBNODE_NIL);
    x64->op(MOVQ, Address(RAX, RBTREE_LAST_OFFSET), RBNODE_NIL);
    
    x64->op(RET);
}


void compile_rbtree_realloc(Label label, TypeSpec elem_ts, X64 *x64) {
    // RAX - array, R10 - new reservation
    int rbnode_size = RbtreeType::get_rbnode_size(elem_ts);

    x64->code_label_local(label, elem_ts.prefix(rbtree_type).symbolize("realloc"));

    container_realloc(RBTREE_HEADER_SIZE, rbnode_size, RBTREE_RESERVATION_OFFSET, x64);

    x64->op(RET);
}


void compile_rbtree_grow(Label label, TypeSpec elem_ts, X64 *x64) {
    // RAX - array, R10 - new reservation
    // Double the reservation until it's enough
    Label realloc_label = x64->once->compile(compile_rbtree_realloc, elem_ts);

    x64->code_label_local(label, elem_ts.prefix(rbtree_type).symbolize("grow"));

    container_grow(RBTREE_RESERVATION_OFFSET, RBTREE_MINIMUM_RESERVATION, realloc_label, x64);
    
    x64->op(RET);
}


void rbtree_preappend2(TypeSpec elem_ts, Storage ref_storage, X64 *x64) {
    // R10 - new addition. Returns the Ref in RAX.
    Label ok;

    x64->runtime->load_lvalue(RAX, R11, ref_storage);
    
    x64->op(ADDQ, R10, Address(RAX, RBTREE_LENGTH_OFFSET));
    x64->op(CMPQ, R10, Address(RAX, RBTREE_RESERVATION_OFFSET));
    x64->op(JBE, ok);

    Label grow_label = x64->once->compile(compile_rbtree_grow, elem_ts);
    x64->op(CALL, grow_label);  // clobbers all
    
    x64->runtime->store_lvalue(RAX, R11, ref_storage);
    
    x64->code_label(ok);
}


void compile_rbtree_clone(Label label, TypeSpec elem_ts, X64 *x64) {
    // RAX - Rbtree Ref
    // Return a cloned Ref
    Label end, vacancy_check, vacancy_loop, elem_check, elem_loop;
    Label alloc_label = x64->once->compile(compile_rbtree_alloc, elem_ts);
    TypeSpec heap_ts = elem_ts.prefix(rbtree_type);
    
    x64->code_label_local(label, elem_ts.prefix(rbtree_type).symbolize("clone"));
    x64->runtime->log("XXX rbtree clone");
    
    x64->op(PUSHQ, RAX);
    x64->op(PUSHQ, RDX);
    x64->op(MOVQ, R10, Address(RAX, RBTREE_RESERVATION_OFFSET));
    x64->op(CALL, alloc_label);  // clobbers all
    x64->op(POPQ, RDX);
    x64->op(POPQ, RBX);  // orig
    
    x64->op(MOVQ, RCX, Address(RBX, RBTREE_LENGTH_OFFSET));
    x64->op(MOVQ, Address(RAX, RBTREE_LENGTH_OFFSET), RCX);

    x64->op(CMPQ, RCX, 0);
    x64->op(JE, end);

    x64->op(MOVQ, RCX, Address(RBX, RBTREE_ROOT_OFFSET));
    x64->op(MOVQ, Address(RAX, RBTREE_ROOT_OFFSET), RCX);
    x64->op(MOVQ, RCX, Address(RBX, RBTREE_FIRST_OFFSET));
    x64->op(MOVQ, Address(RAX, RBTREE_FIRST_OFFSET), RCX);
    x64->op(MOVQ, RCX, Address(RBX, RBTREE_LAST_OFFSET));
    x64->op(MOVQ, Address(RAX, RBTREE_LAST_OFFSET), RCX);
    x64->op(MOVQ, RCX, Address(RBX, RBTREE_VACANT_OFFSET));
    x64->op(MOVQ, Address(RAX, RBTREE_VACANT_OFFSET), RCX);
    
    // Clone vacancies
    x64->op(JMP, vacancy_check);

    x64->code_label(vacancy_loop);
    x64->op(MOVQ, R10, Address(RBX, RCX, RBNODE_NEXT_OFFSET));
    x64->op(MOVQ, Address(RAX, RCX, RBNODE_NEXT_OFFSET), R10);
    x64->op(MOVQ, RCX, R10);
    
    x64->code_label(vacancy_check);
    x64->op(CMPQ, RCX, RBNODE_NIL);
    x64->op(JNE, vacancy_loop);
    
    // Clone elems
    x64->op(MOVQ, RCX, Address(RBX, RBTREE_FIRST_OFFSET));
    x64->op(JMP, elem_check);
    
    x64->code_label(elem_loop);
    elem_ts.create(Storage(MEMORY, Address(RBX, RCX, RBNODE_VALUE_OFFSET)), Storage(MEMORY, Address(RAX, RCX, RBNODE_VALUE_OFFSET)), x64);

    x64->op(MOVQ, R10, Address(RBX, RCX, RBNODE_LEFT_OFFSET));
    x64->op(MOVQ, Address(RAX, RCX, RBNODE_LEFT_OFFSET), R10);
    x64->op(MOVQ, R10, Address(RBX, RCX, RBNODE_RIGHT_OFFSET));
    x64->op(MOVQ, Address(RAX, RCX, RBNODE_RIGHT_OFFSET), R10);
    x64->op(MOVQ, R10, Address(RBX, RCX, RBNODE_PRED_OFFSET));
    x64->op(MOVQ, Address(RAX, RCX, RBNODE_PRED_OFFSET), R10);
    x64->op(MOVQ, R10, Address(RBX, RCX, RBNODE_NEXT_OFFSET));
    x64->op(MOVQ, Address(RAX, RCX, RBNODE_NEXT_OFFSET), R10);
    x64->op(MOVQ, RCX, R10);
    
    x64->code_label(elem_check);
    x64->op(CMPQ, RCX, RBNODE_NIL);
    x64->op(JNE, elem_loop);

    heap_ts.decref(RBX, x64);
    
    x64->code_label(end);
    x64->op(RET);
}




// Initializers

RbtreeEmptyValue::RbtreeEmptyValue(TypeSpec ets, TypeSpec rts)
    :GenericValue(NO_TS, rts, NULL) {
    elem_ts = ets;
}

Regs RbtreeEmptyValue::precompile(Regs preferred) {
    return Regs::all();
}

Storage RbtreeEmptyValue::compile(X64 *x64) {
    Label alloc_label = x64->once->compile(compile_rbtree_alloc, elem_ts);
    
    x64->op(MOVQ, R10, 0);
    x64->op(CALL, alloc_label);  // clobbers all
    
    return Storage(REGISTER, RAX);
}




RbtreeReservedValue::RbtreeReservedValue(TypeSpec ets, TypeSpec rts)
    :GenericValue(INTEGER_TS, rts, NULL) {
    elem_ts = ets;
}

Regs RbtreeReservedValue::precompile(Regs preferred) {
    right->precompile(preferred);
    return Regs::all();
}

Storage RbtreeReservedValue::compile(X64 *x64) {
    Label alloc_label = x64->once->compile(compile_rbtree_alloc, elem_ts);

    right->compile_and_store(x64, Storage(REGISTER, R10));  // FIXME: may be illegal

    x64->op(CALL, alloc_label);  // clobbers all
    
    return Storage(REGISTER, RAX);
}




RbtreeInitializerValue::RbtreeInitializerValue(TypeSpec ets, TypeSpec rts)
    :ContainerInitializerValue(ets, rts, 0, 0, NULL) {
}

Regs RbtreeInitializerValue::precompile(Regs preferred) {
    ContainerInitializerValue::precompile(preferred);
    return Regs::all();
}

Storage RbtreeInitializerValue::compile(X64 *x64) {
    // This won't use the base class subcompile method, because that's inappropriate here.
    Label alloc_label = x64->once->compile(compile_rbtree_alloc, elem_ts);
    Label add_label = x64->once->compile(compile_rbtree_add, elem_ts);
    int stack_size = elem_ts.measure_stack();

    x64->op(MOVQ, R10, elems.size());
    x64->op(CALL, alloc_label);  // clobbers all
    x64->op(PUSHQ, RAX);
    
    for (auto &elem : elems) {
        elem->compile_and_store(x64, Storage(STACK));

        x64->op(MOVQ, KEYX, RSP);  // save key address for stack usage
        x64->op(MOVQ, SELFX, Address(RSP, stack_size));  // Rbtree without incref
        x64->op(MOVQ, ROOTX, Address(SELFX, RBTREE_ROOT_OFFSET));

        x64->op(CALL, add_label);

        x64->op(MOVQ, Address(SELFX, RBTREE_ROOT_OFFSET), R10);
        x64->op(ANDQ, Address(SELFX, R10, RBNODE_PRED_OFFSET), ~RBNODE_RED_BIT);  // blacken root
    
        elem_ts.create(Storage(STACK), Storage(MEMORY, Address(SELFX, KEYX, RBNODE_VALUE_OFFSET)), x64);
    }
    
    return Storage(STACK);
}


// Methods

RbtreeLengthValue::RbtreeLengthValue(Value *l, TypeSpec ets)
    :GenericValue(NO_TS, INTEGER_TS, l) {
    reg = NOREG;
    heap_ts = ets.prefix(rbtree_type);
}

Regs RbtreeLengthValue::precompile(Regs preferred) {
    Regs clob = left->precompile(preferred);
    
    if (!clob.has_gpr())
        clob = clob | RAX;
    
    reg = clob.get_gpr();
        
    return clob;
}

Storage RbtreeLengthValue::compile(X64 *x64) {
    ls = left->compile(x64);

    switch (ls.where) {
    case REGISTER:
        heap_ts.decref(ls.reg, x64);  // FIXME: use after decref
        x64->op(MOVQ, ls.reg, Address(ls.reg, RBTREE_LENGTH_OFFSET));
        return Storage(REGISTER, ls.reg);
    case MEMORY:
        x64->op(MOVQ, reg, ls.address);
        x64->op(MOVQ, reg, Address(reg, RBTREE_LENGTH_OFFSET));
        return Storage(REGISTER, reg);
    default:
        throw INTERNAL_ERROR;
    }
}




RbtreeAddValue::RbtreeAddValue(Value *l, TypeSpec ets, TypeSpec eats)
    :Value(l->ts) {
    pivot.reset(l);
    
    elem_ts = ets;
    elem_arg_ts = eats;
}

bool RbtreeAddValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    //check_alias(scope);
    
    return check_arguments(args, kwargs, {
        { "elem", &elem_arg_ts, scope, &elem }
    });
}

Regs RbtreeAddValue::precompile(Regs preferred) {
    Regs clob = elem->precompile_tail();
    clob = clob | pivot->precompile(preferred & ~clob);
    
    return clob | RBTREE_CLOB | COMPARE_CLOB;
}

Storage RbtreeAddValue::postprocess(Storage s, X64 *x64) {
    return s;
}

Storage RbtreeAddValue::compile(X64 *x64) {
    int elem_arg_size = elem_arg_ts.measure_stack();
    Label clone_label = x64->once->compile(compile_rbtree_clone, elem_ts);
    Label add_label = x64->once->compile(compile_rbtree_add, elem_ts);

    Storage ps = pivot->compile_lvalue(x64);
    elem->compile_and_store(x64, Storage(STACK));
    Storage aps = ps.access(elem_arg_size);

    container_cow(clone_label, aps, x64);  // leaves borrowed Ref in RAX

    x64->op(MOVQ, R10, 1);  // Growth
    rbtree_preappend2(elem_ts, aps, x64);
    x64->op(MOVQ, SELFX, RAX);  // TODO: not nice, maybe SELFX should be RAX?
    
    x64->op(MOVQ, ROOTX, Address(SELFX, RBTREE_ROOT_OFFSET));
    x64->op(LEA, KEYX, Address(RSP, 0));
    x64->op(CALL, add_label);
    
    x64->op(MOVQ, Address(SELFX, RBTREE_ROOT_OFFSET), R10);
    x64->op(ANDQ, Address(SELFX, R10, RBNODE_PRED_OFFSET), ~RBNODE_RED_BIT);  // blacken root

    Address alias_addr(RSP, elem_arg_size);
    Address elem_addr(SELFX, KEYX, RBNODE_VALUE_OFFSET);
    elem_ts.create(Storage(STACK), Storage(MEMORY, elem_addr), x64);

    // Leaves ps/SELFX/KEYX point to the new elem, for subclasses
    return postprocess(ps, x64);
}




RbtreeAddItemValue::RbtreeAddItemValue(Value *l, TypeSpec kts, TypeSpec vts, TypeSpec kats, TypeSpec vats)
    :Value(l->ts) {
    pivot.reset(l);
        
    key_ts = kts;
    value_ts = vts;
    key_arg_ts = kats;
    value_arg_ts = vats;
    
    elem_ts = TypeSpec(item_type, key_ts, value_ts);
}

bool RbtreeAddItemValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    //check_alias(scope);
    
    return check_arguments(args, kwargs, {
        { "key", &key_arg_ts, scope, &key },
        { "value", &value_arg_ts, scope, &value }
    });
}

Regs RbtreeAddItemValue::precompile(Regs preferred) {
    Regs clob = value->precompile_tail();
    clob = clob | key->precompile(~clob);
    clob = clob | pivot->precompile(preferred & ~clob);

    return clob | RBTREE_CLOB | COMPARE_CLOB;
}

Storage RbtreeAddItemValue::postprocess(Storage s, X64 *x64) {
    return s;
}

Storage RbtreeAddItemValue::compile(X64 *x64) {
    int key_size = key_ts.measure_stack();  // NOTE: as it's in an Item, it is rounded up
    int key_arg_size = key_arg_ts.measure_stack();
    int value_arg_size = value_arg_ts.measure_stack();
    Label clone_label = x64->once->compile(compile_rbtree_clone, elem_ts);
    Label add_label = x64->once->compile(compile_rbtree_add, elem_ts);

    Storage ps = pivot->compile_lvalue(x64);
    key->compile_and_store(x64, Storage(STACK));
    value->compile_and_store(x64, Storage(STACK));
    Storage aps = ps.access(key_arg_size + value_arg_size);

    container_cow(clone_label, aps, x64);  // leaves borrowed Ref in RAX

    x64->op(MOVQ, R10, 1);  // Growth
    rbtree_preappend2(elem_ts, aps, x64);
    x64->op(MOVQ, SELFX, RAX);  // TODO: not nice, maybe SELFX should be RAX?

    x64->op(MOVQ, ROOTX, Address(SELFX, RBTREE_ROOT_OFFSET));
    x64->op(LEA, KEYX, Address(RSP, value_arg_size));
    x64->op(CALL, add_label);
    
    x64->op(MOVQ, Address(SELFX, RBTREE_ROOT_OFFSET), R10);
    x64->op(ANDQ, Address(SELFX, R10, RBNODE_PRED_OFFSET), ~RBNODE_RED_BIT);  // blacken root

    // NOTE: we use the fact that Item contains index first, and value second,
    // and since they're parametric types, their sizes will be rounded up.
    Address alias_addr1(RSP, key_arg_size + value_arg_size);
    Address value_addr(SELFX, KEYX, RBNODE_VALUE_OFFSET + key_size);
    value_ts.create(Storage(STACK), Storage(MEMORY, value_addr), x64);
    
    Address alias_addr2(RSP, key_arg_size);
    Address key_addr(SELFX, KEYX, RBNODE_VALUE_OFFSET);
    key_ts.create(Storage(STACK), Storage(MEMORY, key_addr), x64);

    // Leaves ps/SELFX/KEYX point to the new elem, for subclasses
    return postprocess(ps, x64);
}




RbtreeRemoveValue::RbtreeRemoveValue(Value *l, TypeSpec ets, TypeSpec kats)
    :Value(l->ts) {
    pivot.reset(l);
    
    elem_ts = ets;
    key_arg_ts = kats;
}

bool RbtreeRemoveValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    //check_alias(scope);
    
    return check_arguments(args, kwargs, {
        { "key", &key_arg_ts, scope, &key }
    });
}

Regs RbtreeRemoveValue::precompile(Regs preferred) {
    Regs clob = key->precompile_tail();
    clob = clob | pivot->precompile(preferred & ~clob);
    
    return clob | RBTREE_CLOB | COMPARE_CLOB;
}

Storage RbtreeRemoveValue::postprocess(Storage s, X64 *x64) {
    return s;
}

Storage RbtreeRemoveValue::compile(X64 *x64) {
    int key_arg_size = key_arg_ts.measure_stack();
    Label clone_label = x64->once->compile(compile_rbtree_clone, elem_ts);
    Label remove_label = x64->once->compile(compile_rbtree_remove, elem_ts);

    Storage ps = pivot->compile_lvalue(x64);
    key->compile_and_store(x64, Storage(STACK));
    Storage aps = ps.access(key_arg_size);

    container_cow(clone_label, aps, x64);  // leaves borrowed Ref in RAX

    x64->op(MOVQ, SELFX, RAX);
    x64->op(MOVQ, ROOTX, Address(SELFX, RBTREE_ROOT_OFFSET));
    x64->op(LEA, KEYX, Address(RSP, 0));  // NOTE: only the index part is present of the Item
    x64->op(CALL, remove_label);
    
    x64->op(MOVQ, Address(SELFX, RBTREE_ROOT_OFFSET), R10);

    key_arg_ts.store(Storage(STACK), Storage(), x64);

    // Leaves ps/SELFX/KEYX point to the new elem, for subclasses
    return postprocess(ps, x64);
}




RbtreeHasValue::RbtreeHasValue(Value *l, TypeSpec ets, TypeSpec kats)
    :Value(BOOLEAN_TS) {
    pivot.reset(l);

    elem_ts = ets;
    key_arg_ts = kats;
}

bool RbtreeHasValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    return check_arguments(args, kwargs, {
        { "key", &key_arg_ts, scope, &key }
    });
}

Regs RbtreeHasValue::precompile(Regs preferred) {
    Regs clob = key->precompile(preferred);
    clob = clob | pivot->precompile(preferred);
    
    return clob | RBTREE_CLOB | COMPARE_CLOB;
}

Storage RbtreeHasValue::compile(X64 *x64) {
    pivot->compile_and_store(x64, Storage(STACK));
    key->compile_and_store(x64, Storage(STACK));
    
    int key_arg_size = key_arg_ts.measure_stack();
    Label has_label = x64->once->compile(compile_rbtree_has, elem_ts);

    x64->op(MOVQ, SELFX, Address(RSP, key_arg_size));
    x64->op(MOVQ, ROOTX, Address(SELFX, RBTREE_ROOT_OFFSET));
    x64->op(LEA, KEYX, Address(RSP, 0));
    x64->op(CALL, has_label);  // KEYX is the index of the found item, or NIL
    
    key_arg_ts.store(Storage(STACK), Storage(), x64);
    pivot->ts.store(Storage(STACK), Storage(), x64);
    
    x64->op(CMPQ, KEYX, RBNODE_NIL);

    return Storage(FLAGS, CC_NOT_EQUAL);
}




RbtreeIndexValue::RbtreeIndexValue(Value *l, TypeSpec kts, TypeSpec ets, TypeSpec kats, TypeSpec vrts)
    :Value(vrts.lvalue()) {
    pivot.reset(l);

    key_ts = kts;
    elem_ts = ets;
    key_arg_ts = kats;
    
    heap_ts = elem_ts.prefix(rbtree_type);
    may_borrow_heap = false;
}

bool RbtreeIndexValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    if (!check_arguments(args, kwargs, {
        { "key", &key_arg_ts, scope, &key }
    }))
        return false;
    
    if (!check_raise(lookup_exception_type, scope))
        return false;

    //if (!check_reference(scope))
    //    return false;
        
    return true;
}

Regs RbtreeIndexValue::precompile(Regs preferred) {
    may_borrow_heap = (bool)(preferred & Regs::heapvars());
    
    Regs clob = key->precompile_tail();
    clob = clob | pivot->precompile(preferred & ~clob);

    clob = clob | precompile_contained_lvalue();
        
    return clob | RBTREE_CLOB | COMPARE_CLOB;
}

Storage RbtreeIndexValue::compile(X64 *x64) {
    // Try borrowing the container
    Storage ps = pivot->compile(x64);
    bool container_borrowed = false;
    
    if (may_borrow_heap && ps.where == MEMORY) {
        x64->op(PUSHQ, ps.address);
        container_borrowed = true;
    }
    else
        pivot->ts.store(ps, Storage(STACK), x64);
    
    key->compile_and_store(x64, Storage(STACK));

    int key_size = key_ts.measure_stack();  // in an Item it's rounded up
    int key_arg_size = key_arg_ts.measure_stack();
    Label has_label = x64->once->compile(compile_rbtree_has, elem_ts);

    x64->op(MOVQ, SELFX, Address(RSP, key_arg_size));
    x64->op(MOVQ, ROOTX, Address(SELFX, RBTREE_ROOT_OFFSET));
    x64->op(LEA, KEYX, Address(RSP, 0));
    x64->op(CALL, has_label);  // KEYX is the index of the found item, or NIL
    
    key_arg_ts.store(Storage(STACK), Storage(), x64);
    
    if (container_borrowed)
        x64->op(POPQ, SELFX);
    else
        pivot->ts.store(Storage(STACK), Storage(REGISTER, SELFX), x64);

    Label ok;
    //defer_decref(SELFX, x64);
    
    x64->op(CMPQ, KEYX, RBNODE_NIL);
    x64->op(JNE, ok);

    if (!container_borrowed)
        x64->runtime->decref(SELFX);
    raise("NOT_FOUND", x64);
    
    x64->code_label(ok);
    Address addr(SELFX, KEYX, RBNODE_VALUE_OFFSET + key_size);

    return compile_contained_lvalue(addr, container_borrowed ? NOREG : SELFX, ts, x64);
}


// Iteration

RbtreeElemByAgeIterValue::RbtreeElemByAgeIterValue(Value *l, TypeSpec iter_ts)
    :SimpleRecordValue(iter_ts, l) {
}

Storage RbtreeElemByAgeIterValue::compile(X64 *x64) {
    left->compile_and_store(x64, Storage(STACK));
    
    x64->op(POPQ, R10);
    x64->op(PUSHQ, Address(R10, RBTREE_FIRST_OFFSET));
    x64->op(PUSHQ, R10);
    
    return Storage(STACK);
}




RbtreeNextElemByAgeValue::RbtreeNextElemByAgeValue(Value *l, TypeSpec ts)
    :GenericValue(NO_TS, ts, l) {
    is_down = false;  // TODO: get as argument for backward iteration!
}

bool RbtreeNextElemByAgeValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    if (!check_arguments(args, kwargs, {}))
        return false;

    if (!check_raise(iterator_done_exception_type, scope))
        return false;
    
    return true;
}

Regs RbtreeNextElemByAgeValue::precompile(Regs preferred) {
    clob = left->precompile(preferred);
    
    clob.reserve_gpr(4);
    
    return clob;
}

Storage RbtreeNextElemByAgeValue::postprocess(Register r, Register i, X64 *x64) {
    throw INTERNAL_ERROR;
}

Storage RbtreeNextElemByAgeValue::compile(X64 *x64) {
    ls = left->compile_lvalue(x64);  // iterator
    Storage als = ls.access(0);
    
    Register r = (clob & ~ls.regs()).get_gpr();
    Register i = (clob & ~ls.regs() & ~Regs(r)).get_gpr();
    Label ok;

    // Load the ref to r, and the index to i
    x64->runtime->load_lvalue(r, R11, als);
    x64->runtime->load_lvalue(i, R11, als, REFERENCE_SIZE);
    
    x64->op(CMPQ, i, RBNODE_NIL);
    x64->op(JNE, ok);
    
    drop_and_raise(left->ts, ls, "ITERATOR_DONE", x64);
    
    x64->code_label(ok);
    
    if (is_down) {
        x64->op(MOVQ, R10, Address(r, i, RBNODE_PRED_OFFSET));
        x64->op(ANDQ, R10, ~RBNODE_RED_BIT);  // remove color bit
    }
    else {
        x64->op(MOVQ, R10, Address(r, i, RBNODE_NEXT_OFFSET));
    }

    // Save new iterator position
    x64->runtime->store_lvalue(R10, R11, als, REFERENCE_SIZE);

    left->ts.store(ls, Storage(), x64);

    return postprocess(r, i, x64);
}




RbtreeElemByOrderIterValue::RbtreeElemByOrderIterValue(Value *l, TypeSpec iter_ts)
    :ContainerIterValue(iter_ts, l) {
}




RbtreeNextElemByOrderValue::RbtreeNextElemByOrderValue(Value *l, TypeSpec ts)
    :GenericValue(NO_TS, ts, l) {
}

bool RbtreeNextElemByOrderValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    //check_alias(scope);
    
    if (!check_arguments(args, kwargs, {}))
        return false;

    if (!check_raise(iterator_done_exception_type, scope))
        return false;
    
    return true;
}

Regs RbtreeNextElemByOrderValue::precompile(Regs preferred) {
    clob = left->precompile(preferred);
    
    return clob | RAX | RCX | RDX | SELFX;
}

Storage RbtreeNextElemByOrderValue::postprocess(Register r, Register i, X64 *x64) {
    throw INTERNAL_ERROR;
}

Storage RbtreeNextElemByOrderValue::compile(X64 *x64) {
    Label next_label = x64->once->compile(compile_rbtree_next);
    Label ok;

    ls = left->compile_lvalue(x64);
    Storage als = ls.access(0);

    // Load the ref to SELFX, and the index to RAX
    x64->runtime->load_lvalue(SELFX, R11, als);
    x64->runtime->load_lvalue(RAX, R11, als, REFERENCE_SIZE);
        
    x64->op(CALL, next_label);  // RAX - new it, R10 - index
    
    x64->op(CMPQ, RAX, 0);
    x64->op(JNE, ok);

    drop_and_raise(left->ts, ls, "ITERATOR_DONE", x64);
    
    x64->code_label(ok);

    // Save new iterator position
    x64->runtime->store_lvalue(RAX, R11, als, REFERENCE_SIZE);

    x64->op(MOVQ, RAX, R10);
    left->ts.store(ls, Storage(), x64);
    
    return postprocess(SELFX, RAX, x64);
}

