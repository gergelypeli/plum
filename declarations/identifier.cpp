
class Identifier: public Declaration {
public:
    std::string name;
    TypeSpec pivot_ts;

    Identifier(std::string n, TypeSpec pts) {
        name = n;
        pivot_ts = pts;
        
        if (pts == VOID_TS)
            throw INTERNAL_ERROR;
    }

    virtual Value *matched(Value *, TypeMatch &match) {
        std::cerr << "Unmatchable identifier!\n";
        throw INTERNAL_ERROR;
    }
    
    virtual Value *match(std::string n, Value *pivot) {
        if (n != name)
            return NULL;
            
        TypeMatch match;
            
        if (pivot_ts == NO_TS) {
            if (!pivot)
                return matched(NULL, match);
            else
                return NULL;
        }
        else {
            if (typematch(pivot_ts, pivot, match))
                return matched(pivot, match);
            else
                return NULL;
        }
    }
};


class Identity: public Identifier {
public:
    Identity(std::string name, TypeSpec pts)
        :Identifier(name, pts) {
    }

    virtual Value *matched(Value *cpivot, TypeMatch &match) {
        return make_identity_value(cpivot);
    }
};


class Variable: public Identifier {
public:
    TypeSpec var_ts;
    int offset;
    int virtual_index;
    bool xxx_is_allocated;
    StorageWhere where;
    
    Variable(std::string name, TypeSpec pts, TypeSpec vts)
        :Identifier(name, pts) {
        offset = 0;
        virtual_index = -1;
        var_ts = vts;
        
        if (var_ts == NO_TS)
            throw INTERNAL_ERROR;
            
        xxx_is_allocated = false;
        where = var_ts.where(false);
    }
    
    virtual void be_argument() {
        where = var_ts.where(true);
    }
    
    virtual Value *matched(Value *cpivot, TypeMatch &match) {
        // cpivot may be NULL if this is a local variable
        if (var_ts[0] == lvalue_type && var_ts[1] == role_type)
            return make_role_value(this, cpivot);
        else
            return make_variable_value(this, cpivot, match);
    }
    
    virtual void allocate() {
        if (xxx_is_allocated)
            throw INTERNAL_ERROR;
            
        offset = outer_scope->reserve(var_ts.measure(where));

        if (var_ts[0] == lvalue_type && var_ts[1] == role_type) {
            virtual_index = outer_scope->virtual_reserve(var_ts.get_virtual_table());
        }
        
        xxx_is_allocated = true;
        //std::cerr << "Variable " << name << " offset is " << offset << "\n";
    }
    
    virtual Storage get_storage(Storage s) {
        if (!xxx_is_allocated)
            throw INTERNAL_ERROR;

        //if (s.where == STACK && offset == 0)  // FIXME: this is a hack for array wrapper records only!
        //    return Storage(STACK);

        if (s.where != MEMORY)
            throw INTERNAL_ERROR;  // all variable containers must use MEMORY
            
        //std::cerr << "Variable " << name << " offset is now " << offset << "\n";
        return Storage(where, s.address + offset);
    }
    
    virtual void finalize(X64 *x64) {
        Identifier::finalize(x64);  // Place label
        //std::cerr << "Finalizing variable " << name << ".\n";
        
        // This method is only called on local variables
        Storage fn_storage(MEMORY, Address(RBP, 0));
        var_ts.destroy(get_storage(fn_storage), x64);
    }
    
    virtual void create(TypeSpecIter tsi, Storage s, Storage t, X64 *x64) {
        // TODO: this is a bit complicated
        TypeMatch match = type_parameters_to_match(TypeSpec(tsi));
        TypeSpec ts = typesubst(var_ts, match);
        ts.create(s.where == NOWHERE ? s : s + offset, t + offset, x64);
    }

    virtual void store(TypeSpecIter tsi, Storage s, Storage t, X64 *x64) {
        // TODO: this is a bit complicated
        TypeMatch match = type_parameters_to_match(TypeSpec(tsi));
        TypeSpec ts = typesubst(var_ts, match);
        ts.store(s + offset, t + offset, x64);
    }

    virtual void destroy(TypeSpecIter tsi, Storage s, X64 *x64) {
        // TODO: this is a bit complicated
        TypeMatch match = type_parameters_to_match(TypeSpec(tsi));
        TypeSpec ts = typesubst(var_ts, match);
        ts.destroy(s + offset, x64);
    }
    
    virtual Storage boolval(TypeSpecIter tsi, Storage s, X64 *x64, bool probe) {
        // TODO: this is a bit complicated
        TypeMatch match = type_parameters_to_match(TypeSpec(tsi));
        TypeSpec ts = typesubst(var_ts, match);
        return ts.boolval(s + offset, x64, probe);
    }

    virtual void compare(TypeSpecIter tsi, Storage s, Storage t, X64 *x64, Label less, Label greater) {
        // TODO: this is a bit complicated
        TypeMatch match = type_parameters_to_match(TypeSpec(tsi));
        TypeSpec ts = typesubst(var_ts, match);
        ts.compare(s + offset, t + offset, x64, less, greater);
    }
};


Variable *variable_cast(Declaration *decl) {
    return dynamic_cast<Variable *>(decl);
}




