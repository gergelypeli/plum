
TypeSpec container_elem_ts(TypeSpec ts, Type *container_type = NULL) {
    // FIXME: this won't work for ptr
    return ts.rvalue().unprefix(ref_type).unprefix(container_type);
}


int container_elem_size(TypeSpec elem_ts) {
    // Note: this is like array and circularray, but unlike rbtree!
    return elem_ts.measure_elem();
}


void container_alloc(int header_size, int elem_size, int reservation_offset, Label finalizer_label, X64 *x64) {
    // RAX - reservation
    x64->op(PUSHQ, RAX);
    x64->op(IMUL3Q, RAX, RAX, elem_size);
    x64->op(ADDQ, RAX, header_size);
    x64->op(LEA, RBX, Address(finalizer_label, 0));
    
    x64->runtime->alloc_RAX_RBX();
    
    x64->op(POPQ, Address(RAX, reservation_offset));
}


void container_realloc(int header_size, int elem_size, int reservation_offset, X64 *x64) {
    // RAX - array, RBX - new reservation
    x64->op(MOVQ, Address(RAX, reservation_offset), RBX);
    x64->op(IMUL3Q, RBX, RBX, elem_size);
    x64->op(ADDQ, RBX, header_size);
    
    x64->runtime->realloc_RAX_RBX();
}


void container_grow(int reservation_offset, int min_reservation, Label realloc_label, X64 *x64) {
    // RAX - array, RBX - new reservation
    // Double the reservation until it's enough (can be relaxed to 1.5 times, but not less)

    Label check, ok;
    x64->op(XCHGQ, RBX, Address(RAX, reservation_offset));  // save desired value
    x64->op(CMPQ, RBX, min_reservation);
    x64->op(JAE, check);
    x64->op(MOVQ, RBX, min_reservation);
    
    x64->code_label(check);
    x64->op(CMPQ, RBX, Address(RAX, reservation_offset));
    x64->op(JAE, ok);
    x64->op(SHLQ, RBX, 1);
    x64->op(JMP, check);
    
    x64->code_label(ok);
    x64->op(CALL, realloc_label);
}


void container_preappend(int reservation_offset, int length_offset, Label grow_label, X64 *x64) {
    // RAX - array, RBX - new addition

    Label ok;
    x64->op(ADDQ, RBX, Address(RAX, length_offset));
    x64->op(CMPQ, RBX, Address(RAX, reservation_offset));
    x64->op(JBE, ok);

    x64->op(CALL, grow_label);
    
    x64->code_label(ok);
}


class ContainerLengthValue: public GenericValue {
public:
    Register reg;
    
    ContainerLengthValue(Value *l, TypeMatch &match)
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

