
TypeSpec container_elem_ts(Type *container_type, TypeSpec ts) {
    return ts.rvalue().unprefix(reference_type).unprefix(container_type).varvalue();
}


void alloc_container(int header_size, int elem_size, int reservation_offset, Label finalizer_label, X64 *x64) {
    // RAX - reservation
    x64->op(PUSHQ, RAX);
    x64->op(IMUL3Q, RAX, RAX, elem_size);
    x64->op(ADDQ, RAX, header_size);
    x64->op(LEARIP, RBX, finalizer_label);
    
    x64->alloc_RAX_RBX();
    
    x64->op(POPQ, Address(RAX, reservation_offset));
}


void realloc_container(int header_size, int elem_size, int reservation_offset, X64 *x64) {
    // RAX - array, RBX - new reservation
    x64->op(MOVQ, Address(RAX, reservation_offset), RBX);
    x64->op(IMUL3Q, RBX, RBX, elem_size);
    x64->op(ADDQ, RBX, header_size);
    
    x64->realloc_RAX_RBX();
}


void grow_container(int reservation_offset, int min_reservation, Label realloc_label, X64 *x64) {
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


void preappend_container(int reservation_offset, int length_offset, Label grow_label, X64 *x64) {
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
    int length_offset;
    Register reg;
    
    ContainerLengthValue(Value *l, TypeMatch &match, int lo)
        :GenericValue(VOID_TS, INTEGER_TS, l) {
        length_offset = lo;
        reg = NOREG;
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = left->precompile(preferred);
        
        if (!clob.has_any())
            clob.add(RAX);
        
        reg = clob.get_any();
            
        return clob;
    }

    virtual Storage compile(X64 *x64) {
        ls = left->compile(x64);

        switch (ls.where) {
        case REGISTER:
            x64->decref(ls.reg);
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


class ContainerIndexValue: public GenericOperationValue {
public:
    int elems_offset;
    
    ContainerIndexValue(OperationType o, Value *pivot, TypeMatch &match, int eo)
        :GenericOperationValue(o, INTEGER_TS, match[1].lvalue(), pivot) {
        elems_offset = eo;
    }

    virtual int get_elem_size() {
        throw INTERNAL_ERROR;
    }

    virtual void fix_index(Register r, X64 *x64) {
    }

    virtual Storage compile(X64 *x64) {
        int elem_size = get_elem_size();
        
        subcompile(x64);
    
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
            x64->decref(ls.reg);
            fix_index(ls.reg, x64);
            x64->op(IMUL3Q, RBX, RBX, elem_size);
            x64->op(LEA, ls.reg, Address(ls.reg, RBX, elems_offset));
            return Storage(MEMORY, Address(ls.reg, 0));
        case MEMORY:
            x64->op(MOVQ, reg, ls.address);  // reg may be the base of ls.address
            fix_index(reg, x64);
            x64->op(IMUL3Q, RBX, RBX, elem_size);
            x64->op(LEA, reg, Address(reg, RBX, elems_offset));
            return Storage(MEMORY, Address(reg, 0));
        default:
            throw INTERNAL_ERROR;
        }
    }    
};


class ContainerEmptyValue: public GenericValue {
public:
    ContainerEmptyValue(TypeSpec ts)
        :GenericValue(VOID_TS, ts, NULL) {
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob;
        return clob.add(RAX).add(RBX);
    }

    virtual Label get_alloc_label(X64 *x64) {
        throw INTERNAL_ERROR;
    }

    virtual Storage compile(X64 *x64) {
        x64->op(MOVQ, RAX, 0);
        x64->op(CALL, get_alloc_label(x64));
        
        return Storage(REGISTER, RAX);
    }
};


class ContainerInitializerValue: public Value {
public:
    int length_offset;
    int elems_offset;
    int elem_size;
    TypeSpec elem_ts;
    std::vector<std::unique_ptr<Value>> elems;

    ContainerInitializerValue(TypeSpec ts, TypeSpec elem_ts, int length_offset, int elems_offset, int elem_size)
        :Value(ts) {
        this->length_offset = length_offset;
        this->elems_offset = elems_offset;
        this->elem_size = elem_size;
        this->elem_ts = elem_ts; //container_elem_ts(ts);
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
        clob.add(RAX).add(RBX).add(RCX);
        
        for (auto &elem : elems)
            clob = clob | elem->precompile(preferred);
            
        return clob;
    }

    virtual int get_elem_size() {
        throw INTERNAL_ERROR;
    }

    virtual Label get_alloc_label(X64 *x64) {
        throw INTERNAL_ERROR;
    }

    virtual Storage compile(X64 *x64) {
        x64->op(MOVQ, RAX, elems.size());
        x64->op(CALL, get_alloc_label(x64));
        x64->op(MOVQ, Address(RAX, length_offset), elems.size());
        x64->op(PUSHQ, RAX);
        
        unsigned offset = 0;
        
        for (auto &elem : elems) {
            elem->compile_and_store(x64, Storage(STACK));
            x64->op(MOVQ, RAX, Address(RSP, stack_size(elem_size)));
            Storage t(MEMORY, Address(RAX, elems_offset + offset));
            
            elem_ts.create(Storage(STACK), t, x64);
            offset += elem_size;
        }
        
        return Storage(STACK);
    }
};


class ContainerPushValue: public GenericValue {
public:
    TypeSpec elem_ts;
    int reservation_offset;
    int length_offset;
    int elems_offset;
    
    ContainerPushValue(Value *l, TypeMatch &match, int ro, int lo, int eo)
        :GenericValue(match[1].varvalue(), match[0], l) {
        elem_ts = match[1].varvalue();
        reservation_offset = ro;
        length_offset = lo;
        elems_offset = eo;
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = left->precompile(preferred) | right->precompile(preferred);
        return clob.add(RAX).add(RBX).add(RCX);
    }

    virtual void fix_index(Register r, X64 *x64) {
        throw INTERNAL_ERROR;
    }

    virtual Storage compile(X64 *x64) {
        compile_and_store_both(x64, Storage(STACK), Storage(STACK));
    
        int elem_size = ::elem_size(elem_ts.measure(MEMORY));
        int stack_size = ::stack_size(elem_ts.measure(MEMORY));
        Label ok, end;
        
        x64->op(MOVQ, RAX, Address(RSP, stack_size));
        x64->op(MOVQ, RBX, Address(RAX, length_offset));
        x64->op(CMPQ, RBX, Address(RAX, reservation_offset));
        x64->op(JNE, ok);
        
        x64->die("Container full!");
        x64->op(JMP, end);
        
        x64->code_label(ok);
        x64->op(INCQ, Address(RAX, length_offset));

        // RBX contains the index of the newly created element
        fix_index(RAX, x64);
        
        x64->op(IMUL3Q, RBX, RBX, elem_size);
        x64->op(ADDQ, RAX, RBX);
        
        elem_ts.create(Storage(STACK), Storage(MEMORY, Address(RAX, elems_offset)), x64);
        
        x64->code_label(end);
        return Storage(STACK);
    }
};


class CircularrayPopValue: public GenericValue {
public:
    TypeSpec elem_ts;
    
    CircularrayPopValue(Value *l, TypeMatch &match)
        :GenericValue(VOID_TS, circularray_elem_ts(match[0]), l) {
        elem_ts = circularray_elem_ts(match[0]);
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = left->precompile(preferred);
        return clob.add(RAX).add(RBX).add(RCX);
    }

    virtual void fix_index(Register r, X64 *x64) {
        fix_index_overflow(r, x64);
    }

    virtual Storage compile(X64 *x64) {
        int elem_size = ::elem_size(elem_ts.measure(MEMORY));
        Label ok;
        
        left->compile_and_store(x64, Storage(REGISTER, RAX));
        
        x64->op(CMPQ, Address(RAX, CIRCULARRAY_LENGTH_OFFSET), 0);
        x64->op(JNE, ok);
        
        x64->die("Circularray empty!");
        
        x64->code_label(ok);
        x64->op(DECQ, Address(RAX, CIRCULARRAY_LENGTH_OFFSET));
        x64->op(MOVQ, RBX, Address(RAX, CIRCULARRAY_LENGTH_OFFSET));

        // RBX contains the index of the newly removed element
        fix_index(RAX, x64);

        x64->op(IMUL3Q, RBX, RBX, elem_size);
        x64->decref(RAX);

        x64->op(ADDQ, RAX, RBX);
        
        elem_ts.store(Storage(MEMORY, Address(RAX, CIRCULARRAY_ELEMS_OFFSET)), Storage(STACK), x64);
        elem_ts.destroy(Storage(MEMORY, Address(RAX, CIRCULARRAY_ELEMS_OFFSET)), x64);
        
        return Storage(STACK);
    }
};

