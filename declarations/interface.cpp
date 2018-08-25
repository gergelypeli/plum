
class InterfaceType: public Type {
public:
    std::vector<Function *> member_functions;
    
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

            ImplementationType *it = ptr_cast<ImplementationType>(c.get());
            
            if (it)
                continue;
                
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

    virtual void store(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        Type::store(tm, s, t, x64);
    }

    virtual void create(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        // Assume the target MEMORY is uninitialized
        
        switch (s.where * t.where) {
        case NOWHERE_MEMORY:
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



class ImplementationType: public Type {
public:
    std::vector<Function *> member_functions;
    TypeSpec interface_ts;
    TypeSpec implementor_ts;  // aka pivot type

    ImplementationType(std::string name, TypeSpec irts, TypeSpec ifts)
        :Type(name, Metatypes { value_metatype, type_metatype }, implementation_metatype) {
        interface_ts = ifts;
        implementor_ts = irts;
    }

    virtual TypeSpec get_interface_ts(TypeMatch &match) {
        // NOTE: this match is the pivot match, not the TypeSpec match, which has two
        // fixed parameters, the apparent interface TypeSpec, and the concrete TypeSpec!
        
        TypeSpec ts = typesubst(interface_ts, match);
        //std::cerr << "Match: " << match << "\n";
        //std::cerr << "Implementation " << name << " implements " << ts << ".\n";
        return ts;
    }

    virtual bool complete_type() {
        // NOTE: this is kinda weird, but correct.
        // If a parametric type implements an interface with the same type parameter
        // used, we can't concretize that here yet. So the fake_match, despite being
        // a replacement, may still have Same types. When getting the argument types
        // from the interface definition, the substitution will replace Same types
        // with Same types. But the functions in the implementation will be similarly
        // parametrized, so the comparison should compare Same to Same, and succeed.
        InterfaceType *interface_type = ptr_cast<InterfaceType>(interface_ts[0]);
        TypeMatch iftm = interface_ts.match();
        TypeMatch empty_match;

        for (auto &c : inner_scope->contents) {
            Function *f = ptr_cast<Function>(c.get());
            
            if (!f)
                continue;  // Builtin types have a range of classes to implement stuff
            
            //std::cerr << "Checking imp fun: " << f->name << "\n";
            //std::cerr << "XXX " << interface_type->member_functions.size() << "\n";
            
            bool found = false;
            
            for (Function *iff : interface_type->member_functions) {
                if (f->does_implement(empty_match, iff, iftm)) {
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                std::cerr << "Invalid implementation of function: " << interface_type->name << "." << f->name << "!\n";
                return false;
            }
        }

        // FIXME: check order!
        
        for (auto &c : inner_scope->contents) {
            Function *f = ptr_cast<Function>(c.get());
            
            if (f) {
                member_functions.push_back(f);
            }
        }
        
        //std::cerr << "Implementation " << name << " has " << member_functions.size() << " member functions.\n";
        return true;
    }

    virtual Value *match(std::string name, Value *pivot) {
        if (name != this->name)
            return NULL;
            
        TypeMatch match;
        
        if (!typematch(implementor_ts, pivot, match))
            return NULL;

        return make<ImplementationConversionValue>(this, pivot, match);
    }


    virtual Allocation measure(TypeMatch tm) {
        return Allocation();
    }

    virtual void store(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        Type::store(tm, s, t, x64);
    }

    virtual void create(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        // Assume the target MEMORY is uninitialized
        
        switch (s.where * t.where) {
        case NOWHERE_MEMORY:
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

    virtual DataScope *find_inner_scope(std::string n) {
        if (name == n)
            return inner_scope.get();
        else
            return NULL;
    }
    
    virtual Value *lookup_inner(TypeMatch tm, std::string n, Value *pivot) {
        // The second type parameter is the concrete type
        pivot = make<CastValue>(pivot, tm[2]);
            
        return inner_scope->lookup(n, pivot);
    }
    
    virtual Value *autoconv(TypeMatch tm, TypeSpecIter target, Value *orig, TypeSpec &ifts) {
        // The first type parameter is the apparent type
        return tm[1].autoconv(target, orig, ifts);
    }
};


