
class MapAddValue: public Value {
public:
    TypeSpec key_ts, value_ts, item_ts, key_arg_ts, value_arg_ts;
    std::unique_ptr<Value> pivot, key, value;

    MapAddValue(Value *l, TypeMatch &match)
        :Value(VOID_TS) {
        pivot.reset(l);
        key_ts = match[1];
        value_ts = match[2];
        item_ts = match[0].unprefix(weakref_type).reprefix(map_type, item_type);
        
        // To help subclasses tweaking these
        key_arg_ts = key_ts;
        value_arg_ts = value_ts;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        return check_arguments(args, kwargs, {
            { "key", &key_arg_ts, scope, &key },
            { "value", &value_arg_ts, scope, &value }
        });
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = pivot->precompile(preferred) | key->precompile(preferred) | value->precompile(preferred);
        
        // We build on this in WeakValueMap::precreate
        return clob.add(RAX).add(RBX).add(RCX).add(RDX).add(RSI).add(RDI);
    }

    virtual void precreate(Address self_addr, X64 *x64) {
        // To be overridden
    }

    virtual Storage compile(X64 *x64) {
        pivot->compile_and_store(x64, Storage(STACK));
        key->compile_and_store(x64, Storage(STACK));
        value->compile_and_store(x64, Storage(STACK));

        int key_stack_size = key_arg_ts.measure_stack();
        int value_stack_size = value_arg_ts.measure_stack();
        
        Label grow_label = x64->once->compile(compile_rbtree_grow, item_ts);
        Label add_label = x64->once->compile(compile_rbtree_add, item_ts);
        Label ok;

        x64->op(MOVQ, RBX, Address(RSP, key_stack_size + value_stack_size));
        x64->op(MOVQ, RSI, Address(RBX, CLASS_MEMBERS_OFFSET));

        // Autogrow
        x64->op(MOVQ, RBX, Address(RSI, RBTREE_LENGTH_OFFSET));
        x64->op(CMPQ, RBX, Address(RSI, RBTREE_RESERVATION_OFFSET));
        x64->op(JB, ok);
        
        x64->op(MOVQ, RAX, RSI);
        x64->op(INCQ, RBX);
        x64->op(CALL, grow_label);
        x64->op(MOVQ, RBX, Address(RSP, key_stack_size + value_stack_size));
        x64->op(MOVQ, Address(RBX, CLASS_MEMBERS_OFFSET), RAX);
        x64->op(MOVQ, RSI, RAX);
        
        // Add
        x64->code_label(ok);
        x64->op(MOVQ, RAX, Address(RSI, RBTREE_ROOT_OFFSET));
        x64->op(LEA, RDI, Address(RSP, value_stack_size));

        x64->op(CALL, add_label);
        x64->op(MOVQ, Address(RSI, RBTREE_ROOT_OFFSET), RBX);
        x64->op(ANDQ, Address(RSI, RBX, RBNODE_PREV_IS_RED_OFFSET), -2);  // blacken root

        // NOTE: we abuse the fact that Item contains index first, and value second,
        // and since they're parametric types, their sizes will be rounded up.
        precreate(Address(RSP, key_stack_size + value_stack_size), x64);
        value_ts.create(Storage(STACK), Storage(MEMORY, Address(RSI, RDI, RBNODE_VALUE_OFFSET + key_stack_size)), x64);
        key_ts.create(Storage(STACK), Storage(MEMORY, Address(RSI, RDI, RBNODE_VALUE_OFFSET)), x64);
        pivot->ts.store(Storage(STACK), Storage(), x64);
        
        return Storage();
    }
};


class MapRemoveValue: public Value {
public:
    TypeSpec key_ts, item_ts;
    std::unique_ptr<Value> pivot, key;

    MapRemoveValue(Value *l, TypeMatch &match)
        :Value(VOID_TS) {
        pivot.reset(l);
        key_ts = match[1];
        item_ts = match[0].unprefix(weakref_type).reprefix(map_type, item_type);
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        return check_arguments(args, kwargs, {
            { "key", &key_ts, scope, &key }
        });
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = pivot->precompile(preferred) | key->precompile(preferred);
        return clob.add(RAX).add(RBX).add(RCX).add(RDX).add(RSI).add(RDI);
    }

    virtual Storage compile(X64 *x64) {
        pivot->compile_and_store(x64, Storage(STACK));
        key->compile_and_store(x64, Storage(STACK));

        int key_stack_size = key_ts.measure_stack();
        
        Label remove_label = x64->once->compile(compile_rbtree_remove, item_ts);

        x64->op(MOVQ, RBX, Address(RSP, key_stack_size));
        x64->op(MOVQ, RSI, Address(RBX, CLASS_MEMBERS_OFFSET));
        x64->op(MOVQ, RAX, Address(RSI, RBTREE_ROOT_OFFSET));
        x64->op(LEA, RDI, Address(RSP, 0));  // NOTE: only the index part is present of the Item

        x64->op(CALL, remove_label);
        x64->op(MOVQ, Address(RSI, RBTREE_ROOT_OFFSET), RBX);

        key_ts.store(Storage(STACK), Storage(), x64);
        pivot->ts.store(Storage(STACK), Storage(), x64);
        
        return Storage();
    }
};


