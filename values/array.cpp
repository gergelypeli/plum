
void compile_array_alloc(Label label, TypeSpec elem_ts, X64 *x64) {
    // R10 - reservation
    int elem_size = ContainerType::get_elem_size(elem_ts);
    Label finalizer_label = elem_ts.prefix(linearray_type).get_finalizer_label(x64);
    
    x64->code_label_local(label, elem_ts.prefix(array_type).symbolize("alloc"));
    
    container_alloc(LINEARRAY_HEADER_SIZE, elem_size, LINEARRAY_RESERVATION_OFFSET, finalizer_label, x64);

    x64->op(MOVQ, Address(RAX, LINEARRAY_LENGTH_OFFSET), 0);
    
    x64->op(RET);
}


void compile_array_realloc(Label label, TypeSpec elem_ts, X64 *x64) {
    // RAX - array, R10 - new reservation
    int elem_size = ContainerType::get_elem_size(elem_ts);

    x64->code_label_local(label, elem_ts.prefix(array_type).symbolize("realloc"));

    container_realloc(LINEARRAY_HEADER_SIZE, elem_size, LINEARRAY_RESERVATION_OFFSET, x64);

    x64->op(RET);
}


void compile_array_grow(Label label, TypeSpec elem_ts, X64 *x64) {
    // RAX - array, R10 - new reservation
    // Double the reservation until it's enough (can be relaxed to 1.5 times, but not less)
    Label realloc_label = x64->once->compile(compile_array_realloc, elem_ts);

    x64->code_label_local(label, elem_ts.prefix(array_type).symbolize("grow"));
    
    container_grow(LINEARRAY_RESERVATION_OFFSET, LINEARRAY_MINIMUM_RESERVATION, realloc_label, x64);
    
    x64->op(RET);
}


void compile_array_clone(Label label, TypeSpec elem_ts, X64 *x64) {
    // RAX - Linearray Ref
    // Return a cloned Ref
    Label loop, end;
    Label alloc_label = x64->once->compile(compile_array_alloc, elem_ts);
    int elem_size = ContainerType::get_elem_size(elem_ts);
    TypeSpec heap_ts = elem_ts.prefix(linearray_type);
    
    x64->code_label_local(label, elem_ts.prefix(array_type).symbolize("clone"));
    x64->runtime->log("XXX array clone");
    
    x64->op(PUSHQ, RAX);
    x64->op(MOVQ, R10, Address(RAX, LINEARRAY_RESERVATION_OFFSET));
    x64->op(CALL, alloc_label);  // clobbers all
    
    x64->op(POPQ, RBX);  // orig
    x64->op(MOVQ, RCX, Address(RBX, LINEARRAY_LENGTH_OFFSET));
    x64->op(MOVQ, Address(RAX, LINEARRAY_LENGTH_OFFSET), RCX);

    // And this is the general version
    x64->op(CMPQ, RCX, 0);
    x64->op(JE, end);

    x64->op(LEA, RSI, Address(RBX, LINEARRAY_ELEMS_OFFSET));
    x64->op(LEA, RDI, Address(RAX, LINEARRAY_ELEMS_OFFSET));
    
    x64->code_label(loop);
    elem_ts.create(Storage(MEMORY, Address(RSI, 0)), Storage(MEMORY, Address(RDI, 0)), x64);
    x64->op(ADDQ, RSI, elem_size);
    x64->op(ADDQ, RDI, elem_size);
    x64->op(DECQ, RCX);
    x64->op(JNE, loop);

    heap_ts.decref(RBX, x64);
    
    x64->code_label(end);
    x64->op(RET);
}


class ArrayLengthValue: public ContainerLengthValue {
public:
    ArrayLengthValue(Value *l, TypeMatch &match)
        :ContainerLengthValue(l, match, match[1].prefix(linearray_type), LINEARRAY_LENGTH_OFFSET) {
    }
};


class ArrayIndexValue: public ContainerIndexValue {
public:
    ArrayIndexValue(OperationType o, Value *pivot, TypeMatch &match)
        :ContainerIndexValue(o, pivot, match, match[1].prefix(linearray_type), LINEARRAY_LENGTH_OFFSET, LINEARRAY_ELEMS_OFFSET) {
    }
};


