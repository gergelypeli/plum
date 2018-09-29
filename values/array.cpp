
int array_elem_size(TypeSpec elem_ts) {
    return elem_ts.measure_elem();
}


void compile_array_alloc(Label label, TypeSpec elem_ts, X64 *x64) {
    // R10 - reservation
    int elem_size = array_elem_size(elem_ts);
    Label finalizer_label = elem_ts.prefix(array_type).get_finalizer_label(x64);
    
    x64->code_label_local(label, "x_array_alloc");
    
    container_alloc(ARRAY_HEADER_SIZE, elem_size, ARRAY_RESERVATION_OFFSET, finalizer_label, x64);

    x64->op(MOVQ, Address(RAX, ARRAY_LENGTH_OFFSET), 0);
    
    x64->op(RET);
}


void compile_array_realloc(Label label, TypeSpec elem_ts, X64 *x64) {
    // RAX - array, R10 - new reservation
    int elem_size = array_elem_size(elem_ts);

    x64->code_label_local(label, "x_array_realloc");

    container_realloc(ARRAY_HEADER_SIZE, elem_size, ARRAY_RESERVATION_OFFSET, x64);

    x64->op(RET);
}


void compile_array_grow(Label label, TypeSpec elem_ts, X64 *x64) {
    // RAX - array, R10 - new reservation
    // Double the reservation until it's enough (can be relaxed to 1.5 times, but not less)
    Label realloc_label = x64->once->compile(compile_array_realloc, elem_ts);

    x64->code_label_local(label, "x_array_grow");
    //x64->log("grow_array");
    
    container_grow(ARRAY_RESERVATION_OFFSET, ARRAY_MINIMUM_RESERVATION, realloc_label, x64);
    
    x64->op(RET);
}


void compile_array_preappend(Label label, TypeSpec elem_ts, X64 *x64) {
    // RAX - array, R10 - new addition
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
        return clob | RAX | RCX | RSI | RDI;
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
        // RAX - result, R10 - first, R11 - second
        x64->code_label_local(label, "x_array_concatenation");
        Label alloc_array = x64->once->compile(compile_array_alloc, elem_ts);
        int elem_size = array_elem_size(elem_ts);
        
        x64->op(MOVQ, R10, Address(RSP, ADDRESS_SIZE + REFERENCE_SIZE));
        x64->op(MOVQ, R11, Address(RSP, ADDRESS_SIZE));
        
        x64->op(MOVQ, R10, Address(R10, ARRAY_LENGTH_OFFSET));
        x64->op(ADDQ, R10, Address(R11, ARRAY_LENGTH_OFFSET));  // total length
        
        x64->op(CALL, alloc_array);
        
        x64->op(MOVQ, R10, Address(RSP, ADDRESS_SIZE + REFERENCE_SIZE));  // restored
        x64->op(MOVQ, R11, Address(RSP, ADDRESS_SIZE));
        
        x64->op(LEA, RDI, Address(RAX, ARRAY_ELEMS_OFFSET));
        
        x64->op(LEA, RSI, Address(R10, ARRAY_ELEMS_OFFSET));
        x64->op(MOVQ, RCX, Address(R10, ARRAY_LENGTH_OFFSET));
        x64->op(ADDQ, Address(RAX, ARRAY_LENGTH_OFFSET), RCX);
        x64->op(IMUL3Q, RCX, RCX, elem_size);
        x64->op(REPMOVSB);

        x64->op(LEA, RSI, Address(R11, ARRAY_ELEMS_OFFSET));
        x64->op(MOVQ, RCX, Address(R11, ARRAY_LENGTH_OFFSET));
        x64->op(ADDQ, Address(RAX, ARRAY_LENGTH_OFFSET), RCX);
        x64->op(IMUL3Q, RCX, RCX, elem_size);
        x64->op(REPMOVSB);
        
        x64->op(RET);  // new array in RAX
    }
};


class ArrayReallocValue: public OptimizedOperationValue {
public:
    TypeSpec elem_ts;

    ArrayReallocValue(OperationType o, Value *l, TypeMatch &match)
        :OptimizedOperationValue(o, INTEGER_OVALUE_TS, l->ts, l,
        PTR_SUBSET, GPR_SUBSET
        ) {
        elem_ts = match[1];
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = OptimizedOperationValue::precompile(preferred);
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
            x64->op(LEA, R10, ls.address);
            x64->op(PUSHQ, R10);
        }

