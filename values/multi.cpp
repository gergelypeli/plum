

class MultiValue: public Value {
public:
    std::vector<std::unique_ptr<Value>> values;
    std::vector<TypeSpec> tss;
    std::vector<Storage> storages;

    MultiValue()
        :Value(NO_TS) {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (kwargs.size() > 0) {
            std::cerr << "Can't handle labels in multi yet!\n";
            return false;
        }

        bool is_lvalue = true;
        bool is_type = true;
        
        for (auto &arg : args) {
            Value *value = typize(arg.get(), scope);
            TypeMatch match;
            
            if (value->ts[0] != lvalue_type)
                is_lvalue = false;
        
            if (value->ts[0] != type_type)
                is_type = false;
                
            values.push_back(std::unique_ptr<Value>(value));
        }
        
        ts = is_lvalue ? MULTI_LVALUE_TS : is_type ? MULTI_TYPE_TS : MULTI_TS;
        
        for (auto &v : values)
            tss.push_back(!is_lvalue && !is_type ? v->ts.rvalue() : v->ts);
        
        return true;
    }

    bool unpack(std::vector<TypeSpec> &t) {
        t = tss;
        return true;
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob;
        
        for (unsigned i = 0; i < values.size(); i++)
            clob = clob | values[i]->precompile();

        return clob;
    }

    virtual Storage compile(X64 *x64) {
        x64->unwind->push(this);

        for (unsigned i = 0; i < values.size(); i++) {
            StorageWhere where = stacked(tss[i].where(AS_ARGUMENT));
            Storage t(where);
            storages.push_back(t);
            
            values[i]->compile_and_store(x64, t);
        }
        
        x64->unwind->pop(this);

        return Storage();  // Well...
    }

    virtual Scope *unwind(X64 *x64) {
        for (int i = storages.size() - 1; i >= 0; i--)
            tss[i].store(storages[i], Storage(), x64);
            
        return NULL;
    }
};


class UnpackingValue: public Value {
public:
    std::unique_ptr<Value> left, right;
    std::vector<TypeSpec> left_tss, right_tss;
    
    UnpackingValue(Value *l, TypeMatch &match)
        :Value(VOID_TS) {
        left.reset(l);
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() == 0 || kwargs.size() != 0) {
            std::cerr << "Whacky unpacking!\n";
            return false;
        }
        
        Value *value;
        
        if (args.size() > 1) {
            value = new MultiValue();
            if (!value->check(args, kwargs, scope))
                return false;
        }
        else
            value = typize(args[0].get(), scope);

        TypeMatch match;
        
        if (!typematch(MULTI_TS, value, match)) {
            std::cerr << "Multivalue is needed in an unpacking!\n";
            return false;
        }
        
        if (!left->unpack(left_tss))
            throw INTERNAL_ERROR;
        
        if (!value->unpack(right_tss))
            throw INTERNAL_ERROR;
            
        for (unsigned i = 0; i < left_tss.size(); i++) {
            TypeSpec left_ts = left_tss[i];
            
            if (i >= right_tss.size())
                break;
            
            TypeSpec &right_ts = right_tss[i];
            
            // TODO: this may be too strict, but we can't call typespec, because we don't
            // have a value for the right side, and we can't convert the type either.
            if (right_ts != left_ts && right_ts != left_ts.rvalue()) {
                std::cerr << "Mismatching types in unpacking!\n";
                return false;
            }
        }
        