class ArrayEmptyValue: public ContainerEmptyValue {
public:
    ArrayEmptyValue(TypeSpec ts)
        :ContainerEmptyValue(ts, compile_array_alloc) {
    }
};


class ArrayReservedValue: public ContainerReservedValue {
public:
    ArrayReservedValue(TypeSpec ts)
        :ContainerReservedValue(ts, compile_array_alloc) {
    }
};


class ArrayAllValue: public ContainerAllValue {
public:
    ArrayAllValue(TypeSpec ts)
        :ContainerAllValue(ts, LINEARRAY_LENGTH_OFFSET, LINEARRAY_ELEMS_OFFSET, compile_array_alloc) {
    }
};


class ArrayInitializerValue: public ContainerInitializerValue {
public:
    ArrayInitializerValue(TypeSpec ts)
        :ContainerInitializerValue(ts.unprefix(array_type), ts, LINEARRAY_LENGTH_OFFSET, LINEARRAY_ELEMS_OFFSET, compile_array_alloc) {
    }
};


class ArrayPushValue: public ContainerPushValue {
public:
    ArrayPushValue(Value *l, TypeMatch &match)
        :ContainerPushValue(l, match, LINEARRAY_RESERVATION_OFFSET, LINEARRAY_LENGTH_OFFSET, LINEARRAY_ELEMS_OFFSET, compile_array_clone, compile_array_grow) {
    }
};


class ArrayPopValue: public ContainerPopValue {
public:
    ArrayPopValue(Value *l, TypeMatch &match)
        :ContainerPopValue(l, match, match[1].prefix(linearray_type), LINEARRAY_LENGTH_OFFSET, LINEARRAY_ELEMS_OFFSET, compile_array_clone) {
    }
};

/*
class ArrayAutogrowValue: public ContainerAutogrowValue {
public:
    ArrayAutogrowValue(Value *l, TypeMatch &match)
        :ContainerAutogrowValue(l, match) {
    }
    
    virtual Storage compile(X64 *x64) {
        return subcompile(LINEARRAY_RESERVATION_OFFSET, LINEARRAY_LENGTH_OFFSET, compile_array_grow, x64);
    }
};
*/

class ArrayConcatenationValue: public GenericValue {
public:
    TypeSpec elem_ts;
    
    ArrayConcatenationValue(Value *l, TypeMatch &match)
        :GenericValue(match[0], match[0], l) {
        elem_ts = match[1];
    }

    virtual Regs precompile(Regs preferred) {
        right->precompile_tail();
        left->precompile_tail();
        
        return Regs::all();
    }

    virtual Storage compile(X64 *x64) {
        Label l = x64->once->compile(compile_array_concatenation, elem_ts);

        compile_and_store_both(x64, Storage(STACK), Storage(STACK));
    
        x64->op(CALL, l);  // result array ref in RAX
        
        right->ts.store(rs, Storage(), x64);
        left->ts.store(ls, Storage(), x64);
        
        return Storage(REGISTER, RAX);
    }
    
