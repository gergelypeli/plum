

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

        bool is_lvalue = true;  // or uninitialized, both are handled similarly
        bool is_type = true;
        
        for (auto &arg : args) {
            Value *value = typize(arg.get(), scope);
            TypeMatch match;
            
            if (value->ts[0] != lvalue_type && value->ts[0] != uninitialized_type)
                is_lvalue = false;
            else
                tss.push_back(value->ts);
        
            if (value->ts[0]->type != META_TYPE)
                is_type = false;
            else
                tss.push_back(ptr_cast<TypeValue>(value)->represented_ts);
                
            values.push_back(std::unique_ptr<Value>(value));
            //std::cerr << "Multi item ts: " << value->ts << "\n";
        }

        if (is_lvalue)
            ts = MULTILVALUE_TS;
        else if (is_type)
            ts = MULTITYPE_TS;
        else {
            std::cerr << "Multis must be all lvalues or all types!\n";
            return false;
        }
        
        //for (auto &v : values)
        //    tss.push_back(v->ts);
        
        return true;
    }

    virtual bool unpack(std::vector<TypeSpec> &t) {
        t = tss;
        return true;
    }
    
    virtual std::vector<Storage> get_storages() {
        return storages;
    }
    
    virtual Declaration *get_declaration(int i) {
        DeclarationValue *dv = ptr_cast<DeclarationValue>(values[i].get());
        
        return (dv ? declaration_get_decl(dv) : NULL);
    }
    
    virtual TypeSpec fix_bare(int i, TypeSpec implicit_ts, Scope *scope) {
        // Used if a member is a bare declaration, and its type must be derived from the
        // right hand side of an unpacking
        DeclarationValue *dv = ptr_cast<DeclarationValue>(values[i].get());

        std::cerr << "Fixing bare declaration " << i << " with " << implicit_ts << ".\n";
        Value *tv = make_type_value(type_metatype, implicit_ts);
        
        if (!declaration_use(dv, tv, scope))
            throw INTERNAL_ERROR;
            
        tss[i] = dv->ts.unprefix(uninitialized_type);
        return dv->ts;
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
            // Since lvalues, these will be MEMORY
            Storage s = values[i]->compile(x64);

            if (s.is_clobbered(Regs::all())) {
                StorageWhere where = stacked(tss[i].where(AS_ARGUMENT));
                Storage t(where);
                values[i]->ts.store(s, t, x64);
                storages.push_back(t);
            }
            else {
                storages.push_back(s);
            }
        }
        
        x64->unwind->pop(this);

        return Storage();  // Well...
    }

    virtual Scope *unwind(X64 *x64) {
        for (int i = storages.size() - 1; i >= 0; i--)
            tss[i].store(storages[i], Storage(), x64);  // dropping a MEMORY is a no-op
            
        return NULL;
    }
};


class UnpackingValue: public Value {
public:
    std::unique_ptr<MultiValue> left;
    std::unique_ptr<Value> right;
    std::vector<TypeSpec> left_tss, right_tss;
    int left_total;
    std::vector<Declaration *> declarations;
    Scope *scope;
    
    UnpackingValue(Value *l, TypeMatch &match)
        :Value(VOID_TS) {
        left.reset(ptr_cast<MultiValue>(l));
        
        if (!left)
            throw INTERNAL_ERROR;
            
        left_total = 0;
        scope = NULL;
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() != 1 || kwargs.size() != 0) {
            std::cerr << "Whacky unpacking!\n";
            return false;
        }
        
        // Must put potential declarations on the left after all others, like CreateValue
        for (unsigned i = 0; i < left->values.size(); i++)
            declarations.push_back(left->get_declaration(i));
        
        for (int i = declarations.size() - 1; i >= 0; i--)
            if (declarations[i])
                scope->remove(declarations[i]);
        
        Value *value = typize(args[0].get(), scope);
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
            
            if (i >= right_tss.size()) {
                std::cerr << "Too few values to unpack!\n";
                return false;
            }
            
            TypeSpec &right_ts = right_tss[i];
            
