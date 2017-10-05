
class SimpleRecordValue: public GenericValue {
public:
    Variable *variable;
    
    SimpleRecordValue(TypeSpec ret_ts, Value *pivot)
        :GenericValue(VOID_TS, ret_ts, pivot) {
        variable = NULL;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() != 0 || kwargs.size() != 0) {
            std::cerr << "Whacky simple record!\n";
            return false;
        }

        variable = new Variable("<new>", VOID_TS, ts);
        scope->add(variable);

        return true;
    }

    virtual Regs precompile(Regs preferred) {
        return left->precompile(preferred);
    }

    virtual void simple_compile(Storage ls, Storage rec_storage, X64 *x64) {
        throw INTERNAL_ERROR;
    }

    virtual Storage compile(X64 *x64) {
        Storage fn_storage(MEMORY, Address(RBP, 0));  // this must be a local variable
        Storage rec_storage = variable->get_storage(fn_storage);
        
        if (rec_storage.where != MEMORY)
            throw INTERNAL_ERROR;

        ls = left->compile(x64);
        
        simple_compile(ls, rec_storage, x64);
        
        return rec_storage;
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

    virtual void simple_compile(Storage ls, Storage rec_storage, X64 *x64) {
        switch (ls.where) {
        case CONSTANT:
            x64->op(MOVQ, rec_storage.address, ls.value);
            x64->op(MOVQ, rec_storage.address + 8, 0);
            break;
        case REGISTER:
            x64->op(MOVQ, rec_storage.address, ls.reg);
            x64->op(MOVQ, rec_storage.address + 8, 0);
            break;
        case MEMORY:
            x64->op(MOVQ, RBX, ls.address);
            x64->op(MOVQ, rec_storage.address, RBX);
            x64->op(MOVQ, rec_storage.address + 8, 0);
            break;
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class CountdownValue: public SimpleRecordValue {
public:
    CountdownValue(Value *l, TypeMatch &match)
        :SimpleRecordValue(COUNTDOWN_TS, l) {
    }

    virtual void simple_compile(Storage ls, Storage rec_storage, X64 *x64) {
        switch (ls.where) {
        case CONSTANT:
            x64->op(MOVQ, rec_storage.address, -1);
            x64->op(MOVQ, rec_storage.address + 8, ls.value - 1);
            break;
        case REGISTER:
            x64->op(MOVQ, rec_storage.address, -1);
            x64->op(DECQ, ls.reg);
            x64->op(MOVQ, rec_storage.address + 8, ls.reg);
            break;
        case MEMORY:
            x64->op(MOVQ, RBX, ls.address);
            x64->op(DECQ, RBX);
            x64->op(MOVQ, rec_storage.address, -1);
            x64->op(MOVQ, rec_storage.address + 8, RBX);
            break;
        default:
            throw INTERNAL_ERROR;
        }
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
        ls = left->compile(x64);
        Register reg = (clob & ~ls.regs()).get_any();
        Label ok;
        
        switch (ls.where) {
        case MEMORY:
            x64->op(MOVQ, RBX, ls.address + 8);  // value
            x64->op(MOVQ, reg, ls.address); // array reference
            x64->op(CMPQ, RBX, x64->array_length_address(reg));
            x64->op(JNE, ok);
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
    Variable *result_var;
    
    ArrayNextItemValue(Value *l, TypeMatch &match)
        :ArrayNextValue(typesubst(SAME_ITEM_TS, match), match[1], l, false) {
        elem_ts = match[1];
        result_var = NULL;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!ArrayNextValue::check(args, kwargs, scope))
            return false;
            
        result_var = new Variable("<item>", VOID_TS, ts);
        scope->add(result_var);
        return true;
    }
    
    virtual Storage next_compile(Register reg, X64 *x64) {
        Storage fn_storage(MEMORY, Address(RBP, 0));  // this must be a local variable
        Storage rec_storage = result_var->get_storage(fn_storage);
        
        if (rec_storage.where != MEMORY)
            throw INTERNAL_ERROR;
            
        x64->op(MOVQ, rec_storage.address, RBX);
        x64->op(IMUL3Q, RBX, RBX, elem_size);
        x64->op(ADDQ, reg, RBX);
        
        Storage s = Storage(MEMORY, x64->array_items_address(reg));
        Storage t = Storage(MEMORY, rec_storage.address + 8);
        elem_ts.store(s, t, x64);
        
        return rec_storage;
    }
};


class ArrayIterValue: public SimpleRecordValue {
public:
    ArrayIterValue(TypeSpec t, Value *l)
        :SimpleRecordValue(t, l) {
    }

    virtual void simple_compile(Storage ls, Storage rec_storage, X64 *x64) {
        switch (ls.where) {
        case REGISTER:
            x64->op(MOVQ, rec_storage.address, ls.reg);
            x64->op(MOVQ, rec_storage.address + 8, 0);
            break;
        case MEMORY:
            x64->op(MOVQ, RBX, ls.address);
            x64->incref(RBX);
            x64->op(MOVQ, rec_storage.address, RBX);
            x64->op(MOVQ, rec_storage.address + 8, 0);
            break;
        default:
            throw INTERNAL_ERROR;
        }
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