        switch (rs.where) {
        case NOWHERE:
            x64->op(MOVQ, RAX, ls.address);
            x64->op(MOVQ, R10, Address(RAX, ARRAY_LENGTH_OFFSET));  // shrink to fit
            break;
        case CONSTANT:
            x64->op(MOVQ, RAX, ls.address);
            x64->op(MOVQ, R10, rs.value);
            break;
        case REGISTER:
            x64->op(MOVQ, R10, rs.reg);  // Order!
            x64->op(MOVQ, RAX, ls.address);
            break;
        case MEMORY:
            x64->op(MOVQ, R10, rs.address);  // Order!
            x64->op(MOVQ, RAX, ls.address);
            break;
        default:
            throw INTERNAL_ERROR;
        }
        
        x64->op(CALL, realloc_array);
        
        if (ls.address.base == RAX || ls.address.index == RAX) {
            x64->op(MOVQ, R10, RAX);
            x64->op(POPQ, RAX);
            x64->op(MOVQ, Address(RAX, 0), R10);
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
        return left->precompile(preferred) | Regs(RAX, RCX, RDX, RSI, RDI) | COMPARE_CLOB;
    }

    virtual Storage compile(X64 *x64) {
        int elem_size = array_elem_size(elem_ts);
        Label compar = x64->once->compile(compile_compar, elem_ts);
        Label done;

        left->compile_and_store(x64, Storage(STACK));
        
        // RDI = base, RSI = nmemb, RDX = size, RCX = compar
        x64->op(MOVQ, R10, Address(RSP, 0));
        x64->op(LEA, RDI, Address(R10, ARRAY_ELEMS_OFFSET));
        x64->op(MOVQ, RSI, Address(R10, ARRAY_LENGTH_OFFSET));
        x64->op(MOVQ, RDX, elem_size);
        x64->op(LEA, RCX, Address(compar, 0));
        
        x64->runtime->call_sysv(x64->runtime->sysv_sort_label);
        
        left->ts.store(Storage(STACK), Storage(), x64);
        
        return Storage();
    }
    
    static void compile_compar(Label label, TypeSpec elem_ts, X64 *x64) {
        // Generate a SysV function to wrap our compare function.
        // RDI and RSI contains the pointers to the array elements.
        x64->code_label(label);
        
        Storage a(MEMORY, Address(RDI, 0));
        Storage b(MEMORY, Address(RSI, 0));

        elem_ts.compare(a, b, x64);
        
        x64->op(MOVSXBQ, RAX, R10B);
        
        x64->op(RET);
    }
};


class ArrayRemoveValue: public GenericValue {
public:
    TypeSpec elem_ts;
    
    ArrayRemoveValue(Value *l, TypeMatch &match)
        :GenericValue(INTEGER_TS, match[0], l) {
        elem_ts = match[1];
    }

    virtual Regs precompile(Regs preferred) {
        return left->precompile(preferred) | right->precompile(preferred) | Regs::all();  // SysV
    }

    virtual Storage compile(X64 *x64) {
        Label remove_label = x64->once->compile(compile_remove, elem_ts);

        left->compile_and_store(x64, Storage(STACK));
        right->compile_and_store(x64, Storage(STACK));
        
        x64->op(CALL, remove_label);
        
        x64->op(POPQ, R10);
        
        return Storage(STACK);
    }
    
    static void compile_remove(Label label, TypeSpec elem_ts, X64 *x64) {
        int elem_size = array_elem_size(elem_ts);

        x64->code_label(label);
        Label ok, loop, check;
        
        x64->op(MOVQ, RAX, Address(RSP, ADDRESS_SIZE + INTEGER_SIZE));
        x64->op(MOVQ, RCX, Address(RAX, ARRAY_LENGTH_OFFSET));
        x64->op(CMPQ, RCX, Address(RSP, ADDRESS_SIZE));
        x64->op(JAE, ok);
        
        x64->runtime->die("Array remove length out of bounds!");
        
        // Destroy the first elements
        x64->code_label(ok);
        x64->op(MOVQ, RCX, 0);
        x64->op(JMP, check);
        
        x64->code_label(loop);
        x64->op(IMUL3Q, RDX, RCX, elem_size);
        
        elem_ts.destroy(Storage(MEMORY, Address(RAX, RDX, ARRAY_ELEMS_OFFSET)), x64);
        x64->op(INCQ, RCX);
        
        x64->code_label(check);
        x64->op(CMPQ, RCX, Address(RSP, ADDRESS_SIZE));
        x64->op(JB, loop);

        x64->op(SUBQ, Address(RAX, ARRAY_LENGTH_OFFSET), RCX);

        // call memmove - RDI = dest, RSI = src, RDX = n
        x64->op(LEA, RDI, Address(RAX, ARRAY_ELEMS_OFFSET));  // dest
        
        x64->op(IMUL3Q, RSI, RCX, elem_size);
        x64->op(ADDQ, RSI, RDI);  // src
        
        x64->op(MOVQ, RDX, Address(RAX, ARRAY_LENGTH_OFFSET));
        x64->op(IMUL3Q, RDX, RDX, elem_size);  // n
        
        x64->runtime->call_sysv_got(x64->once->import_got("memmove"));

        x64->op(RET);
    }
};


