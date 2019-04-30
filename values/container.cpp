
TypeSpec container_elem_ts(TypeSpec ts, Type *container_type = NULL) {
    // FIXME: this won't work for ptr
    TypeSpec x = ts.rvalue();
    
    if (x[0] == ref_type)
        x = x.unprefix(ref_type);  // FIXME: until Rbtree exists
        
    x = x.unprefix(container_type);
    
    return x;
}


int container_elem_size(TypeSpec elem_ts) {
    // Note: this is like array and circularray, but unlike rbtree!
    return elem_ts.measure_elem();
}


void container_alloc(int header_size, int elem_size, int reservation_offset, Label finalizer_label, X64 *x64) {
    // R10 - reservation size
    x64->op(PUSHQ, R10);
    
    x64->op(IMUL3Q, R10, R10, elem_size);
    x64->op(ADDQ, R10, header_size);
    x64->op(LEA, R11, Address(finalizer_label, 0));
    
    x64->op(PUSHQ, R10);
    x64->op(PUSHQ, R11);
    x64->runtime->heap_alloc();  // clobbers all
    x64->op(ADDQ, RSP, 2 * ADDRESS_SIZE);
    
    x64->op(POPQ, Address(RAX, reservation_offset));
}


void container_realloc(int header_size, int elem_size, int reservation_offset, X64 *x64) {
    // RAX - array, R10 - new reservation
    x64->op(MOVQ, Address(RAX, reservation_offset), R10);
    x64->op(IMUL3Q, R10, R10, elem_size);
    x64->op(ADDQ, R10, header_size);
    
    x64->op(PUSHQ, RAX);
    x64->op(PUSHQ, R10);
    x64->runtime->heap_realloc();  // clobbers all
    x64->op(ADDQ, RSP, 2 * ADDRESS_SIZE);
}


void container_grow(int reservation_offset, int min_reservation, Label realloc_label, X64 *x64) {
    // RAX - array, R10 - new reservation
    // Double the reservation until it's enough (can be relaxed to 1.5 times, but not less)

    Label check, ok;
    x64->op(XCHGQ, R10, Address(RAX, reservation_offset));  // save desired value
    x64->op(CMPQ, R10, min_reservation);
    x64->op(JAE, check);
    x64->op(MOVQ, R10, min_reservation);
    
    x64->code_label(check);
    x64->op(CMPQ, R10, Address(RAX, reservation_offset));
    x64->op(JAE, ok);
    x64->op(SHLQ, R10, 1);
    x64->op(JMP, check);
    
    x64->code_label(ok);
    x64->op(CALL, realloc_label);  // clobbers all
}


void container_preappend2(int reservation_offset, int length_offset, Label grow_label, Storage ref_storage, X64 *x64) {
    // R10 - new addition. Returns the Ref in RAX.
    Label ok;

    x64->runtime->load_lvalue(RAX, R11, ref_storage);

    x64->op(ADDQ, R10, Address(RAX, length_offset));
    x64->op(CMPQ, R10, Address(RAX, reservation_offset));
    x64->op(JBE, ok);

    x64->op(CALL, grow_label);  // clobbers all

    x64->runtime->store_lvalue(RAX, R11, ref_storage);
    
    x64->code_label(ok);
}


void container_cow(Label clone_label, Storage ref_storage, X64 *x64) {
    // No runtime arguments. Returns the borrowed Ref in RAX
    Label end;
    
    x64->runtime->load_lvalue(RAX, R11, ref_storage);

    x64->runtime->oneref(RAX);
    x64->op(JE, end);
    
    // Cloning decreases the refcount of the passed RAX. Since we just loaded it
    // from memory, it effectively decreases the original's refcount. After the clone we get
    // a refcounted copy, which we immediately store back to the same location.
    // So the original lost one reference, and the copy has exactly one.
    // The RAX returned is a borrowed reference, and needs no decref later.
    x64->op(CALL, clone_label);  // clobbers all

    x64->runtime->store_lvalue(RAX, R11, ref_storage);
    
    x64->code_label(end);
}


