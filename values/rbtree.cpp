
int rbtree_elem_size(TypeSpec elem_ts) {
    return elem_ts.measure_stack() + RBNODE_HEADER_SIZE;
}


// NOTE: node indexes stored in root, vacant, *.{left,right,prev,next} are tree-relative
// offsets, so if RSI points to the tree, then RSI + RAX points to the node. The NIL node
// value may be 0, which is an invalid offset, since the tree itself has a nonempty header.

void compile_rbtree_alloc(Label label, TypeSpec elem_ts, X64 *x64) {
    // R10 - reservation
    int elem_size = rbtree_elem_size(elem_ts);
    Label finalizer_label = elem_ts.prefix(rbtree_type).get_finalizer_label(x64);
    
    x64->code_label_local(label, elem_ts.symbolize() + "_rbtree_alloc");
    
    container_alloc(RBTREE_HEADER_SIZE, elem_size, RBTREE_RESERVATION_OFFSET, finalizer_label, x64);

    x64->op(MOVQ, Address(RAX, RBTREE_LENGTH_OFFSET), 0);
    x64->op(MOVQ, Address(RAX, RBTREE_ROOT_OFFSET), RBNODE_NIL);
    x64->op(MOVQ, Address(RAX, RBTREE_VACANT_OFFSET), RBNODE_NIL);
    x64->op(MOVQ, Address(RAX, RBTREE_FIRST_OFFSET), RBNODE_NIL);
    x64->op(MOVQ, Address(RAX, RBTREE_LAST_OFFSET), RBNODE_NIL);
    
    x64->op(RET);
}


void compile_rbtree_realloc(Label label, TypeSpec elem_ts, X64 *x64) {
    // RAX - array, R10 - new reservation
    int elem_size = rbtree_elem_size(elem_ts);

    x64->code_label_local(label, elem_ts.symbolize() + "_rbtree_realloc");

    container_realloc(RBTREE_HEADER_SIZE, elem_size, RBTREE_RESERVATION_OFFSET, x64);

    x64->op(RET);
}


void compile_rbtree_grow(Label label, TypeSpec elem_ts, X64 *x64) {
    // RAX - array, R10 - new reservation
    // Double the reservation until it's enough
    Label realloc_label = x64->once->compile(compile_rbtree_realloc, elem_ts);

    x64->code_label_local(label, elem_ts.symbolize() + "_rbtree_grow");
    //x64->log(elem_ts.symbolize() + "_rbtree_grow");
    container_grow(RBTREE_RESERVATION_OFFSET, RBTREE_MINIMUM_RESERVATION, realloc_label, x64);
    
    x64->op(RET);
}


void rbtree_preappend2(TypeSpec elem_ts, Storage ref_storage, X64 *x64) {
    // R10 - new addition. Returns the Ref in RAX.
    Label ok;

    load_ref(RAX, R11, ref_storage, x64);
    
    x64->op(ADDQ, R10, Address(RAX, RBTREE_LENGTH_OFFSET));
    x64->op(CMPQ, R10, Address(RAX, RBTREE_RESERVATION_OFFSET));
    x64->op(JBE, ok);

    Label grow_label = x64->once->compile(compile_rbtree_grow, elem_ts);
    x64->op(CALL, grow_label);  // clobbers all
    
    store_ref(RAX, R11, ref_storage, x64);
    
    x64->code_label(ok);
}


