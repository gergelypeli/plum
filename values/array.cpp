
int array_elem_size(TypeSpec elem_ts) {
    return elem_ts.measure_elem();
}


void compile_array_alloc(Label label, TypeSpec elem_ts, X64 *x64) {
    // RAX - reservation
    int elem_size = array_elem_size(elem_ts);
    Label finalizer_label = elem_ts.prefix(array_type).get_finalizer_label(x64);
    
    x64->code_label_local(label, "x_array_alloc");
    
    container_alloc(ARRAY_HEADER_SIZE, elem_size, ARRAY_RESERVATION_OFFSET, finalizer_label, x64);

    x64->op(MOVQ, Address(RAX, ARRAY_LENGTH_OFFSET), 0);
    
    x64->op(RET);
}


void compile_array_realloc(Label label, TypeSpec elem_ts, X64 *x64) {
    // RAX - array, RBX - new reservation
    int elem_size = array_elem_size(elem_ts);

    x64->code_label_local(label, "x_array_realloc");

    container_realloc(ARRAY_HEADER_SIZE, elem_size, ARRAY_RESERVATION_OFFSET, x64);

    x64->op(RET);
}


void compile_array_grow(Label label, TypeSpec elem_ts, X64 *x64) {
    // RAX - array, RBX - new reservation
    // Double the reservation until it's enough (can be relaxed to 1.5 times, but not less)
    Label realloc_label = x64->once->compile(compile_array_realloc, elem_ts);

    x64->code_label_local(label, "x_array_grow");
    //x64->log("grow_array");
    
    container_grow(ARRAY_RESERVATION_OFFSET, ARRAY_MINIMUM_RESERVATION, realloc_label, x64);
    
    x64->op(RET);
}


void compile_array_preappend(Label label, TypeSpec elem_ts, X64 *x64) {
    // RAX - array, RBX - new addition
    Label grow_label = x64->once->compile(compile_array_grow, elem_ts);

    x64->code_label_local(label, "x_array_preappend");
    //x64->log("preappend_array");
    
    container_preappend(ARRAY_RESERVATION_OFFSET, ARRAY_LENGTH_OFFSET, grow_label, x64);

    x64->op(RET);
}


class ArrayLengthValue: public ContainerLengthValue {
public:
    ArrayLengthValue(Value *l, TypeMatch &match)
        :ContainerLengthValue(l, match) {
    }
    
    virtual Storage compile(X64 *x64) {
        return subcompile(ARRAY_LENGTH_OFFSET, x64);
    }
};


class ArrayIndexValue: public ContainerIndexValue {
public:
    ArrayIndexValue(OperationType o, Value *pivot, TypeMatch &match)
        :ContainerIndexValue(o, pivot, match) {
    }

    virtual Storage compile(X64 *x64) {
        return subcompile(ARRAY_ELEMS_OFFSET, x64);
    }
};


class ArrayConcatenationValue: public GenericValue {
public:
    TypeSpec elem_ts;
    
    ArrayConcatenationValue(Value *l, TypeMatch &match)
        :GenericValue(match[0], match[0], l) {
        elem_ts = match[1];
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = left->precompile(preferred) | right->precompile(preferred);
        return clob | RAX | RCX | RDX | RSI | RDI;
    }

    virtual Storage compile(X64 *x64) {
        // TODO: this only works for arrays of basic types now, that can be just copied
        Label l = x64->once->compile(compile_array_concatenation, elem_ts);

        compile_and_store_both(x64, Storage(STACK), Storage(STACK));
    
        x64->op(CALL, l);
        
        right->ts.store(rs, Storage(), x64);
        left->ts.store(ls, Storage(), x64);
        
        return Storage(REGISTER, RAX);
    }
    
    static void compile_array_concatenation(Label label, TypeSpec elem_ts, X64 *x64) {
        // RAX - result, RBX - first, RDX - second
        x64->code_label_local(label, "x_array_concatenation");
        Label alloc_array = x64->once->compile(compile_array_alloc, elem_ts);
        int elem_size = array_elem_size(elem_ts);
        
        x64->op(MOVQ, RBX, Address(RSP, ADDRESS_SIZE + REFERENCE_SIZE));
        x64->op(MOVQ, RDX, Address(RSP, ADDRESS_SIZE));
        
        x64->op(MOVQ, RAX, Address(RBX, ARRAY_LENGTH_OFFSET));
        x64->op(ADDQ, RAX, Address(RDX, ARRAY_LENGTH_OFFSET));  // total length in RAX
        
        x64->op(CALL, alloc_array);
        
        x64->op(MOVQ, RBX, Address(RSP, ADDRESS_SIZE + REFERENCE_SIZE));  // restored
        x64->op(MOVQ, RDX, Address(RSP, ADDRESS_SIZE));
        
        x64->op(LEA, RDI, Address(RAX, ARRAY_ELEMS_OFFSET));
        
        x64->op(LEA, RSI, Address(RBX, ARRAY_ELEMS_OFFSET));
        x64->op(MOVQ, RCX, Address(RBX, ARRAY_LENGTH_OFFSET));
        x64->op(ADDQ, Address(RAX, ARRAY_LENGTH_OFFSET), RCX);
        x64->op(IMUL3Q, RCX, RCX, elem_size);
        x64->op(REPMOVSB);

        x64->op(LEA, RSI, Address(RDX, ARRAY_ELEMS_OFFSET));
        x64->op(MOVQ, RCX, Address(RDX, ARRAY_LENGTH_OFFSET));
        x64->op(ADDQ, Address(RAX, ARRAY_LENGTH_OFFSET), RCX);
        x64->op(IMUL3Q, RCX, RCX, elem_size);
        x64->op(REPMOVSB);
        
        x64->op(RET);  // new array in RAX
    }
};


