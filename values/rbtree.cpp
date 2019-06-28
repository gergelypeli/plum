#include "../plum.h"

#include "rbtree_registers.h"


// NOTE: node indexes stored in root, vacant, *.{left,right,prev,next} are tree-relative
// offsets, so if RSI points to the tree, then RSI + RAX points to the node. The NIL node
// value may be 0, which is an invalid offset, since the tree itself has a nonempty header.

void compile_rbtree_alloc(Label label, TypeSpec elem_ts, Cx *cx) {
    // R10 - reservation
    int rbnode_size = RbtreeType::get_rbnode_size(elem_ts);
    Label finalizer_label = elem_ts.prefix(rbtree_type).get_finalizer_label(cx);
    
    cx->code_label_local(label, elem_ts.prefix(rbtree_type).symbolize("alloc"));
    cx->prologue();
    
    container_alloc(RBTREE_HEADER_SIZE, rbnode_size, RBTREE_RESERVATION_OFFSET, finalizer_label, cx);

    cx->op(MOVQ, Address(RAX, RBTREE_LENGTH_OFFSET), 0);
    cx->op(MOVQ, Address(RAX, RBTREE_ROOT_OFFSET), RBNODE_NIL);
    cx->op(MOVQ, Address(RAX, RBTREE_VACANT_OFFSET), RBNODE_NIL);
    cx->op(MOVQ, Address(RAX, RBTREE_FIRST_OFFSET), RBNODE_NIL);
    cx->op(MOVQ, Address(RAX, RBTREE_LAST_OFFSET), RBNODE_NIL);
    
    cx->epilogue();
}


void compile_rbtree_realloc(Label label, TypeSpec elem_ts, Cx *cx) {
    // RAX - array, R10 - new reservation
    int rbnode_size = RbtreeType::get_rbnode_size(elem_ts);

    cx->code_label_local(label, elem_ts.prefix(rbtree_type).symbolize("realloc"));
    cx->prologue();

    container_realloc(RBTREE_HEADER_SIZE, rbnode_size, RBTREE_RESERVATION_OFFSET, cx);

    cx->epilogue();
}


void compile_rbtree_grow(Label label, TypeSpec elem_ts, Cx *cx) {
    // RAX - array, R10 - new reservation
    // Double the reservation until it's enough
    Label realloc_label = cx->once->compile(compile_rbtree_realloc, elem_ts);

    cx->code_label_local(label, elem_ts.prefix(rbtree_type).symbolize("grow"));
    cx->prologue();

    container_grow(RBTREE_RESERVATION_OFFSET, RBTREE_MINIMUM_RESERVATION, realloc_label, cx);
    
    cx->epilogue();
}


void rbtree_preappend2(TypeSpec elem_ts, Storage ref_storage, Cx *cx) {
    // R10 - new addition. Returns the Ref in RAX.
    Label ok;

    cx->runtime->load_lvalue(RAX, R11, ref_storage);
    
    cx->op(ADDQ, R10, Address(RAX, RBTREE_LENGTH_OFFSET));
    cx->op(CMPQ, R10, Address(RAX, RBTREE_RESERVATION_OFFSET));
    cx->op(JBE, ok);

    Label grow_label = cx->once->compile(compile_rbtree_grow, elem_ts);
    cx->op(CALL, grow_label);  // clobbers all
    
    cx->runtime->store_lvalue(RAX, R11, ref_storage);
    
    cx->code_label(ok);
}


