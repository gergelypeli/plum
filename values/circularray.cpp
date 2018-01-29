
TypeSpec circularray_elem_ts(TypeSpec ts) {
    return ts.rvalue().unprefix(reference_type).unprefix(circularray_type).varvalue();
}


void fix_index_overflow(Register r, X64 *x64) {
    Label ok;
    x64->op(ADDQ, RBX, Address(r, CIRCULARRAY_FRONT_OFFSET));
    x64->op(CMPQ, RBX, Address(r, CIRCULARRAY_RESERVATION_OFFSET));
    x64->op(JL, ok);

    //x64->err("Fixing index overflow.");
    x64->op(SUBQ, RBX, Address(r, CIRCULARRAY_RESERVATION_OFFSET));
        
    x64->code_label(ok);
}


void fix_index_underflow(Register r, X64 *x64) {
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
    int elem_size = ::elem_size(elem_ts.measure(MEMORY));
    Label finalizer_label = elem_ts.prefix(circularray_type).get_finalizer_label(x64);
    
    x64->code_label_local(label, "x_circularray_alloc");
    x64->op(PUSHQ, RAX);
    x64->op(IMUL3Q, RAX, RAX, elem_size);
    x64->op(ADDQ, RAX, CIRCULARRAY_HEADER_SIZE);
    x64->op(LEARIP, RBX, finalizer_label);
    
    x64->alloc_RAX_RBX();
    
    x64->op(POPQ, Address(RAX, CIRCULARRAY_RESERVATION_OFFSET));
    x64->op(MOVQ, Address(RAX, CIRCULARRAY_LENGTH_OFFSET), 0);
    x64->op(MOVQ, Address(RAX, CIRCULARRAY_FRONT_OFFSET), 0);
    
    x64->op(RET);
}


void compile_realloc_circularray(Label label, TypeSpec elem_ts, X64 *x64) {
    // RAX - array, RBX - new reservation
    int elem_size = ::elem_size(elem_ts.measure(MEMORY));

    x64->code_label_local(label, "x_circularray_realloc");
    //x64->log("realloc_array");
    x64->op(MOVQ, Address(RAX, CIRCULARRAY_RESERVATION_OFFSET), RBX);
    x64->op(IMUL3Q, RBX, RBX, elem_size);
    x64->op(ADDQ, RBX, CIRCULARRAY_HEADER_SIZE);
    
    x64->realloc_RAX_RBX();
    
    x64->op(RET);
}


void compile_grow_circularray(Label label, TypeSpec elem_ts, X64 *x64) {
    // RAX - array, RBX - new reservation
    // Double the reservation until it's enough (can be relaxed to 1.5 times, but not less)
    // TODO: move the unfolding/stretching code here!
    Label realloc_circularray = x64->once->compile(compile_realloc_circularray, elem_ts);

    x64->code_label_local(label, "x_circularray_grow");
    x64->log("grow_circularray");
    Label more, check;
    
    x64->op(XCHGQ, RBX, Address(RAX, CIRCULARRAY_RESERVATION_OFFSET));  // save desired value
    x64->op(CMPQ, RBX, CIRCULARRAY_MINIMUM_RESERVATION);
    x64->op(JAE, check);
    x64->op(MOVQ, RBX, CIRCULARRAY_MINIMUM_RESERVATION);
    x64->op(JMP, check);
    
    x64->code_label(more);
    x64->op(SHLQ, RBX, 1);
    x64->code_label(check);
    x64->op(CMPQ, RBX, Address(RAX, CIRCULARRAY_RESERVATION_OFFSET));
    x64->op(JB, more);
    
    x64->op(CALL, realloc_circularray);
    
    x64->op(RET);
}


class CircularrayLengthValue: public GenericValue {
public:
    Register reg;
    
    CircularrayLengthValue(Value *l, TypeMatch &match)
        :GenericValue(VOID_TS, INTEGER_TS, l) {
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
            x64->op(MOVQ, ls.reg, Address(ls.reg, CIRCULARRAY_LENGTH_OFFSET));
            return Storage(REGISTER, ls.reg);
        case MEMORY:
            x64->op(MOVQ, reg, ls.address);
            x64->op(MOVQ, reg, Address(reg, CIRCULARRAY_LENGTH_OFFSET));
            return Storage(REGISTER, reg);
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class CircularrayItemValue: public GenericOperationValue {
public:
    CircularrayItemValue(OperationType o, Value *pivot, TypeMatch &match)
        :GenericOperationValue(o, INTEGER_TS, match[1].lvalue(), pivot) {
    }

    virtual void fix_index(Register r, X64 *x64) {
        fix_index_overflow(r, x64);
    }

    virtual Storage compile(X64 *x64) {
        int size = elem_size(ts.measure(MEMORY));
        
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
            x64->op(IMUL3Q, RBX, RBX, size);
            x64->op(ADDQ, ls.reg, RBX);
            return Storage(MEMORY, Address(ls.reg, CIRCULARRAY_ELEMS_OFFSET));
        case MEMORY:
            x64->op(MOVQ, reg, ls.address);  // reg may be the base of ls.address
            fix_index(reg, x64);
            x64->op(IMUL3Q, RBX, RBX, size);
            x64->op(ADDQ, reg, RBX);
            return Storage(MEMORY, Address(reg, CIRCULARRAY_ELEMS_OFFSET));
        default:
            throw INTERNAL_ERROR;
        }
    }    
};


class CircularrayEmptyValue: public GenericValue {
public:
    TypeSpec elem_ts;
    
