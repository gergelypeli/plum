
// Type manipulations for unified case handling

int nosyvalue_offset(TypeSpec elem_ts) {
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


TypeSpec nosyvalue_heap_ts(TypeSpec elem_ts) {
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


TypeSpec denosy_ts(TypeSpec ts) {
    if (ts[0] == nosyvalue_type)
        return ts.reprefix(nosyvalue_type, ptr_type);
    else
        return ts;
}


TypeSpec item_key_ts(TypeSpec ts) {
    if (ts[0] == item_type) {
        TypeMatch tm = ts.match();
        
        return tm[1];
    }
    else
        return ts;
}


TypeSpec item_value_ts(TypeSpec ts) {
    if (ts[0] == item_type) {
        TypeMatch tm = ts.match();
        
        return tm[2];
    }
    else
        return ts;
}


// Helper functions

void nosytree_fcb_action(Label action_label, TypeSpec elem_ts, X64 *x64) {
    // R10 - callback address, R11 - nosytree address
    int noffset = nosyvalue_offset(elem_ts);
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


void compile_nosytree_callback(Label label, TypeSpec elem_ts, X64 *x64) {
    Label remove_label = x64->once->compile(compile_rbtree_remove, elem_ts);

    x64->code_label_local(label, elem_ts.prefix(nosytree_type).symbolize("callback"));
    
    std::stringstream ss;
    ss << elem_ts << " Nosytree callback";
    x64->runtime->log(ss.str());
    
    // arguments: payload1, payload2
    // We may clobber all registers

    x64->op(MOVQ, SELFX, Address(RSP, ADDRESS_SIZE * 2));  // payload1 arg (rbtree ref address)
    x64->op(MOVQ, SELFX, Address(SELFX, 0));  // load current rbtree ref
    x64->op(MOVQ, ROOTX, Address(SELFX, RBTREE_ROOT_OFFSET));
    x64->op(MOVQ, KEYX, Address(RSP, ADDRESS_SIZE));  // payload2 arg (elem index)
    x64->op(LEA, KEYX, Address(SELFX, KEYX, RBNODE_VALUE_OFFSET));  // use the elem key

    x64->op(CALL, remove_label);
    x64->op(MOVQ, Address(SELFX, RBTREE_ROOT_OFFSET), R10);
    
    x64->op(RET);
}


void nosy_postadd(TypeSpec elem_ts, Storage ref_storage, X64 *x64) {
    // Add an FCB to the newly added rbtree elem, and decreases the reference count.
    // ref_storage - the storage of the rbtree ref
    // SELFX/KEYX - points to the newly added elem
    // Clobbers all registers.
    Label callback_label = x64->once->compile(compile_nosytree_callback, elem_ts);
    TypeSpec heap_ts = nosyvalue_heap_ts(elem_ts);
    int noffset = nosyvalue_offset(elem_ts);
    
    // This is the storage of the rbtree ref as the operation's pivot value returned.
    // As it was an Lvalue pivot, it is either MEMORY[RBP+x] or ALIAS[RBP+x].
    // The rbtree ref of a nosy structure is on the heap, so it must be the second one.
    if (ref_storage.where != ALIAS || ref_storage.value != 0)
        throw INTERNAL_ERROR;
        
    x64->op(MOVQ, R11, ref_storage.address);
    
    x64->op(PUSHQ, Address(SELFX, KEYX, RBNODE_VALUE_OFFSET + noffset));  // object
    x64->op(LEA, R10, Address(callback_label, 0));
    x64->op(PUSHQ, R10);  // callback
    x64->op(PUSHQ, R11);  // payload1
    x64->op(PUSHQ, KEYX);  // payload2

    x64->op(CALL, x64->runtime->fcb_alloc_label);  // clobbers all
    x64->op(ADDQ, RSP, 3 * ADDRESS_SIZE);
    x64->op(POPQ, R10);  // object
    
    heap_ts.decref(R10, x64);
}


void nosy_postremove(TypeSpec elem_ts, Storage ref_storage, X64 *x64) {
    // Remove an FCB to the newly removed rbtree elem.
    // ref_storage - the storage of the rbtree ref
    // SELFX/KEYX - points to the just removed elem
    // Clobbers all registers.
    Label callback_label = x64->once->compile(compile_nosytree_callback, elem_ts);
    int noffset = nosyvalue_offset(elem_ts);

    // See above
    if (ref_storage.where != ALIAS || ref_storage.value != 0)
        throw INTERNAL_ERROR;
        
    x64->op(MOVQ, R11, ref_storage.address);
    
    x64->op(PUSHQ, Address(SELFX, KEYX, RBNODE_VALUE_OFFSET + noffset));  // object
    x64->op(LEA, R10, Address(callback_label, 0));
    x64->op(PUSHQ, R10);  // callback
    x64->op(PUSHQ, R11);  // payload1
    x64->op(PUSHQ, KEYX);  // payload2

    x64->op(CALL, x64->runtime->fcb_free_label);  // clobbers all
    x64->op(ADDQ, RSP, 4 * ADDRESS_SIZE);
}


// Nosytree basics

void compile_nosytree_finalizer(Label label, TypeSpec elem_ts, X64 *x64) {
    x64->code_label_local(label, elem_ts.prefix(nosytree_type).symbolize("finalizer"));
    x64->runtime->log("Nosytree finalized.");

    x64->op(MOVQ, RAX, Address(RSP, ADDRESS_SIZE));  // pointer arg
    
    Label callback_label = x64->once->compile(compile_nosytree_callback, elem_ts);
        
    x64->op(LEA, R10, Address(callback_label, 0));
    x64->op(LEA, R11, Address(RAX, NOSYTREE_MEMBER_OFFSET));
    nosytree_fcb_action(x64->runtime->fcb_free_label, elem_ts, x64);  // clobbers all
        
    // If an iterator is referring to this rbtree, it must have increased all
    // reference counts to make sure they continue to point to a valid object.
    // Once we destroy the Rbtree Ref, only iterator(s) will be the owner(s)
    // of this rbtree.

    x64->op(MOVQ, RAX, Address(RSP, ADDRESS_SIZE));
    
    TypeSpec member_ts = elem_ts.prefix(rbtree_type).prefix(ref_type);
    member_ts.destroy(Storage(MEMORY, Address(RAX, NOSYTREE_MEMBER_OFFSET)), x64);
    
    x64->op(RET);
}


void alloc_nosytree(TypeSpec elem_ts, X64 *x64) {
    Label finalizer_label = x64->once->compile(compile_nosytree_finalizer, elem_ts);

    x64->op(PUSHQ, REFERENCE_SIZE);
    x64->op(LEA, R10, Address(finalizer_label, 0));
    x64->op(PUSHQ, R10);
    x64->runtime->heap_alloc();  // clobbers all
    x64->op(ADDQ, RSP, 2 * ADDRESS_SIZE);

    // Ref to Nosytree in RAX
}


void compile_nosytree_clone(Label label, TypeSpec elem_ts, X64 *x64) {
    // RAX - Nosytree Ref
    // Return a cloned Ref
    TypeSpec rbtree_heap_ts = elem_ts.prefix(rbtree_type);
    TypeSpec member_ts = rbtree_heap_ts.prefix(ref_type);
    TypeSpec container_heap_ts = member_ts.prefix(nosytree_type);
    
    Label callback_label = x64->once->compile(compile_nosytree_callback, elem_ts);
    Label clone_label = x64->once->compile(compile_rbtree_clone, elem_ts);
    
    x64->code_label_local(label, elem_ts.prefix(nosytree_type).symbolize("clone"));
    x64->runtime->log("XXX Nosytree clone");

    x64->op(MOVQ, R10, Address(RAX, NOSYTREE_MEMBER_OFFSET));  // Rbtree ref
    rbtree_heap_ts.incref(R10, x64);
    x64->op(PUSHQ, R10);
    
    container_heap_ts.decref(RAX, x64);
    
    alloc_nosytree(elem_ts, x64);  // clobbers all
    
    x64->op(XCHGQ, RAX, Address(RSP, 0));  // push new nosy, pop old rbtree
    
    // Cloning the rbtree means that only the clone will be managed by FCB-s, so the
    // original must be referred by an iterator that increased all refcounts to make
    // sure they continue to point to valid objects until the end of the iteration.
    x64->op(CALL, clone_label);
    
    x64->op(POPQ, RBX);
    member_ts.create(Storage(REGISTER, RAX), Storage(MEMORY, Address(RBX, NOSYTREE_MEMBER_OFFSET)), x64);
    x64->op(PUSHQ, RBX);

    x64->op(LEA, R10, Address(callback_label, 0));
    x64->op(LEA, R11, Address(RBX, NOSYTREE_MEMBER_OFFSET));
    nosytree_fcb_action(x64->runtime->fcb_alloc_label, elem_ts, x64);
    
    x64->op(POPQ, RAX);
    x64->op(RET);
}


// Internally used access to the Rbtree Ref inside the Nosytree Ref

class NosytreeMemberValue: public Value, public Aliaser {
public:
    TypeSpec elem_ts;
    std::unique_ptr<Value> pivot;
    Unborrow *unborrow;

    NosytreeMemberValue(Value *p, TypeSpec ets)
        :Value(ets.prefix(rbtree_type).prefix(ref_type)) {
        pivot.reset(p);
        elem_ts = ets;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (lvalue_needed)
            check_alias(scope);

        TypeSpec heap_ts = ts.prefix(nosytree_type);
        unborrow = new Unborrow(heap_ts);
        scope->add(unborrow);
        
        return Value::check(args, kwargs, scope);
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = pivot->precompile(preferred);
        
        if (lvalue_needed) {
            // Altering the member would clobber the heap vars
            clob = Regs::all() | Regs::heapvars();
        }
        else {
            clob = clob | Regs(RAX);
        }

        return clob;
    }

    virtual Storage compile(X64 *x64) {
        TypeSpec heap_ts = ts.prefix(nosytree_type);
        Register r;
        
        if (lvalue_needed) {
            Label clone_label = x64->once->compile(compile_nosytree_clone, elem_ts);

            Storage ps = pivot->compile_and_alias(x64, get_alias());

            container_cow(clone_label, ps, x64);  // leaves borrowed Ref in RAX
            r = RAX;

            heap_ts.incref(r, x64);
        }
        else {
            Storage s = pivot->compile(x64);
        
            switch (s.where) {
            case REGISTER:
                r = s.reg;
                break;
            case MEMORY:
                r = RAX;
                x64->op(MOVQ, r, s.address);
                heap_ts.incref(r, x64);
                break;
            default:
                throw INTERNAL_ERROR;
            }
        }

        x64->op(MOVQ, unborrow->get_address(), r);
        
        return Storage(MEMORY, Address(r, NOSYTREE_MEMBER_OFFSET));
    }
};


// Wraps a newly created Rbtree in a Nosytree

class WeaktreeValue: public Value {
public:
    TypeSpec elem_ts;
    std::unique_ptr<Value> member_value;

    WeaktreeValue(Value *pivot, TypeSpec ets, TypeSpec cts)
        :Value(cts) {
        elem_ts = ets;
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
        TypeSpec member_ts = elem_ts.prefix(rbtree_type).prefix(ref_type);
        Label callback_label = x64->once->compile(compile_nosytree_callback, elem_ts);

        member_value->compile_and_store(x64, Storage(STACK));

        alloc_nosytree(elem_ts, x64);  // clobbers all

        member_ts.create(Storage(STACK), Storage(MEMORY, Address(RAX, NOSYTREE_MEMBER_OFFSET)), x64);
        
        // Must set up the FCB-s
        x64->op(PUSHQ, RAX);
        
        x64->op(LEA, R10, Address(callback_label, 0));
        x64->op(LEA, R11, Address(RAX, NOSYTREE_MEMBER_OFFSET));
        nosytree_fcb_action(x64->runtime->fcb_alloc_label, elem_ts, x64);
        
        x64->op(POPQ, RAX);
        return Storage(REGISTER, RAX);
    }
};


// Common operations on Rbtree-s with some nosy elements

class NosyRbtreeLengthValue: public RbtreeLengthValue {
public:
    NosyRbtreeLengthValue(Value *l, TypeSpec elem_ts)
        :RbtreeLengthValue(l, elem_ts) {
    }
};


class NosyRbtreeAddValue: public RbtreeAddValue {
public:
    NosyRbtreeAddValue(Value *l, TypeSpec elem_ts)
        :RbtreeAddValue(l, elem_ts, denosy_ts(elem_ts)) {
    }

    virtual Regs precompile(Regs preferred) {
        return RbtreeAddValue::precompile(preferred) | Regs::all();
    }

    virtual Storage compile(X64 *x64) {
        Storage ps = RbtreeAddValue::compile(x64);
        
        // Using the ps+SELFX+KEYX
        nosy_postadd(elem_ts, ps, x64);  // clobbers all
        
        return ps;
    }
};


class NosyRbtreeAddItemValue: public RbtreeAddItemValue {
public:
    NosyRbtreeAddItemValue(Value *l, TypeSpec elem_ts)
        :RbtreeAddItemValue(l, item_key_ts(elem_ts), item_value_ts(elem_ts), denosy_ts(item_key_ts(elem_ts)), denosy_ts(item_value_ts(elem_ts))) {
    }

    virtual Regs precompile(Regs preferred) {
        return RbtreeAddItemValue::precompile(preferred) | Regs::all();
    }

    virtual Storage compile(X64 *x64) {
        Storage ps = RbtreeAddItemValue::compile(x64);
        
        // Using the ps+SELFX+KEYX
        nosy_postadd(elem_ts, ps, x64);  // clobbers all
        
        return ps;
    }
};


class NosyRbtreeRemoveValue: public RbtreeRemoveValue {
public:
    NosyRbtreeRemoveValue(Value *l, TypeSpec elem_ts)
        :RbtreeRemoveValue(l, elem_ts, denosy_ts(item_key_ts(elem_ts))) {
    }

    virtual Regs precompile(Regs preferred) {
        return RbtreeRemoveValue::precompile(preferred) | Regs::all();
    }

    virtual Storage compile(X64 *x64) {
        Storage ps = RbtreeRemoveValue::compile(x64);
        
        // Using the ps+SELFX+KEYX
        nosy_postremove(elem_ts, ps, x64);  // clobbers all
        
        return ps;
    }
};


class NosyRbtreeHasValue: public RbtreeHasValue {
public:
    NosyRbtreeHasValue(Value *l, TypeSpec elem_ts)
        :RbtreeHasValue(l, elem_ts, denosy_ts(item_key_ts(elem_ts))) {
    }
};


class NosyRbtreeIndexValue: public RbtreeIndexValue {
public:
    NosyRbtreeIndexValue(Value *l, TypeSpec elem_ts)
        :RbtreeIndexValue(l, item_key_ts(elem_ts), elem_ts, denosy_ts(item_key_ts(elem_ts)), denosy_ts(item_value_ts(elem_ts))) {
        // NOTE: We must not return an Lvalue to this NosyValue!
    }
};
