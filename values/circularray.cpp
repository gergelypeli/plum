
int circularray_elem_size(TypeSpec elem_ts) {
    return ::elem_size(elem_ts.measure(MEMORY));
}


void fix_RBX_index_overflow(Register r, X64 *x64) {
    Label ok;
    x64->op(ADDQ, RBX, Address(r, CIRCULARRAY_FRONT_OFFSET));
    x64->op(CMPQ, RBX, Address(r, CIRCULARRAY_RESERVATION_OFFSET));
    x64->op(JL, ok);

    //x64->err("Fixing index overflow.");
    x64->op(SUBQ, RBX, Address(r, CIRCULARRAY_RESERVATION_OFFSET));
        
    x64->code_label(ok);
}


void fix_RBX_index_underflow(Register r, X64 *x64) {
    Label ok;
    x64->op(ADDQ, RBX, Address(r, CIRCULARRAY_FRONT_OFFSET));
    x64->op(CMPQ, RBX, 0);
    x64->op(JGE, ok);
        
    //x64->err("Fixing index underflow.");
    x64->op(ADDQ, RBX, Address(r, CIRCULARRAY_RESERVATION_OFFSET));
        
    x64->code_label(ok);
}


void compile_alloc_circularray(Label label, TypeSpec elem_ts, X64 *x64) {
    // RAX - reservation
    int elem_size = circularray_elem_size(elem_ts);
    Label finalizer_label = elem_ts.prefix(circularray_type).get_finalizer_label(x64);
    
    x64->code_label_local(label, "x_circularray_alloc");
    
    alloc_container(CIRCULARRAY_HEADER_SIZE, elem_size, CIRCULARRAY_RESERVATION_OFFSET, finalizer_label, x64);

    x64->op(MOVQ, Address(RAX, CIRCULARRAY_LENGTH_OFFSET), 0);
    x64->op(MOVQ, Address(RAX, CIRCULARRAY_FRONT_OFFSET), 0);
    
    x64->op(RET);
}


void compile_realloc_circularray(Label label, TypeSpec elem_ts, X64 *x64) {
    // RAX - array, RBX - new reservation
    int elem_size = circularray_elem_size(elem_ts);

    x64->code_label_local(label, "x_circularray_realloc");
    //x64->log("realloc_array");
    
    realloc_container(CIRCULARRAY_HEADER_SIZE, elem_size, CIRCULARRAY_RESERVATION_OFFSET, x64);
    
    x64->op(RET);
}


void compile_grow_circularray(Label label, TypeSpec elem_ts, X64 *x64) {
    // RAX - array, RBX - new reservation
    // Double the reservation until it's enough (can be relaxed to 1.5 times, but not less)
    // TODO: move the unfolding/stretching code here!
    Label realloc_label = x64->once->compile(compile_realloc_circularray, elem_ts);

    x64->code_label_local(label, "x_circularray_grow");
    x64->log("grow_circularray");
    
    grow_container(CIRCULARRAY_RESERVATION_OFFSET, CIRCULARRAY_MINIMUM_RESERVATION, realloc_label, x64);
    
    x64->op(RET);
}


class CircularrayLengthValue: public ContainerLengthValue {
public:
    CircularrayLengthValue(Value *l, TypeMatch &match)
        :ContainerLengthValue(l, match) {
    }

    virtual Storage compile(X64 *x64) {
        return subcompile(CIRCULARRAY_LENGTH_OFFSET, x64);
    }
};


class CircularrayIndexValue: public ContainerIndexValue {
public:
    CircularrayIndexValue(OperationType o, Value *pivot, TypeMatch &match)
        :ContainerIndexValue(o, pivot, match) {
    }

    virtual void fix_RBX_index(Register r, X64 *x64) {
        fix_RBX_index_overflow(r, x64);
    }

    virtual Storage compile(X64 *x64) {
        return subcompile(CIRCULARRAY_ELEMS_OFFSET, x64);
    }
};


class CircularrayEmptyValue: public ContainerEmptyValue {
public:
    CircularrayEmptyValue(TypeSpec ts)
        :ContainerEmptyValue(ts) {
    }

    virtual Storage compile(X64 *x64) {
        return subcompile(compile_alloc_circularray, x64);
    }
};


class CircularrayInitializerValue: public ContainerInitializerValue {
public:
    CircularrayInitializerValue(TypeSpec ts)
        :ContainerInitializerValue(ts) {
    }

    virtual Storage compile(X64 *x64) {
        return subcompile(CIRCULARRAY_LENGTH_OFFSET, CIRCULARRAY_ELEMS_OFFSET, compile_alloc_circularray, x64);
    }
};


class CircularrayPushValue: public ContainerPushValue {
public:
    CircularrayPushValue(Value *l, TypeMatch &match)
        :ContainerPushValue(l, match) {
    }

    virtual void fix_RBX_index(Register r, X64 *x64) {
        fix_RBX_index_overflow(r, x64);
    }

