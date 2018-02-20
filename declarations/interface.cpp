
class InterfaceType: public Type {
public:
    std::vector<Function *> member_functions;
    
    InterfaceType(std::string name, int pc)
        :Type(name, pc) {
    }

    virtual void complete_type() {
        for (auto &c : inner_scope->contents) {
            Function *f = dynamic_cast<Function *>(c.get());
            
            if (f) {
                member_functions.push_back(f);
            }
        }
        
        std::cerr << "Interface " << name << " has " << member_functions.size() << " member functions.\n";
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

    virtual StorageWhere where(TypeMatch tm, bool is_arg, bool is_lvalue) {
        return (is_arg ? throw INTERNAL_ERROR : (is_lvalue ? MEMORY : throw INTERNAL_ERROR));
    }

    virtual Storage boolval(TypeMatch tm, Storage s, X64 *x64, bool probe) {
        throw INTERNAL_ERROR;
    }

    virtual Value *lookup_initializer(TypeMatch tm, std::string name) {
        std::cerr << "No interface initializer called " << name << "!\n";
        return NULL;
    }
};



class ImplementationType: public Type {
public:
    std::vector<Function *> member_functions;
    TypeSpec interface_ts;
    TypeSpec implementor_ts;

    ImplementationType(std::string name, TypeSpec irts, TypeSpec ifts)
        :Type(name, 1) {
        interface_ts = ifts;
        implementor_ts = irts;
    }

    virtual void complete_type() {
        // FIXME: check order!
        
        for (auto &c : inner_scope->contents) {
            Function *f = dynamic_cast<Function *>(c.get());
            
            if (f) {
                member_functions.push_back(f);
            }
        }
        
        std::cerr << "Implementation " << name << " has " << member_functions.size() << " member functions.\n";
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

    virtual StorageWhere where(TypeMatch tm, bool is_arg, bool is_lvalue) {
        return (is_arg ? throw INTERNAL_ERROR : (is_lvalue ? MEMORY : throw INTERNAL_ERROR));
    }

    virtual Storage boolval(TypeMatch tm, Storage s, X64 *x64, bool probe) {
        throw INTERNAL_ERROR;
    }

    virtual Value *lookup_initializer(TypeMatch tm, std::string name) {
        std::cerr << "No implementation initializer called " << name << "!\n";
        return NULL;
    }

    virtual Value *lookup_inner(TypeMatch tm, std::string n, Value *pivot) {
        pivot = make_cast_value(pivot, get_typespec(pivot).begin() + 1);
            
        return inner_scope->lookup(n, pivot);
    }
    
    virtual TypeSpec get_interface_ts(TypeMatch &match) {
        return typesubst(interface_ts, match);
    }
};