class MapIndexValue: public Value {
public:
    TypeSpec key_ts, value_ts, item_ts;
    std::unique_ptr<Value> pivot, key, value;

    MapIndexValue(Value *l, TypeMatch &match)
        :Value(match[2]) {
        pivot.reset(l);
        key_ts = match[1];
        item_ts = match[0].unprefix(weakref_type).reprefix(map_type, item_type);
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        return check_arguments(args, kwargs, {
            { "key", &key_ts, scope, &key }
        });
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = pivot->precompile(preferred) | key->precompile(preferred);
        return clob.add(RAX).add(RBX).add(RCX).add(RDX).add(RSI).add(RDI);
    }

    virtual Storage compile(X64 *x64) {
        int key_stack_size = key_ts.measure_stack();
        
        pivot->compile_and_store(x64, Storage(STACK));
        key->compile_and_store(x64, Storage(STACK));
        
        Label has_label = x64->once->compile(compile_rbtree_has, item_ts);
        Label ok;

        x64->op(MOVQ, RBX, Address(RSP, key_stack_size));
        x64->op(MOVQ, RSI, Address(RBX, CLASS_MEMBERS_OFFSET));
        x64->op(MOVQ, RAX, Address(RSI, RBTREE_ROOT_OFFSET));
        x64->op(LEA, RDI, Address(RSP, 0));

        x64->op(CALL, has_label);
        x64->op(CMPQ, RDI, RBNODE_NIL);
        x64->op(JNE, ok);

        x64->die("Map missing!");  // TODO

        x64->code_label(ok);
        key_ts.store(Storage(STACK), Storage(), x64);
        pivot->ts.store(Storage(STACK), Storage(), x64);
        
        return Storage(MEMORY, Address(RSI, RDI, RBNODE_VALUE_OFFSET + key_stack_size));
    }
};


TypeMatch &wvmatch(TypeMatch &match) {
    static TypeMatch tm;
    tm[0] = typesubst(SAME_SAMEID2_WEAKANCHOR_MAP_WEAKREF_TS, match);
    tm[1] = match[1];
    tm[2] = match[2].prefix(weakanchor_type);
    tm[3] = NO_TS;
    return tm;
}


class WeakValueMapAddValue: public MapAddValue {
public:
    WeakValueMapAddValue(Value *l, TypeMatch &match)
        :MapAddValue(l, wvmatch(match)) {
        value_arg_ts = value_ts.reprefix(weakanchor_type, weakref_type);
    }

    virtual void precreate(Address self_addr, X64 *x64) {
        // Allocate and push a FCB pointer. We may clobber RAX, RBX, RCX, RDX.
        Label callback_label = x64->once->compile(compile_callback, item_ts);
        
        x64->op(MOVQ, RAX, Address(RSP, 0));  // heap
        x64->op(LEARIP, RBX, callback_label);  // callback
        x64->op(MOVQ, RCX, self_addr);  // payload1
        x64->op(MOVQ, RDX, RDI);  // payload2
        x64->op(CALL, x64->alloc_fcb_label);
        x64->op(PUSHQ, RAX);
    }
    
    static void compile_callback(Label label, TypeSpec item_ts, X64 *x64) {
        x64->code_label_local(label, "x_weakvaluemap_callback");
        // RAX - fcb, RCX - payload1, RDX - payload2
        // We may clobber all registers

        x64->log("WeakValueMap callback.");
        
        Label remove_label = x64->once->compile(compile_rbtree_remove, item_ts);

        x64->op(MOVQ, RSI, Address(RCX, CLASS_MEMBERS_OFFSET));
        x64->op(MOVQ, RAX, Address(RSI, RBTREE_ROOT_OFFSET));
        x64->op(LEA, RDI, Address(RSI, RDX, RBNODE_VALUE_OFFSET));

        x64->op(CALL, remove_label);
        
        x64->op(MOVQ, Address(RSI, RBTREE_ROOT_OFFSET), RBX);
        
        x64->op(RET);
    }
};


class WeakValueMapRemoveValue: public MapRemoveValue {
public:
    WeakValueMapRemoveValue(Value *l, TypeMatch &match)
        :MapRemoveValue(l, wvmatch(match)) {
    }
};


class WeakValueMapIndexValue: public MapIndexValue {
public:
    WeakValueMapIndexValue(Value *l, TypeMatch &match)
        :MapIndexValue(l, wvmatch(match)) {
        ts = ts.reprefix(weakanchor_type, weakref_type);
    }
};
