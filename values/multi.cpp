

class MultiValue: public Value {
public:
    std::vector<std::unique_ptr<Value>> values;

    MultiValue()
        :Value(MULTI_LVALUE_TS) {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (kwargs.size() > 0) {
            std::cerr << "Can't handle labels in multi yet!\n";
            return false;
        }
        
        for (auto &arg : args) {
            Value *value = typize(arg.get(), scope);
            TypeMatch match;
            
            if (!typematch(ANY_LVALUE_TS, value, match)) {
                std::cerr << "Lvalue is needed in a multivalue!\n";
                return false;
            }
            
            values.push_back(std::unique_ptr<Value>(value));
        }
        
        return true;
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob;
        
        for (unsigned i = 0; i < values.size(); i++)
            clob = clob | values[i]->precompile();

        return clob;
    }

    virtual Storage compile(X64 *x64) {
        for (unsigned i = 0; i < values.size(); i++)
            values[i]->compile_and_store(x64, Storage(ALISTACK));
            
        return Storage(ALISTACK);
    }
};


class UnpackingValue: public Value {
public:
    std::unique_ptr<Value> left, right;
    std::vector<TypeSpec> res_tss;
    
    UnpackingValue(Value *l, TypeMatch &match)
        :Value(VOID_TS) {
        left.reset(l);
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() != 1 || kwargs.size() != 0) {
            std::cerr << "Whacky unpacking!\n";
            return false;
        }
        
        Value *value = typize(args[0].get(), scope);
        TypeMatch match;
        
        if (!typematch(MULTI_TS, value, match)) {
            std::cerr << "Multivalue is needed in an unpacking!\n";
            return false;
        }
        
        MultiValue *mv = dynamic_cast<MultiValue *>(left.get());
        if (!mv)
            throw INTERNAL_ERROR;
        
        FunctionCallValue *fcv = dynamic_cast<FunctionCallValue *>(value);
        if (!fcv)
            throw INTERNAL_ERROR;
            
        res_tss = fcv->function->get_result_tss();
        
        for (unsigned i = 0; i < mv->values.size(); i++) {
            Value *v = mv->values[i].get();
            
            if (i >= res_tss.size())
                break;
            
            TypeSpec &res_ts = res_tss[i];
            
            // TODO: this may be too strict, but we can't call typespec, because we don't
            // have a value for the right side, and we can't convert the type either.
            if (res_ts != v->ts.rvalue()) {
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
        Storage x = left->compile(x64);
        if (x.where != ALISTACK)
            throw INTERNAL_ERROR;
            
        Storage y = right->compile(x64);
        if (y.where != STACK)
            throw INTERNAL_ERROR;

        std::vector<StorageWhere> res_wheres;
        std::vector<unsigned> res_sizes;
        
        for (auto &res_ts : res_tss) {
            StorageWhere where = res_ts.where(true);
            res_wheres.push_back(where);
            res_sizes.push_back(res_ts.measure(where));
        }
            
        int aliases_total = 8 * res_tss.size();
        int offset = 0;
        for (unsigned &s : res_sizes)
            offset += s;

        for (int i = res_tss.size() - 1; i >= 0; i--) {
            StorageWhere where = (res_wheres[i] == MEMORY ? STACK : res_wheres[i] == ALIAS ? ALISTACK : throw INTERNAL_ERROR);
            Storage s(where);
            
            Storage t(MEMORY, Address(RAX, 0));
            res_tss[i].store(Storage(ALIAS, Address(RSP, offset)), t, x64);
            
            std::cerr << "Unpacking item " << i << " from " << s << " occupying " << res_sizes[i] << " bytes.\n";
            res_tss[i].store(s, t, x64);
            
            offset += 8;
            offset -= res_sizes[i];
        }
        
        if (offset != aliases_total)
            throw INTERNAL_ERROR;
            
        x64->op(ADDQ, RSP, aliases_total);
            
        return Storage();
    }
};
