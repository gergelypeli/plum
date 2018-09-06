
int rbtree_elem_size(TypeSpec elem_ts) {
    return elem_ts.measure_stack() + RBNODE_HEADER_SIZE;
}


// NOTE: node indexes stored in root, vacant, *.{left,right,prev,next} are tree-relative
// offsets, so if RSI points to the tree, then RSI + RAX points to the node. The NIL node
// value may be 0, which is an invalid offset, since the tree itself has a nonempty header.

void compile_rbtree_alloc(Label label, TypeSpec elem_ts, X64 *x64) {
    // RAX - reservation
    int elem_size = rbtree_elem_size(elem_ts);
    Label finalizer_label = elem_ts.prefix(rbtree_type).get_finalizer_label(x64);
    
    x64->code_label_local(label, "x_rbtree_alloc");
    
    container_alloc(RBTREE_HEADER_SIZE, elem_size, RBTREE_RESERVATION_OFFSET, finalizer_label, x64);

    x64->op(MOVQ, Address(RAX, RBTREE_LENGTH_OFFSET), 0);
    x64->op(MOVQ, Address(RAX, RBTREE_ROOT_OFFSET), RBNODE_NIL);
    x64->op(MOVQ, Address(RAX, RBTREE_VACANT_OFFSET), RBNODE_NIL);
    x64->op(MOVQ, Address(RAX, RBTREE_FIRST_OFFSET), RBNODE_NIL);
    x64->op(MOVQ, Address(RAX, RBTREE_LAST_OFFSET), RBNODE_NIL);
    
    x64->op(RET);
}


void compile_rbtree_realloc(Label label, TypeSpec elem_ts, X64 *x64) {
    // RAX - array, RBX - new reservation
    int elem_size = rbtree_elem_size(elem_ts);

    x64->code_label_local(label, "x_rbtree_realloc");

    container_realloc(RBTREE_HEADER_SIZE, elem_size, RBTREE_RESERVATION_OFFSET, x64);

    x64->op(RET);
}


void compile_rbtree_grow(Label label, TypeSpec elem_ts, X64 *x64) {
    // RAX - array, RBX - new reservation
    // Double the reservation until it's enough
    Label realloc_label = x64->once->compile(compile_rbtree_realloc, elem_ts);

    x64->code_label_local(label, "x_rbtree_grow");
    //x64->log("x_rbtree_grow");
    container_grow(RBTREE_RESERVATION_OFFSET, RBTREE_MINIMUM_RESERVATION, realloc_label, x64);
    
    x64->op(RET);
}


// Register usage:
// ROOTX - index of current node
// RBX - return of operation
// THISX, THATX - child indexes
// SELFX - address of the tree
// KEYX - address of key (input), dark soul (output during removal)

#define SELFX R8
#define KEYX  R9
#define ROOTX R10
#define THISX RCX
#define THATX RDX

#include "rbtree_helpers.cpp"


class RbtreeEmptyValue: public GenericValue {
public:
    TypeSpec elem_ts;
    
    RbtreeEmptyValue(TypeSpec ts)
        :GenericValue(NO_TS, ts, NULL) {
        elem_ts = container_elem_ts(ts);
    }

    virtual Regs precompile(Regs preferred) {
        return Regs(RAX);
    }

    virtual Storage compile(X64 *x64) {
        Label alloc_label = x64->once->compile(compile_rbtree_alloc, elem_ts);
        
        x64->op(MOVQ, RAX, 0);
        x64->op(CALL, alloc_label);
        
        return Storage(REGISTER, RAX);
    }
};


class RbtreeReservedValue: public GenericValue {
public:
    TypeSpec elem_ts;
    
    RbtreeReservedValue(TypeSpec ts)
        :GenericValue(INTEGER_TS, ts, NULL) {
        elem_ts = container_elem_ts(ts);
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = right->precompile(preferred);
        return clob | RAX;
    }