    virtual Storage compile(X64 *x64) {
        return subcompile(CIRCULARRAY_RESERVATION_OFFSET, CIRCULARRAY_LENGTH_OFFSET, CIRCULARRAY_ELEMS_OFFSET, x64);
    }
};


class CircularrayPopValue: public ContainerPopValue {
public:
    CircularrayPopValue(Value *l, TypeMatch &match)
        :ContainerPopValue(l, match) {
    }

    virtual void fix_RBX_index(Register r, X64 *x64) {
        fix_RBX_index_overflow(r, x64);
    }

    virtual Storage compile(X64 *x64) {
        return subcompile(CIRCULARRAY_LENGTH_OFFSET, CIRCULARRAY_ELEMS_OFFSET, x64);
    }
};


class CircularrayUnshiftValue: public CircularrayPushValue {
public:
    CircularrayUnshiftValue(Value *l, TypeMatch &match)
        :CircularrayPushValue(l, match) {
    }

    virtual void fix_RBX_index(Register r, X64 *x64) {
        // Compute the new front, and use it for the element index
        x64->op(MOVQ, RBX, -1);
        fix_RBX_index_underflow(r, x64);
        x64->op(MOVQ, Address(r, CIRCULARRAY_FRONT_OFFSET), RBX);
    }
};


class CircularrayShiftValue: public CircularrayPopValue {
public:
    CircularrayShiftValue(Value *l, TypeMatch &match)
        :CircularrayPopValue(l, match) {
    }

    virtual void fix_RBX_index(Register r, X64 *x64) {
        // Compute the new front, and use the old one for the element index
        x64->op(MOVQ, RBX, 1);
        fix_RBX_index_overflow(r, x64);
        x64->op(XCHGQ, RBX, Address(r, CIRCULARRAY_FRONT_OFFSET));
    }
};


// Iteration

class CircularrayElemIterValue: public ContainerIterValue {
public:
    CircularrayElemIterValue(Value *l, TypeMatch &match)
        :ContainerIterValue(typesubst(SAME_CIRCULARRAYELEMITER_TS, match), l) {
    }
};


class CircularrayIndexIterValue: public ContainerIterValue {
public:
    CircularrayIndexIterValue(Value *l, TypeMatch &match)
        :ContainerIterValue(typesubst(SAME_CIRCULARRAYINDEXITER_TS, match), l) {
    }
};


class CircularrayItemIterValue: public ContainerIterValue {
public:
    CircularrayItemIterValue(Value *l, TypeMatch &match)
        :ContainerIterValue(typesubst(SAME_CIRCULARRAYITEMITER_TS, match), l) {
    }
};


class CircularrayNextElemValue: public ContainerNextValue {
public:
    CircularrayNextElemValue(Value *l, TypeMatch &match)
        :ContainerNextValue(match[1].varvalue(), match[1].varvalue(), l, false) {
    }

    virtual Storage compile(X64 *x64) {
        int elem_size = circularray_elem_size(elem_ts);

        Storage r = subcompile(CIRCULARRAY_LENGTH_OFFSET, x64);

        fix_RBX_index_overflow(r.reg, x64);
        
        x64->op(IMUL3Q, RBX, RBX, elem_size);
        x64->op(LEA, r.reg, Address(r.reg, RBX, CIRCULARRAY_ELEMS_OFFSET));
        
        return Storage(MEMORY, Address(r.reg, 0));
    }
};


class CircularrayNextIndexValue: public ContainerNextValue {
public:
    CircularrayNextIndexValue(Value *l, TypeMatch &match)
        :ContainerNextValue(INTEGER_TS, match[1].varvalue(), l, false) {
    }
    
    virtual Storage compile(X64 *x64) {
        Storage r = subcompile(CIRCULARRAY_LENGTH_OFFSET, x64);
        
        x64->op(MOVQ, r.reg, RBX);
        
        return Storage(REGISTER, r.reg);
    }
};


class CircularrayNextItemValue: public ContainerNextValue {
public:
    CircularrayNextItemValue(Value *l, TypeMatch &match)
        :ContainerNextValue(typesubst(SAME_ITEM_TS, match), match[1].varvalue(), l, false) {
    }

    virtual Storage compile(X64 *x64) {
        int elem_size = circularray_elem_size(elem_ts);
        int item_stack_size = ts.measure(STACK);

        Storage r = subcompile(CIRCULARRAY_LENGTH_OFFSET, x64);
        
        x64->op(SUBQ, RSP, item_stack_size);
        x64->op(MOVQ, Address(RSP, 0), RBX);

        fix_RBX_index_overflow(r.reg, x64);

        x64->op(IMUL3Q, RBX, RBX, elem_size);
        x64->op(LEA, r.reg, Address(r.reg, RBX, CIRCULARRAY_ELEMS_OFFSET));
        
        Storage s = Storage(MEMORY, Address(r.reg, 0));
        Storage t = Storage(MEMORY, Address(RSP, INTEGER_SIZE));
        elem_ts.create(s, t, x64);
        
        return Storage(STACK);
    }
};

