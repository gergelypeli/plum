
class ClassType: public HeapType {
public:
    std::vector<Variable *> member_variables;
    std::vector<TypeSpec> member_tss;  // rvalues, for the initializer arguments
    std::vector<std::string> member_names;

    Label virtual_table_label;

    ClassType(std::string name, Label vtl)
        :HeapType(name, 0) {
        virtual_table_label = vtl;
    }

    virtual void complete_type() {
        for (auto &c : inner_scope->contents) {
            Variable *v = dynamic_cast<Variable *>(c.get());
            
            if (v) {
                member_variables.push_back(v);
                member_tss.push_back(v->var_ts.rvalue());
                member_names.push_back(v->name);
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
        TypeSpec ts(tsi);

        // NOTE: initializers must only appear in code scopes, and there all types
        // must be concrete, not having free parameters. Also, the automatic variable is
        // put in the local scope, so there will be no pivot for it to derive any
        // type parameters from. 
        
        if (name == "{}") {
            // Anonymous initializer
            TypeMatch match = type_parameters_to_match(ts);
            return make_class_initializer_value(match);
        }
        else {
            // Named initializer
            TypeSpec pts = TypeSpec(tsi).prefix(partial_reference_type);
            Value *pre = make_class_preinitializer_value(pts);

            Value *value = inner_scope->lookup(name, pre);

            // FIXME: check if the method is Void!
            if (value)
                return make_identity_value(value, TypeSpec(tsi).prefix(reference_type));
        }
        
        std::cerr << "Can't initialize class as " << name << "!\n";
        return NULL;
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

    virtual std::vector<Variable *> get_member_variables() {
        return member_variables;
    }

    virtual std::vector<Function *> get_virtual_table(TypeSpecIter tsi) {
        return inner_scope->get_virtual_table();
    }

    virtual Label get_virtual_table_label(TypeSpecIter tsi) {
        return virtual_table_label;
    }
};

/*
std::vector<Variable *> partial_class_get_member_variables(TypeSpec var_ts) {
    if (var_ts[0] != partial_reference_type) {
        std::cerr << "Partial variable with " << var_ts << "!\n";
        throw INTERNAL_ERROR;
    }
        
    ClassType *class_type = dynamic_cast<ClassType *>(var_ts[1]);
    return class_type->get_member_variables();
}
*/