    virtual Storage compile(X64 *x64) {
        Label alloc_label = x64->once->compile(compile_rbtree_alloc, elem_ts);

        right->compile_and_store(x64, Storage(REGISTER, RAX));

        x64->op(CALL, alloc_label);
        
        return Storage(REGISTER, RAX);
    }
};


class RbtreeInitializerValue: public ContainerInitializerValue {
public:
    RbtreeInitializerValue(TypeSpec ts)
        :ContainerInitializerValue(ts) {
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = ContainerInitializerValue::precompile(preferred);
        return clob | SELFX | ROOTX | KEYX | COMPARE_CLOB;
    }

    virtual Storage compile(X64 *x64) {
        // This won't use the base class subcompile method, because that's inappropriate here.
        Label alloc_label = x64->once->compile(compile_rbtree_alloc, elem_ts);
        Label add_label = x64->once->compile(compile_rbtree_add, elem_ts);
        int stack_size = elem_ts.measure_stack();
    
        x64->op(MOVQ, RAX, elems.size());
        x64->op(CALL, alloc_label);
        //x64->op(MOVQ, Address(RAX, RBTREE_LENGTH_OFFSET), elems.size());
        x64->op(PUSHQ, RAX);
        
        for (auto &elem : elems) {
            elem->compile_and_store(x64, Storage(STACK));

            x64->op(MOVQ, KEYX, RSP);  // save key address for stack usage
            x64->op(MOVQ, SELFX, Address(RSP, stack_size));  // Rbtree without incref
            x64->op(MOVQ, ROOTX, Address(SELFX, RBTREE_ROOT_OFFSET));

            x64->op(CALL, add_label);

            x64->op(MOVQ, Address(SELFX, RBTREE_ROOT_OFFSET), RBX);
            x64->op(ANDQ, Address(SELFX, RBX, RBNODE_PRED_OFFSET), ~RBNODE_RED_BIT);  // blacken root
        
            elem_ts.create(Storage(STACK), Storage(MEMORY, Address(SELFX, KEYX, RBNODE_VALUE_OFFSET)), x64);
        }
        
        return Storage(STACK);
    }
};


class RbtreeLengthValue: public GenericValue {
public:
    Register reg;
    