class ContainerLengthValue: public GenericValue {
public:
    Register reg;
    TypeSpec heap_ts;
    int length_offset;
    
    ContainerLengthValue(Value *l, TypeMatch &match, TypeSpec hts, int lo)
        :GenericValue(NO_TS, INTEGER_TS, l) {
        reg = NOREG;
        heap_ts = hts;
        length_offset = lo;
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
            x64->op(MOVQ, ls.reg, Address(ls.reg, length_offset));
            return Storage(REGISTER, ls.reg);
        case STACK:
            x64->op(POPQ, RBX);
            x64->op(MOVQ, reg, Address(RBX, length_offset));
            heap_ts.decref(RBX, x64);
            return Storage(REGISTER, reg);
        case MEMORY:
            x64->op(MOVQ, reg, ls.address);
            x64->op(MOVQ, reg, Address(reg, length_offset));
            return Storage(REGISTER, reg);
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class ContainerIndexValue: public OptimizedOperationValue, public Raiser {
public:
    TypeSpec heap_ts;
    TypeSpec elem_ts;
    Unborrow *unborrow;
    int length_offset;
    int elems_offset;
    Storage value_storage;
    
    ContainerIndexValue(OperationType o, Value *pivot, TypeMatch &match, TypeSpec hts, int lo, int eo)
        :OptimizedOperationValue(o, INTEGER_TS, match[1].lvalue(), pivot,
        GPR_SUBSET, GPR_SUBSET
        ) {
        heap_ts = hts;
        elem_ts = match[1];
        length_offset = lo;
        elems_offset = eo;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!check_raise(lookup_exception_type, scope))
            return false;

        unborrow = new Unborrow(heap_ts);
        scope->add(unborrow);

        return OptimizedOperationValue::check(args, kwargs, scope);
    }

    virtual void fix_index(Register r, Register i, X64 *x64) {
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = OptimizedOperationValue::precompile(preferred);
        
        clob = clob | Regs(RAX) | Regs(RBX);
        
        if (lvalue_needed)
            clob = clob | Regs::heapvars();
        else {
            if (!(preferred & (Regs::heapvars() | Regs::relaxvars()))) {
                value_storage = ts.optimal_value_storage(preferred);
                clob = clob | value_storage.regs();
            }
        }

        return clob;
    }

    virtual Storage compile(X64 *x64) {
        int elem_size = container_elem_size(elem_ts);
    
        OptimizedOperationValue::subcompile(x64);

        Register r = (ls.where == REGISTER ? ls.reg : ls.where == MEMORY ? auxls.reg : throw INTERNAL_ERROR);
        Register i = (rs.where == REGISTER ? rs.reg : r == RAX ? RBX : RAX);

        switch (rs.where) {
        case CONSTANT:
            x64->op(MOVQ, i, rs.value);
            break;
        case REGISTER:
            break;
        case MEMORY:
            x64->op(MOVQ, i, rs.address);
            break;
        default:
            throw INTERNAL_ERROR;
        }
    
        switch (ls.where) {
        case REGISTER:
            break;
        case MEMORY:
            x64->op(MOVQ, r, ls.address);  // r may be the base of ls.address
            heap_ts.incref(r, x64);
            break;
        default:
            throw INTERNAL_ERROR;
        }

        x64->op(MOVQ, unborrow->get_address(), r);

        Label ok;
        x64->op(CMPQ, i, Address(r, length_offset));  // needs logical index
        x64->op(JB, ok);

        raise("NOT_FOUND", x64);

        x64->code_label(ok);
        fix_index(r, i, x64);  // turns logical index into physical
        Address addr = x64->runtime->make_address(r, i, elem_size, elems_offset);
        Storage t(MEMORY, addr);
        
        if (value_storage.where != NOWHERE)
            t = ts.store(t, value_storage, x64);
            
        return t;
    }
};


class ContainerEmptyValue: public GenericValue {
public:
    TypeSpec elem_ts;
    Once::TypedFunctionCompiler compile_alloc;
    