class ArrayReallocValue: public GenericOperationValue {
public:
    TypeSpec elem_ts;

    ArrayReallocValue(OperationType o, Value *l, TypeMatch &match)
        :GenericOperationValue(o, INTEGER_OVALUE_TS, l->ts, l) {
        elem_ts = match[1];
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = GenericOperationValue::precompile(preferred);
        return clob | RAX | RCX;
    }

    virtual Storage compile(X64 *x64) {
        // TODO: this only works for arrays of basic types now, that can be just copied
        // TODO: can't any type be moved?

        Label realloc_array = x64->once->compile(compile_array_realloc, elem_ts);
        
        subcompile(x64);
        
        if (ls.where != MEMORY)
            throw INTERNAL_ERROR;

        if (ls.address.base == RAX || ls.address.index == RAX) {
            x64->op(LEA, RBX, ls.address);
            x64->op(PUSHQ, RBX);
        }

        switch (rs.where) {
        case NOWHERE:
            x64->op(MOVQ, RAX, ls.address);
            x64->op(MOVQ, RBX, Address(RAX, ARRAY_LENGTH_OFFSET));  // shrink to fit
            break;
        case CONSTANT:
            x64->op(MOVQ, RAX, ls.address);
            x64->op(MOVQ, RBX, rs.value);
            break;
        case REGISTER:
            x64->op(MOVQ, RBX, rs.reg);  // Order!
            x64->op(MOVQ, RAX, ls.address);
            break;
        case MEMORY:
            x64->op(MOVQ, RBX, rs.address);  // Order!
            x64->op(MOVQ, RAX, ls.address);
            break;
        default:
            throw INTERNAL_ERROR;
        }
        
        x64->op(CALL, realloc_array);
        
        if (ls.address.base == RAX || ls.address.index == RAX) {
            x64->op(MOVQ, RBX, RAX);
            x64->op(POPQ, RAX);
            x64->op(MOVQ, Address(RAX, 0), RBX);
            return Storage(MEMORY, Address(RAX, 0));
        }
        else {
            x64->op(MOVQ, ls.address, RAX);
            return ls;
        }
    }
};


class ArraySortValue: public GenericValue {
public:
    TypeSpec elem_ts;
    
    ArraySortValue(Value *l, TypeMatch &match)
        :GenericValue(NO_TS, VOID_TS, l) {
        elem_ts = match[1];
    }

    virtual Regs precompile(Regs preferred) {
        return Regs::all();
    }

    virtual Storage compile(X64 *x64) {
        int elem_size = array_elem_size(elem_ts);
        Label compar = x64->once->compile(compile_compar, elem_ts);
        Label done;

        left->compile_and_store(x64, Storage(STACK));
        
        // RDI = base, RSI = nmemb, RDX = size, RCX = compar
        x64->op(MOVQ, RBX, Address(RSP, 0));
        x64->op(LEA, RDI, Address(RBX, ARRAY_ELEMS_OFFSET));
        x64->op(MOVQ, RSI, Address(RBX, ARRAY_LENGTH_OFFSET));
        x64->op(MOVQ, RDX, elem_size);
        x64->op(LEARIP, RCX, compar);
        
        x64->op(CALL, x64->sort_label);
        
        left->ts.store(Storage(STACK), Storage(), x64);
        
        return Storage();
    }
    
    static void compile_compar(Label label, TypeSpec elem_ts, X64 *x64) {
        // Generate a SysV function to wrap our compare function.
        // RDI and RSI contains the pointers to the array elements.
        // RBX must be preserved.
        x64->code_label(label);
        x64->op(PUSHQ, RBX);
        
        Storage a(MEMORY, Address(RDI, 0));
        Storage b(MEMORY, Address(RSI, 0));

        Label less, greater, end;
        elem_ts.compare(a, b, x64, less, greater);
        
        x64->op(MOVQ, RAX, 0);
        x64->op(JMP, end);
        
        x64->code_label(less);
        x64->op(MOVQ, RAX, -1);
        x64->op(JMP, end);
        
        x64->code_label(greater);
        x64->op(MOVQ, RAX, 1);
        
        x64->code_label(end);
        
        x64->op(POPQ, RBX);
        x64->op(RET);
    }
};