    RbtreeLengthValue(Value *l, TypeMatch &match)
        :GenericValue(NO_TS, INTEGER_TS, l) {
        reg = NOREG;
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
            x64->runtime->decref(ls.reg);
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


class RbtreeHasValue: public GenericValue {
public:
    TypeSpec elem_ts;
    
    RbtreeHasValue(Value *pivot, TypeMatch &match)
        :GenericValue(match[1], BOOLEAN_TS, pivot) {
        elem_ts = match[1];
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = left->precompile(preferred) | right->precompile(preferred);
        return clob | SELFX | ROOTX | KEYX | COMPARE_CLOB;
    }

    virtual Storage compile(X64 *x64) {
        int key_size = elem_ts.measure_stack();
        Label has = x64->once->compile(compile_rbtree_has, elem_ts);
        
        compile_and_store_both(x64, Storage(STACK), Storage(STACK));
    
        x64->op(MOVQ, KEYX, RSP);  // save key address for stack usage
        x64->op(MOVQ, SELFX, Address(RSP, key_size));  // Rbtree without incref
        x64->op(MOVQ, ROOTX, Address(SELFX, RBTREE_ROOT_OFFSET));
        
        x64->op(CALL, has);
        x64->op(CMPQ, KEYX, RBNODE_NIL);
        x64->op(SETNE, AL);
        
        elem_ts.store(Storage(STACK), Storage(), x64);
        left->ts.store(Storage(STACK), Storage(), x64);
        
        return Storage(REGISTER, AL);
    }
};


class RbtreeAddValue: public ContainerGrowableValue {
public:
    TypeSpec elem_ts;
    
    RbtreeAddValue(Value *pivot, TypeMatch &match)
        :ContainerGrowableValue(match[1], VOID_TS, pivot) {
        elem_ts = match[1];
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = left->precompile(preferred) | right->precompile(preferred);
        return clob | SELFX | ROOTX | KEYX | THISX | THATX | COMPARE_CLOB;
    }

    virtual Storage compile(X64 *x64) {
        int key_size = elem_ts.measure_stack();
        Label add = x64->once->compile(compile_rbtree_add, elem_ts);
        Label ok;
        
        compile_and_store_both(x64, Storage(STACK), Storage(STACK));

        x64->op(MOVQ, KEYX, RSP);  // save key address for stack usage
        x64->op(MOVQ, SELFX, Address(RSP, key_size));  // Rbtree without incref
        x64->op(MOVQ, ROOTX, Address(SELFX, RBTREE_ROOT_OFFSET));

        x64->op(CALL, add);

        x64->op(MOVQ, Address(SELFX, RBTREE_ROOT_OFFSET), RBX);
        x64->op(ANDQ, Address(SELFX, RBX, RBNODE_PRED_OFFSET), ~RBNODE_RED_BIT);  // blacken root
        
        x64->op(CMPQ, KEYX, RBNODE_NIL);
        x64->op(JNE, ok);
        
        // Non-autogrowing Rbtree-s only raise a CONTAINER_FULL exception, if the operation
        // actually tried to increase the size, not when an existing node is updated.
        if (raising_dummy) {
            elem_ts.store(Storage(STACK), Storage(), x64);
            left->ts.store(Storage(STACK), Storage(), x64);
            raise("CONTAINER_FULL", x64);
        }
        else
            x64->runtime->die("Rbtree full even if autogrowing!");
        
        x64->code_label(ok);
        elem_ts.create(Storage(STACK), Storage(MEMORY, Address(SELFX, KEYX, RBNODE_VALUE_OFFSET)), x64);
        left->ts.store(Storage(STACK), Storage(), x64);
        
        return Storage();
    }
};


class RbtreeRemoveValue: public ContainerShrinkableValue {
public:
    TypeSpec elem_ts;
    
    RbtreeRemoveValue(Value *pivot, TypeMatch &match)
        :ContainerShrinkableValue(match[1], VOID_TS, pivot) {
        elem_ts = match[1];
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = left->precompile(preferred) | right->precompile(preferred);
        return clob | SELFX | ROOTX | KEYX | THISX | THATX | COMPARE_CLOB;
    }

    virtual Storage compile(X64 *x64) {
        int key_size = elem_ts.measure_stack();
        Label remove = x64->once->compile(compile_rbtree_remove, elem_ts);
        
        compile_and_store_both(x64, Storage(STACK), Storage(STACK));

        x64->op(MOVQ, KEYX, RSP);  // save key address for stack usage
        x64->op(MOVQ, SELFX, Address(RSP, key_size));  // Rbtree without incref
        x64->op(MOVQ, ROOTX, Address(SELFX, RBTREE_ROOT_OFFSET));

        x64->op(CALL, remove);

        x64->op(MOVQ, Address(SELFX, RBTREE_ROOT_OFFSET), RBX);
        
        elem_ts.store(Storage(STACK), Storage(), x64);
        left->ts.store(Storage(STACK), Storage(), x64);
        
        return Storage();
    }
};


class RbtreeAutogrowValue: public ContainerAutogrowValue {
public:
    RbtreeAutogrowValue(Value *l, TypeMatch &match)
        :ContainerAutogrowValue(l, match) {
    }
    
    virtual Storage compile(X64 *x64) {
        return subcompile(RBTREE_RESERVATION_OFFSET, RBTREE_LENGTH_OFFSET, compile_rbtree_grow, x64);
    }
};


// Iteration

class RbtreeElemByAgeIterValue: public SimpleRecordValue {
public:
    RbtreeElemByAgeIterValue(Value *l, TypeMatch &match)
        :SimpleRecordValue(typesubst(SAME_RBTREEELEMBYAGEITER_TS, match), l) {
    }

    virtual Storage compile(X64 *x64) {
        left->compile_and_store(x64, Storage(STACK));
        
        x64->op(POPQ, RBX);
        x64->op(PUSHQ, Address(RBX, RBTREE_FIRST_OFFSET));
        x64->op(PUSHQ, RBX);
        
        return Storage(STACK);
    }
};


class RbtreeNextElemByAgeValue: public GenericValue, public Raiser {
public:
    Regs clob;
    bool is_down;
    TypeSpec elem_ts;

    RbtreeNextElemByAgeValue(Value *l, TypeMatch &match)
        :GenericValue(NO_TS, match[1], l) {
        is_down = false;  // TODO: get as argument for backward iteration!
        elem_ts = match[1];
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
            x64->op(MOVQ, RBX, ls.address + REFERENCE_SIZE);  // offset
            x64->op(MOVQ, reg, ls.address); // tree reference without incref
            x64->op(CMPQ, RBX, RBNODE_NIL);
            x64->op(JNE, ok);
            
            raise("ITERATOR_DONE", x64);
            
            x64->code_label(ok);
            x64->op(ADDQ, reg, RBX);
            
            if (is_down) {
                x64->op(MOVQ, RBX, Address(reg, RBNODE_PRED_OFFSET));
                x64->op(ANDQ, RBX, ~RBNODE_RED_BIT);  // remove color bit
            }
            else {
                x64->op(MOVQ, RBX, Address(reg, RBNODE_NEXT_OFFSET));
            }
            
            x64->op(MOVQ, ls.address + REFERENCE_SIZE, RBX);
            
            return Storage(MEMORY, Address(reg, RBNODE_VALUE_OFFSET));
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class RbtreeElemByOrderIterValue: public ContainerIterValue {
public:
    RbtreeElemByOrderIterValue(Value *l, TypeMatch &match)
        :ContainerIterValue(typesubst(SAME_RBTREEELEMBYORDERITER_TS, match), l) {
    }
};


class RbtreeNextElemByOrderValue: public GenericValue, public Raiser {
public:
    Regs clob;
    TypeSpec elem_ts;

    RbtreeNextElemByOrderValue(Value *l, TypeMatch &match)
        :GenericValue(NO_TS, match[1], l) {
        elem_ts = match[1];
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
        
        return clob | RAX | RCX | RDX | SELFX;
    }

    virtual Storage compile(X64 *x64) {
        Label next_label = x64->once->compile(compile_rbtree_next);
        Label ok;

        left->compile_and_store(x64, Storage(ALISTACK));  // iterator

        x64->op(MOVQ, RCX, Address(RSP, 0));
        x64->op(MOVQ, RAX, Address(RCX, REFERENCE_SIZE));  // it
        x64->op(MOVQ, SELFX, Address(RCX, 0)); // tree reference without incref

        x64->op(CALL, next_label);
        
        x64->op(POPQ, RCX);  // ALISTACK popped
        x64->op(CMPQ, RAX, 0);
        x64->op(JNE, ok);

        raise("ITERATOR_DONE", x64);
        
        x64->code_label(ok);
        x64->op(MOVQ, Address(RCX, REFERENCE_SIZE), RAX);  // save it
        x64->op(LEA, RAX, Address(SELFX, RBX, RBNODE_VALUE_OFFSET));

        return Storage(MEMORY, Address(RAX, 0));
    }
};


// Map and friends

class MapAddValue: public Value {
public:
    Value *temp_l;
    TypeSpec key_ts, value_ts, item_ts, key_arg_ts, value_arg_ts;
    std::unique_ptr<Value> pivot, key, value;

    MapAddValue(Value *l, TypeMatch &match)
        :Value(VOID_TS) {
        temp_l = l;
        key_ts = match[1];
        value_ts = match[2];
        item_ts = match[0].unprefix(ptr_type).reprefix(map_type, item_type);
        
        // To help subclasses tweaking these
        key_arg_ts = key_ts;
        value_arg_ts = value_ts;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        // Autogrow stuff
        pivot.reset(temp_l->lookup_inner("wrapped", scope)->lookup_inner("autogrow", scope));

        Args fake_args;
        Kwargs fake_kwargs;
        if (!pivot->check(fake_args, fake_kwargs, scope))
            return false;

        if (value_arg_ts == NO_TS)  // used in WeakSet
            return check_arguments(args, kwargs, {
                { "key", &key_arg_ts, scope, &key }
            });
        else
            return check_arguments(args, kwargs, {
                { "key", &key_arg_ts, scope, &key },
                { "value", &value_arg_ts, scope, &value }
            });
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = pivot->precompile(preferred) | key->precompile(preferred);
        
        if (value)
            clob = clob | value->precompile(preferred);
        
        // We build on this in WeakValueMap::precreate
        return clob | SELFX | ROOTX | KEYX | THISX | THATX | COMPARE_CLOB;
    }

    virtual void prekey(Address alias_addr, X64 *x64) {
        // To be overridden
    }

    virtual void prevalue(Address alias_addr, X64 *x64) {
        // To be overridden
    }

    virtual Storage compile(X64 *x64) {
        pivot->compile_and_store(x64, Storage(ALISTACK));  // Push the address of the rbtree ref
        key->compile_and_store(x64, Storage(STACK));
        
        if (value)
            value->compile_and_store(x64, Storage(STACK));

        int key_size = key_ts.measure_stack();  // NOTE: as it's in an Item, it is rounded up
        int key_stack_size = key_arg_ts.measure_stack();
        int value_stack_size = value_arg_ts != NO_TS ? value_arg_ts.measure_stack() : 0;
        
        Label add_label = x64->once->compile(compile_rbtree_add, item_ts);

        x64->op(MOVQ, RBX, Address(RSP, key_stack_size + value_stack_size));
        x64->op(MOVQ, SELFX, Address(RBX, 0));

        x64->op(MOVQ, ROOTX, Address(SELFX, RBTREE_ROOT_OFFSET));
        x64->op(LEA, KEYX, Address(RSP, value_stack_size));

        x64->op(CALL, add_label);
        x64->op(MOVQ, Address(SELFX, RBTREE_ROOT_OFFSET), RBX);
        x64->op(ANDQ, Address(SELFX, RBX, RBNODE_PRED_OFFSET), ~RBNODE_RED_BIT);  // blacken root

        // NOTE: we abuse the fact that Item contains index first, and value second,
        // and since they're parametric types, their sizes will be rounded up.
        if (value) {
            prevalue(Address(RSP, key_stack_size + value_stack_size), x64);
            value_ts.create(Storage(STACK), Storage(MEMORY, Address(SELFX, KEYX, RBNODE_VALUE_OFFSET + key_size)), x64);
        }
        
        prekey(Address(RSP, key_stack_size), x64);
        key_ts.create(Storage(STACK), Storage(MEMORY, Address(SELFX, KEYX, RBNODE_VALUE_OFFSET)), x64);

        pivot->ts.store(Storage(ALISTACK), Storage(), x64);
        
        return Storage();
    }
};


class MapRemoveValue: public Value {
public:
    TypeSpec key_ts, item_ts, key_arg_ts;
    std::unique_ptr<Value> pivot, key;

    MapRemoveValue(Value *l, TypeMatch &match)
        :Value(VOID_TS) {
        pivot.reset(l);
        key_ts = match[1];
        item_ts = match[0].unprefix(ptr_type).reprefix(map_type, item_type);
        
        key_arg_ts = key_ts;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        return check_arguments(args, kwargs, {
            { "key", &key_arg_ts, scope, &key }
        });
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = pivot->precompile(preferred) | key->precompile(preferred);
        return clob | SELFX | KEYX | ROOTX | THISX | THATX | COMPARE_CLOB;
    }

    virtual Storage compile(X64 *x64) {
        pivot->compile_and_store(x64, Storage(STACK));
        key->compile_and_store(x64, Storage(STACK));

        int key_stack_size = key_arg_ts.measure_stack();
        
        Label remove_label = x64->once->compile(compile_rbtree_remove, item_ts);

        x64->op(MOVQ, RBX, Address(RSP, key_stack_size));
        x64->op(MOVQ, SELFX, Address(RBX, CLASS_MEMBERS_OFFSET));
        x64->op(MOVQ, ROOTX, Address(SELFX, RBTREE_ROOT_OFFSET));
        x64->op(LEA, KEYX, Address(RSP, 0));  // NOTE: only the index part is present of the Item

        x64->op(CALL, remove_label);
        x64->op(MOVQ, Address(SELFX, RBTREE_ROOT_OFFSET), RBX);

        key_arg_ts.store(Storage(STACK), Storage(), x64);
        pivot->ts.store(Storage(STACK), Storage(), x64);
        
        return Storage();
    }
};


class MapHasValue: public Value {
public:
    TypeSpec key_ts, value_ts, item_ts, key_arg_ts;
    std::unique_ptr<Value> pivot, key, value;

    MapHasValue(Value *l, TypeMatch &match)
        :Value(BOOLEAN_TS) {
        pivot.reset(l);
        key_ts = match[1];
        item_ts = match[0].unprefix(ptr_type).reprefix(map_type, item_type);
        
        key_arg_ts = key_ts;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        return check_arguments(args, kwargs, {
            { "key", &key_arg_ts, scope, &key }
        });
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = pivot->precompile(preferred) | key->precompile(preferred);
        return clob | SELFX | KEYX | ROOTX | THISX | THATX | COMPARE_CLOB;
    }

    virtual Storage compile(X64 *x64) {
        int key_stack_size = key_arg_ts.measure_stack();
        
        pivot->compile_and_store(x64, Storage(STACK));
        key->compile_and_store(x64, Storage(STACK));
        
        Label has_label = x64->once->compile(compile_rbtree_has, item_ts);

        x64->op(MOVQ, RBX, Address(RSP, key_stack_size));
        x64->op(MOVQ, SELFX, Address(RBX, CLASS_MEMBERS_OFFSET));
        x64->op(MOVQ, ROOTX, Address(SELFX, RBTREE_ROOT_OFFSET));
        x64->op(LEA, KEYX, Address(RSP, 0));

        x64->op(CALL, has_label);  // KEYX is the index of the found item, or NIL
        
        key_arg_ts.store(Storage(STACK), Storage(), x64);
        pivot->ts.store(Storage(STACK), Storage(), x64);
        
        x64->op(CMPQ, KEYX, RBNODE_NIL);

        return Storage(FLAGS, CC_NOT_EQUAL);
    }
};


class MapIndexValue: public Value {
public:
    TypeSpec key_ts, value_ts, item_ts, key_arg_ts;
    std::unique_ptr<Value> pivot, key, value;

    MapIndexValue(Value *l, TypeMatch &match)
        :Value(match[2]) {
        pivot.reset(l);
        key_ts = match[1];
        item_ts = match[0].unprefix(ptr_type).reprefix(map_type, item_type);
        
        key_arg_ts = key_ts;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        return check_arguments(args, kwargs, {
            { "key", &key_arg_ts, scope, &key }
        });
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = pivot->precompile(preferred) | key->precompile(preferred);
        return clob | SELFX | KEYX | ROOTX | THISX | THATX | COMPARE_CLOB;
    }

    virtual Storage compile(X64 *x64) {
        int key_stack_size = key_arg_ts.measure_stack();
        
        pivot->compile_and_store(x64, Storage(STACK));
        key->compile_and_store(x64, Storage(STACK));
        
        Label has_label = x64->once->compile(compile_rbtree_has, item_ts);

        x64->op(MOVQ, RBX, Address(RSP, key_stack_size));
        x64->op(MOVQ, SELFX, Address(RBX, CLASS_MEMBERS_OFFSET));
        x64->op(MOVQ, ROOTX, Address(SELFX, RBTREE_ROOT_OFFSET));
        x64->op(LEA, KEYX, Address(RSP, 0));

        x64->op(CALL, has_label);  // KEYX is the index of the found item, or NIL
        
        key_arg_ts.store(Storage(STACK), Storage(), x64);
        pivot->ts.store(Storage(STACK), Storage(), x64);
        
        int key_size = key_ts.measure_stack();  // in an Item it's rounded up

        Label ok;
        x64->op(CMPQ, KEYX, RBNODE_NIL);
        x64->op(JNE, ok);

        x64->runtime->die("Map missing!");  // TODO

        x64->code_label(ok);
        
        return Storage(MEMORY, Address(SELFX, KEYX, RBNODE_VALUE_OFFSET + key_size));
    }
};


// Weak map helpers

static void compile_nosyvalue_callback(Label label, TypeSpec item_ts, X64 *x64) {
    std::stringstream ss;
    ss << item_ts << " Nosyvalue callback";
    
    x64->code_label_local(label, ss.str());
    x64->runtime->log(ss.str());
    
    // RAX - fcb, RCX - payload1, RDX - payload2
    // We may clobber all registers
    // FIXME: make sure these registers can be safely moved to the RB-pseudoregisters!
    
    Label remove_label = x64->once->compile(compile_rbtree_remove, item_ts);

    x64->op(MOVQ, SELFX, Address(RCX, 0));  // load current rbtree ref
    x64->op(MOVQ, ROOTX, Address(SELFX, RBTREE_ROOT_OFFSET));
    x64->op(LEA, KEYX, Address(SELFX, RDX, RBNODE_VALUE_OFFSET));

    x64->op(CALL, remove_label);
    
    x64->op(MOVQ, Address(SELFX, RBTREE_ROOT_OFFSET), RBX);
    
    x64->op(RET);
}


static void ptr_to_nosyvalue(TypeSpec item_ts, Address alias_addr, X64 *x64) {
    // Turn a Ptr into a NosyValue. We may clobber RAX, RBX, RCX, RDX.
    Label callback_label = x64->once->compile(compile_nosyvalue_callback, item_ts);
    
    x64->op(MOVQ, RAX, Address(RSP, 0));  // referred heap object
    x64->op(LEA, RBX, Address(callback_label, 0));  // callback
    x64->op(MOVQ, RCX, alias_addr);  // payload1, the rbtree ref address, RSP based
    x64->op(MOVQ, RDX, KEYX);  // payload2, the rbnode index
    
    x64->op(PUSHQ, RAX);  // in NosyValue the object pointer is at the lower address...
    
    x64->op(CALL, x64->runtime->alloc_fcb_label);
    
    x64->op(MOVQ, Address(RSP, ADDRESS_SIZE), RAX);  // .. and the fcb at the higher address
}


// WeakValueMap

TypeMatch &wvmatch(TypeMatch &match) {
    static TypeMatch tm;
    tm[0] = typesubst(SAME_SAMEID2_NOSYVALUE_MAP_PTR_TS, match);
    tm[1] = match[1];
    tm[2] = match[2].prefix(nosyvalue_type);
    tm[3] = NO_TS;
    return tm;
}


class WeakValueMapAddValue: public MapAddValue {
public:
    WeakValueMapAddValue(Value *l, TypeMatch &match)
        :MapAddValue(l, wvmatch(match)) {
        value_arg_ts = value_ts.reprefix(nosyvalue_type, ptr_type);
    }

    virtual void prevalue(Address alias_addr, X64 *x64) {
        ptr_to_nosyvalue(item_ts, alias_addr, x64);
    }
};


class WeakValueMapRemoveValue: public MapRemoveValue {
public:
    WeakValueMapRemoveValue(Value *l, TypeMatch &match)
        :MapRemoveValue(l, wvmatch(match)) {
    }
};


class WeakValueMapHasValue: public MapHasValue {
public:
    WeakValueMapHasValue(Value *l, TypeMatch &match)
        :MapHasValue(l, wvmatch(match)) {
    }
};


class WeakValueMapIndexValue: public MapIndexValue {
public:
    Unborrow *unborrow;
    
    WeakValueMapIndexValue(Value *l, TypeMatch &match)
        :MapIndexValue(l, wvmatch(match)) {
        ts = ts.reprefix(nosyvalue_type, ptr_type);
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!MapIndexValue::check(args, kwargs, scope))
            return false;
            
        unborrow = new Unborrow;
        scope->add(unborrow);
        
        return true;
    }
    
    virtual Storage compile(X64 *x64) {
        Storage s = MapIndexValue::compile(x64);
        
        if (s.where != MEMORY)
            throw INTERNAL_ERROR;
            
        x64->op(MOVQ, RBX, s.address);
        x64->runtime->incref(RBX);
        
        return s;
    }
};


// WeakIndexMap

TypeMatch &wimatch(TypeMatch &match) {
    static TypeMatch tm;
    tm[0] = typesubst(SAMEID_NOSYVALUE_SAME2_MAP_PTR_TS, match);
    tm[1] = match[1].prefix(nosyvalue_type);
    tm[2] = match[2];
    tm[3] = NO_TS;
    return tm;
}


class WeakIndexMapAddValue: public MapAddValue {
public:
    WeakIndexMapAddValue(Value *l, TypeMatch &match)
        :MapAddValue(l, wimatch(match)) {
        key_arg_ts = key_ts.reprefix(nosyvalue_type, ptr_type);
    }

    virtual void prekey(Address alias_addr, X64 *x64) {
        ptr_to_nosyvalue(item_ts, alias_addr, x64);
    }
};


class WeakIndexMapRemoveValue: public MapRemoveValue {
public:
    WeakIndexMapRemoveValue(Value *l, TypeMatch &match)
        :MapRemoveValue(l, wimatch(match)) {
        key_arg_ts = key_ts.reprefix(nosyvalue_type, ptr_type);
    }
};


class WeakIndexMapHasValue: public MapHasValue {
public:
    WeakIndexMapHasValue(Value *l, TypeMatch &match)
        :MapHasValue(l, wimatch(match)) {
        key_arg_ts = key_ts.reprefix(nosyvalue_type, ptr_type);
    }
};


class WeakIndexMapIndexValue: public MapIndexValue {
public:
    WeakIndexMapIndexValue(Value *l, TypeMatch &match)
        :MapIndexValue(l, wimatch(match)) {
        key_arg_ts = key_ts.reprefix(nosyvalue_type, ptr_type);
    }
};


// WeakSet

TypeMatch &wsmatch(TypeMatch &match) {
    static TypeMatch tm;
    tm[0] = typesubst(SAMEID_NOSYVALUE_UNIT_MAP_PTR_TS, match);
    tm[1] = match[1].prefix(nosyvalue_type);
    tm[2] = UNIT_TS;
    tm[3] = NO_TS;
    return tm;
}


class WeakSetAddValue: public MapAddValue {
public:
    WeakSetAddValue(Value *l, TypeMatch &match)
        :MapAddValue(l, wsmatch(match)) {
        key_arg_ts = key_ts.reprefix(nosyvalue_type, ptr_type);
        value_arg_ts = NO_TS;  // special handling
    }

    virtual void prekey(Address alias_addr, X64 *x64) {
        ptr_to_nosyvalue(item_ts, alias_addr, x64);
    }
};


class WeakSetRemoveValue: public MapRemoveValue {
public:
    WeakSetRemoveValue(Value *l, TypeMatch &match)
        :MapRemoveValue(l, wsmatch(match)) {
        key_arg_ts = key_ts.reprefix(nosyvalue_type, ptr_type);
    }
};


class WeakSetHasValue: public MapHasValue {
public:
    WeakSetHasValue(Value *l, TypeMatch &match)
        :MapHasValue(l, wsmatch(match)) {
        key_arg_ts = key_ts.reprefix(nosyvalue_type, ptr_type);
    }
};


#undef SELFX
#undef KEYX
#undef ROOTX
#undef THISX
#undef THATX
