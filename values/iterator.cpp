
class CounterNextValue: public GenericValue {
public:
    Declaration *dummy;
    Regs clob;
    
    CounterNextValue(Value *l, TypeMatch &match)
        :GenericValue(match[0], INTEGER_TS, l) {
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
            x64->op(INCQ, ls.address + 8);
            return Storage(REGISTER, reg);
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
        :GenericValue(match[0], match[1], l) {
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


class ArrayIterableIterValue: public GenericValue {
public:
    Variable *variable;
    
    ArrayIterableIterValue(Value *l, TypeMatch &match)
        :GenericValue(match[0], match[1].prefix(array_iterator_type), l) {
        variable = NULL;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() != 0 || kwargs.size() != 0) {
            std::cerr << "Whacky Iterable iter!\n";
            return false;
        }

        variable = new Variable("<new>", VOID_TS, ts);
        scope->add(variable);

        return true;
    }

    virtual Regs precompile(Regs preferred) {
        return left->precompile(preferred);
    }

    virtual Storage compile(X64 *x64) {
        Storage fn_storage(MEMORY, Address(RBP, 0));  // this must be a local variable
        Storage rec_storage = variable->get_storage(fn_storage);
        
        if (rec_storage.where != MEMORY)
            throw INTERNAL_ERROR;

        ls = left->compile(x64);
        
        switch (ls.where) {
        case REGISTER:
            x64->op(MOVQ, rec_storage.address, ls.reg);
            x64->op(MOVQ, rec_storage.address + 8, 0);
            return rec_storage;
        case MEMORY:
            x64->op(MOVQ, RBX, ls.address);
            x64->incref(RBX);
            x64->op(MOVQ, rec_storage.address, RBX);
            x64->op(MOVQ, rec_storage.address + 8, 0);
            return rec_storage;
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class IteratorIterableIterValue: public GenericValue {
public:
    IteratorIterableIterValue(Value *l, TypeMatch &match)
        :GenericValue(match[0], match[0], l) {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() != 0 || kwargs.size() != 0) {
            std::cerr << "Whacky Iterable iter!\n";
            return false;
        }

        return true;
    }

    virtual Regs precompile(Regs preferred) {
        return left->precompile(preferred);
    }

    virtual Storage compile(X64 *x64) {
        return left->compile(x64);
    }
};
