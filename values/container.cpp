
TypeSpec container_elem_ts(TypeSpec ts, Type *container_type = NULL) {
    // FIXME: this won't work for ptr
    return ts.rvalue().unprefix(ref_type).unprefix(container_type);
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


void container_preappend(int reservation_offset, int length_offset, Label grow_label, X64 *x64) {
    // RAX - array, R10 - new addition

    Label ok;
    x64->op(ADDQ, R10, Address(RAX, length_offset));
    x64->op(CMPQ, R10, Address(RAX, reservation_offset));
    x64->op(JBE, ok);

    x64->op(CALL, grow_label);  // clobbers all
    
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
            x64->runtime->decref(ls.reg);  // FIXME: not nice!
            x64->op(MOVQ, ls.reg, Address(ls.reg, length_offset));
            return Storage(REGISTER, ls.reg);
        case STACK:
            x64->op(POPQ, RBX);
            x64->op(MOVQ, reg, Address(RBX, length_offset));
            x64->runtime->decref(RBX);
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


class ContainerIndexValue: public OptimizedOperationValue {
public:
    TypeSpec elem_ts;
    Unborrow *unborrow;
    
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
        unborrow = new Unborrow;
        scope->add(unborrow);
        
        return OptimizedOperationValue::check(args, kwargs, scope);
    }

    virtual void fix_R10_index(Register r, X64 *x64) {
    }

    virtual Storage subcompile(int elems_offset, X64 *x64) {
        int elem_size = container_elem_size(elem_ts);
    
        OptimizedOperationValue::subcompile(x64);

        switch (rs.where) {
        case CONSTANT:
            x64->op(MOVQ, R10, rs.value);
            break;
        case REGISTER:
            x64->op(MOVQ, R10, rs.reg);
            break;
        case MEMORY:
            x64->op(MOVQ, R10, rs.address);
            break;
        default:
            throw INTERNAL_ERROR;
        }

        switch (ls.where) {
        case REGISTER:
            // Borrow Lvalue container
            x64->op(MOVQ, unborrow->get_address(), ls.reg);
            
            fix_R10_index(ls.reg, x64);
            
            x64->op(IMUL3Q, R10, R10, elem_size);
            x64->op(LEA, ls.reg, Address(ls.reg, R10, elems_offset));
            return Storage(MEMORY, Address(ls.reg, 0));
        case MEMORY:
            x64->op(MOVQ, auxls.reg, ls.address);  // reg may be the base of ls.address
            // Borrow Lvalue container
            x64->runtime->incref(auxls.reg);
            x64->op(MOVQ, unborrow->get_address(), auxls.reg);
            
            fix_R10_index(auxls.reg, x64);
            
            x64->op(IMUL3Q, R10, R10, elem_size);
            x64->op(LEA, auxls.reg, Address(auxls.reg, R10, elems_offset));
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
        return Regs::all();
    }

    virtual Storage subcompile(Once::TypedFunctionCompiler compile_alloc, X64 *x64) {
        Label alloc_label = x64->once->compile(compile_alloc, elem_ts);

        x64->op(MOVQ, R10, 0);
        x64->op(CALL, alloc_label);  // clobbers all
        
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
        right->precompile(preferred);
        return Regs::all();
    }

    virtual Storage subcompile(Once::TypedFunctionCompiler compile_alloc, X64 *x64) {
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
        fill_value->precompile(preferred);
        length_value->precompile(preferred);
        return Regs::all();
    }
    
    virtual Storage subcompile(int length_offset, int elems_offset, Once::TypedFunctionCompiler compile_alloc, X64 *x64) {
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

    virtual Storage subcompile(int length_offset, int elems_offset, Once::TypedFunctionCompiler compile_alloc, X64 *x64) {
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
        left->precompile(preferred);
        return Regs::all();
    }
    
    virtual Storage subcompile(int reservation_offset, int length_offset, Once::TypedFunctionCompiler compile_grow, X64 *x64) {
        TypeSpec elem_ts = container_elem_ts(ts);
        Label grow_label = x64->once->compile(compile_grow, elem_ts);
        
        left->compile_and_store(x64, Storage(ALISTACK));
        
        Label ok;
        x64->op(MOVQ, R11, Address(RSP, 0));
        x64->op(MOVQ, RAX, Address(R11, 0));
        
        Label unshared;
        x64->runtime->check_unshared(RAX);
        x64->op(JE, unshared);

        x64->op(POPQ, R11);  // popped
        raise("CONTAINER_LENT", x64);

        x64->code_label(unshared);
        x64->op(MOVQ, R10, Address(RAX, length_offset));
        x64->op(CMPQ, R10, Address(RAX, reservation_offset));
        x64->op(JB, ok);
        
        x64->op(INCQ, R10);
        x64->op(CALL, grow_label);  // clobbers all
        x64->op(MOVQ, R11, Address(RSP, 0));
        x64->op(MOVQ, Address(R11, 0), RAX);  // technically not an assignment
        
        x64->code_label(ok);

        // Since the value will be needed, return MEMORY, because ALISTACK-STACK store
        // is not implemented directly.
        x64->op(POPQ, RAX);
        return Storage(MEMORY, Address(RAX, 0));
    }
};


class ContainerGrowableValue: public GenericValue, public Raiser {
public:
    ContainerGrowableValue(TypeSpec arg_ts, TypeSpec res_ts, Value *pivot)
        :GenericValue(arg_ts, res_ts, pivot) {
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        // If we are operating on an autogrow, that can also raise an exception,
        // but then we won't. Unfortunately this is a lame way to check this.
        
        if (!scope->get_try_scope()->get_exception_type())
            if (!check_raise(container_full_exception_type, scope))
                return false;
        
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

    virtual void fix_R10_index(Register r, X64 *x64) {
    }

    virtual Storage subcompile(int reservation_offset, int length_offset, int elems_offset, X64 *x64) {
        int elem_size = container_elem_size(elem_ts);
        int stack_size = elem_ts.measure_stack();
    
        compile_and_store_both(x64, Storage(STACK), Storage(STACK));
    
        Label ok, end;
        
        x64->op(MOVQ, RAX, Address(RSP, stack_size));
        x64->op(MOVQ, R10, Address(RAX, length_offset));
        x64->op(CMPQ, R10, Address(RAX, reservation_offset));
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

        // R10 contains the index of the newly created element
        fix_R10_index(RAX, x64);
        
        x64->op(IMUL3Q, R10, R10, elem_size);
        x64->op(ADDQ, RAX, R10);
        
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

    virtual void fix_R10_index(Register r, X64 *x64) {
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
        x64->op(MOVQ, R10, Address(RAX, length_offset));

        // R10 contains the index of the newly removed element
        fix_R10_index(RAX, x64);

        x64->op(IMUL3Q, R10, R10, elem_size);
        x64->runtime->decref(RAX);

        x64->op(ADDQ, RAX, R10);
        
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
            x64->op(MOVQ, R10, ls.address + REFERENCE_SIZE);  // value
            x64->op(MOVQ, reg, ls.address); // array reference without incref
            x64->op(CMPQ, R10, Address(reg, length_offset));
            x64->op(JNE, ok);

            // MEMORY arg
            raise("ITERATOR_DONE", x64);
            
            x64->code_label(ok);
            x64->op(is_down ? DECQ : INCQ, ls.address + REFERENCE_SIZE);
            //x64->err("NEXT COMPILE");
            return Storage(REGISTER, reg);  // non-refcounted reference, with R10 index
        default:
            throw INTERNAL_ERROR;
        }
    }
};

