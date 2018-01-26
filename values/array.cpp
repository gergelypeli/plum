
TypeSpec array_elem_ts(TypeSpec ts) {
    TypeSpec ets = ts.rvalue().unprefix(reference_type);
    ets = ets.unprefix(ets[0]);  // TODO: just to make it work with multiple array-like types
    
    if (is_heap_type(ets[0]))
        ets = ets.prefix(reference_type);
        
    return ets;
}


void fix_index_overflow(Register r, X64 *x64) {
    Label ok;
    x64->op(ADDQ, RBX, x64->array_front_address(r));
    x64->op(CMPQ, RBX, x64->array_reservation_address(r));
    x64->op(JL, ok);

    //x64->err("Fixing index overflow.");
    x64->op(SUBQ, RBX, x64->array_reservation_address(r));
        
    x64->code_label(ok);
}


void fix_index_underflow(Register r, X64 *x64) {
    Label ok;
    x64->op(ADDQ, RBX, x64->array_front_address(r));
    x64->op(CMPQ, RBX, 0);
    x64->op(JGE, ok);
        
    //x64->err("Fixing index underflow.");
    x64->op(ADDQ, RBX, x64->array_reservation_address(r));
        
    x64->code_label(ok);
}


class ArrayLengthValue: public GenericValue {
public:
    Register reg;
    
    ArrayLengthValue(Value *l, TypeMatch &match)
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
            x64->op(MOVQ, ls.reg, x64->array_length_address(ls.reg));
            return Storage(REGISTER, ls.reg);
        case MEMORY:
            x64->op(MOVQ, reg, ls.address);
            x64->op(MOVQ, reg, x64->array_length_address(reg));
            return Storage(REGISTER, reg);
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class ArrayItemValue: public GenericOperationValue {
public:
    ArrayItemValue(OperationType o, Value *pivot, TypeMatch &match)
        :GenericOperationValue(o, INTEGER_TS, match[1].lvalue(), pivot) {
    }

    virtual void fix_index(Register r, X64 *x64) {
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
            return Storage(MEMORY, x64->array_elems_address(ls.reg));
        case MEMORY:
            x64->op(MOVQ, reg, ls.address);  // reg may be the base of ls.address
            fix_index(reg, x64);
            x64->op(IMUL3Q, RBX, RBX, size);
            x64->op(ADDQ, reg, RBX);
            return Storage(MEMORY, x64->array_elems_address(reg));
        default:
            throw INTERNAL_ERROR;
        }
    }    
};


class ArrayConcatenationValue: public GenericValue {
public:
    ArrayConcatenationValue(Value *l, TypeMatch &match)
        :GenericValue(match[0], match[0], l) {
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = left->precompile(preferred) | right->precompile(preferred);
        return clob.add(RAX).add(RBX).add(RCX).add(RDX).add(RSI).add(RDI);
    }

    virtual Storage compile(X64 *x64) {
        // TODO: this only works for arrays of basic types now, that can be just copied
        Label l = x64->once(compile_array_concatenation);

        compile_and_store_both(x64, Storage(STACK), Storage(STACK));
    
        int elem_size = ::elem_size(array_elem_ts(ts).measure(MEMORY));
        x64->op(PUSHQ, elem_size);
        
        x64->op(CALL, l);
        
        x64->op(ADDQ, RSP, INTEGER_SIZE);
        right->ts.store(rs, Storage(), x64);
        left->ts.store(ls, Storage(), x64);
        
        return Storage(REGISTER, RAX);
    }
    
