
class InterfaceType: public Type {
public:
    std::vector<Function *> member_functions;
    std::vector<Implementation *> member_implementations;
    
    InterfaceType(std::string name, Metatypes param_metatypes)
        :Type(name, param_metatypes, interface_metatype) {
    }

    virtual bool complete_type() {
        for (auto &c : inner_scope->contents) {
            Function *f = ptr_cast<Function>(c.get());
            
            if (f) {
                member_functions.push_back(f);
                continue;
            }
            
            FunctionScope *fs = ptr_cast<FunctionScope>(c.get());
            
            if (fs)
                continue;

            Implementation *imp = ptr_cast<Implementation>(c.get());
            
            if (imp) {
                member_implementations.push_back(imp);
                continue;
            }
                
            // OK, we have to forgive built-in implementations of nested interfaces,
            // which may be of any class, but at least they have a qualified name.
            //Identifier *i = ptr_cast<Identifier>(c.get());
            
            //if (i && i->name.find(".") != std::string::npos)
            //    continue;
                
            std::cerr << "Not a function or implementation in an interface!\n";
            throw INTERNAL_ERROR;
            return false;
        }
        
        //std::cerr << "Interface " << name << " has " << member_functions.size() << " member functions.\n";
        return true;
    }

    virtual Allocation measure(TypeMatch tm) {
        return Allocation();
    }

    virtual void create(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        // Assume the target MEMORY is uninitialized
        
        switch (s.where * t.where) {
        case NOWHERE_MEMORY:  // TODO: hm?
            return;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual void destroy(TypeMatch tm, Storage s, X64 *x64) {
        if (s.where == MEMORY) {
            return;
        }
        else
            throw INTERNAL_ERROR;
    }

    virtual StorageWhere where(TypeMatch tm, AsWhat as_what) {
        return (
            as_what == AS_VARIABLE ? MEMORY :
            throw INTERNAL_ERROR
        );
    }

};



class Implementation: public Identifier {
public:
    TypeSpec interface_ts;
    std::string prefix;
    DataScope *implementor_scope;
    Implementation *original_implementation;
    std::vector<std::unique_ptr<Implementation>> shadow_implementations;

    Implementation(std::string name, TypeSpec pts, TypeSpec ifts)
        :Identifier(name, pts) {
        interface_ts = ifts;
        prefix = name + ".";
        implementor_scope = NULL;
        original_implementation = NULL;
        
        InterfaceType *ift = ptr_cast<InterfaceType>(ifts[0]);
        if (!ift)
            throw INTERNAL_ERROR;

        for (auto &imp : ift->member_implementations) {
            shadow_implementations.push_back(std::make_unique<Implementation>(prefix, imp));
        }
    }
    
    Implementation(std::string p, Implementation *oi)
        :Identifier(p + oi->name, NO_TS) {
        interface_ts = oi->interface_ts;
        prefix = name + ".";
        implementor_scope = NULL;
        original_implementation = oi;
        
        for (auto &imp : oi->shadow_implementations) {
            shadow_implementations.push_back(std::make_unique<Implementation>(prefix, imp.get()));
        }
    }

    virtual void set_implementor(DataScope *is, TypeSpec ts) {
        implementor_scope = is;
        
        for (auto &si : shadow_implementations) {
            si->set_implementor(is, ts);
        }
    }
    
    virtual void set_outer_scope(Scope *os) {
        if (original_implementation)
            throw INTERNAL_ERROR;
            
        DataScope *ds = ptr_cast<DataScope>(os);
        if (!ds)
            throw INTERNAL_ERROR;
            
        set_implementor(ds, pivot_ts);
    }

    virtual void set_name(std::string n) {
        throw INTERNAL_ERROR;  // too late!
    }

    virtual TypeSpec get_interface_ts(TypeMatch &match) {
        return typesubst(interface_ts, match);
    }

    virtual Implementation *lookup_implementation(std::string n) {
        if (n == name)
            return this;
        else if (has_prefix(n, prefix)) {
            for (auto &si : shadow_implementations) {
                Implementation *i = si->lookup_implementation(n);
                if (i)
                    return i;
            }
        }
        
        return NULL;
    }

    virtual Value *find_implementation(TypeMatch &match, TypeSpecIter target, Value *orig, TypeSpec &ifts) {
        ifts = get_interface_ts(match);   // pivot match

        if (ifts[0] == *target) {
            // Direct implementation
            //std::cerr << "Found direct implementation.\n";
            return make<ImplementationConversionValue>(this, orig, match);
        }
        else {
            //std::cerr << "Trying indirect implementation with " << ifts << "\n";
            TypeMatch iftm = ifts.match();
            
            for (auto &si : shadow_implementations) {
                Value *v = si->find_implementation(iftm, target, orig, ifts);
                
                if (v)
                    return v;
            }
            
            return NULL;
        }
    }

    virtual bool check_associated(Declaration *decl) {
        // NOTE: this is kinda weird, but correct.
        // If a parametric type implements an interface with the same type parameter
        // used, we can't concretize that here yet. So the fake_match, despite being
        // a replacement, may still have Same types. When getting the argument types
        // from the interface definition, the substitution will replace Same types
        // with Same types. But the functions in the implementation will be similarly
        // parametrized, so the comparison should compare Same to Same, and succeed.
        
        Function *override = ptr_cast<Function>(decl);
        if (!override) {
            std::cerr << "This declaration can't be associated with implementation " << name << "!\n";
            return false;
        }
        
        InterfaceType *interface_type = ptr_cast<InterfaceType>(interface_ts[0]);
        TypeMatch iftm = interface_ts.match();
        TypeMatch empty_match;
        bool found = false;
        
        for (Function *iff : interface_type->member_functions) {
            if (override->does_implement(prefix, empty_match, iff, iftm)) {
                found = true;
                break;
            }
        }
        
        if (!found) {
            std::cerr << "Invalid implementation of function: " << interface_type->name << "." << override->name << "!\n";
            return false;
        }
        
        return true;
    }

    virtual Value *matched(Value *pivot, Scope *scope, TypeMatch &match) {
        return make<ImplementationConversionValue>(this, pivot, match);
    }
};