    CircularrayEmptyValue(TypeSpec ts)
        :GenericValue(VOID_TS, ts, NULL) {
        elem_ts = circularray_elem_ts(ts);
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob;
        return clob.add(RAX).add(RBX).add(RCX);
    }

    virtual Storage compile(X64 *x64) {
        //TypeSpec ats = ts.unprefix(reference_type);
        //Label array_finalizer_label = ats.get_finalizer_label(x64);
        //int elem_size = ::elem_size(elem_ts.measure(MEMORY));
        Label alloc_circularray = x64->once->compile(compile_alloc_circularray, elem_ts);
        
        x64->op(MOVQ, RAX, 0);
        x64->op(CALL, alloc_circularray);
        
        return Storage(REGISTER, RAX);
    }
};


class CircularrayInitializerValue: public Value {
public:
    TypeSpec elem_ts;
    std::vector<std::unique_ptr<Value>> elems;

    CircularrayInitializerValue(TypeSpec ts)
        :Value(ts) {
        elem_ts = circularray_elem_ts(ts);
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (kwargs.size() != 0) {
            std::cerr << "Whacky circularray initializer keyword argument!\n";
            return false;
        }
        
        for (auto &arg : args) {
            Value *v = typize(arg.get(), scope, &elem_ts);
            if (!v)
                return false;
                
            TypeMatch match;
            
            if (!typematch(elem_ts, v, match)) {
                std::cerr << "Circularray element is not " << elem_ts << ", but " << get_typespec(v) << "!\n";
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

    virtual Storage compile(X64 *x64) {
        //TypeSpec ats = ts.unprefix(reference_type);
        //Label array_finalizer_label = ats.get_finalizer_label(x64);
        int elem_size = ::elem_size(elem_ts.measure(MEMORY));
        Label alloc_circularray = x64->once->compile(compile_alloc_circularray, elem_ts);
    
        x64->op(MOVQ, RAX, elems.size());
        x64->op(CALL, alloc_circularray);
        x64->op(MOVQ, Address(RAX, CIRCULARRAY_LENGTH_OFFSET), elems.size());
        x64->op(PUSHQ, RAX);
        
        unsigned offset = 0;
        
        for (auto &elem : elems) {
            elem->compile_and_store(x64, Storage(STACK));
            x64->op(MOVQ, RAX, Address(RSP, stack_size(elem_size)));
            Storage t(MEMORY, Address(RAX, CIRCULARRAY_ELEMS_OFFSET + offset));
            
            elem_ts.create(Storage(STACK), t, x64);
            offset += elem_size;
        }
        
        return Storage(STACK);
    }
};


class CircularrayPushValue: public GenericValue {
public:
    TypeSpec elem_ts;
    
    CircularrayPushValue(Value *l, TypeMatch &match)
        :GenericValue(circularray_elem_ts(match[0]), match[0], l) {
        elem_ts = circularray_elem_ts(match[0]);
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = left->precompile(preferred) | right->precompile(preferred);
        return clob.add(RAX).add(RBX).add(RCX);
    }

    virtual void fix_index(Register r, X64 *x64) {
        fix_index_overflow(r, x64);
    }

    virtual Storage compile(X64 *x64) {
        compile_and_store_both(x64, Storage(STACK), Storage(STACK));
    
        int elem_size = ::elem_size(elem_ts.measure(MEMORY));
        int stack_size = ::stack_size(elem_ts.measure(MEMORY));
        Label ok, end;
        
        x64->op(MOVQ, RAX, Address(RSP, stack_size));
        x64->op(MOVQ, RBX, Address(RAX, CIRCULARRAY_LENGTH_OFFSET));
        x64->op(CMPQ, RBX, Address(RAX, CIRCULARRAY_RESERVATION_OFFSET));
        x64->op(JNE, ok);
        
        //elem_ts.destroy(Storage(STACK), x64);  // FIXME: what to do here?
        x64->die("Circularray full!");
        x64->op(JMP, end);
        
        x64->code_label(ok);
        x64->op(INCQ, Address(RAX, CIRCULARRAY_LENGTH_OFFSET));

        // RBX contains the index of the newly created element
        fix_index(RAX, x64);
        
        x64->op(IMUL3Q, RBX, RBX, elem_size);
        x64->op(ADDQ, RAX, RBX);
        
        elem_ts.create(Storage(STACK), Storage(MEMORY, Address(RAX, CIRCULARRAY_ELEMS_OFFSET)), x64);
        
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


class CircularrayUnshiftValue: public CircularrayPushValue {
public:
    CircularrayUnshiftValue(Value *l, TypeMatch &match)
        :CircularrayPushValue(l, match) {
    }

    virtual void fix_index(Register r, X64 *x64) {
        // Compute the new front, and use it for the element index
        x64->op(MOVQ, RBX, -1);
        fix_index_underflow(r, x64);
        x64->op(MOVQ, Address(r, CIRCULARRAY_FRONT_OFFSET), RBX);
    }
};


class CircularrayShiftValue: public CircularrayPopValue {
public:
    CircularrayShiftValue(Value *l, TypeMatch &match)
        :CircularrayPopValue(l, match) {
    }

    virtual void fix_index(Register r, X64 *x64) {
        // Compute the new front, and use the old one for the element index
        x64->op(MOVQ, RBX, 1);
        fix_index_overflow(r, x64);
        x64->op(XCHGQ, RBX, Address(r, CIRCULARRAY_FRONT_OFFSET));
    }
};