    static void compile_array_concatenation(Label label, X64 *x64) {
        // RAX - result, RBX - elem size, RCX - first, RDX - second
        x64->code_label_export(label, "arraycat", 0, false);
        
        x64->op(MOVQ, RCX, Address(RSP, ADDRESS_SIZE + INTEGER_SIZE + REFERENCE_SIZE));
        x64->op(MOVQ, RDX, Address(RSP, ADDRESS_SIZE + INTEGER_SIZE));
        x64->op(MOVQ, RBX, Address(RSP, ADDRESS_SIZE));
        
        x64->op(MOVQ, RAX, x64->array_length_address(RCX));
        x64->op(ADDQ, RAX, x64->array_length_address(RDX));  // total length in RAX
        x64->op(PUSHQ, RAX);
        
        x64->op(MOVQ, RCX, x64->heap_finalizer_address(RCX));  // for a moment
        
        x64->alloc_array_RAX_RBX_RCX();  // array length, elem size, finalizer
        
        x64->op(POPQ, x64->array_length_address(RAX));

        x64->op(MOVQ, RBX, Address(RSP, ADDRESS_SIZE));  // restored
        x64->op(MOVQ, RCX, Address(RSP, ADDRESS_SIZE + INTEGER_SIZE + REFERENCE_SIZE));  // restored
        
        x64->op(LEA, RDI, x64->array_elems_address(RAX));
        
        x64->op(LEA, RSI, x64->array_elems_address(RCX));
        x64->op(MOVQ, RCX, x64->array_length_address(RCX));  // first array not needed anymore
        x64->op(IMUL2Q, RCX, RBX);
        x64->op(REPMOVSB);

        x64->op(LEA, RSI, x64->array_elems_address(RDX));
        x64->op(MOVQ, RCX, x64->array_length_address(RDX));
        x64->op(IMUL2Q, RCX, RBX);
        x64->op(REPMOVSB);
        
        x64->op(RET);  // new array in RAX
    }
};


class ArrayReallocValue: public GenericOperationValue {
public:
    ArrayReallocValue(OperationType o, Value *l, TypeMatch &match)
        :GenericOperationValue(o, INTEGER_OVALUE_TS, l->ts, l) {
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = GenericOperationValue::precompile(preferred);
        return clob.add(RAX).add(RCX);
    }

    virtual Storage compile(X64 *x64) {
        // TODO: this only works for arrays of basic types now, that can be just copied
        // TODO: can't any type be moved?
        
        subcompile(x64);
        
        if (ls.where != MEMORY)
            throw INTERNAL_ERROR;

        if (ls.address.base == RAX || ls.address.base == RCX)
            x64->op(PUSHQ, ls.address.base);

        switch (rs.where) {
        case NOWHERE:
            x64->op(MOVQ, RAX, ls.address);
            x64->op(MOVQ, RBX, x64->array_length_address(RAX));  // shrink to fit
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
        
        int elem_size = ::elem_size(array_elem_ts(ts).measure(MEMORY));
        x64->op(MOVQ, RCX, elem_size);
        x64->realloc_array_RAX_RBX_RCX();
        
        if (ls.address.base == RAX || ls.address.base == RCX) {
            x64->op(MOVQ, RBX, RAX);
            x64->op(POPQ, ls.address.base);
            x64->op(MOVQ, ls.address, RBX);
        }
        else
            x64->op(MOVQ, ls.address, RAX);
        
        return ls;
    }
};


class ArraySortValue: public GenericValue {
public:
    TypeSpec elem_ts;
    
    ArraySortValue(Value *l, TypeMatch &match)
        :GenericValue(VOID_TS, VOID_TS, l) {
        elem_ts = match[1];
    }

    virtual Regs precompile(Regs preferred) {
        return Regs::all();
    }

    virtual Storage compile(X64 *x64) {
        Label done, compar;

        left->compile_and_store(x64, Storage(STACK));
        
        // RDI = base, RSI = nmemb, RDX = size, RCX = compar
        x64->op(MOVQ, RBX, Address(RSP, 0));
        x64->op(LEA, RDI, x64->array_elems_address(RBX));
        x64->op(MOVQ, RSI, x64->array_length_address(RBX));
        x64->op(MOVQ, RDX, elem_size(elem_ts.measure(MEMORY)));
        x64->op(LEARIP, RCX, compar);
        
        x64->op(CALL, x64->sort_label);
        
        left->ts.store(Storage(STACK), Storage(), x64);
        
        x64->op(JMP, done);
        
        // Generate a SysV function to wrap our compare function.
        // RDI and RSI contains the pointers to the array elements.
        // RBX must be preserved.
        x64->code_label(compar);
        x64->op(PUSHQ, RBX);
        
        Storage a(MEMORY, Address(RDI, 0));
        Storage b(MEMORY, Address(RSI, 0));
        elem_ts.compare(a, b, x64, RAX);
        
        x64->op(POPQ, RBX);
        x64->op(RET);
        
        x64->code_label(done);
        
        return Storage();
    }
};


class ArrayEmptyValue: public GenericValue {
public:
    TypeSpec elem_ts;
    