    virtual Storage subcompile(int length_offset, X64 *x64) {
        ls = left->compile(x64);

        switch (ls.where) {
        case REGISTER:
            x64->runtime->decref(ls.reg);
            x64->op(MOVQ, ls.reg, Address(ls.reg, length_offset));
            return Storage(REGISTER, ls.reg);
        case MEMORY:
            x64->op(MOVQ, reg, ls.address);
            x64->op(MOVQ, reg, Address(reg, length_offset));
            return Storage(REGISTER, reg);
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class ContainerIndexValue: public OptimizedOperationValue {
public:
    TypeSpec elem_ts;
    // If the container is guaranteed, then the elements as well
    //Borrow *borrow;
    
    ContainerIndexValue(OperationType o, Value *pivot, TypeMatch &match)
        :OptimizedOperationValue(o, INTEGER_TS, match[1].lvalue(), pivot,
        GPR_SUBSET, GPR_SUBSET
        ) {
        elem_ts = match[1];
        //borrow = NULL;
        
        if (pivot->ts.rvalue()[0] != ptr_type)
            throw INTERNAL_ERROR;  // sanity check
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        //borrow = new Borrow;
        //scope->add(borrow);
        return OptimizedOperationValue::check(args, kwargs, scope);
    }

    virtual void fix_RBX_index(Register r, X64 *x64) {
    }

    virtual Storage subcompile(int elems_offset, X64 *x64) {
        int elem_size = container_elem_size(elem_ts);
    
        OptimizedOperationValue::subcompile(x64);

        switch (rs.where) {
        case CONSTANT:
            x64->op(MOVQ, RBX, rs.value);
            break;
        case REGISTER:
            x64->op(MOVQ, RBX, rs.reg);
            break;
        case MEMORY:
            x64->op(MOVQ, RBX, rs.address);
            break;
        default:
            throw INTERNAL_ERROR;
        }

        switch (ls.where) {
        case REGISTER:
            // Keep REGISTER weakreference, defer decweakref
            //x64->op(MOVQ, borrow->get_address(), ls.reg);
            
            fix_RBX_index(ls.reg, x64);
            
            x64->op(IMUL3Q, RBX, RBX, elem_size);
            x64->op(LEA, ls.reg, Address(ls.reg, RBX, elems_offset));
            return Storage(MEMORY, Address(ls.reg, 0));
        case MEMORY:
            // Add weak reference, defer decweakref
            x64->op(MOVQ, auxls.reg, ls.address);  // reg may be the base of ls.address
            //x64->op(MOVQ, borrow->get_address(), auxls.reg);
            //x64->runtime->incweakref(auxls.reg);
            
            fix_RBX_index(auxls.reg, x64);
            
            x64->op(IMUL3Q, RBX, RBX, elem_size);
            x64->op(LEA, auxls.reg, Address(auxls.reg, RBX, elems_offset));
            return Storage(MEMORY, Address(auxls.reg, 0));
        default:
            throw INTERNAL_ERROR;
        }
    }    
};


class ContainerEmptyValue: public GenericValue {
public:
    TypeSpec elem_ts;
    
    ContainerEmptyValue(TypeSpec ts)
        :GenericValue(NO_TS, ts, NULL) {
        elem_ts = container_elem_ts(ts);
    }

    virtual Regs precompile(Regs preferred) {
        return Regs(RAX);
    }

    virtual Storage subcompile(Once::TypedFunctionCompiler compile_alloc, X64 *x64) {
        Label alloc_label = x64->once->compile(compile_alloc, elem_ts);

        x64->op(MOVQ, RAX, 0);
        x64->op(CALL, alloc_label);
        
        return Storage(REGISTER, RAX);
    }
};


class ContainerReservedValue: public GenericValue {
public:
    TypeSpec elem_ts;
    
    ContainerReservedValue(TypeSpec ts)
        :GenericValue(INTEGER_TS, ts, NULL) {
        elem_ts = container_elem_ts(ts);
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = right->precompile(preferred);
        return clob | RAX;
    }

    virtual Storage subcompile(Once::TypedFunctionCompiler compile_alloc, X64 *x64) {
        Label alloc_label = x64->once->compile(compile_alloc, elem_ts);

        right->compile_and_store(x64, Storage(REGISTER, RAX));

        x64->op(CALL, alloc_label);
        
        return Storage(REGISTER, RAX);
    }
};


class ContainerAllValue: public Value {
public:
    std::unique_ptr<Value> fill_value;
    std::unique_ptr<Value> length_value;
    TypeSpec elem_ts;
    
    ContainerAllValue(TypeSpec ts)
        :Value(ts) {
        elem_ts = container_elem_ts(ts);
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
        return fill_value->precompile(preferred) | length_value->precompile(preferred) | Regs(RAX, RCX, RDX);
    }
    
    virtual Storage subcompile(int length_offset, int elems_offset, Once::TypedFunctionCompiler compile_alloc, X64 *x64) {
        Label alloc_label = x64->once->compile(compile_alloc, elem_ts);
        int elem_size = container_elem_size(elem_ts);

        fill_value->compile_and_store(x64, Storage(STACK));
        length_value->compile_and_store(x64, Storage(STACK));
        
        x64->op(MOVQ, RAX, Address(RSP, 0));
        x64->op(CALL, alloc_label);  // RAX - container reference
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

    ContainerInitializerValue(TypeSpec ts)
        :Value(ts) {
        elem_ts = container_elem_ts(ts);
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
            
            if (!typematch(elem_ts, v, scope, match)) {
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
            
        return clob | RAX | RCX;
    }

    virtual Storage subcompile(int length_offset, int elems_offset, Once::TypedFunctionCompiler compile_alloc, X64 *x64) {
        Label alloc_label = x64->once->compile(compile_alloc, elem_ts);
        int elem_size = container_elem_size(elem_ts);
        int stack_size = elem_ts.measure_stack();
    
        x64->op(MOVQ, RAX, elems.size());
        x64->op(CALL, alloc_label);
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


class ContainerAutogrowValue: public GenericValue, public Raiser {
public:
    ContainerAutogrowValue(Value *l, TypeMatch &match)
        :GenericValue(NO_TS, l->ts, l) {
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!check_raise(container_lent_exception_type, scope))
            return false;

        return GenericValue::check(args, kwargs, scope);
    }
    
    virtual Regs precompile(Regs preferred) {
        return left->precompile(preferred) | RAX | RCX;
    }
    
    virtual Storage subcompile(int reservation_offset, int length_offset, Once::TypedFunctionCompiler compile_grow, X64 *x64) {
        TypeSpec elem_ts = container_elem_ts(ts);
        Label grow_label = x64->once->compile(compile_grow, elem_ts);
        
        Storage s = left->compile(x64);
        
        switch (s.where) {
        case MEMORY: {
            if (s.address.base == RAX || s.address.index == RAX) {
                x64->op(LEA, RCX, s.address);
                s.address = Address(RCX, 0);
            }
            
            Label ok;
            x64->op(MOVQ, RAX, s.address);
            
            Label locked;
            x64->runtime->lock(RAX, locked);

            // MEMORY arg
            raise("CONTAINER_LENT", x64);

            x64->code_label(locked);
            x64->op(MOVQ, RBX, Address(RAX, length_offset));
            x64->op(CMPQ, RBX, Address(RAX, reservation_offset));
            x64->op(JB, ok);
            
            x64->op(INCQ, RBX);
            x64->op(CALL, grow_label);
            x64->op(MOVQ, s.address, RAX);
            
            x64->code_label(ok);
            return s;
        }
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class ContainerGrowableValue: public GenericValue, public Raiser {
public:
    ContainerGrowableValue(TypeSpec arg_ts, TypeSpec res_ts, Value *pivot)
        :GenericValue(arg_ts, res_ts, pivot) {
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (ptr_cast<ContainerAutogrowValue>(left.get())) {
            Args fake_args;
            Kwargs fake_kwargs;
            if (!left->check(fake_args, fake_kwargs, scope))
                return false;
            // Allow autogrow raise its own exceptions
        }
        else {
            if (!check_raise(container_full_exception_type, scope))
                return false;
        }
        
        return GenericValue::check(args, kwargs, scope);
    }
};


class ContainerShrinkableValue: public GenericValue, public Raiser {
public:
    ContainerShrinkableValue(TypeSpec arg_ts, TypeSpec res_ts, Value *pivot)
        :GenericValue(arg_ts, res_ts, pivot) {
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!check_raise(container_empty_exception_type, scope))
            return false;
        
        return GenericValue::check(args, kwargs, scope);
    }
};


class ContainerPushValue: public ContainerGrowableValue {
public:
    TypeSpec elem_ts;
    
    ContainerPushValue(Value *l, TypeMatch &match)
        :ContainerGrowableValue(match[1], match[0], l) {
        elem_ts = match[1];
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = left->precompile(preferred) | right->precompile(preferred);
        return clob | RAX | RCX;
    }

    virtual void fix_RBX_index(Register r, X64 *x64) {
    }

    virtual Storage subcompile(int reservation_offset, int length_offset, int elems_offset, X64 *x64) {
        int elem_size = container_elem_size(elem_ts);
        int stack_size = elem_ts.measure_stack();
    
        compile_and_store_both(x64, Storage(STACK), Storage(STACK));
    
        Label ok, end;
        
        x64->op(MOVQ, RAX, Address(RSP, stack_size));
        x64->op(MOVQ, RBX, Address(RAX, length_offset));
        x64->op(CMPQ, RBX, Address(RAX, reservation_offset));
        x64->op(JNE, ok);

        if (raising_dummy) {
            elem_ts.store(Storage(STACK), Storage(), x64);
            ts.store(Storage(STACK), Storage(), x64);
            raise("CONTAINER_FULL", x64);
        }
        else
            x64->runtime->die("Container full even if autogrowing!");
        
        x64->code_label(ok);
        x64->op(INCQ, Address(RAX, length_offset));

        // RBX contains the index of the newly created element
        fix_RBX_index(RAX, x64);
        
        x64->op(IMUL3Q, RBX, RBX, elem_size);
        x64->op(ADDQ, RAX, RBX);
        
        elem_ts.create(Storage(STACK), Storage(MEMORY, Address(RAX, elems_offset)), x64);
        
        x64->code_label(end);
        return Storage(STACK);
    }
};


class ContainerPopValue: public ContainerShrinkableValue {
public:
    TypeSpec elem_ts;
    
    ContainerPopValue(Value *l, TypeMatch &match)
        :ContainerShrinkableValue(NO_TS, match[1], l) {
        elem_ts = match[1];
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = left->precompile(preferred);
        return clob | RAX | RCX;
    }

    virtual void fix_RBX_index(Register r, X64 *x64) {
    }

    virtual Storage subcompile(int length_offset, int elems_offset, X64 *x64) {
        int elem_size = container_elem_size(elem_ts);
        Label ok;
        
        left->compile_and_store(x64, Storage(REGISTER, RAX));
        
        x64->op(CMPQ, Address(RAX, length_offset), 0);
        x64->op(JNE, ok);

        // REGISTER arg
        raise("CONTAINER_EMPTY", x64);
        
        x64->code_label(ok);
        x64->op(DECQ, Address(RAX, length_offset));
        x64->op(MOVQ, RBX, Address(RAX, length_offset));

        // RBX contains the index of the newly removed element
        fix_RBX_index(RAX, x64);

        x64->op(IMUL3Q, RBX, RBX, elem_size);
        x64->runtime->decref(RAX);

        x64->op(ADDQ, RAX, RBX);
        
        elem_ts.store(Storage(MEMORY, Address(RAX, elems_offset)), Storage(STACK), x64);
        elem_ts.destroy(Storage(MEMORY, Address(RAX, elems_offset)), x64);
        
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
    bool is_down;
    TypeSpec elem_ts;
    
    ContainerNextValue(TypeSpec ts, TypeSpec ets, Value *l, bool d)
        :GenericValue(NO_TS, ts, l) {
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
        
        if (!clob.has_any())
            clob = clob | RAX;
        
        return clob;
    }

    virtual Storage subcompile(int length_offset, X64 *x64) {
        ls = left->compile(x64);  // iterator
        Register reg = (clob & ~ls.regs()).get_any();
        Label ok;
        
        switch (ls.where) {
        case MEMORY:
            //std::cerr << "Compiling itemiter with reg=" << reg << " ls=" << ls << "\n";
            x64->op(MOVQ, RBX, ls.address + REFERENCE_SIZE);  // value
            x64->op(MOVQ, reg, ls.address); // array reference without incref
            x64->op(CMPQ, RBX, Address(reg, length_offset));
            x64->op(JNE, ok);

            // MEMORY arg
            raise("ITERATOR_DONE", x64);
            
            x64->code_label(ok);
            x64->op(is_down ? DECQ : INCQ, ls.address + REFERENCE_SIZE);
            //x64->err("NEXT COMPILE");
            return Storage(REGISTER, reg);  // non-refcounted reference, with RBX index
        default:
            throw INTERNAL_ERROR;
        }
    }
};

