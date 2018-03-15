
class InterfaceType: public Type {
public:
    std::vector<Function *> member_functions;
    
    InterfaceType(std::string name, TTs param_tts)
        :Type(name, param_tts, GENERIC_TYPE) {
    }

    virtual bool complete_type() {
        for (auto &c : inner_scope->contents) {
            Function *f = ptr_cast<Function>(c.get());
            
            if (f) {
                member_functions.push_back(f);
            }
        }
        
        std::cerr << "Interface " << name << " has " << member_functions.size() << " member functions.\n";
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

    virtual StorageWhere where(TypeMatch tm, AsWhat as_what, bool as_lvalue) {
        return (
            as_what == AS_VARIABLE ? MEMORY :
            throw INTERNAL_ERROR
        );
    }

    virtual Storage boolval(TypeMatch tm, Storage s, X64 *x64, bool probe) {
        throw INTERNAL_ERROR;
    }
};



class ImplementationType: public Type {
public:
    std::vector<Function *> member_functions;
    TypeSpec interface_ts;
    TypeSpec implementor_ts;  // aka pivot type

    ImplementationType(std::string name, TypeSpec irts, TypeSpec ifts)
        :Type(name, TTs { VALUE_TYPE, GENERIC_TYPE }, GENERIC_TYPE) {
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
        // FIXME: check order!
        
        for (auto &c : inner_scope->contents) {
            Function *f = ptr_cast<Function>(c.get());
            
            if (f) {
                member_functions.push_back(f);
            }
        }
        
        std::cerr << "Implementation " << name << " has " << member_functions.size() << " member functions.\n";
        return true;
    }

    virtual Value *match(std::string name, Value *pivot) {
        if (name != this->name)
            return NULL;
            
        TypeMatch match;
        
        if (!typematch(implementor_ts, pivot, match))
            return NULL;

        return make_implementation_conversion_value(this, pivot, match);
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

    virtual StorageWhere where(TypeMatch tm, AsWhat as_what, bool as_lvalue) {
        return (
            as_what == AS_VARIABLE ? MEMORY :
            throw INTERNAL_ERROR
        );
    }

    virtual Storage boolval(TypeMatch tm, Storage s, X64 *x64, bool probe) {
        throw INTERNAL_ERROR;
    }

    virtual DataScope *find_inner_scope(std::string n) {
        if (name == n)
            return inner_scope;
        else
            return NULL;
    }
    
    virtual Value *lookup_inner(TypeMatch tm, std::string n, Value *pivot) {
        // The second type parameter is the concrete type
        pivot = make_cast_value(pivot, tm[2]);
            
        return inner_scope->lookup(n, pivot);
    }
    
    virtual Value *autoconv(TypeMatch tm, TypeSpecIter target, Value *orig, TypeSpec &ifts) {
        // The first type parameter is the apparent type
        return tm[1].autoconv(target, orig, ifts);
    }
};


