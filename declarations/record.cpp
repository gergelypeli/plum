
class RecordType: public Type {
public:
    Scope *inner_scope;
    
    std::vector<Variable *> member_variables;
    std::vector<TypeSpec> member_tss;  // rvalues, for the initializer arguments
    std::vector<std::string> member_names;

    RecordType(std::string n, int pc)
        :Type(n, pc) {
        inner_scope = NULL;
    }

    virtual void set_inner_scope(Scope *is) {
        inner_scope = is;
        
        for (auto &c : inner_scope->contents) {
            Variable *v = dynamic_cast<Variable *>(c.get());
            
            if (v) {
                member_variables.push_back(v);
                member_tss.push_back(v->var_ts.rvalue());
                member_names.push_back(v->name);
            }
        }
        
        std::cerr << "Record " << name << " has " << member_variables.size() << " member variables.\n";
    }
    
    virtual unsigned measure(TypeSpecIter tsi, StorageWhere where) {
        switch (where) {
        case MEMORY:
            return inner_scope->get_size(tsi);
        case STACK:
            return stack_size(inner_scope->get_size(tsi));
        default:
            return Type::measure(tsi, where);
        }
    }

    virtual void store(TypeSpecIter tsi, Storage s, Storage t, X64 *x64) {
        switch (s.where * t.where) {
        case STACK_NOWHERE:
            destroy(tsi, Storage(MEMORY, Address(RSP, 0)), x64);
            x64->op(ADDQ, RSP, measure(tsi, STACK));
            return;
        case STACK_STACK:
            return;
        case STACK_MEMORY:
            store(tsi, Storage(MEMORY, Address(RSP, 0)), t, x64);
            store(tsi, s, Storage(), x64);
            return;
        case MEMORY_NOWHERE:
            return;
        case MEMORY_STACK:
            x64->op(SUBQ, RSP, measure(tsi, STACK));
            create(tsi, s, Storage(MEMORY, Address(RSP, 0)), x64);
            return;
        case MEMORY_MEMORY:  // duplicates data
            for (auto &var : member_variables)
                var->store(tsi, Storage(MEMORY, s.address), Storage(MEMORY, t.address), x64);
            return;
        default:
            Type::store(tsi, s, t, x64);
        }
    }

