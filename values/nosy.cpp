
int rbtree_elem_nosy_offset(TypeSpec elem_ts) {
    if (elem_ts[0] == nosyvalue_type)
        return 0;
    else if (elem_ts[0] == item_type) {
        TypeMatch tm = elem_ts.match();
        
        if (tm[1][0] == nosyvalue_type)
            return 0;
        else if (tm[2][0] == nosyvalue_type)
            return tm[1].measure_stack();
        else
            throw INTERNAL_ERROR;
    }
    else
        throw INTERNAL_ERROR;
}

TypeSpec rbtree_elem_nosy_heap_ts(TypeSpec elem_ts) {
    if (elem_ts[0] == nosyvalue_type)
        return elem_ts.unprefix(nosyvalue_type);
    else if (elem_ts[0] == item_type) {
        TypeMatch tm = elem_ts.match();
        
        if (tm[1][0] == nosyvalue_type)
            return tm[1].unprefix(nosyvalue_type);
        else if (tm[2][0] == nosyvalue_type)
            return tm[2].unprefix(nosyvalue_type);
        else
            throw INTERNAL_ERROR;
    }
    else
        throw INTERNAL_ERROR;
}

void rbtree_fcb_action(Label action_label, TypeSpec elem_ts, X64 *x64) {
    // R10 - callback address, R11 - nosycontainer address
    int noffset = rbtree_elem_nosy_offset(elem_ts);
    Label elem_check, elem_loop;
    
    x64->op(MOVQ, RAX, Address(R11, 0));
    x64->op(MOVQ, RCX, Address(RAX, RBTREE_FIRST_OFFSET));
    x64->op(JMP, elem_check);
    
    x64->code_label(elem_loop);
    x64->op(PUSHQ, Address(RAX, RCX, RBNODE_VALUE_OFFSET + noffset));  // object
    x64->op(PUSHQ, R10);  // callback
    x64->op(PUSHQ, R11);  // payload1
    x64->op(PUSHQ, RCX);  // payload2
    
    x64->op(CALL, action_label);  // clobbers all, returns nothing
    
    x64->op(POPQ, RCX);
    x64->op(POPQ, R11);
    x64->op(POPQ, R10);
    x64->op(POPQ, RAX);
    
    x64->op(MOVQ, RAX, Address(R11, 0));
    x64->op(MOVQ, RCX, Address(RAX, RCX, RBNODE_NEXT_OFFSET));
    
    x64->code_label(elem_check);
    x64->op(CMPQ, RCX, RBNODE_NIL);
    x64->op(JNE, elem_loop);
}


void alloc_nosycontainer(TypeSpec member_ts, X64 *x64) {
    TypeSpec ts = member_ts.prefix(nosycontainer_type);
    Label finalizer_label = ts.get_finalizer_label(x64);
    int member_size = member_ts.measure_stack();  // type parameter
    
    x64->op(PUSHQ, member_size);
    x64->op(LEA, R10, Address(finalizer_label, 0));
    x64->op(PUSHQ, R10);
    x64->runtime->heap_alloc();  // clobbers all
    x64->op(ADDQ, RSP, 2 * ADDRESS_SIZE);

    // Ref to NosyContainer in RAX
}


void compile_nosycontainer_clone(Label label, TypeSpec member_ts, X64 *x64) {
    // This is only called for rbtree members, because Weakref does not clone
    // RAX - NosyContainer Ref
    // Return a cloned Ref
    TypeSpec container_heap_ts = member_ts.prefix(nosycontainer_type);
    TypeSpec rbtree_heap_ts = member_ts.unprefix(ref_type);
    TypeSpec elem_ts = rbtree_heap_ts.unprefix(rbtree_type);
    
    Label callback_label = x64->once->compile(compile_rbtree_nosy_callback, elem_ts);
    Label clone_label = x64->once->compile(compile_rbtree_clone, elem_ts);
    
    x64->code_label_local(label, member_ts.symbolize() + "_nosycontainer_clone");
    x64->runtime->log("XXX nosycontainer clone");

    x64->op(MOVQ, R10, Address(RAX, NOSYCONTAINER_MEMBER_OFFSET));  // Rbtree ref
    rbtree_heap_ts.incref(R10, x64);
    x64->op(PUSHQ, R10);
    
    container_heap_ts.decref(RAX, x64);
    
    alloc_nosycontainer(member_ts, x64);  // clobbers all
    
    x64->op(XCHGQ, RAX, Address(RSP, 0));  // push new nosy, pop old rbtree
    
    // Cloning the rbtree means that only the clone will be managed by FCB-s, so the
    // original must be referred by an iterator that increased all refcounts to make
    // sure they continue to point to valid objects until the end of the iteration.
    x64->op(CALL, clone_label);
    
    x64->op(POPQ, RBX);
    member_ts.create(Storage(REGISTER, RAX), Storage(MEMORY, Address(RBX, NOSYCONTAINER_MEMBER_OFFSET)), x64);
    x64->op(PUSHQ, RBX);

    x64->op(LEA, R10, Address(callback_label, 0));
    x64->op(LEA, R11, Address(RBX, NOSYCONTAINER_MEMBER_OFFSET));
    rbtree_fcb_action(x64->runtime->fcb_alloc_label, elem_ts, x64);
    
    x64->op(POPQ, RAX);
    x64->op(RET);
}