    static void compile_array_concatenation(Label label, TypeSpec elem_ts, X64 *x64) {
        // RAX - result, R10 - first, R11 - second
        x64->code_label_local(label, elem_ts.prefix(array_type).symbolize("concatenation"));
        Label alloc_array = x64->once->compile(compile_array_alloc, elem_ts);
        int elem_size = ContainerType::get_elem_size(elem_ts);
        
        x64->op(MOVQ, R10, Address(RSP, ADDRESS_SIZE + REFERENCE_SIZE));
        x64->op(MOVQ, R11, Address(RSP, ADDRESS_SIZE));
        
        x64->op(MOVQ, R10, Address(R10, LINEARRAY_LENGTH_OFFSET));
        x64->op(ADDQ, R10, Address(R11, LINEARRAY_LENGTH_OFFSET));  // total length
        
        x64->op(CALL, alloc_array);  // clobbers all
        
        x64->op(LEA, RDI, Address(RAX, LINEARRAY_ELEMS_OFFSET));
        
        // copy left array
        x64->op(MOVQ, R10, Address(RSP, ADDRESS_SIZE + REFERENCE_SIZE));  // restored
        x64->op(LEA, RSI, Address(R10, LINEARRAY_ELEMS_OFFSET));
        x64->op(MOVQ, RCX, Address(R10, LINEARRAY_LENGTH_OFFSET));
        x64->op(ADDQ, Address(RAX, LINEARRAY_LENGTH_OFFSET), RCX);
        
        // This is the raw bytes version
        //x64->op(IMUL3Q, RCX, RCX, elem_size);
        //x64->op(REPMOVSB);
        
        // And this is the general version
        Label left_loop, left_end;
        x64->op(CMPQ, RCX, 0);
        x64->op(JE, left_end);
        
        x64->code_label(left_loop);
        elem_ts.create(Storage(MEMORY, Address(RSI, 0)), Storage(MEMORY, Address(RDI, 0)), x64);
        x64->op(ADDQ, RSI, elem_size);
        x64->op(ADDQ, RDI, elem_size);
        x64->op(DECQ, RCX);
        x64->op(JNE, left_loop);
        x64->code_label(left_end);

        // copy right array
        x64->op(MOVQ, R11, Address(RSP, ADDRESS_SIZE));  // restored
        x64->op(LEA, RSI, Address(R11, LINEARRAY_ELEMS_OFFSET));
        x64->op(MOVQ, RCX, Address(R11, LINEARRAY_LENGTH_OFFSET));
        x64->op(ADDQ, Address(RAX, LINEARRAY_LENGTH_OFFSET), RCX);
        
        // This is the raw bytes version
        //x64->op(IMUL3Q, RCX, RCX, elem_size);
        //x64->op(REPMOVSB);

        // And this is the general version
        Label right_loop, right_end;
        x64->op(CMPQ, RCX, 0);
        x64->op(JE, right_end);
        
        x64->code_label(right_loop);
        elem_ts.create(Storage(MEMORY, Address(RSI, 0)), Storage(MEMORY, Address(RDI, 0)), x64);
        x64->op(ADDQ, RSI, elem_size);
        x64->op(ADDQ, RDI, elem_size);
        x64->op(DECQ, RCX);
        x64->op(JNE, right_loop);
        x64->code_label(right_end);
        
        x64->op(RET);  // new array in RAX
    }
};


class ArrayExtendValue: public GenericValue {
public:
    TypeSpec elem_ts;
    TypeSpec heap_ts;
    
    ArrayExtendValue(Value *l, TypeMatch &match)
        :GenericValue(match[0].rvalue(), match[0].rvalue(), l) {
        elem_ts = match[1];
        heap_ts = elem_ts.prefix(linearray_type);
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        //check_alias(scope);
        
        return GenericValue::check(args, kwargs, scope);
    }

    virtual Regs precompile(Regs preferred) {
        Regs rclob = right->precompile_tail();
        left->precompile(preferred & ~rclob);
        
        return Regs::all();
    }

    virtual Storage compile(X64 *x64) {
        // TODO: This is just a concatenation and an assignment, could be more optimal.
        Label l = x64->once->compile(ArrayConcatenationValue::compile_array_concatenation, elem_ts);

        ls = left->compile_lvalue(x64);

        x64->op(SUBQ, RSP, ADDRESS_SIZE);  // placeholder for borrowed left side Ref
        right->compile_and_store(x64, Storage(STACK));

        Storage als = ls.access(ADDRESS_SIZE * 2);
        x64->runtime->load_lvalue(R10, R11, als);
        x64->op(MOVQ, Address(RSP, ADDRESS_SIZE), R10);  // borrow left side Ref

        x64->op(CALL, l);  // result array ref in RAX

        // Now mimic a ref assignment
        x64->runtime->exchange_lvalue(RAX, R11, als);
        x64->runtime->decref(RAX);
        
        right->ts.store(Storage(STACK), Storage(), x64);
        x64->op(ADDQ, RSP, ADDRESS_SIZE);
        
        return ls;
    }
};


class ArrayReallocValue: public GenericOperationValue {
public:
    TypeSpec elem_ts;

    ArrayReallocValue(OperationType o, Value *l, TypeMatch &match)
        :GenericOperationValue(o, INTEGER_OVALUE_TS, l->ts, l) {
        elem_ts = match[1];
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        //check_alias(scope);
        
        return GenericOperationValue::check(args, kwargs, scope);
    }

