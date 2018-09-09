
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

            Implementation *it = ptr_cast<Implementation>(c.get());
            
            if (it)
                continue;
                
            // OK, we have to forgive built-in implementations of nested interfaces,
            // which may be of any class, but at least they have a qualified name.
            Identifier *i = ptr_cast<Identifier>(c.get());
            
            if (i && i->name.find(".") != std::string::npos)
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



class Implementation: public Identifier {
public:
    TypeSpec interface_ts;
    std::string prefix;

    Implementation(std::string name, TypeSpec pts, TypeSpec ifts)
        :Identifier(name, pts) {
        interface_ts = ifts;
        prefix = name + ".";
    }

    virtual void set_name(std::string n) {
        name = n;
        prefix = n + ".";
    }

    virtual TypeSpec get_interface_ts(TypeMatch &match) {
        return typesubst(interface_ts, match);
    }

    virtual Implementation *lookup_implementation(std::string name) {
        if (deprefix(name, prefix))
            return this;
        else
            return NULL;
    }

    virtual bool check_implementation(Function *override) {
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