    ArrayEmptyValue(TypeSpec ts)
        :GenericValue(VOID_TS, ts, NULL) {
        elem_ts = array_elem_ts(ts);
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob;
        return clob.add(RAX).add(RBX).add(RCX);
    }

    virtual Storage compile(X64 *x64) {
        TypeSpec ats = ts.unprefix(reference_type);
        Label array_finalizer_label = finalizer_label(ats, x64);
        int elem_size = ::elem_size(elem_ts.measure(MEMORY));
    
        x64->op(MOVQ, RAX, 0);
        x64->op(MOVQ, RBX, elem_size);
        x64->op(LEARIP, RCX, array_finalizer_label);
    
        x64->alloc_array_RAX_RBX_RCX();  // array length, elem size, finalizer
        
        return Storage(REGISTER, RAX);
    }
};


class ArrayInitializerValue: public Value {
public:
    TypeSpec elem_ts;
    std::vector<std::unique_ptr<Value>> elems;

    ArrayInitializerValue(TypeSpec ts)
        :Value(ts) {
        elem_ts = array_elem_ts(ts);
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (kwargs.size() != 0) {
            std::cerr << "Whacky array initializer keyword argument!\n";
            return false;
        }
        
        for (auto &arg : args) {
            Value *v = typize(arg.get(), scope, &elem_ts);
            if (!v)
                return false;
                
            TypeMatch match;
            
            if (!typematch(elem_ts, v, match)) {
                std::cerr << "Array element is not " << elem_ts << ", but " << get_typespec(v) << "!\n";
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
        TypeSpec ats = ts.unprefix(reference_type);
        Label array_finalizer_label = finalizer_label(ats, x64);
        int elem_size = ::elem_size(elem_ts.measure(MEMORY));
    
        x64->op(MOVQ, RAX, elems.size());
        x64->op(MOVQ, RBX, elem_size);
        x64->op(LEARIP, RCX, array_finalizer_label);
    
        x64->alloc_array_RAX_RBX_RCX();  // array length, elem size, finalizer
        x64->op(MOVQ, x64->array_length_address(RAX), elems.size());
        x64->op(PUSHQ, RAX);
        
        unsigned offset = 0;
        
        for (auto &elem : elems) {
            Storage s = elem->compile(x64);
            Register reg = s.regs().has(RAX) ? RCX : RAX;
            
            int soffset = (s.where == STACK ? stack_size(elem_size) : 0);
            x64->op(MOVQ, reg, Address(RSP, soffset));
            Storage t(MEMORY, x64->array_elems_address(reg) + offset);
            
            elem_ts.create(s, t, x64);
            offset += elem_size;
        }
        
        x64->op(POPQ, RAX);
        
        return Storage(REGISTER, RAX);
    }
};


class ArrayPushValue: public GenericValue {
public:
    TypeSpec elem_ts;
    
    ArrayPushValue(Value *l, TypeMatch &match)
        :GenericValue(array_elem_ts(match[0]), match[0], l) {
        elem_ts = array_elem_ts(match[0]);
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = left->precompile(preferred) | right->precompile(preferred);
        return clob.add(RAX).add(RBX).add(RCX);
    }

    virtual void fix_index(Register r, X64 *x64) {
    }

    virtual Storage compile(X64 *x64) {
        compile_and_store_both(x64, Storage(STACK), Storage(STACK));
    
        int elem_size = ::elem_size(elem_ts.measure(MEMORY));
        int stack_size = ::stack_size(elem_ts.measure(MEMORY));
        Label ok, end;
        
        x64->op(MOVQ, RAX, Address(RSP, stack_size));
        x64->op(MOVQ, RBX, x64->array_length_address(RAX));
        x64->op(CMPQ, RBX, x64->array_reservation_address(RAX));
        x64->op(JNE, ok);
        
        //elem_ts.destroy(Storage(STACK), x64);  // FIXME: what to do here?
        x64->die("Array full!");
        x64->op(JMP, end);
        
        x64->code_label(ok);
        x64->op(INCQ, x64->array_length_address(RAX));

        // RBX contains the index of the newly created element
        fix_index(RAX, x64);
        
        x64->op(IMUL3Q, RBX, RBX, elem_size);
        x64->op(ADDQ, RAX, RBX);
        
        elem_ts.create(Storage(STACK), Storage(MEMORY, x64->array_elems_address(RAX)), x64);
        
        x64->code_label(end);
        return Storage(STACK);
    }
};


class ArrayPopValue: public GenericValue {
public:
    TypeSpec elem_ts;
    
    ArrayPopValue(Value *l, TypeMatch &match)
        :GenericValue(VOID_TS, array_elem_ts(match[0]), l) {
        elem_ts = array_elem_ts(match[0]);
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = left->precompile(preferred);
        return clob.add(RAX).add(RBX).add(RCX);
    }

    virtual void fix_index(Register r, X64 *x64) {
    }

    virtual Storage compile(X64 *x64) {
        int elem_size = ::elem_size(elem_ts.measure(MEMORY));
        Label ok;
        
        left->compile_and_store(x64, Storage(REGISTER, RAX));
        
        x64->op(CMPQ, x64->array_length_address(RAX), 0);
        x64->op(JNE, ok);
        
        x64->die("Array empty!");
        
        x64->code_label(ok);
        x64->op(DECQ, x64->array_length_address(RAX));
        x64->op(MOVQ, RBX, x64->array_length_address(RAX));

        // RBX contains the index of the newly removed element
        fix_index(RAX, x64);

        x64->op(IMUL3Q, RBX, RBX, elem_size);
        x64->decref(RAX);

        x64->op(ADDQ, RAX, RBX);
        
        elem_ts.store(Storage(MEMORY, x64->array_elems_address(RAX)), Storage(STACK), x64);
        elem_ts.destroy(Storage(MEMORY, x64->array_elems_address(RAX)), x64);
        
        return Storage(STACK);
    }
};


class CircularrayItemValue: public ArrayItemValue {
public:
    CircularrayItemValue(OperationType o, Value *pivot, TypeMatch &match)
        :ArrayItemValue(o, pivot, match) {
    }

    virtual void fix_index(Register r, X64 *x64) {
        fix_index_overflow(r, x64);
    }
};


class CircularrayPushValue: public ArrayPushValue {
public:
    CircularrayPushValue(Value *l, TypeMatch &match)
        :ArrayPushValue(l, match) {
    }

    virtual void fix_index(Register r, X64 *x64) {
        fix_index_overflow(r, x64);
    }
};


class CircularrayPopValue: public ArrayPopValue {
public:
    CircularrayPopValue(Value *l, TypeMatch &match)
        :ArrayPopValue(l, match) {
    }

    virtual void fix_index(Register r, X64 *x64) {
        fix_index_overflow(r, x64);
    }
};


class CircularrayUnshiftValue: public ArrayPushValue {
public:
    CircularrayUnshiftValue(Value *l, TypeMatch &match)
        :ArrayPushValue(l, match) {
    }

    virtual void fix_index(Register r, X64 *x64) {
        // Compute the new front, and use it for the element index
        x64->op(MOVQ, RBX, -1);
        fix_index_underflow(r, x64);
        x64->op(MOVQ, x64->array_front_address(r), RBX);
    }
};


class CircularrayShiftValue: public ArrayPopValue {
public:
    CircularrayShiftValue(Value *l, TypeMatch &match)
        :ArrayPopValue(l, match) {
    }

    virtual void fix_index(Register r, X64 *x64) {
        // Compute the new front, and use the old one for the element index
        x64->op(MOVQ, RBX, 1);
        fix_index_overflow(r, x64);
        x64->op(XCHGQ, RBX, x64->array_front_address(r));
    }
};
