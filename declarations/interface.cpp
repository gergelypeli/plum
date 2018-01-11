
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

    virtual Value *lookup_inner(TypeSpecIter tsi, std::string n, Value *pivot) {
        unprefix_value(pivot);
            
        return inner_scope->lookup(n, pivot);
    }
    
    virtual TypeSpec get_interface_ts(TypeMatch &match) {
        return typesubst(interface_ts, match);
    }
};


bool is_implementation(Type *t, TypeMatch &match, TypeSpecIter target, TypeSpec &ifts) {
    ImplementationType *imp = dynamic_cast<ImplementationType *>(t);

    if (imp) {
        ifts = imp->get_interface_ts(match);
        
        if (ifts[0] == *target)
            return true;
    }

    return false;
}


Value *find_implementation(Scope *inner_scope, TypeMatch &match, TypeSpecIter target, Value *orig, TypeSpec &ifts) {
    if (!inner_scope)
        return NULL;

    for (auto &d : inner_scope->contents) {
        ImplementationType *imp = dynamic_cast<ImplementationType *>(d.get());

        if (imp) {
            ifts = imp->get_interface_ts(match);

            // FIXME: check for proper type match!
            if (ifts[0] == *target) {
                // Direct implementation
                return make_implementation_conversion_value(imp, orig, match);
            }
            else {
                // Maybe indirect implementation
                Scope *ifscope = ifts.get_inner_scope();
                TypeMatch ifmatch = type_parameters_to_match(ifts);
                Value *v = find_implementation(ifscope, ifmatch, target, orig, ifts);

                if (v)
                    return v;
            }
        }
    }

    return NULL;
}