    ContainerEmptyValue(TypeSpec ts, Once::TypedFunctionCompiler ca)
        :GenericValue(NO_TS, ts, NULL) {
        elem_ts = container_elem_ts(ts);
        compile_alloc = ca;
    }

    virtual Regs precompile(Regs preferred) {
        return Regs::all();
    }

    virtual Storage compile(X64 *x64) {
        Label alloc_label = x64->once->compile(compile_alloc, elem_ts);

        x64->op(MOVQ, R10, 0);
        x64->op(CALL, alloc_label);  // clobbers all
        
        return Storage(REGISTER, RAX);
    }
};


class ContainerReservedValue: public GenericValue {
public:
    TypeSpec elem_ts;
    Once::TypedFunctionCompiler compile_alloc;
    
    ContainerReservedValue(TypeSpec ts, Once::TypedFunctionCompiler ca)
        :GenericValue(INTEGER_TS, ts, NULL) {
        elem_ts = container_elem_ts(ts);
        compile_alloc = ca;
    }

    virtual Regs precompile(Regs preferred) {
        right->precompile(preferred);
        return Regs::all();
    }

    virtual Storage compile(X64 *x64) {
        Label alloc_label = x64->once->compile(compile_alloc, elem_ts);

        right->compile_and_store(x64, Storage(REGISTER, R10));  // FIXME: this may be illegal

        x64->op(CALL, alloc_label);  // clobbers all
        
        return Storage(REGISTER, RAX);
    }
};


class ContainerAllValue: public Value {
public:
    std::unique_ptr<Value> fill_value;
    std::unique_ptr<Value> length_value;
    TypeSpec elem_ts;
    int length_offset;
    int elems_offset;
    Once::TypedFunctionCompiler compile_alloc;
    
    ContainerAllValue(TypeSpec ts, int lo, int eo, Once::TypedFunctionCompiler ca)
        :Value(ts) {
        elem_ts = container_elem_ts(ts);
        length_offset = lo;
        elems_offset = eo;
        compile_alloc = ca;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        ArgInfos infos = {
            { "fill", &elem_ts, scope, &fill_value },
            { "length", &INTEGER_TS, scope, &length_value }
        };
        
        if (!check_arguments(args, kwargs, infos))
            return false;

        return true;
    }

    virtual Regs precompile(Regs preferred) {
        fill_value->precompile(preferred);
        length_value->precompile(preferred);
        return Regs::all();
    }
    
    virtual Storage compile(X64 *x64) {
        Label alloc_label = x64->once->compile(compile_alloc, elem_ts);
        int elem_size = container_elem_size(elem_ts);

        fill_value->compile_and_store(x64, Storage(STACK));
        length_value->compile_and_store(x64, Storage(STACK));
        
        x64->op(MOVQ, R10, Address(RSP, 0));
        x64->op(CALL, alloc_label);  // clobbers all
        x64->op(POPQ, Address(RAX, length_offset));
        
        Label loop, check;
        x64->op(MOVQ, RCX, 0);
        x64->op(JMP, check);
        
        x64->code_label(loop);
        x64->op(IMUL3Q, RDX, RCX, elem_size);
        
        elem_ts.create(Storage(MEMORY, Address(RSP, 0)), Storage(MEMORY, Address(RAX, RDX, elems_offset)), x64);
        x64->op(INCQ, RCX);
        
        x64->code_label(check);
        x64->op(CMPQ, RCX, Address(RAX, length_offset));
        x64->op(JB, loop);
        
        elem_ts.store(Storage(STACK), Storage(), x64);
        
        return Storage(REGISTER, RAX);
    }
};


class ContainerInitializerValue: public Value {
public:
    TypeSpec elem_ts;
    std::vector<std::unique_ptr<Value>> elems;
    int length_offset;
    int elems_offset;
    Once::TypedFunctionCompiler compile_alloc;

