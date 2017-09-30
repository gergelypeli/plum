
class RecordType: public Type {
public:
    Scope *inner_scope;
    Label virtual_table_label;
    
    std::vector<Variable *> member_variables;
    std::vector<TypeSpec> member_tss;  // rvalues, for the initializer arguments
    std::vector<std::string> member_names;

    RecordType(std::string n, Label vtl)
        :Type(n, 0) {
        virtual_table_label = vtl;
        inner_scope = NULL;
    }

    virtual Label get_virtual_table_label(TypeSpecIter tsi) {
        return virtual_table_label;
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
            return inner_scope->get_size();
        case STACK:
            return stack_size(inner_scope->get_size());
        default:
            return Type::measure(tsi, where);
        }
    }

    virtual void store(TypeSpecIter tsi, Storage s, Storage t, X64 *x64) {
        switch (s.where * t.where) {
        case MEMORY_NOWHERE:
            return;
        case MEMORY_MEMORY:  // duplicates data
            for (auto &var : member_variables)
                var->var_ts.store(Storage(MEMORY, s.address + var->offset), Storage(MEMORY, t.address + var->offset), x64);
            return;
        default:
            Type::store(tsi, s, t, x64);
        }
    }

    virtual void create(TypeSpecIter tsi, Storage s, Storage t, X64 *x64) {
        switch (s.where * t.where) {
        case NOWHERE_MEMORY:
            for (auto &var : member_variables)
                var->var_ts.create(Storage(), Storage(MEMORY, t.address + var->offset), x64);
            return;
        case MEMORY_MEMORY:  // duplicates data
            for (auto &var : member_variables)
                var->var_ts.create(Storage(MEMORY, s.address + var->offset), Storage(MEMORY, t.address + var->offset), x64);
            return;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual void destroy(TypeSpecIter tsi, Storage s, X64 *x64) {
        if (s.where == MEMORY)
            for (auto &var : member_variables)  // FIXME: reverse!
                var->var_ts.destroy(Storage(MEMORY, s.address + var->offset), x64);
        else
            throw INTERNAL_ERROR;
    }

    virtual StorageWhere where(TypeSpecIter tsi, bool is_arg, bool is_lvalue) {
        return (is_arg ? ALIAS : MEMORY);
    }
    
    virtual Storage boolval(TypeSpecIter tsi, Storage s, X64 *x64, bool probe) {
        Address address;
        
        switch (s.where) {
        case MEMORY:
            address = s.address;
            break;
        default:
            throw INTERNAL_ERROR;
        }
        
        Label done;
        
        for (auto &var : member_variables) {
            Storage t = var->var_ts.boolval(Storage(MEMORY, address + var->offset), x64, true);
            
            if (t.where == FLAGS && t.bitset == SETNE)
                x64->op(JNE, done);
            else
                throw INTERNAL_ERROR;
        }

        x64->code_label(done);
        
        return Storage(FLAGS, SETNE);
    }
    
    virtual Value *lookup_initializer(TypeSpecIter tsi, std::string n, Scope *scope) {
        if (n == "{}") {
            // Anonymous initializer
            Variable *var = new Variable("<new>", VOID_TS, TypeSpec(tsi));
            scope->add(var);
            
            return make_record_initializer_value(var);
        }
        else {
            // Named initializer
            TypeSpec ts(tsi);
            Value *dv = make_declaration_by_type("<new>", ts, scope);

            TypeMatch match;
            Value *value = inner_scope->lookup(n, dv, match);

            if (value)
                return value;
            
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
            return inner_scope->get_size();
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
                var->var_ts.create(Storage(), Storage(MEMORY, t.address + var->offset), x64);
            return;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual void destroy(TypeSpecIter , Storage s, X64 *x64) {
        if (s.where == MEMORY) {
            for (auto &var : member_variables)  // FIXME: reverse!
                var->var_ts.destroy(Storage(MEMORY, s.address + var->offset), x64);
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