        right.reset(value);
        return true;
    }
    
    virtual Regs precompile(Regs preferred) {
        Regs clob = left->precompile(preferred) | right->precompile(preferred);
        clob.add(RAX);
        
        return clob;
    }
    
    virtual Storage compile(X64 *x64) {
        left->compile(x64);
        
        x64->unwind->push(this);
        
        right->compile(x64);
        
        x64->unwind->pop(this);

        std::vector<StorageWhere> right_wheres;
        std::vector<unsigned> right_sizes;
        int right_total = 0;
        
        for (auto &right_ts : right_tss) {
            StorageWhere where = right_ts.where(AS_ARGUMENT);
            right_wheres.push_back(where);
            int size = right_ts.measure_where(where);
            right_sizes.push_back(size);
            right_total += size;
        }

        std::vector<StorageWhere> left_wheres;
        std::vector<unsigned> left_sizes;
        int left_total = 0;
        
        for (auto &left_ts : left_tss) {
            StorageWhere where = left_ts.where(AS_ARGUMENT);
            left_wheres.push_back(where);
            int size = left_ts.measure_where(where);
            left_sizes.push_back(size);
            left_total += size;
        }
            
        int offset = right_total;

        for (int i = right_tss.size() - 1; i >= 0; i--) {
            Storage s, t;

            // Order of these two matters, because we must first load an RSP relative address,
            // then the right may pop an ALISTACK, which moves RSP.
            
            switch (left_wheres[i]) {
            case ALIAS:  // TODO: it would be nice not to have to ALISTACK all these
                t = Storage(MEMORY, Address(RAX, 0));
                left_tss[i].store(Storage(ALIAS, Address(RSP, offset)), t, x64);
                break;
            default:
                throw INTERNAL_ERROR;
            }

            switch (right_wheres[i]) {
            case MEMORY:
                s = Storage(STACK);
                break;
            case ALIAS:
                s = Storage(MEMORY, Address(RCX, 0));
                right_tss[i].store(Storage(ALISTACK), s, x64);
                break;
            default:
                throw INTERNAL_ERROR;
            }
            
            std::cerr << "Unpacking multi member " << i << " from " << s << " to " << t << " occupying " << right_sizes[i] << " bytes.\n";
            right_tss[i].store(s, t, x64);
            
            offset += left_sizes[i];
            offset -= right_sizes[i];
        }
        
        if (offset != left_total)
            throw INTERNAL_ERROR;
            
        x64->op(ADDQ, RSP, left_total);
            
        return Storage();
    }

    virtual Scope *unwind(X64 *x64) {
        for (int i = left_tss.size() - 1; i >= 0; i--) {
            StorageWhere where = stacked(left_tss[i].where(AS_ARGUMENT));
            Storage s(where);
        
            left_tss[i].store(s, Storage(), x64);
        }
            
        return NULL;
    }
};


class ScalarConversionValue: public Value {
public:
    std::unique_ptr<Value> orig;
    std::vector<TypeSpec> tss;
    
    ScalarConversionValue(Value *p)
        :Value(NO_TS) {
        orig.reset(p);
        
        if (!p->unpack(tss))
            throw INTERNAL_ERROR;
            
        ts = tss[0];
    }
    
    virtual Regs precompile(Regs preferred) {
        Regs clob = orig->precompile(preferred);
        clob.add(RAX);
        return clob;
    }
    
    virtual Storage compile(X64 *x64) {
        orig->compile(x64);
        
        Storage t;
        
        for (int i = tss.size() - 1; i >= 0; i--) {
            TypeSpec ts = tss[i];
            StorageWhere where = ts.where(AS_ARGUMENT);
            where = (where == MEMORY ? STACK : where == ALIAS ? ALISTACK : throw INTERNAL_ERROR);
            Storage s(where);
            
            if (i > 0) {
                std::cerr << "Discarding multi member " << i << " from " << s << ".\n";
                ts.store(s, Storage(), x64);
            }
            else {
                // TODO: shouldn't we handle STACK here instead of MEMORY?
                where = ts.where(AS_VALUE);
                t = (where == REGISTER ? Storage(REGISTER, RAX) : where == MEMORY ? Storage(MEMORY, Address(RAX, 0)) : throw INTERNAL_ERROR);
                std::cerr << "Scalarizing multi member " << i << " from " << s << " to " << t << ".\n";
                ts.store(s, t, x64);
            }
        }

        return t;
    }
};