void compile_weakref_nosy_callback(Label label, X64 *x64) {
    x64->code_label_local(label, "weakref_nosy_callback");
    x64->runtime->log("Weakref Nosy callback.");

    x64->op(MOVQ, RCX, Address(RSP, ADDRESS_SIZE * 2));  // payload1 arg (nosy address)
    x64->op(MOVQ, Address(RCX, NOSYCONTAINER_MEMBER_OFFSET + NOSYVALUE_RAW_OFFSET), 0);
        
    x64->op(RET);
}


class NosyContainerMemberValue: public Value {
public:
    TypeSpec heap_ts;
    std::unique_ptr<Value> pivot;
    Unborrow *unborrow;

    NosyContainerMemberValue(Value *p, TypeSpec member_ts, Scope *scope)
        :Value(member_ts) {
        pivot.reset(p);
        heap_ts = member_ts.prefix(nosycontainer_type);
        
        unborrow = new Unborrow(heap_ts);
        scope->add(unborrow);
    }

    virtual Regs precompile(Regs preferred) {
        return pivot->precompile(preferred) | Regs(RAX);
    }

    virtual Storage compile(X64 *x64) {
        Storage s = pivot->compile(x64);
        
        switch (s.where) {
        case REGISTER:
            x64->op(MOVQ, unborrow->get_address(), s.reg);
            return Storage(MEMORY, Address(s.reg, NOSYCONTAINER_MEMBER_OFFSET));
        case MEMORY:
            x64->op(MOVQ, RAX, s.address);
            heap_ts.incref(RAX, x64);
            x64->op(MOVQ, unborrow->get_address(), RAX);
            return Storage(MEMORY, Address(RAX, NOSYCONTAINER_MEMBER_OFFSET));
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class NosyCowContainerMemberValue: public Value {
public:
    TypeSpec heap_ts;
    std::unique_ptr<Value> pivot;
    Unborrow *unborrow;

    NosyCowContainerMemberValue(Value *p, TypeSpec member_ts, Scope *scope)
        :Value(member_ts) {
        pivot.reset(p);
        heap_ts = member_ts.prefix(nosycontainer_type);
        
        unborrow = new Unborrow(heap_ts);
        scope->add(unborrow);
    }

    virtual Regs precompile(Regs preferred) {
        return pivot->precompile(preferred) | Regs::all();
    }

    virtual Storage compile(X64 *x64) {
        Label clone_label = x64->once->compile(compile_nosycontainer_clone, ts);

        pivot->compile_and_store(x64, Storage(ALISTACK));

        container_cow(clone_label, Address(RSP, 0), x64);  // clobbers all, returns RAX

        x64->op(ADDQ, RSP, ALIAS_SIZE);
        
        heap_ts.incref(RAX, x64);
        x64->op(MOVQ, unborrow->get_address(), RAX);
        return Storage(MEMORY, Address(RAX, NOSYCONTAINER_MEMBER_OFFSET));
    }
};


class WeakContainerValue: public Value {
public:
    TypeSpec member_ts;
    std::unique_ptr<Value> member_value;

    WeakContainerValue(Value *pivot, TypeSpec mts, TypeSpec cts)
        :Value(cts) {
        member_ts = mts;
        member_value.reset(pivot);
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        return member_value->check(args, kwargs, scope);  // for the Rbtree initializers
    }
    
    virtual Regs precompile(Regs preferred) {
        member_value->precompile(preferred);
        return Regs::all();
    }

    virtual Storage compile(X64 *x64) {
        member_value->compile_and_store(x64, Storage(STACK));

        alloc_nosycontainer(member_ts, x64);  // clobbers all

        member_ts.create(Storage(STACK), Storage(MEMORY, Address(RAX, NOSYCONTAINER_MEMBER_OFFSET)), x64);
        
        // Must set up the FCB-s
        TypeSpec elem_ts = member_ts.unprefix(ref_type).unprefix(rbtree_type);
        Label callback_label = x64->once->compile(compile_rbtree_nosy_callback, elem_ts);
        
        x64->op(PUSHQ, RAX);
        x64->op(LEA, R10, Address(callback_label, 0));
        x64->op(LEA, R11, Address(RAX, NOSYCONTAINER_MEMBER_OFFSET));
        rbtree_fcb_action(x64->runtime->fcb_alloc_label, elem_ts, x64);
        
        x64->op(POPQ, RAX);
        return Storage(REGISTER, RAX);
    }
};


class WeakrefToValue: public Value {
public:
    std::unique_ptr<Value> target;
    TypeSpec heap_ts;

    WeakrefToValue(TypeSpec hts)
        :Value(hts.prefix(weakref_type)) {
        heap_ts = hts;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        TypeSpec arg_ts = heap_ts.prefix(ptr_type);
        
        return check_arguments(args, kwargs, {
            { "target", &arg_ts, scope, &target }
        });
    }
    
    virtual Regs precompile(Regs preferred) {
        target->precompile(preferred);
        return Regs::all();
    }

    virtual Storage compile(X64 *x64) {
        Label callback_label = x64->once->compile(compile_weakref_nosy_callback);
        
        target->compile_and_store(x64, Storage(STACK));  // object address
        
        x64->op(LEA, R10, Address(callback_label, 0));
        x64->op(PUSHQ, R10);  // callback address

        alloc_nosycontainer(heap_ts.prefix(nosyvalue_type), x64);
        x64->op(PUSHQ, RAX);  // nosy container address as payload1
        
        x64->op(PUSHQ, 0);  // payload2
        
        x64->op(CALL, x64->runtime->fcb_alloc_label);  // clobbers all, returns nothing
        
        x64->op(POPQ, RBX);
        x64->op(POPQ, RBX);  // nosy address
        x64->op(POPQ, RCX);
        x64->op(POPQ, RCX);  // object address

        x64->op(MOVQ, Address(RBX, NOSYCONTAINER_MEMBER_OFFSET + NOSYVALUE_RAW_OFFSET), RCX);

        heap_ts.decref(RCX, x64);
        
        return Storage(REGISTER, RBX);
    }
};


class WeakrefDeadMatcherValue: public GenericValue, public Raiser {
public:
    TypeSpec param_heap_ts;

    WeakrefDeadMatcherValue(Value *p, TypeMatch &match)
        :GenericValue(NO_TS, VOID_TS, p) {
        param_heap_ts = match[1];
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!check_raise(match_unmatched_exception_type, scope))
            return false;
        
        return GenericValue::check(args, kwargs, scope);
    }

    virtual Regs precompile(Regs preferred) {
        return left->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        TypeSpec nosy_heap_ts = param_heap_ts.prefix(nosyvalue_type).prefix(nosycontainer_type);
        Label ok;
        left->compile_and_store(x64, Storage(STACK));
        
        x64->op(POPQ, R10);
        x64->op(MOVQ, R11, Address(R10, NOSYCONTAINER_MEMBER_OFFSET + NOSYVALUE_RAW_OFFSET));
        nosy_heap_ts.decref(R10, x64);
        x64->op(CMPQ, R11, 0);
        x64->op(JE, ok);

        // popped
        raise("UNMATCHED", x64);
                
        x64->code_label(ok);
        return Storage();
    }
};


class WeakrefLiveMatcherValue: public GenericValue, public Raiser {
public:
    TypeSpec param_heap_ts;
    
    WeakrefLiveMatcherValue(Value *p, TypeMatch &match)
        :GenericValue(NO_TS, match[1].prefix(ptr_type), p) {
        param_heap_ts = match[1];
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!check_raise(match_unmatched_exception_type, scope))
            return false;
        
        return GenericValue::check(args, kwargs, scope);
    }

    virtual Regs precompile(Regs preferred) {
        return left->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        TypeSpec nosy_heap_ts = param_heap_ts.prefix(nosyvalue_type).prefix(nosycontainer_type);
        Label ok;
        left->compile_and_store(x64, Storage(STACK));
        
        x64->op(POPQ, R10);
        x64->op(MOVQ, R11, Address(R10, NOSYCONTAINER_MEMBER_OFFSET + NOSYVALUE_RAW_OFFSET));
        nosy_heap_ts.decref(R10, x64);
        x64->op(CMPQ, R11, 0);
        x64->op(JNE, ok);
        
        // popped
        raise("UNMATCHED", x64);
                
        x64->code_label(ok);
        param_heap_ts.incref(R11, x64);
        x64->op(PUSHQ, R11);
        
        return Storage(STACK);
    }
};
