

class MultiValue: public Value {
public:
    std::vector<std::unique_ptr<Value>> values;
    std::vector<TypeSpec> tss;
    bool is_rvalue;

    MultiValue()
        :Value(BOGUS_TS) {
        is_rvalue = false;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (kwargs.size() > 0) {
            std::cerr << "Can't handle labels in multi yet!\n";
            return false;
        }
        
        for (auto &arg : args) {
            Value *value = typize(arg.get(), scope);
            TypeMatch match;
            
            if (!typematch(ANY_LVALUE_TS, value, match))
                is_rvalue = true;
            
            values.push_back(std::unique_ptr<Value>(value));
        }
        
        ts = is_rvalue ? MULTI_TS : MULTI_LVALUE_TS;
        
        for (auto &v : values)
            tss.push_back(is_rvalue ? v->ts.rvalue() : v->ts);
        
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
        for (unsigned i = 0; i < values.size(); i++) {
            StorageWhere where = tss[i].where(true);
            where = (where == MEMORY ? STACK : where == ALIAS ? ALISTACK : throw INTERNAL_ERROR);
            values[i]->compile_and_store(x64, Storage(where));
        }
        
        return Storage();  // Well...
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
        right->compile(x64);

        std::vector<StorageWhere> right_wheres;
        std::vector<unsigned> right_sizes;
        int right_total = 0;
        
        for (auto &right_ts : right_tss) {
            StorageWhere where = right_ts.where(true);
            right_wheres.push_back(where);
            int size = right_ts.measure(where);
            right_sizes.push_back(size);
            right_total += size;
        }

        std::vector<StorageWhere> left_wheres;
        std::vector<unsigned> left_sizes;
        int left_total = 0;
        
        for (auto &left_ts : left_tss) {
            StorageWhere where = left_ts.where(true);
            left_wheres.push_back(where);
            int size = left_ts.measure(where);
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
            
            std::cerr << "Unpacking item " << i << " from " << s << " to " << t << " occupying " << right_sizes[i] << " bytes.\n";
            right_tss[i].store(s, t, x64);
            
            offset += left_sizes[i];
            offset -= right_sizes[i];
        }
        
        if (offset != left_total)
            throw INTERNAL_ERROR;
            
        x64->op(ADDQ, RSP, left_total);
            
        return Storage();
    }
};