    ContainerInitializerValue(TypeSpec ets, TypeSpec ts, int lo, int eo, Once::TypedFunctionCompiler ca)
        :Value(ts) {
        elem_ts = ets;
        length_offset = lo;
        elems_offset = eo;
        compile_alloc = ca;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
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
                std::cerr << "Container element is not " << elem_ts << ", but " << get_typespec(v) << "!\n";
                return false;
            }
            
            elems.push_back(std::unique_ptr<Value>(v));
        }
        
        return true;
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob;
        
        for (auto &elem : elems)
            clob = clob | elem->precompile(preferred);
            
        return Regs::all();
    }

    virtual Storage compile(X64 *x64) {
        Label alloc_label = x64->once->compile(compile_alloc, elem_ts);
        int elem_size = container_elem_size(elem_ts);
        int stack_size = elem_ts.measure_stack();
    
        x64->op(MOVQ, R10, elems.size());
        x64->op(CALL, alloc_label);  // clobbers all
        x64->op(MOVQ, Address(RAX, length_offset), elems.size());
        x64->op(PUSHQ, RAX);
        
        unsigned offset = 0;
        
        for (auto &elem : elems) {
            elem->compile_and_store(x64, Storage(STACK));
            x64->op(MOVQ, RAX, Address(RSP, stack_size));
            Storage t(MEMORY, Address(RAX, elems_offset + offset));
            
            elem_ts.create(Storage(STACK), t, x64);
            offset += elem_size;
        }
        
        return Storage(STACK);
    }
};


class ContainerEmptiableValue: public GenericValue, public Raiser {
public:
    ContainerEmptiableValue(TypeSpec arg_ts, TypeSpec res_ts, Value *pivot)
        :GenericValue(arg_ts, res_ts, pivot) {
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!check_raise(container_empty_exception_type, scope))
            return false;
        
        return GenericValue::check(args, kwargs, scope);
    }
};


class ContainerPushValue: public GenericValue, public Aliaser {
public:
    TypeSpec elem_ts;
    int reservation_offset;
    int length_offset;
    int elems_offset;
    Once::TypedFunctionCompiler compile_clone, compile_grow;
    
    ContainerPushValue(Value *l, TypeMatch &match, int ro, int lo, int eo, Once::TypedFunctionCompiler cc, Once::TypedFunctionCompiler cg)
        :GenericValue(match[1], match[0], l) {
        elem_ts = match[1];
        reservation_offset = ro;
        length_offset = lo;
        elems_offset = eo;
        compile_clone = cc;
        compile_grow = cg;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        check_alias(scope);
        
        return GenericValue::check(args, kwargs, scope);
    }
    
    virtual Regs precompile(Regs preferred) {
        left->precompile(preferred);
        right->precompile(preferred);
        return Regs::all();
    }

    virtual void fix_index(Register r, Register i, X64 *x64) {
    }

    virtual Storage compile(X64 *x64) {
        int elem_size = container_elem_size(elem_ts);
        Label clone_label = x64->once->compile(compile_clone, elem_ts);
        Label grow_label = x64->once->compile(compile_grow, elem_ts);

        ls = left->compile_and_alias(x64, get_alias());
        right->compile_and_store(x64, Storage(STACK));

        container_cow(clone_label, ls, x64);  // leaves borrowed Ref in RAX

        x64->op(MOVQ, R10, 1);
        container_preappend2(reservation_offset, length_offset, grow_label, ls, x64);
        // RAX - Ref
        
        x64->op(MOVQ, R10, Address(RAX, length_offset));
        x64->op(INCQ, Address(RAX, length_offset));

        // R10 contains the index of the newly created element
        fix_index(RAX, R10, x64);
        
        x64->op(IMUL3Q, R10, R10, elem_size);
        x64->op(ADDQ, RAX, R10);
        
        elem_ts.create(Storage(STACK), Storage(MEMORY, Address(RAX, elems_offset)), x64);
        
        return ls;
    }
};


class ContainerPopValue: public ContainerEmptiableValue, public Aliaser {
public:
    TypeSpec elem_ts;
    TypeSpec heap_ts;
    int length_offset;
    int elems_offset;
    Once::TypedFunctionCompiler compile_clone;
    