void compile_rbtree_clone(Label label, TypeSpec elem_ts, Cx *cx) {
    // RAX - Rbtree Ref
    // Return a cloned Ref
    Label end, vacancy_check, vacancy_loop, elem_check, elem_loop;
    Label alloc_label = cx->once->compile(compile_rbtree_alloc, elem_ts);
    TypeSpec heap_ts = elem_ts.prefix(rbtree_type);
    
    cx->code_label_local(label, elem_ts.prefix(rbtree_type).symbolize("clone"));
    cx->prologue();
    cx->runtime->log("XXX rbtree clone");
    
    cx->op(PUSHQ, RAX);
    cx->op(PUSHQ, RDX);
    cx->op(MOVQ, R10, Address(RAX, RBTREE_RESERVATION_OFFSET));
    cx->op(CALL, alloc_label);  // clobbers all
    cx->op(POPQ, RDX);
    cx->op(POPQ, RBX);  // orig
    
    cx->op(MOVQ, RCX, Address(RBX, RBTREE_LENGTH_OFFSET));
    cx->op(MOVQ, Address(RAX, RBTREE_LENGTH_OFFSET), RCX);

    cx->op(CMPQ, RCX, 0);
    cx->op(JE, end);

    cx->op(MOVQ, RCX, Address(RBX, RBTREE_ROOT_OFFSET));
    cx->op(MOVQ, Address(RAX, RBTREE_ROOT_OFFSET), RCX);
    cx->op(MOVQ, RCX, Address(RBX, RBTREE_FIRST_OFFSET));
    cx->op(MOVQ, Address(RAX, RBTREE_FIRST_OFFSET), RCX);
    cx->op(MOVQ, RCX, Address(RBX, RBTREE_LAST_OFFSET));
    cx->op(MOVQ, Address(RAX, RBTREE_LAST_OFFSET), RCX);
    cx->op(MOVQ, RCX, Address(RBX, RBTREE_VACANT_OFFSET));
    cx->op(MOVQ, Address(RAX, RBTREE_VACANT_OFFSET), RCX);
    
    // Clone vacancies
    cx->op(JMP, vacancy_check);

    cx->code_label(vacancy_loop);
    cx->op(MOVQ, R10, Address(RBX, RCX, RBNODE_NEXT_OFFSET));
    cx->op(MOVQ, Address(RAX, RCX, RBNODE_NEXT_OFFSET), R10);
    cx->op(MOVQ, RCX, R10);
    
    cx->code_label(vacancy_check);
    cx->op(CMPQ, RCX, RBNODE_NIL);
    cx->op(JNE, vacancy_loop);
    
    // Clone elems
    cx->op(MOVQ, RCX, Address(RBX, RBTREE_FIRST_OFFSET));
    cx->op(JMP, elem_check);
    
    cx->code_label(elem_loop);
    elem_ts.create(Storage(MEMORY, Address(RBX, RCX, RBNODE_VALUE_OFFSET)), Storage(MEMORY, Address(RAX, RCX, RBNODE_VALUE_OFFSET)), cx);

    cx->op(MOVQ, R10, Address(RBX, RCX, RBNODE_LEFT_OFFSET));
    cx->op(MOVQ, Address(RAX, RCX, RBNODE_LEFT_OFFSET), R10);
    cx->op(MOVQ, R10, Address(RBX, RCX, RBNODE_RIGHT_OFFSET));
    cx->op(MOVQ, Address(RAX, RCX, RBNODE_RIGHT_OFFSET), R10);
    cx->op(MOVQ, R10, Address(RBX, RCX, RBNODE_PRED_OFFSET));
    cx->op(MOVQ, Address(RAX, RCX, RBNODE_PRED_OFFSET), R10);
    cx->op(MOVQ, R10, Address(RBX, RCX, RBNODE_NEXT_OFFSET));
    cx->op(MOVQ, Address(RAX, RCX, RBNODE_NEXT_OFFSET), R10);
    cx->op(MOVQ, RCX, R10);
    
    cx->code_label(elem_check);
    cx->op(CMPQ, RCX, RBNODE_NIL);
    cx->op(JNE, elem_loop);

    heap_ts.decref(RBX, cx);
    
    cx->code_label(end);
    cx->epilogue();
}




// Initializers

RbtreeEmptyValue::RbtreeEmptyValue(TypeSpec ets, TypeSpec rts)
    :GenericValue(NO_TS, rts, NULL) {
    elem_ts = ets;
}

Regs RbtreeEmptyValue::precompile(Regs preferred) {
    return Regs::all();
}

