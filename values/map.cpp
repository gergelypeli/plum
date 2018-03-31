
class MapAddValue: public Value {
public:
    TypeSpec key_ts, value_ts, item_ts, key_arg_ts, value_arg_ts;
    std::unique_ptr<Value> pivot, key, value;

    MapAddValue(Value *l, TypeMatch &match)
        :Value(VOID_TS) {
        pivot.reset(l->lookup_inner("wrapped")->lookup_inner("autogrow"));
        key_ts = match[1];
        value_ts = match[2];
        item_ts = match[0].unprefix(weakref_type).reprefix(map_type, item_type);
        
        // To help subclasses tweaking these
        key_arg_ts = key_ts;
        value_arg_ts = value_ts;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        // Autogrow stuff
        Args fake_args;
        Kwargs fake_kwargs;
        if (!pivot->check(fake_args, fake_kwargs, scope))
            return false;

        return check_arguments(args, kwargs, {
            { "key", &key_arg_ts, scope, &key },
            { "value", &value_arg_ts, scope, &value }  // disabled if NO_TS
        });
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = pivot->precompile(preferred) | key->precompile(preferred);
        
        if (value)
            clob = clob | value->precompile(preferred);
        
        // We build on this in WeakValueMap::precreate
        return clob | RAX | RCX | RDX | RSI | RDI;
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
        x64->op(MOVQ, RSI, Address(RBX, 0));

        x64->op(MOVQ, RAX, Address(RSI, RBTREE_ROOT_OFFSET));
        x64->op(LEA, RDI, Address(RSP, value_stack_size));

        x64->op(CALL, add_label);
        x64->op(MOVQ, Address(RSI, RBTREE_ROOT_OFFSET), RBX);
        x64->op(ANDQ, Address(RSI, RBX, RBNODE_PREV_IS_RED_OFFSET), -2);  // blacken root

        // NOTE: we abuse the fact that Item contains index first, and value second,
        // and since they're parametric types, their sizes will be rounded up.
        if (value) {
            prevalue(Address(RSP, key_stack_size + value_stack_size), x64);
            value_ts.create(Storage(STACK), Storage(MEMORY, Address(RSI, RDI, RBNODE_VALUE_OFFSET + key_size)), x64);
        }
        
        prekey(Address(RSP, key_stack_size), x64);
        key_ts.create(Storage(STACK), Storage(MEMORY, Address(RSI, RDI, RBNODE_VALUE_OFFSET)), x64);

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
        item_ts = match[0].unprefix(weakref_type).reprefix(map_type, item_type);
        
        key_arg_ts = key_ts;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        return check_arguments(args, kwargs, {
            { "key", &key_arg_ts, scope, &key }
        });
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = pivot->precompile(preferred) | key->precompile(preferred);
        return clob | RAX | RCX | RDX | RSI | RDI;
    }

    virtual Storage compile(X64 *x64) {
        pivot->compile_and_store(x64, Storage(STACK));
        key->compile_and_store(x64, Storage(STACK));

        int key_stack_size = key_arg_ts.measure_stack();
        
        Label remove_label = x64->once->compile(compile_rbtree_remove, item_ts);

        x64->op(MOVQ, RBX, Address(RSP, key_stack_size));
        x64->op(MOVQ, RSI, Address(RBX, CLASS_MEMBERS_OFFSET));
        x64->op(MOVQ, RAX, Address(RSI, RBTREE_ROOT_OFFSET));
        x64->op(LEA, RDI, Address(RSP, 0));  // NOTE: only the index part is present of the Item

        x64->op(CALL, remove_label);
        x64->op(MOVQ, Address(RSI, RBTREE_ROOT_OFFSET), RBX);

        key_arg_ts.store(Storage(STACK), Storage(), x64);
        pivot->ts.store(Storage(STACK), Storage(), x64);
        
        return Storage();
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
        item_ts = match[0].unprefix(weakref_type).reprefix(map_type, item_type);
        
        key_arg_ts = key_ts;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        return check_arguments(args, kwargs, {
            { "key", &key_arg_ts, scope, &key }
        });
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = pivot->precompile(preferred) | key->precompile(preferred);
        return clob | RAX | RCX | RDX | RSI | RDI;
    }

    virtual Storage postresult(X64 *x64) {
        int key_size = key_ts.measure_stack();  // in an Item it's rounded up

        Label ok;
        x64->op(CMPQ, RDI, RBNODE_NIL);
        x64->op(JNE, ok);

        x64->runtime->die("Map missing!");  // TODO

        x64->code_label(ok);
        
        return Storage(MEMORY, Address(RSI, RDI, RBNODE_VALUE_OFFSET + key_size));
    }

    virtual Storage compile(X64 *x64) {
        int key_stack_size = key_arg_ts.measure_stack();
        
        pivot->compile_and_store(x64, Storage(STACK));
        key->compile_and_store(x64, Storage(STACK));
        
        Label has_label = x64->once->compile(compile_rbtree_has, item_ts);

        x64->op(MOVQ, RBX, Address(RSP, key_stack_size));
        x64->op(MOVQ, RSI, Address(RBX, CLASS_MEMBERS_OFFSET));
        x64->op(MOVQ, RAX, Address(RSI, RBTREE_ROOT_OFFSET));
        x64->op(LEA, RDI, Address(RSP, 0));

        x64->op(CALL, has_label);  // RDI is the index of the found item, or NIL
        
        key_arg_ts.store(Storage(STACK), Storage(), x64);
        pivot->ts.store(Storage(STACK), Storage(), x64);
        
        return postresult(x64);
    }
};


