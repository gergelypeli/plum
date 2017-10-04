
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


class ArrayIteratorNextValue: public GenericValue {
public:
    Declaration *dummy;
    Regs clob;
    
    ArrayIteratorNextValue(Value *l, TypeMatch &match)
        :GenericValue(VOID_TS, match[1], l) {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() != 0 || kwargs.size() != 0) {
            std::cerr << "Whacky Iterator next!\n";
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

    virtual Storage compile(X64 *x64) {
        int size = item_size(ts.measure(MEMORY));

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
            x64->op(INCQ, ls.address + 8);
            x64->op(IMUL3Q, RBX, RBX, size);
            x64->op(ADDQ, reg, RBX);
            return Storage(MEMORY, x64->array_items_address(reg));
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class ArrayIterableIterValue: public SimpleRecordValue {
public:
    ArrayIterableIterValue(Value *l, TypeMatch &match)
        :SimpleRecordValue(match[1].prefix(array_iterator_type), l) {
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
