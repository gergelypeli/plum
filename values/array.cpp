
TypeSpec array_elem_ts(TypeSpec ts) {
    TypeSpec ets = ts.rvalue().unprefix(reference_type).unprefix(array_type);
    
    if (is_heap_type(ets[0]))
        ets = ets.prefix(reference_type);
        
    return ets;
}


std::map<TypeSpec, Label> array_finalizer_labels;


// TODO
void compile_array_finalizer(TypeSpec ets, X64 *x64) {
    int elem_size = ::elem_size(ets.measure(MEMORY));
    Label start, end, loop;
    
    x64->op(PUSHQ, RAX);
    x64->op(PUSHQ, RBX);  // finalizers must protect RBX
    x64->op(PUSHQ, RCX);
    
    x64->op(MOVQ, RAX, Address(RSP, 32));
    x64->op(MOVQ, RCX, x64->array_length_address(RAX));
    x64->op(CMPQ, RCX, 0);
    x64->op(JE, end);
    
    x64->op(IMUL3Q, RBX, RCX, elem_size);
    x64->op(LEA, RAX, x64->array_elems_address(RAX));
    x64->op(ADDQ, RAX, RBX);
    
    x64->code_label(loop);
    x64->op(SUBQ, RAX, elem_size);
    ets.destroy(Storage(MEMORY, Address(RAX, 0)), x64);
    x64->op(DECQ, RCX);
    x64->op(JNE, loop);
    
    x64->code_label(end);
    x64->op(POPQ, RCX);
    x64->op(POPQ, RBX);
    x64->op(POPQ, RAX);
    x64->op(RET);
}


void compile_array_finalizers(X64 *x64) {
    for (auto &kv : array_finalizer_labels) {
        TypeSpec ets = kv.first;
        Label label = kv.second;
        std::cerr << "Compiling " << ets << " Array finalizer.\n";
        
        x64->code_label(label);
        compile_array_finalizer(ets, x64);
    }
}


Label array_finalizer(TypeSpec ets, X64 *x64) {
    x64->once(compile_array_finalizers);
    
    return array_finalizer_labels[ets];
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

    virtual Storage compile(X64 *x64) {
        int size = elem_size(ts.measure(MEMORY));
        
        subcompile(x64);
    
        // TODO: probably this is the point where we need to borrow a reference to the
        // array, and unborrow it sometimes later during the stack unwinding.
    
        // NOTE: the reg we selected is for the left value, and if that uses a register,
        // then it is not available, unless we make sure to use its value before
        // overwriting it. 
    
        switch (ls.where * rs.where) {
        case REGISTER_CONSTANT:
            x64->decref(ls.reg);
            x64->op(LEA, ls.reg, x64->array_elems_address(ls.reg) + rs.value * size);
            return Storage(MEMORY, Address(ls.reg, 0));
        case REGISTER_REGISTER:
            x64->decref(ls.reg);
            x64->op(IMUL3Q, rs.reg, rs.reg, size);
            x64->op(ADDQ, ls.reg, rs.reg);
            return Storage(MEMORY, x64->array_elems_address(ls.reg));
        case REGISTER_MEMORY:
            x64->decref(ls.reg);
            x64->op(IMUL3Q, RBX, rs.address, size);
            x64->op(ADDQ, ls.reg, RBX);
            return Storage(MEMORY, x64->array_elems_address(ls.reg));
        case MEMORY_CONSTANT:
            x64->op(MOVQ, reg, ls.address);  // reg may be the base of ls.address
            return Storage(MEMORY, x64->array_elems_address(reg) + rs.value * size);
        case MEMORY_REGISTER:
            x64->op(IMUL3Q, rs.reg, rs.reg, size);
            x64->op(ADDQ, rs.reg, ls.address);
            return Storage(MEMORY, x64->array_elems_address(rs.reg));
        case MEMORY_MEMORY:
            if (reg != ls.address.base) {
                x64->op(IMUL3Q, reg, rs.address, size);  // reg may be the base of rs.address
                x64->op(ADDQ, reg, ls.address);
            }
            else {
                x64->op(MOVQ, reg, ls.address);  // reg is the base of ls.address
                x64->op(IMUL3Q, RBX, rs.address, size);
                x64->op(ADDQ, reg, RBX);
            }
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
        
        x64->op(ADDQ, RSP, 8);
        right->ts.store(rs, Storage(), x64);
        left->ts.store(ls, Storage(), x64);
        
        return Storage(REGISTER, RAX);
    }
    
    static void compile_array_concatenation(X64 *x64) {
        // RAX - result, RBX - elem size, RCX - first, RDX - second
        x64->op(MOVQ, RCX, Address(RSP, 24));
        x64->op(MOVQ, RDX, Address(RSP, 16));
        x64->op(MOVQ, RBX, Address(RSP, 8));
        
        x64->op(MOVQ, RAX, x64->array_length_address(RCX));
        x64->op(ADDQ, RAX, x64->array_length_address(RDX));  // total length in RAX
        x64->op(PUSHQ, RAX);
        
        x64->op(MOVQ, RCX, x64->heap_finalizer_address(RCX));  // for a moment
        
        x64->alloc_array_RAX_RBX_RCX();  // array length, elem size, finalizer
        
        x64->op(POPQ, x64->array_length_address(RAX));

        x64->op(MOVQ, RBX, Address(RSP, 8));  // restored
        x64->op(MOVQ, RCX, Address(RSP, 24));  // restored
        
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
        Label array_finalizer_label = array_finalizer(elem_ts, x64);
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
        Label array_finalizer_label = array_finalizer(elem_ts, x64);
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