void compile_rbtree_clone(Label label, TypeSpec elem_ts, X64 *x64) {
    // RAX - Rbtree Ref
    // Return a cloned Ref
    Label end, vacancy_check, vacancy_loop, elem_check, elem_loop;
    Label alloc_label = x64->once->compile(compile_rbtree_alloc, elem_ts);
    TypeSpec heap_ts = elem_ts.prefix(rbtree_type);
    
    x64->code_label_local(label, elem_ts.symbolize() + "_rbtree_clone");
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

class RbtreeEmptyValue: public GenericValue {
public:
    TypeSpec elem_ts;
    
    RbtreeEmptyValue(TypeSpec ets, TypeSpec rts)
        :GenericValue(NO_TS, rts, NULL) {
        elem_ts = ets;
    }

    virtual Regs precompile(Regs preferred) {
        return Regs::all();
    }

    virtual Storage compile(X64 *x64) {
        Label alloc_label = x64->once->compile(compile_rbtree_alloc, elem_ts);
        
        x64->op(MOVQ, R10, 0);
        x64->op(CALL, alloc_label);  // clobbers all
        
        return Storage(REGISTER, RAX);
    }
};


class RbtreeReservedValue: public GenericValue {
public:
    TypeSpec elem_ts;
    
    RbtreeReservedValue(TypeSpec ets, TypeSpec rts)
        :GenericValue(INTEGER_TS, rts, NULL) {
        elem_ts = ets;
    }

    virtual Regs precompile(Regs preferred) {
        right->precompile(preferred);
        return Regs::all();
    }

    virtual Storage compile(X64 *x64) {
        Label alloc_label = x64->once->compile(compile_rbtree_alloc, elem_ts);

        right->compile_and_store(x64, Storage(REGISTER, R10));  // FIXME: may be illegal

        x64->op(CALL, alloc_label);  // clobbers all
        
        return Storage(REGISTER, RAX);
    }
};


class RbtreeInitializerValue: public ContainerInitializerValue {
public:
    RbtreeInitializerValue(TypeSpec ets, TypeSpec rts)
        :ContainerInitializerValue(ets, rts, 0, 0, NULL) {
    }

    virtual Regs precompile(Regs preferred) {
        ContainerInitializerValue::precompile(preferred);
        return Regs::all();
    }

    virtual Storage compile(X64 *x64) {
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
};


// Methods

class RbtreeLengthValue: public GenericValue {
public:
    Register reg;
    TypeSpec heap_ts;
    
    RbtreeLengthValue(Value *l, TypeSpec ets)
        :GenericValue(NO_TS, INTEGER_TS, l) {
        reg = NOREG;
        heap_ts = ets.prefix(rbtree_type);
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = left->precompile(preferred);
        
        if (!clob.has_any())
            clob = clob | RAX;
        
        reg = clob.get_any();
            
        return clob;
    }

    virtual Storage compile(X64 *x64) {
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
};


class RbtreeAddValue: public Value, public Aliaser {
public:
    TypeSpec elem_ts, elem_arg_ts;
    std::unique_ptr<Value> pivot, elem;

    RbtreeAddValue(Value *l, TypeSpec ets, TypeSpec eats)
        :Value(l->ts) {
        pivot.reset(l);
        
        elem_ts = ets;
        elem_arg_ts = eats;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        check_alias(scope);
        
        return check_arguments(args, kwargs, {
            { "elem", &elem_arg_ts, scope, &elem }
        });
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = pivot->precompile(preferred) | elem->precompile(preferred);
        
        return clob | RBTREE_CLOB | COMPARE_CLOB;
    }

    virtual Storage compile(X64 *x64) {
        Storage ps = pivot->compile_and_alias(x64, get_alias());
        elem->compile_and_store(x64, Storage(STACK));
        
        int elem_arg_size = elem_arg_ts.measure_stack();
        Label clone_label = x64->once->compile(compile_rbtree_clone, elem_ts);
        Label add_label = x64->once->compile(compile_rbtree_add, elem_ts);

        container_cow(clone_label, ps, x64);

        x64->op(MOVQ, R10, 1);  // Growth
        rbtree_preappend2(elem_ts, ps, x64);
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
        return ps;
    }
};


class RbtreeAddItemValue: public Value, public Aliaser {
public:
    TypeSpec key_ts, value_ts, key_arg_ts, value_arg_ts;
    TypeSpec elem_ts;
    std::unique_ptr<Value> pivot, key, value;

    RbtreeAddItemValue(Value *l, TypeSpec kts, TypeSpec vts, TypeSpec kats, TypeSpec vats)
        :Value(l->ts) {
        pivot.reset(l);
            
        key_ts = kts;
        value_ts = vts;
        key_arg_ts = kats;
        value_arg_ts = vats;
        
        elem_ts = TypeSpec(item_type, key_ts, value_ts);
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        check_alias(scope);
        
        return check_arguments(args, kwargs, {
            { "key", &key_arg_ts, scope, &key },
            { "value", &value_arg_ts, scope, &value }
        });
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = pivot->precompile(preferred) | key->precompile(preferred) | value->precompile(preferred);

        return clob | RBTREE_CLOB | COMPARE_CLOB;
    }

    virtual Storage compile(X64 *x64) {
        Storage ps = pivot->compile_and_alias(x64, get_alias());
        key->compile_and_store(x64, Storage(STACK));
        value->compile_and_store(x64, Storage(STACK));

        int key_size = key_ts.measure_stack();  // NOTE: as it's in an Item, it is rounded up
        int key_arg_size = key_arg_ts.measure_stack();
        int value_arg_size = value_arg_ts.measure_stack();
        Label clone_label = x64->once->compile(compile_rbtree_clone, elem_ts);
        Label add_label = x64->once->compile(compile_rbtree_add, elem_ts);

        container_cow(clone_label, ps, x64);

        x64->op(MOVQ, R10, 1);  // Growth
        rbtree_preappend2(elem_ts, ps, x64);
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
        return ps;
    }
};


class RbtreeRemoveValue: public Value, public Aliaser {
public:
    TypeSpec elem_ts, key_arg_ts;
    std::unique_ptr<Value> pivot, key;

    RbtreeRemoveValue(Value *l, TypeSpec ets, TypeSpec kats)
        :Value(l->ts) {
        pivot.reset(l);
        
        elem_ts = ets;
        key_arg_ts = kats;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        check_alias(scope);
        
        return check_arguments(args, kwargs, {
            { "key", &key_arg_ts, scope, &key }
        });
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = pivot->precompile(preferred) | key->precompile(preferred);
        return clob | RBTREE_CLOB | COMPARE_CLOB;
    }

    virtual Storage compile(X64 *x64) {
        Storage ps = pivot->compile_and_alias(x64, get_alias());
        key->compile_and_store(x64, Storage(STACK));

        Label clone_label = x64->once->compile(compile_rbtree_clone, elem_ts);
        Label remove_label = x64->once->compile(compile_rbtree_remove, elem_ts);

        container_cow(clone_label, ps, x64);  // Leaves Ref in RAX

        x64->op(MOVQ, SELFX, RAX);
        x64->op(MOVQ, ROOTX, Address(SELFX, RBTREE_ROOT_OFFSET));
        x64->op(LEA, KEYX, Address(RSP, 0));  // NOTE: only the index part is present of the Item
        x64->op(CALL, remove_label);
        
        x64->op(MOVQ, Address(SELFX, RBTREE_ROOT_OFFSET), R10);

        key_arg_ts.store(Storage(STACK), Storage(), x64);

        // Leaves ps/SELFX/KEYX point to the new elem, for subclasses
        return ps;
    }
};


class RbtreeHasValue: public Value {
public:
    TypeSpec elem_ts, key_arg_ts;
    std::unique_ptr<Value> pivot, key, value;

    RbtreeHasValue(Value *l, TypeSpec ets, TypeSpec kats)
        :Value(BOOLEAN_TS) {
        pivot.reset(l);

        elem_ts = ets;
        key_arg_ts = kats;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        return check_arguments(args, kwargs, {
            { "key", &key_arg_ts, scope, &key }
        });
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = pivot->precompile(preferred) | key->precompile(preferred);
        return clob | RBTREE_CLOB | COMPARE_CLOB;
    }

    virtual Storage compile(X64 *x64) {
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
};


class RbtreeIndexValue: public Value {
public:
    TypeSpec key_ts, elem_ts, key_arg_ts, heap_ts;
    std::unique_ptr<Value> pivot, key;
    Unborrow *unborrow;

    RbtreeIndexValue(Value *l, TypeSpec kts, TypeSpec ets, TypeSpec kats, TypeSpec vrts)
        :Value(vrts) {
        pivot.reset(l);

        key_ts = kts;
        elem_ts = ets;
        key_arg_ts = kats;
        
        heap_ts = elem_ts.prefix(rbtree_type);
        unborrow = NULL;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!check_arguments(args, kwargs, {
            { "key", &key_arg_ts, scope, &key }
        }))
            return false;
            
        unborrow = new Unborrow(heap_ts);
        scope->add(unborrow);
        
        return true;
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = pivot->precompile(preferred) | key->precompile(preferred);
        return clob | RBTREE_CLOB | COMPARE_CLOB;
    }

    virtual Storage compile(X64 *x64) {
        pivot->compile_and_store(x64, Storage(STACK));
        key->compile_and_store(x64, Storage(STACK));

        int key_size = key_ts.measure_stack();  // in an Item it's rounded up
        int key_arg_size = key_arg_ts.measure_stack();
        Label has_label = x64->once->compile(compile_rbtree_has, elem_ts);

        x64->op(MOVQ, SELFX, Address(RSP, key_arg_size));
        x64->op(MOVQ, ROOTX, Address(SELFX, RBTREE_ROOT_OFFSET));
        x64->op(LEA, KEYX, Address(RSP, 0));
        x64->op(CALL, has_label);  // KEYX is the index of the found item, or NIL
        
        key_arg_ts.store(Storage(STACK), Storage(), x64);
        pivot->ts.store(Storage(STACK), Storage(), x64);

        Label ok;
        x64->op(CMPQ, KEYX, RBNODE_NIL);
        x64->op(JNE, ok);

        x64->runtime->die("Map missing!");  // FIXME: raise something

        x64->code_label(ok);

        // Borrow Lvalue container
        heap_ts.incref(SELFX, x64);
        x64->op(MOVQ, unborrow->get_address(), SELFX);
        
        Address value_addr(SELFX, KEYX, RBNODE_VALUE_OFFSET + key_size);
        return Storage(MEMORY, value_addr);
    }
};


// Iteration

class RbtreeElemByAgeIterValue: public SimpleRecordValue {
public:
    RbtreeElemByAgeIterValue(Value *l, TypeSpec iter_ts)
        :SimpleRecordValue(iter_ts, l) {
    }

    virtual Storage compile(X64 *x64) {
        left->compile_and_store(x64, Storage(STACK));
        
        x64->op(POPQ, R10);
        x64->op(PUSHQ, Address(R10, RBTREE_FIRST_OFFSET));
        x64->op(PUSHQ, R10);
        
        return Storage(STACK);
    }
};


class RbtreeNextElemByAgeValue: public GenericValue, public Raiser {
public:
    Regs clob;
    bool is_down;

    RbtreeNextElemByAgeValue(Value *l, TypeSpec ts)
        :GenericValue(NO_TS, ts, l) {
        is_down = false;  // TODO: get as argument for backward iteration!
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!check_arguments(args, kwargs, {}))
            return false;

        if (!check_raise(iterator_done_exception_type, scope))
            return false;
        
        return true;
    }

    virtual Regs precompile(Regs preferred) {
        clob = left->precompile(preferred);
        
        if (!clob.has_any())
            clob = clob | RAX;
        
        return clob;
    }

    virtual Storage compile(X64 *x64) {
        ls = left->compile(x64);  // iterator
        Register reg = (clob & ~ls.regs()).get_any();
        Label ok;
        
        switch (ls.where) {
        case MEMORY:
            x64->op(MOVQ, R10, ls.address + REFERENCE_SIZE);  // offset
            x64->op(MOVQ, reg, ls.address); // tree reference without incref
            x64->op(CMPQ, R10, RBNODE_NIL);
            x64->op(JNE, ok);
            
            raise("ITERATOR_DONE", x64);
            
            x64->code_label(ok);
            x64->op(ADDQ, reg, R10);
            
            if (is_down) {
                x64->op(MOVQ, R10, Address(reg, RBNODE_PRED_OFFSET));
                x64->op(ANDQ, R10, ~RBNODE_RED_BIT);  // remove color bit
            }
            else {
                x64->op(MOVQ, R10, Address(reg, RBNODE_NEXT_OFFSET));
            }
            
            x64->op(MOVQ, ls.address + REFERENCE_SIZE, R10);
            
            return Storage(MEMORY, Address(reg, RBNODE_VALUE_OFFSET));
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class RbtreeElemByOrderIterValue: public ContainerIterValue {
public:
    RbtreeElemByOrderIterValue(Value *l, TypeSpec iter_ts)
        :ContainerIterValue(iter_ts, l) {
    }
};


class RbtreeNextElemByOrderValue: public GenericValue, public Raiser, public Aliaser {
public:
    Regs clob;

    RbtreeNextElemByOrderValue(Value *l, TypeSpec ts)
        :GenericValue(NO_TS, ts, l) {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        check_alias(scope);
        
        if (!check_arguments(args, kwargs, {}))
            return false;

        if (!check_raise(iterator_done_exception_type, scope))
            return false;
        
        return true;
    }

    virtual Regs precompile(Regs preferred) {
        clob = left->precompile(preferred);
        
        return clob | RAX | RCX | RDX | SELFX;
    }

    virtual Storage compile(X64 *x64) {
        Label next_label = x64->once->compile(compile_rbtree_next);
        Label ok;

        ls = left->compile_and_alias(x64, get_alias());

        if (ls.where == MEMORY) {
            x64->op(MOVQ, SELFX, ls.address);
            x64->op(MOVQ, RAX, ls.address + REFERENCE_SIZE);
        }
        else if (ls.where == ALIAS) {
            x64->op(MOVQ, R11, ls.address);
            x64->op(MOVQ, SELFX, Address(R11, 0));
            x64->op(MOVQ, RAX, Address(R11, REFERENCE_SIZE));
        }
        else
            throw INTERNAL_ERROR;
            
        x64->op(CALL, next_label);
        
        x64->op(CMPQ, RAX, 0);
        x64->op(JNE, ok);

        raise("ITERATOR_DONE", x64);
        
        x64->code_label(ok);

        // Save new iterator position
        if (ls.where == MEMORY) {
            x64->op(MOVQ, ls.address + REFERENCE_SIZE, RAX);
        }
        else if (ls.where == ALIAS) {
            x64->op(MOVQ, R11, ls.address);
            x64->op(MOVQ, Address(R11, REFERENCE_SIZE), RAX);
        }
        
        // Return new item address
        x64->op(LEA, RAX, Address(SELFX, R10, RBNODE_VALUE_OFFSET));

        return Storage(MEMORY, Address(RAX, 0));
    }
};