class ArrayEmptyValue: public ContainerEmptyValue {
public:
    ArrayEmptyValue(TypeSpec ts)
        :ContainerEmptyValue(ts) {
    }

    virtual Storage compile(X64 *x64) {
        return subcompile(compile_array_alloc, x64);
    }
};


class ArrayInitializerValue: public ContainerInitializerValue {
public:
    ArrayInitializerValue(TypeSpec ts)
        :ContainerInitializerValue(ts) {
    }

    virtual Storage compile(X64 *x64) {
        return subcompile(ARRAY_LENGTH_OFFSET, ARRAY_ELEMS_OFFSET, compile_array_alloc, x64);
    }
};


class ArrayPushValue: public ContainerPushValue {
public:
    ArrayPushValue(Value *l, TypeMatch &match)
        :ContainerPushValue(l, match) {
    }

    virtual Storage compile(X64 *x64) {
        return subcompile(ARRAY_RESERVATION_OFFSET, ARRAY_LENGTH_OFFSET, ARRAY_ELEMS_OFFSET, x64);
    }
};


class ArrayPopValue: public ContainerPopValue {
public:
    ArrayPopValue(Value *l, TypeMatch &match)
        :ContainerPopValue(l, match) {
    }

    virtual Storage compile(X64 *x64) {
        return subcompile(ARRAY_LENGTH_OFFSET, ARRAY_ELEMS_OFFSET, x64);
    }
};


class ArrayAutogrowValue: public ContainerAutogrowValue {
public:
    ArrayAutogrowValue(Value *l, TypeMatch &match)
        :ContainerAutogrowValue(l, match) {
    }
    
    virtual Storage compile(X64 *x64) {
        return subcompile(ARRAY_RESERVATION_OFFSET, ARRAY_LENGTH_OFFSET, compile_array_grow, x64);
    }
};


// Iteration

class ArrayElemIterValue: public ContainerIterValue {
public:
    ArrayElemIterValue(Value *l, TypeMatch &match)
        :ContainerIterValue(typesubst(SAME_ARRAYELEMITER_TS, match), l) {
    }
};


class ArrayIndexIterValue: public ContainerIterValue {
public:
    ArrayIndexIterValue(Value *l, TypeMatch &match)
        :ContainerIterValue(typesubst(SAME_ARRAYINDEXITER_TS, match), l) {
    }
};


class ArrayItemIterValue: public ContainerIterValue {
public:
    ArrayItemIterValue(Value *l, TypeMatch &match)
        :ContainerIterValue(typesubst(SAME_ARRAYITEMITER_TS, match), l) {
    }
};


class ArrayNextElemValue: public ContainerNextValue {
public:
    ArrayNextElemValue(Value *l, TypeMatch &match)
        :ContainerNextValue(match[1], match[1], l, false) {
    }

    virtual Storage compile(X64 *x64) {
        int elem_size = array_elem_size(elem_ts);
        
        Storage r = subcompile(ARRAY_LENGTH_OFFSET, x64);
        
        x64->op(IMUL3Q, RBX, RBX, elem_size);
        x64->op(LEA, r.reg, Address(r.reg, RBX, ARRAY_ELEMS_OFFSET));
        
        return Storage(MEMORY, Address(r.reg, 0));
    }
};


class ArrayNextIndexValue: public ContainerNextValue {
public:
    ArrayNextIndexValue(Value *l, TypeMatch &match)
        :ContainerNextValue(INTEGER_TS, match[1], l, false) {
    }
    
    virtual Storage compile(X64 *x64) {
        Storage r = subcompile(ARRAY_LENGTH_OFFSET, x64);
        
        x64->op(MOVQ, r.reg, RBX);
        
        return Storage(REGISTER, r.reg);
    }
};


class ArrayNextItemValue: public ContainerNextValue {
public:
    ArrayNextItemValue(Value *l, TypeMatch &match)
        :ContainerNextValue(typesubst(INTEGER_SAME_ITEM_TS, match), match[1], l, false) {
    }

    virtual Storage compile(X64 *x64) {
        int elem_size = array_elem_size(elem_ts);
        int item_stack_size = ts.measure_stack();

        Storage r = subcompile(ARRAY_LENGTH_OFFSET, x64);

        x64->op(SUBQ, RSP, item_stack_size);
        x64->op(MOVQ, Address(RSP, 0), RBX);
        
        x64->op(IMUL3Q, RBX, RBX, elem_size);
        x64->op(LEA, r.reg, Address(r.reg, RBX, ARRAY_ELEMS_OFFSET));
        
        Storage s = Storage(MEMORY, Address(r.reg, 0));
        Storage t = Storage(MEMORY, Address(RSP, INTEGER_SIZE));
        elem_ts.create(s, t, x64);
        
        return Storage(STACK);
    }
};