    virtual void create(TypeSpecIter tsi, Storage s, Storage t, X64 *x64) {
        switch (s.where * t.where) {
        case NOWHERE_STACK:
            x64->op(SUBQ, RSP, measure(tsi, STACK));
            create(tsi, Storage(), Storage(MEMORY, Address(RSP, 0)), x64);
            return;
        case NOWHERE_MEMORY:
            for (auto &var : member_variables)
                var->create(tsi, Storage(), Storage(MEMORY, t.address), x64);
            return;
        case STACK_MEMORY:
            create(tsi, Storage(MEMORY, Address(RSP, 0)), t, x64);
            store(tsi, s, Storage(), x64);
            return;
        case MEMORY_MEMORY:  // duplicates data
            for (auto &var : member_variables)
                var->create(tsi, Storage(MEMORY, s.address), Storage(MEMORY, t.address), x64);
            return;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual void destroy(TypeSpecIter tsi, Storage s, X64 *x64) {
        if (s.where == MEMORY)
            for (auto &var : member_variables)  // FIXME: reverse!
                var->destroy(tsi, Storage(MEMORY, s.address), x64);
        else
            throw INTERNAL_ERROR;
    }

    virtual StorageWhere where(TypeSpecIter tsi, bool is_arg, bool is_lvalue) {
        return (is_arg ? (is_lvalue ? ALIAS : MEMORY) : (is_lvalue ? MEMORY : STACK));
    }
    
    virtual Storage boolval(TypeSpecIter tsi, Storage s, X64 *x64, bool probe) {
        Address address;
        
        switch (s.where) {
        case STACK:
            address = Address(RSP, 0);
        case MEMORY:
            address = s.address;
            break;
        default:
            throw INTERNAL_ERROR;
        }
        
        Label done;
        
        for (auto &var : member_variables) {
            Storage t = var->boolval(tsi, Storage(MEMORY, address), x64, true);
            
            if (t.where == FLAGS && t.bitset == SETNE)
                x64->op(JNE, done);
            else
                throw INTERNAL_ERROR;
        }

        x64->code_label(done);
        
        if (!probe) {
            x64->op(SETNE, BL);
            x64->op(PUSHQ, RBX);
            destroy(tsi, Storage(MEMORY, Address(RSP, 8)), x64);
            x64->op(POPQ, RBX);
            x64->op(ADDQ, RSP, measure(tsi, STACK));
            x64->op(CMPB, BL, 0);
        }
        
        return Storage(FLAGS, SETNE);
    }
    
    virtual Value *lookup_initializer(TypeSpecIter tsi, std::string n, Scope *scope) {
        TypeSpec ts(tsi);
        TypeMatch match;

        // NOTE: initializers must only appear in code scopes, and there all types
        // must be concrete, not having free parameters. Also, the automatic variable is
        // put in the local scope, so there will be no pivot for it to derive any
        // type parameters from. 
        
        if (n == "{}") {
            // Anonymous initializer
            match = type_parameters_to_match(ts);
            return make_record_initializer_value(match);
        }
        else {
            // Named initializer
            //Value *dv = make_declaration_by_type("<new>", ts, scope);
            Value *pre = make_record_preinitializer_value(ts.lvalue());

            Value *value = inner_scope->lookup(n, pre, match);

            if (value)
                return make_record_postinitializer_value(value);
            
            std::cerr << "Can't initialize record as " << n << "!\n";
        
            // OK, we gonna leak dv here, because it's just not possible to delete it.
            //   error: possible problem detected in invocation of delete operator
            //   error: ‘dv’ has incomplete type
            //   note: neither the destructor nor the class-specific operator delete
            //     will be called, even if they are declared when the class is defined
            // Thanks, C++!
        
            return NULL;
        }
    }
    
    virtual Scope *get_inner_scope(TypeSpecIter tsi) {
        return inner_scope;
    }
    
    virtual std::vector<TypeSpec> get_member_tss(TypeMatch &match) {
        std::vector<TypeSpec> tss;
        for (auto &ts : member_tss)
            tss.push_back(typesubst(ts, match));
        return tss;
    }
    
    virtual std::vector<std::string> get_member_names() {
        return member_names;
    }
};


class ClassType: public HeapType {
public:
    DataScope *inner_scope;
    std::vector<Variable *> member_variables;
    Label virtual_table_label;

    ClassType(std::string name, Label vtl)
        :HeapType(name, 0) {
        virtual_table_label = vtl;
        inner_scope = NULL;
    }

    virtual std::vector<Function *> get_virtual_table(TypeSpecIter tsi) {
        return inner_scope->get_virtual_table();
    }

    virtual void set_inner_scope(DataScope *is) {
        inner_scope = is;
        
        for (auto &c : inner_scope->contents) {
            Variable *v = dynamic_cast<Variable *>(c.get());
            
            if (v) {
                member_variables.push_back(v);
                //member_tss.push_back(v->var_ts.rvalue());
                //member_names.push_back(v->name);
            }
        }
        
        std::cerr << "Class " << name << " has " << member_variables.size() << " member variables.\n";
    }

    virtual unsigned measure(TypeSpecIter tsi, StorageWhere where) {
        switch (where) {
        case MEMORY:
            return inner_scope->get_size(tsi);
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
            for (auto &var : member_variables)
                var->create(tsi, Storage(), t, x64);
            return;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual void destroy(TypeSpecIter tsi, Storage s, X64 *x64) {
        if (s.where == MEMORY) {
            for (auto &var : member_variables)  // FIXME: reverse!
                var->destroy(tsi, s, x64);
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
        std::cerr << "No class initializer called " << name << "!\n";
        return NULL;
    }

    virtual Scope *get_inner_scope(TypeSpecIter tsi) {
        return inner_scope;
    }
};
