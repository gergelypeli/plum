
class Identifier: public Declaration {
public:
    std::string name;
    TypeSpec pivot_ts;

    Identifier(std::string n, TypeSpec pts) {
        name = n;
        pivot_ts = pts;
    }

    virtual Value *matched(Value *, TypeMatch &match) {
        std::cerr << "Unmatchable identifier!\n";
        throw INTERNAL_ERROR;
    }
    
    virtual Value *match(std::string n, Value *pivot, TypeMatch &match) {
        if (n != name)
            return NULL;
            
        if (typematch(pivot_ts, pivot, match))
            return matched(pivot, match);
        else
            return NULL;
    }
};


class Variable: public Identifier {
public:
    TypeSpec var_ts;
    int offset;
    bool xxx_is_allocated;
    StorageWhere where;
    
    Variable(std::string name, TypeSpec pts, TypeSpec vts)
        :Identifier(name, pts) {
        offset = 0;
        var_ts = vts;
        
        if (var_ts == BOGUS_TS)
            throw INTERNAL_ERROR;
            
        xxx_is_allocated = false;
        where = var_ts.where(false);
    }
    
    virtual void be_argument() {
        where = var_ts.where(true);
    }
    
    virtual Value *matched(Value *cpivot, TypeMatch &match) {
        // cpivot may be NULL if this is a local variable
        return make_variable_value(this, cpivot);
    }
    
    virtual void allocate() {
        if (xxx_is_allocated)
            throw INTERNAL_ERROR;
            
        offset = outer_scope->reserve(var_ts.measure(where));
        
        xxx_is_allocated = true;
        //std::cerr << "Variable " << name << " offset is " << offset << "\n";
    }
    
    virtual Storage get_storage(Storage s) {
        if (s.where != MEMORY)
            throw INTERNAL_ERROR;  // all variable containers must use MEMORY
            
        if (!xxx_is_allocated)
            throw INTERNAL_ERROR;
            
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

    Label x64_label;
    bool is_sysv;
    
    Function(std::string n, TypeSpec pts, std::vector<TypeSpec> ats, std::vector<std::string> ans, std::vector<TypeSpec> rts, Type *et)
        :Identifier(n, pts) {
        arg_tss = ats;
        arg_names = ans;
        res_tss = rts;
        exception_type = et;
        
        is_sysv = false;
    }

    virtual Value *matched(Value *cpivot, TypeMatch &match) {
        return make_function_call_value(this, cpivot);
    }

    virtual std::vector<TypeSpec> &get_result_tss() {
        return res_tss;
    }
    
    virtual TypeSpec get_pivot_typespec() {
        return pivot_ts;
    }
    
    virtual std::vector<TypeSpec> &get_argument_tss() {
        return arg_tss;
    }
    
    virtual std::vector<std::string> &get_argument_names() {
        return arg_names;
    }
};


class ImportedFunction: public Function {
public:
    std::string import_name;
    
    ImportedFunction(std::string in, std::string n, TypeSpec pts, std::vector<TypeSpec> ats, std::vector<std::string> ans, std::vector<TypeSpec> rts, Type *et)
        :Function(n, pts, ats, ans, rts, et) {
        import_name = in;
    }

    virtual void import(X64 *x64) {
        is_sysv = true;
        x64->code_label_import(x64_label, import_name);
    }
};


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


class Yield: public Identifier {
public:
    EvalScope *eval_scope;
    
    Yield(std::string n, EvalScope *es)
        :Identifier(n, VOID_TS) {
        eval_scope = es;
    }
    
    virtual Value *matched(Value *cpivot, TypeMatch &match) {
        return make_yield_value(eval_scope);
    }
};