// Weak map helpers

static void compile_process_fcb(Label label, TypeSpec item_ts, X64 *x64) {
    x64->code_label_local(label, "xy_weakmap_callback");
    // RAX - fcb, RCX - payload1, RDX - payload2
    // We may clobber all registers

    std::stringstream ss;
    ss << item_ts << " callback";
    x64->runtime->log(ss.str().c_str());  // "WeakMap callback.");
    
    Label remove_label = x64->once->compile(compile_rbtree_remove, item_ts);

    x64->op(MOVQ, RSI, Address(RCX, 0));  // load current rbtree ref
    x64->op(MOVQ, RAX, Address(RSI, RBTREE_ROOT_OFFSET));
    x64->op(LEA, RDI, Address(RSI, RDX, RBNODE_VALUE_OFFSET));

    x64->op(CALL, remove_label);
    
    x64->op(MOVQ, Address(RSI, RBTREE_ROOT_OFFSET), RBX);
    
    x64->op(RET);
}


static void alloc_fcb(TypeSpec item_ts, Address alias_addr, X64 *x64) {
    // Allocate and push a FCB pointer. We may clobber RAX, RBX, RCX, RDX.
    Label callback_label = x64->once->compile(compile_process_fcb, item_ts);
    
    x64->op(MOVQ, RAX, Address(RSP, 0));  // heap
    x64->op(LEARIP, RBX, callback_label);  // callback
    x64->op(MOVQ, RCX, alias_addr);  // payload1, the rbtree ref address
    x64->op(MOVQ, RDX, RDI);  // payload2, the rbnode index
    x64->op(CALL, x64->runtime->alloc_fcb_label);
    x64->op(PUSHQ, RAX);
}


// WeakValueMap

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

    virtual void prevalue(Address alias_addr, X64 *x64) {
        alloc_fcb(item_ts, alias_addr, x64);
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


// WeakIndexMap

TypeMatch &wimatch(TypeMatch &match) {
    static TypeMatch tm;
    tm[0] = typesubst(SAMEID_WEAKANCHOR_SAME2_MAP_WEAKREF_TS, match);
    tm[1] = match[1].prefix(weakanchor_type);
    tm[2] = match[2];
    tm[3] = NO_TS;
    return tm;
}


class WeakIndexMapAddValue: public MapAddValue {
public:
    WeakIndexMapAddValue(Value *l, TypeMatch &match)
        :MapAddValue(l, wimatch(match)) {
        key_arg_ts = key_ts.reprefix(weakanchor_type, weakref_type);
    }

    virtual void prekey(Address alias_addr, X64 *x64) {
        alloc_fcb(item_ts, alias_addr, x64);
    }
};


class WeakIndexMapRemoveValue: public MapRemoveValue {
public:
    WeakIndexMapRemoveValue(Value *l, TypeMatch &match)
        :MapRemoveValue(l, wimatch(match)) {
        key_arg_ts = key_ts.reprefix(weakanchor_type, weakref_type);
    }
};


class WeakIndexMapIndexValue: public MapIndexValue {
public:
    WeakIndexMapIndexValue(Value *l, TypeMatch &match)
        :MapIndexValue(l, wimatch(match)) {
        key_arg_ts = key_ts.reprefix(weakanchor_type, weakref_type);
    }
};


// WeakSet

TypeMatch &wsmatch(TypeMatch &match) {
    static TypeMatch tm;
    tm[0] = typesubst(SAMEID_WEAKANCHOR_VOID_MAP_WEAKREF_TS, match);
    tm[1] = match[1].prefix(weakanchor_type);
    tm[2] = VOID_TS;
    tm[3] = NO_TS;
    return tm;
}


class WeakSetAddValue: public MapAddValue {
public:
    WeakSetAddValue(Value *l, TypeMatch &match)
        :MapAddValue(l, wsmatch(match)) {
        key_arg_ts = key_ts.reprefix(weakanchor_type, weakref_type);
        value_arg_ts = NO_TS;
    }

    virtual void prekey(Address alias_addr, X64 *x64) {
        alloc_fcb(item_ts, alias_addr, x64);
    }
};


class WeakSetRemoveValue: public MapRemoveValue {
public:
    WeakSetRemoveValue(Value *l, TypeMatch &match)
        :MapRemoveValue(l, wsmatch(match)) {
        key_arg_ts = key_ts.reprefix(weakanchor_type, weakref_type);
    }
};


class WeakSetIndexValue: public MapIndexValue {
public:
    WeakSetIndexValue(Value *l, TypeMatch &match)
        :MapIndexValue(l, wsmatch(match)) {
        key_arg_ts = key_ts.reprefix(weakanchor_type, weakref_type);
        ts = BOOLEAN_TS;
    }

    virtual Storage postresult(X64 *x64) {
        x64->op(CMPQ, RDI, RBNODE_NIL);
        
        return Storage(FLAGS, SETNE);
    }
};
