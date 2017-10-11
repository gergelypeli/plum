

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
        int size = item_size(ts.measure(MEMORY));
        
        subcompile(x64);
    
        // TODO: probably this is the point where we need to borrow a reference to the
        // array, and unborrow it sometimes later during the stack unwinding.
    
        // NOTE: the reg we selected is for the left value, and if that uses a register,
        // then it is not available, unless we make sure to use its value before
        // overwriting it. 
    
        switch (ls.where * rs.where) {
        case REGISTER_CONSTANT:
            x64->decref(ls.reg);
            x64->op(LEA, ls.reg, x64->array_items_address(ls.reg) + rs.value * size);
            return Storage(MEMORY, Address(ls.reg, 0));
        case REGISTER_REGISTER:
            x64->decref(ls.reg);
            x64->op(IMUL3Q, rs.reg, rs.reg, size);
            x64->op(ADDQ, ls.reg, rs.reg);
            return Storage(MEMORY, x64->array_items_address(ls.reg));
        case REGISTER_MEMORY:
            x64->decref(ls.reg);
            x64->op(IMUL3Q, RBX, rs.address, size);
            x64->op(ADDQ, ls.reg, RBX);
            return Storage(MEMORY, x64->array_items_address(ls.reg));
        case MEMORY_CONSTANT:
            x64->op(MOVQ, reg, ls.address);  // reg may be the base of ls.address
            return Storage(MEMORY, x64->array_items_address(reg) + rs.value * size);
        case MEMORY_REGISTER:
            x64->op(IMUL3Q, rs.reg, rs.reg, size);
            x64->op(ADDQ, rs.reg, ls.address);
            return Storage(MEMORY, x64->array_items_address(rs.reg));
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
            return Storage(MEMORY, x64->array_items_address(reg));
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
    
        int item_size = ::item_size(ts.unprefix(reference_type).unprefix(array_type).measure(MEMORY));
        x64->op(PUSHQ, item_size);
        
        x64->op(CALL, l);
        
        x64->op(ADDQ, RSP, 8);
        right->ts.store(rs, Storage(), x64);
        left->ts.store(ls, Storage(), x64);
        
        return Storage(REGISTER, RAX);
    }
    
    static void compile_array_concatenation(X64 *x64) {
        // RAX - result, RBX - item size, RCX - first, RDX - second
        x64->op(MOVQ, RCX, Address(RSP, 24));
        x64->op(MOVQ, RDX, Address(RSP, 16));
        x64->op(MOVQ, RBX, Address(RSP, 8));
        
        x64->op(MOVQ, RAX, x64->array_length_address(RCX));
        x64->op(ADDQ, RAX, x64->array_length_address(RDX));  // total length in RAX
        x64->op(PUSHQ, RAX);
        
        x64->alloc_array_RAX_RBX();  // array length, item size
        
        x64->op(POPQ, x64->array_length_address(RAX));
        
        x64->op(LEA, RDI, x64->array_items_address(RAX));
        
        x64->op(LEA, RSI, x64->array_items_address(RCX));
        x64->op(MOVQ, RCX, x64->array_length_address(RCX));  // first array not needed anymore
        x64->op(IMUL2Q, RCX, RBX);
        x64->op(REPMOVSB);

        x64->op(LEA, RSI, x64->array_items_address(RDX));
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
        
        subcompile(x64);
        
        if (ls.where != MEMORY)
            throw INTERNAL_ERROR;

        if (ls.address.base == RAX)
            x64->op(PUSHQ, RAX);

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
        
        int item_size = ::item_size(ts.rvalue().unprefix(reference_type).unprefix(array_type).measure(MEMORY));
        x64->op(MOVQ, RCX, item_size);
        x64->realloc_array_RAX_RBX_RCX();
        
        if (ls.address.base == RAX) {
            x64->op(MOVQ, RBX, RAX);
            x64->op(POPQ, RAX);
            x64->op(MOVQ, ls.address, RBX);
        }
        else
            x64->op(MOVQ, ls.address, RAX);
        
        return ls;
    }
};


class StringCharsValue: public Value {
public:
    std::unique_ptr<Value> pivot;

    StringCharsValue(Value *p)
        :Value(CHARACTER_ARRAY_REFERENCE_TS) {
        pivot.reset(p);
    }

    virtual Regs precompile(Regs preferred) {
        return pivot->precompile(preferred);
    }

    virtual Storage compile(X64 *x64) {
        return pivot->compile(x64);
    }
};


class StringWrapperValue: public GenericValue {
public:
    std::unique_ptr<GenericValue> wrap;
    TypeMatch charmatch;

    StringWrapperValue(TypeSpec arg_ts, TypeSpec ret_ts)
        :GenericValue(arg_ts, ret_ts, NULL) {
        charmatch = TypeMatch { CHARACTER_ARRAY_REFERENCE_TS, CHARACTER_TS };
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!GenericValue::check(args, kwargs, scope))
            return false;
            
        if (right) {
            Value *k = right.release();
        
            if (arg_ts == STRING_TS)
                k = new StringCharsValue(k);
                
            wrap->right.reset(k);
        }
        
        return true;
    }
    
    virtual Regs precompile(Regs preferred) {
        return wrap->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        return wrap->compile(x64);
    }
};


class StringLengthValue: public StringWrapperValue {
public:
    StringLengthValue(Value *l, TypeMatch &match)
        :StringWrapperValue(VOID_TS, INTEGER_TS) {
        Value *k = new StringCharsValue(l);
        wrap.reset(new ArrayLengthValue(k, charmatch));
    }
};


class StringItemValue: public StringWrapperValue {
public:
    StringItemValue(Value *l, TypeMatch &match)
        :StringWrapperValue(INTEGER_TS, CHARACTER_LVALUE_TS) {  // FIXME: should be rvalue!
        Value *k = new StringCharsValue(l);
        wrap.reset(new ArrayItemValue(TWEAK, k, charmatch));
    }
};


class StringConcatenationValue: public StringWrapperValue {
public:
    StringConcatenationValue(Value *l, TypeMatch &match)
        :StringWrapperValue(STRING_TS, STRING_TS) {
        Value *k = new StringCharsValue(l);
        wrap.reset(new ArrayConcatenationValue(k, charmatch));
    }
};


class StringReallocValue: public StringWrapperValue {
public:
    StringReallocValue(Value *l, TypeMatch &match)
        :StringWrapperValue(INTEGER_OVALUE_TS, STRING_TS) {
        Value *k = new StringCharsValue(l);
        wrap.reset(new ArrayReallocValue(TWEAK, k, charmatch));
    }
};


class StringEqualityValue: public GenericValue {
public:
    StringEqualityValue(Value *l, TypeMatch &match)
        :GenericValue(STRING_TS, BOOLEAN_TS, l) {
    }

    virtual Regs precompile(Regs preferred) {
        return left->precompile(preferred) | right->precompile(preferred);
    }

    virtual Storage compile(X64 *x64) {
        compile_and_store_both(x64, Storage(), Storage());
        return Storage(CONSTANT, 1);
    }
};