    virtual Regs precompile(Regs preferred) {
        GenericOperationValue::precompile(preferred);
        return Regs::all();
    }

    virtual Storage compile(X64 *x64) {
        Label realloc_array = x64->once->compile(compile_array_realloc, elem_ts);

        ls = left->compile_lvalue(x64);

        if (right) {
            right->compile_and_store(x64, Storage(STACK));
            x64->op(POPQ, R10);
        }

        Storage als = ls.access(0);
        x64->runtime->load_lvalue(RAX, R11, als);
        
        if (!right)
            x64->op(MOVQ, R10, Address(RAX, LINEARRAY_LENGTH_OFFSET));  // shrink to fit
            
        x64->op(CALL, realloc_array);  // clobbers all

        // Although realloc may change the address of the container, it is technically
        // the same object with the same refcount, so we can just store it.
        x64->runtime->store_lvalue(RAX, R11, als);
        
        return ls;
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
        return left->precompile_tail() | Regs(RAX, RCX, RDX, RSI, RDI) | COMPARE_CLOB;
    }

    virtual Storage compile(X64 *x64) {
        int elem_size = ContainerType::get_elem_size(elem_ts);
        Label compar = x64->once->compile(compile_compar, elem_ts);
        Label done;

        left->compile_and_store(x64, Storage(STACK));
        
        // RDI = base, RSI = nmemb, RDX = size, RCX = compar
        x64->op(MOVQ, R10, Address(RSP, 0));
        x64->op(LEA, RDI, Address(R10, LINEARRAY_ELEMS_OFFSET));
        x64->op(MOVQ, RSI, Address(R10, LINEARRAY_LENGTH_OFFSET));
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
        return left->precompile_tail() | right->precompile_tail() | Regs::all();  // SysV
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
        int elem_size = ContainerType::get_elem_size(elem_ts);

        x64->code_label(label);
        Label ok, loop, check;
        
        x64->op(MOVQ, RAX, Address(RSP, ADDRESS_SIZE + INTEGER_SIZE));
        x64->op(MOVQ, RCX, Address(RAX, LINEARRAY_LENGTH_OFFSET));
        x64->op(CMPQ, RCX, Address(RSP, ADDRESS_SIZE));
        x64->op(JAE, ok);
        
        x64->runtime->die("Array remove length out of bounds!");  // FIXME: raise instead
        
        // Destroy the first elements
        x64->code_label(ok);
        x64->op(MOVQ, RCX, 0);
        x64->op(JMP, check);
        
        x64->code_label(loop);
        x64->op(IMUL3Q, RDX, RCX, elem_size);
        
        elem_ts.destroy(Storage(MEMORY, Address(RAX, RDX, LINEARRAY_ELEMS_OFFSET)), x64);
        x64->op(INCQ, RCX);
        
        x64->code_label(check);
        x64->op(CMPQ, RCX, Address(RSP, ADDRESS_SIZE));
        x64->op(JB, loop);

        x64->op(SUBQ, Address(RAX, LINEARRAY_LENGTH_OFFSET), RCX);

        // call memmove - RDI = dest, RSI = src, RDX = n
        x64->op(LEA, RDI, Address(RAX, LINEARRAY_ELEMS_OFFSET));  // dest
        
        x64->op(IMUL3Q, RSI, RCX, elem_size);
        x64->op(ADDQ, RSI, RDI);  // src
        
        x64->op(MOVQ, RDX, Address(RAX, LINEARRAY_LENGTH_OFFSET));
        x64->op(IMUL3Q, RDX, RDX, elem_size);  // n
        
        x64->runtime->call_sysv(x64->runtime->sysv_memmove_label);

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
        //check_alias(scope);
        
        ArgInfos infos = {
            { "fill", &elem_ts, scope, &fill_value },
            { "length", &INTEGER_TS, scope, &length_value }
        };
        
        if (!check_arguments(args, kwargs, infos))
            return false;

        return true;
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = length_value->precompile_tail();
        clob = clob | fill_value->precompile_tail();
        clob = clob | array_value->precompile(preferred & ~clob);

        return Regs::all();
    }
    
