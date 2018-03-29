
class Identifier: public Declaration {
public:
    std::string name;
    TypeSpec pivot_ts;

    Identifier(std::string n, TypeSpec pts) {
        name = n;
        pivot_ts = pts;
        
        if (pts == VOID_TS)
            throw INTERNAL_ERROR;  // should have used NO_TS probably
    }

    virtual bool is_called(std::string n) {
        return n == name;
    }

    virtual Value *matched(Value *pivot, TypeMatch &match) {
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
            else {
                //std::cerr << "Identifier pivot " << get_typespec(pivot) << " did not match " << pivot_ts << "!\n";
                return NULL;
            }
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
        Value *member = get_typespec(pivot).lookup_inner("wrapped", pivot);
        
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