class ArrayRefillValue: public Value {
public:
    std::unique_ptr<Value> array_value;
    std::unique_ptr<Value> fill_value;
    std::unique_ptr<Value> length_value;
    TypeSpec elem_ts;
    
    ArrayRefillValue(Value *l, TypeMatch &match)
        :Value(match[0]) {
        array_value.reset(l);
        elem_ts = match[1];
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
        return array_value->precompile(preferred) | fill_value->precompile(preferred) | length_value->precompile(preferred) | Regs(RAX, RCX, RDX);
    }
    
    virtual Storage compile(X64 *x64) {
        Label ok, loop, check;
        int elem_size = container_elem_size(elem_ts);
        int stack_size = elem_ts.measure_stack();
        Label realloc_array = x64->once->compile(compile_array_realloc, elem_ts);

        array_value->compile_and_store(x64, Storage(ALISTACK));
        fill_value->compile_and_store(x64, Storage(STACK));
        length_value->compile_and_store(x64, Storage(STACK));
        
        x64->op(MOVQ, R10, Address(RSP, 0));  // extra length
        x64->op(MOVQ, RDX, Address(RSP, stack_size + INTEGER_SIZE));  // array alias
        x64->op(MOVQ, RAX, Address(RDX, 0));  // array ref without incref
        
        x64->op(ADDQ, R10, Address(RAX, ARRAY_LENGTH_OFFSET));
        x64->op(CMPQ, R10, Address(RAX, ARRAY_RESERVATION_OFFSET));
        x64->op(JBE, ok);
        
        // Need to reallocate
        x64->op(CALL, realloc_array);
        x64->op(MOVQ, Address(RDX, 0), RAX);
        
        x64->code_label(ok);
        x64->op(MOVQ, RDX, Address(RAX, ARRAY_LENGTH_OFFSET));
        x64->op(IMUL3Q, RDX, RDX, elem_size);
        x64->op(POPQ, RCX);  // extra length
        x64->op(ADDQ, Address(RAX, ARRAY_LENGTH_OFFSET), RCX);
        x64->op(JMP, check);
        
        x64->code_label(loop);
        elem_ts.create(Storage(MEMORY, Address(RSP, 0)), Storage(MEMORY, Address(RAX, RDX, ARRAY_ELEMS_OFFSET)), x64);
        x64->op(ADDQ, RDX, elem_size);
        x64->op(DECQ, RCX);
        
        x64->code_label(check);
        x64->op(CMPQ, RCX, 0);
        x64->op(JNE, loop);
        
        elem_ts.store(Storage(STACK), Storage(), x64);
        
        return Storage(ALISTACK);
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


class ArrayReservedValue: public ContainerReservedValue {
public:
    ArrayReservedValue(TypeSpec ts)
        :ContainerReservedValue(ts) {
    }

    virtual Storage compile(X64 *x64) {
        return subcompile(compile_array_alloc, x64);
    }
};


class ArrayAllValue: public ContainerAllValue {
public:
    ArrayAllValue(TypeSpec ts)
        :ContainerAllValue(ts) {
    }

    virtual Storage compile(X64 *x64) {
        return subcompile(ARRAY_LENGTH_OFFSET, ARRAY_ELEMS_OFFSET, compile_array_alloc, x64);
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
        
        x64->op(IMUL3Q, R10, R10, elem_size);
        x64->op(LEA, r.reg, Address(r.reg, R10, ARRAY_ELEMS_OFFSET));
        
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
        
        x64->op(MOVQ, r.reg, R10);
        
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
        x64->op(MOVQ, Address(RSP, 0), R10);
        
        x64->op(IMUL3Q, R10, R10, elem_size);
        x64->op(LEA, r.reg, Address(r.reg, R10, ARRAY_ELEMS_OFFSET));
        
        Storage s = Storage(MEMORY, Address(r.reg, 0));
        Storage t = Storage(MEMORY, Address(RSP, INTEGER_SIZE));
        elem_ts.create(s, t, x64);
        
        return Storage(STACK);
    }
};

