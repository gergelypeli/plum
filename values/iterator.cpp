
class SimpleRecordValue: public GenericValue {
public:
    SimpleRecordValue(TypeSpec ret_ts, Value *pivot)
        :GenericValue(VOID_TS, ret_ts, pivot) {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() != 0 || kwargs.size() != 0) {
            std::cerr << "Whacky simple record!\n";
            return false;
        }

        return true;
    }

    virtual Regs precompile(Regs preferred) {
        return left->precompile(preferred);
    }
};


class CountupNextValue: public GenericValue {
public:
    Declaration *dummy;
    Regs clob;
    
    CountupNextValue(Value *l, TypeMatch &match)
        :GenericValue(VOID_TS, INTEGER_TS, l) {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() != 0 || kwargs.size() != 0) {
            std::cerr << "Whacky Counter next!\n";
            return false;
        }

        dummy = new Declaration;
        scope->add(dummy);
        
        return true;
    }


    virtual Regs precompile(Regs preferred) {
        clob = left->precompile(preferred);
        
        if (!clob.has_any())
            clob.add(RAX);
        
        return clob;
    }

    virtual void advance(Address addr, X64 *x64) {
        x64->op(INCQ, addr);
    }

    virtual Storage compile(X64 *x64) {
        ls = left->compile(x64);
        Register reg = (clob & ~ls.regs()).get_any();
        Label ok;
        
        switch (ls.where) {
        case MEMORY:
            x64->op(MOVQ, reg, ls.address + 8);  // value
            x64->op(CMPQ, reg, ls.address); // limit
            x64->op(JNE, ok);
            x64->op(MOVB, EXCEPTION_ADDRESS, DONE_EXCEPTION);
            x64->unwind->initiate(dummy, x64);
            x64->code_label(ok);
            advance(ls.address + 8, x64);
            return Storage(REGISTER, reg);
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class CountdownNextValue: public CountupNextValue {
public:

    CountdownNextValue(Value *l, TypeMatch &match)
        :CountupNextValue(l, match) {
    }
    
    virtual void advance(Address addr, X64 *x64) {
        x64->op(DECQ, addr);
    }
};


class CountupValue: public SimpleRecordValue {
public:
    CountupValue(Value *l, TypeMatch &match)
        :SimpleRecordValue(COUNTUP_TS, l) {
    }

    virtual Storage compile(X64 *x64) {
        ls = left->compile(x64);  // integer limit
        
        x64->op(PUSHQ, 0);  // value
        
        switch (ls.where) {
        case CONSTANT:
            x64->op(PUSHQ, ls.value);
            break;
        case REGISTER:
            x64->op(PUSHQ, ls.reg);
            break;
        case MEMORY:
            x64->op(PUSHQ, ls.address);
            break;
        default:
            throw INTERNAL_ERROR;
        }
        
        return Storage(STACK);
    }
};


class CountdownValue: public SimpleRecordValue {
public:
    CountdownValue(Value *l, TypeMatch &match)
        :SimpleRecordValue(COUNTDOWN_TS, l) {
    }

    virtual Storage compile(X64 *x64) {
        ls = left->compile(x64);
        
        switch (ls.where) {  // value
        case CONSTANT:
            x64->op(PUSHQ, ls.value - 1);
            break;
        case REGISTER:
            x64->op(DECQ, ls.reg);
            x64->op(PUSHQ, ls.reg);
            break;
        case MEMORY:
            x64->op(MOVQ, RBX, ls.address);
            x64->op(DECQ, RBX);
            x64->op(PUSHQ, RBX);
            break;
        default:
            throw INTERNAL_ERROR;
        }
        
        x64->op(PUSHQ, -1);
        
        return Storage(STACK);
    }
};


class ArrayNextValue: public GenericValue {
public:
    Declaration *dummy;
    Regs clob;
    int elem_size;
    bool is_down;
    
    ArrayNextValue(TypeSpec t, TypeSpec et, Value *l, bool d)
        :GenericValue(VOID_TS, t, l) {
        elem_size = item_size(et.measure(MEMORY));
        is_down = d;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() != 0 || kwargs.size() != 0) {
            std::cerr << "Whacky Array_iterator next!\n";
            return false;
        }

        dummy = new Declaration;
        scope->add(dummy);
        
        return true;
    }

    virtual Regs precompile(Regs preferred) {
        clob = left->precompile(preferred);
        
        if (!clob.has_any())
            clob.add(RAX);
        
        return clob;
    }

    virtual Storage next_compile(Register reg, X64 *x64) {
        throw INTERNAL_ERROR;
    }

    virtual Storage compile(X64 *x64) {
        ls = left->compile(x64);  // iterator
        Register reg = (clob & ~ls.regs()).get_any();
        Label ok;
        
        switch (ls.where) {
        case MEMORY:
            x64->op(MOVQ, RBX, ls.address + 8);  // value
            x64->op(MOVQ, reg, ls.address); // array reference
            x64->op(CMPQ, RBX, x64->array_length_address(reg));
            x64->op(JNE, ok);
            x64->op(MOVB, EXCEPTION_ADDRESS, DONE_EXCEPTION);
            x64->unwind->initiate(dummy, x64);
            x64->code_label(ok);
            x64->op(is_down ? DECQ : INCQ, ls.address + 8);
            return next_compile(reg, x64);
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class ArrayNextElemValue: public ArrayNextValue {
public:
    ArrayNextElemValue(Value *l, TypeMatch &match)
        :ArrayNextValue(match[1], match[1], l, false) {
    }
    
    virtual Storage next_compile(Register reg, X64 *x64) {
        x64->op(IMUL3Q, RBX, RBX, elem_size);
        x64->op(ADDQ, reg, RBX);
        return Storage(MEMORY, x64->array_items_address(reg));
    }
};


class ArrayNextIndexValue: public ArrayNextValue {
public:
    ArrayNextIndexValue(Value *l, TypeMatch &match)
        :ArrayNextValue(INTEGER_TS, match[1], l, false) {
    }
    
    virtual Storage next_compile(Register reg, X64 *x64) {
        x64->op(MOVQ, reg, RBX);
        return Storage(REGISTER, reg);
    }
};


class ArrayNextItemValue: public ArrayNextValue {
public:
    TypeSpec elem_ts;
    
    ArrayNextItemValue(Value *l, TypeMatch &match)
        :ArrayNextValue(typesubst(SAME_ITEM_TS, match), match[1], l, false) {
        elem_ts = match[1];
    }

    virtual Storage next_compile(Register reg, X64 *x64) {
        x64->op(SUBQ, RSP, ts.measure(STACK));
        x64->op(MOVQ, Address(RSP, 0), RBX);
        x64->op(IMUL3Q, RBX, RBX, elem_size);
        x64->op(ADDQ, reg, RBX);
        
        Storage s = Storage(MEMORY, x64->array_items_address(reg));
        Storage t = Storage(MEMORY, Address(RSP, 8));
        elem_ts.store(s, t, x64);
        
        return Storage(STACK);
    }
};


class ArrayIterValue: public SimpleRecordValue {
public:
    ArrayIterValue(TypeSpec t, Value *l)
        :SimpleRecordValue(t, l) {
    }

    virtual Storage compile(X64 *x64) {
        x64->op(PUSHQ, 0);

        left->compile_and_store(x64, Storage(STACK));
        /*
        switch (ls.where) {
        case REGISTER:
            x64->op(PUSHQ, ls.reg);
            break;
        case STACK:  // this is the case for lazy String operations
            break;
        case MEMORY:
            x64->op(MOVQ, RBX, ls.address);
            x64->incref(RBX);
            x64->op(PUSHQ, RBX);
            break;
        default:
            throw INTERNAL_ERROR;
        }
        */
        return Storage(STACK);
    }
};

class ArrayElemIterValue: public ArrayIterValue {
public:
    ArrayElemIterValue(Value *l, TypeMatch &match)
        :ArrayIterValue(typesubst(SAME_ARRAYELEMITER_TS, match), l) {
    }
};


class ArrayIndexIterValue: public ArrayIterValue {
public:
    ArrayIndexIterValue(Value *l, TypeMatch &match)
        :ArrayIterValue(typesubst(SAME_ARRAYINDEXITER_TS, match), l) {
    }
};


class ArrayItemIterValue: public ArrayIterValue {
public:
    ArrayItemIterValue(Value *l, TypeMatch &match)
        :ArrayIterValue(typesubst(SAME_ARRAYITEMITER_TS, match), l) {
    }
};


class IteratorIterableIterValue: public GenericValue {
public:
    IteratorIterableIterValue(Value *l, TypeMatch &match)
        :GenericValue(VOID_TS, match[0], l) {
    }

    virtual Regs precompile(Regs preferred) {
        return left->precompile(preferred);
    }

    virtual Storage compile(X64 *x64) {
        return left->compile(x64);
    }
};


class StringElemIterValue: public ArrayIterValue {
public:
    StringElemIterValue(Value *l, TypeMatch &match)
        :ArrayIterValue(TypeSpec { SAME_ARRAYELEMITER_TS[0], character_type }, l) {
        // This is kinda ugly
    }
};


class StringIndexIterValue: public ArrayIterValue {
public:
    StringIndexIterValue(Value *l, TypeMatch &match)
        :ArrayIterValue(TypeSpec { SAME_ARRAYINDEXITER_TS[0], character_type }, l) {
    }
};


class StringItemIterValue: public ArrayIterValue {
public:
    StringItemIterValue(Value *l, TypeMatch &match)
        :ArrayIterValue(TypeSpec { SAME_ARRAYITEMITER_TS[0], character_type }, l) {
    }
};