Storage RbtreeEmptyValue::compile(Cx *cx) {
    Label alloc_label = cx->once->compile(compile_rbtree_alloc, elem_ts);
    
    cx->op(MOVQ, R10, 0);
    cx->op(CALL, alloc_label);  // clobbers all
    
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

Storage RbtreeReservedValue::compile(Cx *cx) {
    Label alloc_label = cx->once->compile(compile_rbtree_alloc, elem_ts);

    right->compile_and_store(cx, Storage(REGISTER, R10));  // FIXME: may be illegal

    cx->op(CALL, alloc_label);  // clobbers all
    
    return Storage(REGISTER, RAX);
}




RbtreeInitializerValue::RbtreeInitializerValue(TypeSpec ets, TypeSpec rts)
    :ContainerInitializerValue(ets, rts, 0, 0, NULL) {
}

Regs RbtreeInitializerValue::precompile(Regs preferred) {
    ContainerInitializerValue::precompile(preferred);
    return Regs::all();
}

Storage RbtreeInitializerValue::compile(Cx *cx) {
    // This won't use the base class subcompile method, because that's inappropriate here.
    Label alloc_label = cx->once->compile(compile_rbtree_alloc, elem_ts);
    Label add_label = cx->once->compile(compile_rbtree_add, elem_ts);
    int stack_size = elem_ts.measure_stack();

    cx->op(MOVQ, R10, elems.size());
    cx->op(CALL, alloc_label);  // clobbers all
    cx->op(PUSHQ, RAX);
    
    for (auto &elem : elems) {
        elem->compile_and_store(cx, Storage(STACK));

        cx->op(MOVQ, KEYX, RSP);  // save key address for stack usage
        cx->op(MOVQ, SELFX, Address(RSP, stack_size));  // Rbtree without incref
        cx->op(MOVQ, ROOTX, Address(SELFX, RBTREE_ROOT_OFFSET));

        cx->op(CALL, add_label);

        cx->op(MOVQ, Address(SELFX, RBTREE_ROOT_OFFSET), R10);
        cx->op(ANDQ, Address(SELFX, R10, RBNODE_PRED_OFFSET), ~RBNODE_RED_BIT);  // blacken root
    
        elem_ts.create(Storage(STACK), Storage(MEMORY, Address(SELFX, KEYX, RBNODE_VALUE_OFFSET)), cx);
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

Storage RbtreeLengthValue::compile(Cx *cx) {
    ls = left->compile(cx);

    switch (ls.where) {
    case REGISTER:
        heap_ts.decref(ls.reg, cx);  // FIXME: use after decref
        cx->op(MOVQ, ls.reg, Address(ls.reg, RBTREE_LENGTH_OFFSET));
        return Storage(REGISTER, ls.reg);
    case MEMORY:
        cx->op(MOVQ, reg, ls.address);
        cx->op(MOVQ, reg, Address(reg, RBTREE_LENGTH_OFFSET));
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

Storage RbtreeAddValue::postprocess(Storage s, Cx *cx) {
    return s;
}

Storage RbtreeAddValue::compile(Cx *cx) {
    int elem_arg_size = elem_arg_ts.measure_stack();
    Label clone_label = cx->once->compile(compile_rbtree_clone, elem_ts);
    Label add_label = cx->once->compile(compile_rbtree_add, elem_ts);

    Storage ps = pivot->compile_lvalue(cx);
    elem->compile_and_store(cx, Storage(STACK));
    Storage aps = ps.access(elem_arg_size);

    container_cow(clone_label, aps, cx);  // leaves borrowed Ref in RAX

    cx->op(MOVQ, R10, 1);  // Growth
    rbtree_preappend2(elem_ts, aps, cx);
    cx->op(MOVQ, SELFX, RAX);  // TODO: not nice, maybe SELFX should be RAX?
    
    cx->op(MOVQ, ROOTX, Address(SELFX, RBTREE_ROOT_OFFSET));
    cx->op(LEA, KEYX, Address(RSP, 0));
    cx->op(CALL, add_label);
    
    cx->op(MOVQ, Address(SELFX, RBTREE_ROOT_OFFSET), R10);
    cx->op(ANDQ, Address(SELFX, R10, RBNODE_PRED_OFFSET), ~RBNODE_RED_BIT);  // blacken root

    Address alias_addr(RSP, elem_arg_size);
    Address elem_addr(SELFX, KEYX, RBNODE_VALUE_OFFSET);
    elem_ts.create(Storage(STACK), Storage(MEMORY, elem_addr), cx);

    // Leaves ps/SELFX/KEYX point to the new elem, for subclasses
    return postprocess(ps, cx);
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

Storage RbtreeAddItemValue::postprocess(Storage s, Cx *cx) {
    return s;
}

Storage RbtreeAddItemValue::compile(Cx *cx) {
    int key_size = key_ts.measure_stack();  // NOTE: as it's in an Item, it is rounded up
    int key_arg_size = key_arg_ts.measure_stack();
    int value_arg_size = value_arg_ts.measure_stack();
    Label clone_label = cx->once->compile(compile_rbtree_clone, elem_ts);
    Label add_label = cx->once->compile(compile_rbtree_add, elem_ts);

    Storage ps = pivot->compile_lvalue(cx);
    key->compile_and_store(cx, Storage(STACK));
    value->compile_and_store(cx, Storage(STACK));
    Storage aps = ps.access(key_arg_size + value_arg_size);

    container_cow(clone_label, aps, cx);  // leaves borrowed Ref in RAX

    cx->op(MOVQ, R10, 1);  // Growth
    rbtree_preappend2(elem_ts, aps, cx);
    cx->op(MOVQ, SELFX, RAX);  // TODO: not nice, maybe SELFX should be RAX?

    cx->op(MOVQ, ROOTX, Address(SELFX, RBTREE_ROOT_OFFSET));
    cx->op(LEA, KEYX, Address(RSP, value_arg_size));
    cx->op(CALL, add_label);
    
    cx->op(MOVQ, Address(SELFX, RBTREE_ROOT_OFFSET), R10);
    cx->op(ANDQ, Address(SELFX, R10, RBNODE_PRED_OFFSET), ~RBNODE_RED_BIT);  // blacken root

    // NOTE: we use the fact that Item contains index first, and value second,
    // and since they're parametric types, their sizes will be rounded up.
    Address alias_addr1(RSP, key_arg_size + value_arg_size);
    Address value_addr(SELFX, KEYX, RBNODE_VALUE_OFFSET + key_size);
    value_ts.create(Storage(STACK), Storage(MEMORY, value_addr), cx);
    
    Address alias_addr2(RSP, key_arg_size);
    Address key_addr(SELFX, KEYX, RBNODE_VALUE_OFFSET);
    key_ts.create(Storage(STACK), Storage(MEMORY, key_addr), cx);

    // Leaves ps/SELFX/KEYX point to the new elem, for subclasses
    return postprocess(ps, cx);
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

Storage RbtreeRemoveValue::postprocess(Storage s, Cx *cx) {
    return s;
}

Storage RbtreeRemoveValue::compile(Cx *cx) {
    int key_arg_size = key_arg_ts.measure_stack();
    Label clone_label = cx->once->compile(compile_rbtree_clone, elem_ts);
    Label remove_label = cx->once->compile(compile_rbtree_remove, elem_ts);

    Storage ps = pivot->compile_lvalue(cx);
    key->compile_and_store(cx, Storage(STACK));
    Storage aps = ps.access(key_arg_size);

    container_cow(clone_label, aps, cx);  // leaves borrowed Ref in RAX

    cx->op(MOVQ, SELFX, RAX);
    cx->op(MOVQ, ROOTX, Address(SELFX, RBTREE_ROOT_OFFSET));
    cx->op(LEA, KEYX, Address(RSP, 0));  // NOTE: only the index part is present of the Item
    cx->op(CALL, remove_label);
    
    cx->op(MOVQ, Address(SELFX, RBTREE_ROOT_OFFSET), R10);

    key_arg_ts.store(Storage(STACK), Storage(), cx);

    // Leaves ps/SELFX/KEYX point to the destroyed elem, for subclasses
    return postprocess(ps, cx);
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

Storage RbtreeHasValue::compile(Cx *cx) {
    pivot->compile_and_store(cx, Storage(STACK));
    key->compile_and_store(cx, Storage(STACK));
    
    int key_arg_size = key_arg_ts.measure_stack();
    Label has_label = cx->once->compile(compile_rbtree_has, elem_ts);

    cx->op(MOVQ, SELFX, Address(RSP, key_arg_size));
    cx->op(MOVQ, ROOTX, Address(SELFX, RBTREE_ROOT_OFFSET));
    cx->op(LEA, KEYX, Address(RSP, 0));
    cx->op(CALL, has_label);  // KEYX is the index of the found item, or NIL
    
    key_arg_ts.store(Storage(STACK), Storage(), cx);
    pivot->ts.store(Storage(STACK), Storage(), cx);
    
    cx->op(CMPQ, KEYX, RBNODE_NIL);

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

Storage RbtreeIndexValue::compile(Cx *cx) {
    // Try borrowing the container
    Storage ps = pivot->compile(cx);
    bool container_borrowed = false;
    
    if (may_borrow_heap && ps.where == MEMORY) {
        cx->op(PUSHQ, ps.address);
        container_borrowed = true;
    }
    else
        pivot->ts.store(ps, Storage(STACK), cx);
    
    key->compile_and_store(cx, Storage(STACK));

    int key_size = key_ts.measure_stack();  // in an Item it's rounded up
    int key_arg_size = key_arg_ts.measure_stack();
    Label has_label = cx->once->compile(compile_rbtree_has, elem_ts);

    cx->op(MOVQ, SELFX, Address(RSP, key_arg_size));
    cx->op(MOVQ, ROOTX, Address(SELFX, RBTREE_ROOT_OFFSET));
    cx->op(LEA, KEYX, Address(RSP, 0));
    cx->op(CALL, has_label);  // KEYX is the index of the found item, or NIL
    
    key_arg_ts.store(Storage(STACK), Storage(), cx);
    
    if (container_borrowed)
        cx->op(POPQ, SELFX);
    else
        pivot->ts.store(Storage(STACK), Storage(REGISTER, SELFX), cx);

    Label ok;
    //defer_decref(SELFX, cx);
    
    cx->op(CMPQ, KEYX, RBNODE_NIL);
    cx->op(JNE, ok);

    if (!container_borrowed)
        cx->runtime->decref(SELFX);
    raise("NOT_FOUND", cx);
    
    cx->code_label(ok);
    Address addr(SELFX, KEYX, RBNODE_VALUE_OFFSET + key_size);

    return compile_contained_lvalue(addr, container_borrowed ? NOREG : SELFX, ts, cx);
}


// Iteration

RbtreeElemByAgeIterValue::RbtreeElemByAgeIterValue(Value *l, TypeSpec iter_ts)
    :SimpleRecordValue(iter_ts, l) {
}

Storage RbtreeElemByAgeIterValue::compile(Cx *cx) {
    left->compile_and_store(cx, Storage(STACK));
    
    cx->op(POPQ, R10);
    cx->op(PUSHQ, Address(R10, RBTREE_FIRST_OFFSET));
    cx->op(PUSHQ, R10);
    
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

Storage RbtreeNextElemByAgeValue::postprocess(Register r, Register i, Cx *cx) {
    throw INTERNAL_ERROR;
}

Storage RbtreeNextElemByAgeValue::compile(Cx *cx) {
    ls = left->compile_lvalue(cx);  // iterator
    Storage als = ls.access(0);
    
    Register r = (clob & ~ls.regs()).get_gpr();
    Register i = (clob & ~ls.regs() & ~Regs(r)).get_gpr();
    Label ok;

    // Load the ref to r, and the index to i
    cx->runtime->load_lvalue(r, R11, als);
    cx->runtime->load_lvalue(i, R11, als, REFERENCE_SIZE);
    
    cx->op(CMPQ, i, RBNODE_NIL);
    cx->op(JNE, ok);
    
    drop_and_raise(left->ts, ls, "ITERATOR_DONE", cx);
    
    cx->code_label(ok);
    
    if (is_down) {
        cx->op(MOVQ, R10, Address(r, i, RBNODE_PRED_OFFSET));
        cx->op(ANDQ, R10, ~RBNODE_RED_BIT);  // remove color bit
    }
    else {
        cx->op(MOVQ, R10, Address(r, i, RBNODE_NEXT_OFFSET));
    }

    // Save new iterator position
    cx->runtime->store_lvalue(R10, R11, als, REFERENCE_SIZE);

    left->ts.store(ls, Storage(), cx);

    return postprocess(r, i, cx);
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

Storage RbtreeNextElemByOrderValue::postprocess(Register r, Register i, Cx *cx) {
    throw INTERNAL_ERROR;
}

Storage RbtreeNextElemByOrderValue::compile(Cx *cx) {
    Label next_label = cx->once->compile(compile_rbtree_next);
    Label ok;

    ls = left->compile_lvalue(cx);
    Storage als = ls.access(0);

    // Load the ref to SELFX, and the index to RAX
    cx->runtime->load_lvalue(SELFX, R11, als);
    cx->runtime->load_lvalue(RAX, R11, als, REFERENCE_SIZE);
        
    cx->op(CALL, next_label);  // RAX - new it, R10 - index
    
    cx->op(CMPQ, RAX, 0);
    cx->op(JNE, ok);

    drop_and_raise(left->ts, ls, "ITERATOR_DONE", cx);
    
    cx->code_label(ok);

    // Save new iterator position
    cx->runtime->store_lvalue(RAX, R11, als, REFERENCE_SIZE);

    cx->op(MOVQ, RAX, R10);
    left->ts.store(ls, Storage(), cx);
    
    return postprocess(SELFX, RAX, cx);
}