            // TODO: this may be too strict, but we can't call typespec, because we don't
            // have a value for the right side, and we can't convert the type either.
            if (left_ts[0] == uninitialized_type) {
                if (left_ts[1] == void_type) {
                    // Fix bare declaration, and place it in its scope
                    left_ts = left->fix_bare(i, right_ts, scope);
                    declarations[i] = left->get_declaration(i);
                }
                else {
                    // Put typed declaration back to its scope
                    scope->add(declarations[i]);
                }

                left_ts = left_ts.reprefix(uninitialized_type, lvalue_type);
            }
            
            if (right_ts != left_ts.rvalue()) {
                std::cerr << "Mismatching types in unpacking: " << left_ts << " = " << right_ts << "\n";
                return false;
            }
        }
        
        right.reset(value);
        this->scope = scope;
        return true;
    }
    
    virtual Regs precompile(Regs preferred) {
        Regs clob = left->precompile(preferred) | right->precompile(preferred);
        clob.add(RAX);
        
        return clob;
    }
    
    virtual Storage compile(X64 *x64) {
        left->compile(x64);

        std::vector<Storage> left_storages = left->get_storages();
        std::vector<unsigned> left_sizes;
        
        for (auto &left_storage : left_storages) {
            StorageWhere where = left_storage.where;
            int size = (where == MEMORY ? 0 : where == ALIAS ? ALIAS_SIZE : throw INTERNAL_ERROR);
            left_sizes.push_back(size);
            left_total += size;
        }
        
        x64->unwind->push(this);
        right->compile(x64);
        x64->unwind->pop(this);

        std::vector<Storage> right_storages;
        std::vector<unsigned> right_sizes;
        int right_total = 0;
        
        for (auto &right_ts : right_tss) {
            StorageWhere where = stacked(right_ts.where(AS_ARGUMENT));  // always STACK?
            right_storages.push_back(Storage(where));
            int size = right_ts.measure_where(where);
            right_sizes.push_back(size);
            right_total += size;
        }

        int offset = right_total;
        int left_count = left_tss.size();

        for (int i = right_tss.size() - 1; i >= 0; i--) {
            Storage s, t;

            // Order of these two matters, because we must first load an RSP relative address,
            // then the right may pop an ALISTACK, which moves RSP.
            
            switch (i < left_count ? left_storages[i].where : NOWHERE) {
            case NOWHERE:
                t = Storage();
                break;
            case MEMORY:
                t = left_storages[i];
                break;
            case ALIAS:
                x64->op(MOVQ, RAX, Address(RSP, offset));
                t = Storage(MEMORY, Address(RAX, 0));
                break;
            default:
                throw INTERNAL_ERROR;
            }

            switch (right_storages[i].where) {
            case STACK:
                s = right_storages[i];
                break;
            case ALISTACK:  // TODO: is this still possible?
                x64->op(POPQ, RCX);
                s = Storage(MEMORY, Address(RCX, 0));
                break;
            default:
                throw INTERNAL_ERROR;
            }
            
            if (i >= left_count) {
                std::cerr << "Dropping multi member " << i << " from " << s << " to " << t << " occupying " << right_sizes[i] << " bytes.\n";
                right_tss[i].store(s, t, x64);
            }
            else if (left_tss[i][0] == uninitialized_type) {
                std::cerr << "Initializing multi member " << i << " from " << s << " to " << t << " occupying " << right_sizes[i] << " bytes.\n";
                right_tss[i].create(s, t, x64);
            }
            else {
                std::cerr << "Assigning multi member " << i << " from " << s << " to " << t << " occupying " << right_sizes[i] << " bytes.\n";
                right_tss[i].store(s, t, x64);
            }
            
            offset += (i < left_count ? left_sizes[i] : 0);
            offset -= right_sizes[i];
        }
        
        if (offset != left_total)
            throw INTERNAL_ERROR;
            
        // Drop potential ALISTACK-s
        x64->op(ADDQ, RSP, left_total);
            
        return Storage();
    }

    virtual Scope *unwind(X64 *x64) {
        if (left_total)
            x64->op(ADDQ, RSP, left_total);
            
        return NULL;
    }
    
    virtual void escape_statement_variables() {
        for (int i = declarations.size() - 1; i >= 0 ; i--)
            if (declarations[i])
                scope->remove(declarations[i]);
                
        for (unsigned i = 0; i < declarations.size() ; i++)
            if (declarations[i])
                scope->outer_scope->add(declarations[i]);
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