class Function: public Identifier {
public:
    std::vector<TypeSpec> arg_tss;
    std::vector<std::string> arg_names;
    std::vector<TypeSpec> res_tss;
    Type *exception_type;
    int virtual_index;
    bool is_interface_function;

    Label x64_label;
    bool is_sysv;
    
    Function(std::string n, TypeSpec pts, std::vector<TypeSpec> ats, std::vector<std::string> ans, std::vector<TypeSpec> rts, Type *et)
        :Identifier(n, pts) {
        arg_tss = ats;
        arg_names = ans;
        res_tss = rts;
        exception_type = et;
        virtual_index = -1;
        is_interface_function = false;
        
        is_sysv = false;
    }

    virtual void be_interface_function() {
        is_interface_function = true;
    }

    virtual Value *matched(Value *cpivot, TypeMatch &match) {
        // TODO: do this properly!
        
        if (is_interface_function) {
            std::cerr << "Oops, interface function " << name << " was called instead of an implementation!\n";
            throw INTERNAL_ERROR;
        }
        
        return make_function_call_value(this, cpivot, match);
    }

    virtual std::vector<TypeSpec> get_result_tss(TypeMatch &match) {
        std::vector<TypeSpec> tss;
        for (auto &ts : res_tss)
            tss.push_back(typesubst(ts, match));
        return tss;
    }
    
    virtual TypeSpec get_pivot_typespec(TypeMatch &match) {
        return typesubst(pivot_ts, match);
    }
    
    virtual std::vector<TypeSpec> get_argument_tss(TypeMatch &match) {
        std::vector<TypeSpec> tss;
        for (auto &ts : arg_tss)
            tss.push_back(typesubst(ts, match));
        return tss;
    }
    
    virtual std::vector<std::string> get_argument_names() {
        return arg_names;
    }

    virtual void allocate() {
        if (outer_scope->is_virtual_scope()) {
            std::vector<Function *> vt;
            vt.push_back(this);
            virtual_index = outer_scope->virtual_reserve(vt);
        }
    }
};


class ImportedFunction: public Function {
public:
    static std::vector<ImportedFunction *> to_be_imported;
    
    static void import_all(X64 *x64) {
        for (auto i : to_be_imported)
            i->import(x64);
    }
    
    std::string import_name;
    
    ImportedFunction(std::string in, std::string n, TypeSpec pts, std::vector<TypeSpec> ats, std::vector<std::string> ans, std::vector<TypeSpec> rts, Type *et)
        :Function(n, pts, ats, ans, rts, et) {
        import_name = in;
        to_be_imported.push_back(this);
    }

    virtual void import(X64 *x64) {
        is_sysv = true;
        x64->code_label_import(x64_label, import_name);
    }
};

std::vector<ImportedFunction *> ImportedFunction::to_be_imported;


typedef Value *(*GenericValueFactory)(OperationType, Value *, TypeMatch &);

class GenericOperation: public Identifier {
public:
    GenericValueFactory factory;
    OperationType operation;
    
    GenericOperation(std::string n, TypeSpec t, GenericValueFactory f, OperationType o)
        :Identifier(n, t) {
        factory = f;
        operation = o;
    }

    virtual Value *matched(Value *cpivot, TypeMatch &match) {
        return factory(operation, cpivot, match);
    }
};


template <typename T>
class TemplateOperation: public Identifier {
public:
    OperationType operation;

    TemplateOperation(std::string n, TypeSpec t, OperationType o)
        :Identifier(n, t) {
        operation = o;
    }
    
    virtual Value *matched(Value *cpivot, TypeMatch &match) {
        return new T(operation, cpivot, match);
    }
};


template <typename T>
class TemplateIdentifier: public Identifier {
public:
    TemplateIdentifier(std::string n, TypeSpec t)
        :Identifier(n, t) {
    }
    
    virtual Value *matched(Value *cpivot, TypeMatch &match) {
        return new T(cpivot, match);
    }
};


class WrapperIdentifier: public Identifier {
public:
    TypeSpec arg_ts;
    TypeSpec result_ts;
    TypeSpec pivot_cast_ts;
    TypeSpec arg_cast_ts;
    std::string operation_name;
    
    WrapperIdentifier(std::string n,
        TypeSpec pivot_ts, TypeSpec pcts,
        TypeSpec ats, TypeSpec acts,
        TypeSpec rts, std::string on)
        :Identifier(n, pivot_ts) {
        arg_ts = ats;
        result_ts = rts;
        pivot_cast_ts = pcts;
        arg_cast_ts = acts;
        operation_name = on;
    }
    
    virtual Value *matched(Value *pivot, TypeMatch &match) {
        if (!pivot)
            throw INTERNAL_ERROR;
            
        if (pivot_cast_ts != NO_TS)
            pivot = make_unwrap_value(pivot_cast_ts, pivot);
        
        Value *operation = get_typespec(pivot).lookup_inner(operation_name, pivot);
        if (!operation)
            throw INTERNAL_ERROR;
        
        Value *wrapper = make_wrapper_value(arg_ts, result_ts, arg_cast_ts, operation);
        
        return wrapper;
    }
};


class Yield: public Identifier {
public:
    EvalScope *eval_scope;
    
    Yield(std::string n, EvalScope *es)
        :Identifier(n, NO_TS) {
        eval_scope = es;
    }
    
    virtual Value *matched(Value *cpivot, TypeMatch &match) {
        return make_yield_value(eval_scope);
    }
};