    ContainerPopValue(Value *l, TypeMatch &match, TypeSpec hts, int lo, int eo, Once::TypedFunctionCompiler cc)
        :ContainerEmptiableValue(NO_TS, match[1], l) {
        elem_ts = match[1];
        heap_ts = hts;
        length_offset = lo;
        elems_offset = eo;
        compile_clone = cc;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        check_alias(scope);
        
        return ContainerEmptiableValue::check(args, kwargs, scope);
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = left->precompile(preferred);
        return clob | RAX | RCX;
    }

    virtual void fix_index(Register r, Register i, X64 *x64) {
    }

    virtual Storage compile(X64 *x64) {
        int elem_size = container_elem_size(elem_ts);
        Label clone_label = x64->once->compile(compile_clone, elem_ts);
        Label ok;
        
        ls = left->compile_and_alias(x64, get_alias());

        container_cow(clone_label, ls, x64);  // leaves borrowed Ref in RAX
        
        // Get rid of pivot (technically, it must be a nop now)
        left->ts.store(ls, Storage(), x64);
        
        x64->op(CMPQ, Address(RAX, length_offset), 0);
        x64->op(JNE, ok);

        // popped pivot
        raise("CONTAINER_EMPTY", x64);
        
        x64->code_label(ok);
        x64->op(DECQ, Address(RAX, length_offset));
        x64->op(MOVQ, R10, Address(RAX, length_offset));

        // R10 contains the index of the newly removed element
        fix_index(RAX, R10, x64);

        x64->op(IMUL3Q, RCX, R10, elem_size);
        
        // TODO: optimize this move!
        elem_ts.store(Storage(MEMORY, Address(RAX, RCX, elems_offset)), Storage(STACK), x64);
        elem_ts.destroy(Storage(MEMORY, Address(RAX, RCX, elems_offset)), x64);
        
        return Storage(STACK);
    }
};


// Iteration

class ContainerIterValue: public SimpleRecordValue {
public:
    ContainerIterValue(TypeSpec t, Value *l)
        :SimpleRecordValue(t, l) {
    }

    virtual Storage compile(X64 *x64) {
        x64->op(PUSHQ, 0);

        left->compile_and_store(x64, Storage(STACK));
        
        return Storage(STACK);
    }
};


// Array iterator next methods

class ContainerNextValue: public GenericValue, public Raiser {
public:
    Regs clob;
    int length_offset;
    bool is_down;
    TypeSpec elem_ts;
    
    ContainerNextValue(TypeSpec ts, TypeSpec ets, Value *l, int lo, bool d)
        :GenericValue(NO_TS, ts, l) {
        length_offset = lo;
        is_down = d;
        elem_ts = ets;
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

        clob.reserve(4);
        
        return clob;
    }

    virtual Storage postprocess(Register r, Register i, X64 *x64) {
        throw INTERNAL_ERROR;
    }

    virtual Storage compile(X64 *x64) {
        ls = left->compile(x64);  // iterator
        Register r = (clob & ~ls.regs()).get_any();
        Register i = (clob & ~ls.regs() & ~Regs(r)).get_any();
        Label ok;
        
        switch (ls.where) {
        case MEMORY:
            //std::cerr << "Compiling itemiter with reg=" << reg << " ls=" << ls << "\n";
            x64->op(MOVQ, i, ls.address + REFERENCE_SIZE);  // value
            x64->op(MOVQ, r, ls.address); // array reference without incref
            x64->op(CMPQ, i, Address(r, length_offset));
            x64->op(JNE, ok);

            // MEMORY arg
            raise("ITERATOR_DONE", x64);
            
            x64->code_label(ok);
            x64->op(is_down ? DECQ : INCQ, ls.address + REFERENCE_SIZE);

            return postprocess(r, i, x64);  // borrowed reference, with index
        default:
            throw INTERNAL_ERROR;
        }
    }
};

