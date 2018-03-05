
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
        return cpivot;
    }
};


class Cast: public Identifier {
public:
    TypeSpec cast_ts;
    
    Cast(std::string name, TypeSpec pts, TypeSpec cts)
        :Identifier(name, pts) {
        cast_ts = cts;
    }

    virtual Value *matched(Value *cpivot, TypeMatch &match) {
        return make_cast_value(cpivot, typesubst(cast_ts, match));
    }
};


enum FunctionType {
    GENERIC_FUNCTION, INTERFACE_FUNCTION, INITIALIZER_FUNCTION, FINALIZER_FUNCTION
};

    
class Function: public Identifier {
public:
    std::vector<TypeSpec> arg_tss;
    std::vector<std::string> arg_names;
    std::vector<TypeSpec> res_tss;
    TreenumerationType *exception_type;
    int virtual_index;
    FunctionType type;

    Label x64_label;
    bool is_sysv;
    
    Function(std::string n, TypeSpec pts, FunctionType ft, std::vector<TypeSpec> ats, std::vector<std::string> ans, std::vector<TypeSpec> rts, TreenumerationType *et)
        :Identifier(n, pts) {
        type = ft;
        arg_tss = ats;
        arg_names = ans;
        res_tss = rts;
        exception_type = et;
        virtual_index = -1;
        
        is_sysv = false;
    }

    virtual Value *matched(Value *cpivot, TypeMatch &match) {
        // TODO: do this properly!
        
        if (type == INTERFACE_FUNCTION) {
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
        if (outer_scope->is_virtual_scope() && type == GENERIC_FUNCTION) {  // FIXME
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
    
    ImportedFunction(std::string in, std::string n, TypeSpec pts, FunctionType ft, std::vector<TypeSpec> ats, std::vector<std::string> ans, std::vector<TypeSpec> rts, TreenumerationType *et)
        :Function(n, pts, ft, ats, ans, rts, et) {
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


class RecordWrapperIdentifier: public Identifier {
public:
    TypeSpec result_ts;
    TypeSpec pivot_cast_ts;
    std::string operation_name;
    std::string arg_operation_name;
    
    RecordWrapperIdentifier(std::string n,
        TypeSpec pivot_ts, TypeSpec pcts,
        TypeSpec rts, std::string on,
        std::string aon = "")
        :Identifier(n, pivot_ts) {
        result_ts = rts;
        pivot_cast_ts = pcts;
        operation_name = on;
        arg_operation_name = aon;
    }
    
    virtual Value *matched(Value *pivot, TypeMatch &match) {
        if (!pivot)
            throw INTERNAL_ERROR;
        
        TypeSpec rts = typesubst(result_ts, match);
        TypeSpec pcts = typesubst(pivot_cast_ts, match);
            
        Value *wrapper = make_record_wrapper_value(pivot, pcts, rts, operation_name, arg_operation_name);
        
        return wrapper;
    }
};


class ClassWrapperIdentifier: public Identifier {
public:
    TypeSpec pivot_cast_ts;
    std::string operation_name;
    bool autogrow;
    
    ClassWrapperIdentifier(std::string n, TypeSpec pivot_ts, TypeSpec pcts, std::string on, bool ag = false)
        :Identifier(n, pivot_ts) {
        pivot_cast_ts = pcts;
        operation_name = on;
        autogrow = ag;
    }
    
    virtual Value *matched(Value *pivot, TypeMatch &match) {
        if (!pivot)
            throw INTERNAL_ERROR;
        
        Scope *is = pivot_ts.get_inner_scope();
        if (is->contents.size() < 1)
            throw INTERNAL_ERROR;
            
        Variable *v = variable_cast(is->contents[0].get());
        if (!v)
            throw INTERNAL_ERROR;
        
        Value *member = make_variable_value(v, pivot, match);
        
        if (autogrow) {
            TypeSpec mts = get_typespec(member);
            member = mts.lookup_inner("autogrow", member);
            if (!member) {
                std::cerr << "No autogrow for " << mts << "!\n";
                throw INTERNAL_ERROR;
            }
        }
        
        Value *operation = get_typespec(member).lookup_inner(operation_name, member);
        if (!operation) {
            std::cerr << "No operation " << operation_name << " in " << get_typespec(member) << "!\n";
            throw INTERNAL_ERROR;
        }
        
        return operation;
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
