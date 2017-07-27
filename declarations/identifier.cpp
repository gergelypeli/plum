
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
    
    virtual Value *match(std::string n, Value *pivot) {
        if (n != name)
            return NULL;
            
        TypeMatch match;
        
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
    
    Variable(std::string name, TypeSpec pts, TypeSpec vts)
        :Identifier(name, pts) {
        offset = 0;
        var_ts = vts;
    }
    
    virtual Value *matched(Value *cpivot, TypeMatch &match) {
        // cpivot may be NULL if this is a local variable
        return make_variable_value(this, cpivot);
    }
    
    virtual void allocate() {
        offset = outer_scope->reserve(var_ts.measure());
        //std::cerr << "Variable " << name << " offset is " << offset << "\n";
    }
    
    virtual Storage get_storage(Storage s) {
        if (s.where != MEMORY)
            throw INTERNAL_ERROR;  // for now, variables are all in memory
            
        //std::cerr << "Variable " << name << " offset is now " << offset << "\n";
        return Storage(MEMORY, s.address + offset);
    }
    
    virtual void finalize(FinalizationType ft, Storage s, X64 *x64) {
        var_ts.destroy(get_storage(s), x64);
        
        Identifier::finalize(ft, s, x64);
    }
};


Variable *variable_cast(Declaration *decl) {
    return dynamic_cast<Variable *>(decl);
}


class Function: public Identifier {
public:
    std::vector<TypeSpec> arg_tss;
    std::vector<std::string> arg_names;
    TypeSpec ret_ts;

    Label x64_label;
    bool is_sysv;
    
    Function(std::string n, TypeSpec pts, std::vector<TypeSpec> ats, std::vector<std::string> ans, TypeSpec rts)
        :Identifier(n, pts) {
        arg_tss = ats;
        arg_names = ans;
        ret_ts = rts;
        
        is_sysv = false;
    }

    virtual Value *matched(Value *cpivot, TypeMatch &match) {
        return make_function_value(this, cpivot);
    }

    virtual TypeSpec get_return_typespec() {
        return ret_ts;
    }
    
    virtual TypeSpec get_pivot_typespec() {
        return pivot_ts;
    }
    
    virtual unsigned get_argument_count() {
        return arg_tss.size();
    }
    
    virtual TypeSpec get_argument_typespec(unsigned i) {
        //std::cerr << "Returning typespec for argument " << i << ".\n";
        return arg_tss[i];
    }

    virtual int get_argument_index(std::string keyword) {  // FIXME
        for (unsigned i = 0; i < arg_names.size(); i++)
            if (arg_names[i] == keyword)
                return i;
                
        return (unsigned)-1;
    }

    virtual void allocate() {
    }
};


class ImportedFunction: public Function {
public:
    std::string import_name;
    
    ImportedFunction(std::string in, std::string n, TypeSpec pts, std::vector<TypeSpec> ats, std::vector<std::string> ans, TypeSpec rts)
        :Function(n, pts, ats, ans, rts) {
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

