
class InterfaceType: public Type {
public:
    std::vector<Function *> member_functions;

    InterfaceType(std::string name, int pc)
        :Type(name, pc) {
    }

    virtual void set_inner_scope(DataScope *is) {
        DataScope *inner_scope = is;
        
        for (auto &c : inner_scope->contents) {
            Function *f = dynamic_cast<Function *>(c.get());
            
            if (f) {
                member_functions.push_back(f);
            }
        }
        
        std::cerr << "Interface " << name << " has " << member_functions.size() << " member functions.\n";
    }

    virtual unsigned measure(TypeSpecIter tsi, StorageWhere where) {
        switch (where) {
        case MEMORY:
            return 0;
        default:
            return Type::measure(tsi, where);
        }
    }

    virtual void store(TypeSpecIter tsi, Storage s, Storage t, X64 *x64) {
        Type::store(tsi, s, t, x64);
    }

    virtual void create(TypeSpecIter tsi, Storage s, Storage t, X64 *x64) {
        // Assume the target MEMORY is uninitialized
        
        switch (s.where * t.where) {
        case NOWHERE_MEMORY:
            return;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual void destroy(TypeSpecIter , Storage s, X64 *x64) {
        if (s.where == MEMORY) {
            return;
        }
        else
            throw INTERNAL_ERROR;
    }

    virtual StorageWhere where(TypeSpecIter, bool is_arg, bool is_lvalue) {
        return (is_arg ? throw INTERNAL_ERROR : (is_lvalue ? MEMORY : throw INTERNAL_ERROR));
    }

    virtual Storage boolval(TypeSpecIter , Storage s, X64 *x64, bool probe) {
        throw INTERNAL_ERROR;
    }

    virtual Value *lookup_initializer(TypeSpecIter tsi, std::string name, Scope *scope) {
        std::cerr << "No interface initializer called " << name << "!\n";
        return NULL;
    }

    virtual Scope *get_inner_scope(TypeSpecIter tsi) {
        throw INTERNAL_ERROR;
    }
};



class ImplementationType: public Type {
public:
    DataScope *inner_scope;
    std::vector<Function *> member_functions;
    TypeSpec interface_ts;
    TypeSpec implementor_ts;

    ImplementationType(std::string name, TypeSpec irts, TypeSpec ifts)
        :Type(name, 0) {
        interface_ts = ifts;
        implementor_ts = irts;
        inner_scope = NULL;
    }

    virtual void set_inner_scope(DataScope *is) {
        // NOTE: this is called twice to give a preview of the inner scope for
        // external declarations.
        // FIXME: check order!
        inner_scope = is;
        
        for (auto &c : inner_scope->contents) {
            Function *f = dynamic_cast<Function *>(c.get());
            
            if (f) {
                member_functions.push_back(f);
            }
        }
        
        std::cerr << "Implementation " << name << " has " << member_functions.size() << " member functions.\n";
    }

    virtual Value *match(std::string name, Value *pivot, TypeMatch &match) {
        if (name != this->name)
            return NULL;
            
        if (!typematch(implementor_ts, pivot, match))
            return NULL;

        return make_implementation_conversion_value(this, pivot, match);
    }


    virtual unsigned measure(TypeSpecIter tsi, StorageWhere where) {
        switch (where) {
        case MEMORY:
            return 0;
        default:
            return Type::measure(tsi, where);
        }
    }

    virtual void store(TypeSpecIter tsi, Storage s, Storage t, X64 *x64) {
        Type::store(tsi, s, t, x64);
    }

    virtual void create(TypeSpecIter tsi, Storage s, Storage t, X64 *x64) {
        // Assume the target MEMORY is uninitialized
        
        switch (s.where * t.where) {
        case NOWHERE_MEMORY:
            return;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual void destroy(TypeSpecIter , Storage s, X64 *x64) {
        if (s.where == MEMORY) {
            return;
        }
        else
            throw INTERNAL_ERROR;
    }

    virtual StorageWhere where(TypeSpecIter, bool is_arg, bool is_lvalue) {
        return (is_arg ? throw INTERNAL_ERROR : (is_lvalue ? MEMORY : throw INTERNAL_ERROR));
    }

    virtual Storage boolval(TypeSpecIter , Storage s, X64 *x64, bool probe) {
        throw INTERNAL_ERROR;
    }

    virtual Value *lookup_initializer(TypeSpecIter tsi, std::string name, Scope *scope) {
        std::cerr << "No implementation initializer called " << name << "!\n";
        return NULL;
    }

    virtual Scope *get_inner_scope(TypeSpecIter tsi) {
        return inner_scope;
    }
    
    virtual TypeSpec get_interface_ts(TypeMatch &match) {
        return typesubst(interface_ts, match);
    }
};


ImplementationType *implementation_cast(Declaration *d, Type *t) {
    ImplementationType *imp = dynamic_cast<ImplementationType *>(d);
    
    if (imp && imp->interface_ts[0] == t)
        return imp;
    else
        return NULL;
}