    virtual Storage compile(X64 *x64) {
        Label ok, loop, check;
        int elem_size = ContainerType::get_elem_size(elem_ts);
        Label realloc_array = x64->once->compile(compile_array_realloc, elem_ts);

        Storage as = array_value->compile_lvalue(x64);
        fill_value->compile_and_store(x64, Storage(STACK));
        length_value->compile_and_store(x64, Storage(STACK));
        
        // borrow array ref
        Storage aas = as.access(fill_value->ts.measure_stack() + INTEGER_TS.measure_stack());
        x64->runtime->load_lvalue(RAX, R11, aas);
        
        x64->op(MOVQ, R10, Address(RSP, 0));  // extra length (keep on stack)
        x64->op(ADDQ, R10, Address(RAX, LINEARRAY_LENGTH_OFFSET));
        x64->op(CMPQ, R10, Address(RAX, LINEARRAY_RESERVATION_OFFSET));
        x64->op(JBE, ok);
        
        // Need to reallocate
        x64->op(CALL, realloc_array);  // clobbers all

        x64->runtime->store_lvalue(RAX, R11, aas);
        
        x64->code_label(ok);
        x64->op(MOVQ, RDX, Address(RAX, LINEARRAY_LENGTH_OFFSET));
        x64->op(IMUL3Q, RDX, RDX, elem_size);
        x64->op(POPQ, RCX);  // pop extra length, leave fill value on stack
        x64->op(ADDQ, Address(RAX, LINEARRAY_LENGTH_OFFSET), RCX);
        x64->op(JMP, check);
        
        x64->code_label(loop);
        // Use MEMORY for the create source, so it won't be popped
        elem_ts.create(Storage(MEMORY, Address(RSP, 0)), Storage(MEMORY, Address(RAX, RDX, LINEARRAY_ELEMS_OFFSET)), x64);
        x64->op(ADDQ, RDX, elem_size);
        x64->op(DECQ, RCX);
        
        x64->code_label(check);
        x64->op(CMPQ, RCX, 0);
        x64->op(JNE, loop);
        
        // drop fill value
        elem_ts.store(Storage(STACK), Storage(), x64);
        
        return as;
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


class ArrayNextElemValue: public ContainerNextValue, public ContainedLvalue {
public:
    ArrayNextElemValue(Value *l, TypeMatch &match)
        :ContainerNextValue(typesubst(SAME_LVALUE_TUPLE1_TS, match), match[1], l, LINEARRAY_LENGTH_OFFSET, false) {
    }

    virtual Regs precompile(Regs preferred) {
        return ContainerNextValue::precompile(preferred) | precompile_contained_lvalue(preferred, lvalue_needed, ts);
    }

    virtual Storage postprocess(Register r, Register i, X64 *x64) {
        int elem_size = ContainerType::get_elem_size(elem_ts);
        
        Address addr = x64->runtime->make_address(r, i, elem_size, LINEARRAY_ELEMS_OFFSET);

        return compile_contained_lvalue(addr, NOREG, ts, x64);
    }
};


class ArrayNextIndexValue: public ContainerNextValue {
public:
    ArrayNextIndexValue(Value *l, TypeMatch &match)
        :ContainerNextValue(INTEGER_TUPLE1_TS, match[1], l, LINEARRAY_LENGTH_OFFSET, false) {
    }
    
    virtual Storage postprocess(Register r, Register i, X64 *x64) {
        return Storage(REGISTER, i);
    }
};


class ArrayNextItemValue: public ContainerNextValue {
public:
    ArrayNextItemValue(Value *l, TypeMatch &match)
        :ContainerNextValue(typesubst(INTEGER_SAME_LVALUE_TUPLE2_TS, match), match[1], l, LINEARRAY_LENGTH_OFFSET, false) {
    }

    virtual Storage postprocess(Register r, Register i, X64 *x64) {
        int elem_size = ContainerType::get_elem_size(elem_ts);
        //int item_stack_size = ts.measure_stack();

        x64->op(PUSHQ, i);
        
        Address addr = x64->runtime->make_address(r, i, elem_size, LINEARRAY_ELEMS_OFFSET);
        
        x64->op(PUSHQ, 0);
        x64->op(LEA, R10, addr);
        x64->op(PUSHQ, R10);
        
        return Storage(STACK);
    }
};
