
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



class Implementation: public Identifier, public Associable {
public:
    TypeSpec interface_ts;
    std::string prefix;
    DataScope *implementor_scope;
    Implementation *original_implementation;
    std::vector<std::unique_ptr<Implementation>> shadow_implementations;
    std::set<std::string> missing_function_names;
    Lself *associated_lself;

    Implementation(std::string name, TypeSpec pts, TypeSpec ifts)
        :Identifier(name, pts) {
        interface_ts = ifts;
        prefix = name + ".";
        implementor_scope = NULL;
        original_implementation = NULL;
        associated_lself = NULL;
        
        InterfaceType *ift = ptr_cast<InterfaceType>(interface_ts[0]);
        if (!ift)
            throw INTERNAL_ERROR;

        for (auto &imp : ift->member_implementations)
            shadow_implementations.push_back(std::make_unique<Implementation>(prefix, imp));
        
        for (auto &f : ift->member_functions)
            missing_function_names.insert(prefix + f->name);
    }
    
    Implementation(std::string p, Implementation *oi)
        :Identifier(p + oi->name, NO_TS) {
        interface_ts = oi->interface_ts;
        prefix = name + ".";
        implementor_scope = NULL;
        original_implementation = oi;
        
        for (auto &imp : oi->shadow_implementations)
            shadow_implementations.push_back(std::make_unique<Implementation>(prefix, imp.get()));

        InterfaceType *ift = ptr_cast<InterfaceType>(interface_ts[0]);
        if (!ift)
            throw INTERNAL_ERROR;
            
        for (auto &f : ift->member_functions)
            missing_function_names.insert(prefix + f->name);
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
        
        Identifier::set_outer_scope(os);
    }

    virtual void outer_scope_left() {
        for (auto &si : shadow_implementations)
            si->outer_scope_left();
        
        if (missing_function_names.size() > 0) {
            std::cerr << "Missing functions in implementation " << name << "!\n";
            throw TYPE_ERROR;
        }
    }

    virtual void set_name(std::string n) {
        throw INTERNAL_ERROR;  // too late!
    }

    virtual TypeSpec get_interface_ts(TypeMatch &match) {
        return typesubst(interface_ts, match);
    }

    virtual Value *find_implementation(TypeMatch &match, Type *target, Value *orig, TypeSpec &ifts, bool assume_lvalue) {
        if (associated_lself && !assume_lvalue)
            return NULL;

        ifts = get_interface_ts(match);   // pivot match

        if (ifts[0] == target) {
            // Direct implementation
            //std::cerr << "Found direct implementation.\n";
            return make<ImplementationConversionValue>(this, orig, match);
        }
        else {
            //std::cerr << "Trying indirect implementation with " << ifts << "\n";
            TypeMatch iftm = ifts.match();
            
            for (auto &si : shadow_implementations) {
                Value *v = si->find_implementation(iftm, target, orig, ifts, assume_lvalue);
                
                if (v)
                    return v;
            }
            
            return NULL;
        }
    }

    virtual void set_associated_lself(Lself *l) {
        associated_lself = l;
    }

    virtual Associable *lookup_associable(std::string n) {
        if (n == name)
            return this;
        else if (has_prefix(n, prefix)) {
            for (auto &si : shadow_implementations) {
                Associable *a = si->lookup_associable(n);
                
                if (a)
                    return a;
            }
        }
        
        return NULL;
    }

    virtual bool check_associated(Declaration *decl) {
        // NOTE: this is kinda weird, but correct.
        // If a parametric type implements an interface with the same type parameter
        // used, we can't concretize that here yet. So the fake_match, despite being
        // a replacement, may still have Same types. When getting the argument types
        // from the interface definition, the substitution will replace Same types
        // with Same types. But the functions in the implementation will be similarly
        // parametrized, so the comparison should compare Same to Same, and succeed.
        
        Identifier *id = ptr_cast<Identifier>(decl);
        if (!id)
            return false;
        
        if (missing_function_names.count(id->name) != 1) {
            std::cerr << "Unknown member " << id->name << " in implementation " << name << "!\n";
            std::cerr << missing_function_names << "\n";
            return false;
        }
        
        missing_function_names.erase(id->name);
        
        // TODO: We can only check the validity of Function-s
        Function *override = ptr_cast<Function>(decl);
        if (!override) {
            //std::cerr << "This declaration can't be associated with implementation " << name << "!\n";
            //return false;
            return true;
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
        
        override->set_associated_implementation(this);
        
        if (associated_lself)
            override->set_associated_lself(associated_lself);
        
        return true;
    }

    virtual Value *matched(Value *pivot, Scope *scope, TypeMatch &match) {
        return make<ImplementationConversionValue>(this, pivot, match);
    }
};


class Lself: public Identifier, public Associable {
public:
    std::string prefix;
    DataScope *implementor_scope;
    std::vector<Implementation *> outer_implementations;

    Lself(std::string name, TypeSpec pts)
        :Identifier(name, pts) {
        prefix = name + ".";
        implementor_scope = NULL;
    }

    virtual void set_outer_scope(Scope *os) {
        DataScope *ds = ptr_cast<DataScope>(os);
        if (!ds)
            throw INTERNAL_ERROR;
        
        implementor_scope = ds;
    }

    virtual void set_name(std::string n) {
        throw INTERNAL_ERROR;  // too late!
    }

    virtual Associable *lookup_associable(std::string n) {
        if (n == name)
            return this;
        else if (has_prefix(n, prefix)) {
            for (auto &oi : outer_implementations) {
                Associable *a = oi->lookup_associable(n);
                
                if (a)
                    return a;
            }
        }
        
        return NULL;
    }

    virtual bool check_associated(Declaration *decl) {
        Function *f = ptr_cast<Function>(decl);
        if (f) {
            f->set_associated_lself(this);
            return true;
        }

        Implementation *i = ptr_cast<Implementation>(decl);
        if (i) {
            i->set_associated_lself(this);
            return true;
        }
        
        std::cerr << "This declaration can't be associated with Lself " << name << "!\n";
        return false;
    }
};
