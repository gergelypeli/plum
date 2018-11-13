
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

    virtual Value *matched(Value *pivot, Scope *scope, TypeMatch &match) {
        std::cerr << "Unmatchable identifier!\n";
        throw INTERNAL_ERROR;
    }
    
    virtual Value *match(std::string n, Value *pivot, Scope *scope) {
        if (n != name)
            return NULL;
            
        TypeMatch match;
        //std::cerr << "Identifier match " << name << " from " << get_typespec(pivot) << " to " << pivot_ts << "\n";
            
        if (pivot_ts == NO_TS) {
            if (!pivot)
                return matched(NULL, scope, match);
            else
                return NULL;
        }
        
        if (pivot_ts.has_meta(record_metatype)) {
            // Identifiers in records may handle lvalue pivots differently than rvalues
            if (typematch(pivot_ts.lvalue(), pivot, match))
                return matched(pivot, scope, match);
        }
        
        if (typematch(pivot_ts, pivot, match))
            return matched(pivot, scope, match);
        else {
            //std::cerr << "Identifier pivot " << get_typespec(pivot) << " did not match " << pivot_ts << "!\n";
            return NULL;
        }
    }
};


class Identity: public Identifier {
public:
    Identity(std::string name, TypeSpec pts)
        :Identifier(name, pts) {
    }

    virtual Value *matched(Value *cpivot, Scope *scope, TypeMatch &match) {
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

    virtual Value *matched(Value *cpivot, Scope *scope, TypeMatch &match) {
        return make<CastValue>(cpivot, typesubst(cast_ts, match));
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
    
    virtual Value *matched(Value *cpivot, Scope *scope, TypeMatch &match) {
        return new T(operation, cpivot, match);
    }
};


template <typename T>
class TemplateIdentifier: public Identifier {
public:
    TemplateIdentifier(std::string n, TypeSpec t)
        :Identifier(n, t) {
    }
    
    virtual Value *matched(Value *cpivot, Scope *scope, TypeMatch &match) {
        return new T(cpivot, match);
    }
};


template <typename T>
class NosytreeTemplateIdentifier: public Identifier {
public:
    TypeSpec member_ts;
    
    NosytreeTemplateIdentifier(std::string n, TypeSpec t, TypeSpec mts)
        :Identifier(n, t) {
        member_ts = mts.rvalue();
    }
    
    virtual Value *matched(Value *cpivot, Scope *scope, TypeMatch &match) {
        // Take the Rbtree Ref from the Nosytree before instantiating
        TypeSpec mts = typesubst(member_ts, match);
        
        Value *pivot = (
            pivot_ts[0] == lvalue_type ? make<NosytreeCowMemberValue>(cpivot, mts, scope) :
            make<NosytreeMemberValue>(cpivot, mts, scope)
        );
        
        return new T(pivot, match);
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
    
    virtual Value *matched(Value *pivot, Scope *scope, TypeMatch &match) {
        if (!pivot)
            throw INTERNAL_ERROR;
        
        TypeSpec rts = typesubst(result_ts, match);
        TypeSpec pcts = typesubst(pivot_cast_ts, match);
            
        Value *wrapper = make<RecordWrapperValue>(pivot, pcts, rts, operation_name, arg_operation_name, scope);
        
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
    
    virtual Value *matched(Value *pivot, Scope *scope, TypeMatch &match) {
        Value *member = value_lookup_inner(pivot, "wrapped", scope);
        
        if (autogrow) {
            member = value_lookup_inner(member, "autogrow", scope);
            
            if (!member) {
                std::cerr << "No autogrow for " << get_typespec(member) << "!\n";
                throw INTERNAL_ERROR;
            }
            
            Args args;
            Kwargs kwargs;
            
            if (!value_check(member, args, kwargs, scope))
                throw INTERNAL_ERROR;
        }
        
        Value *operation = value_lookup_inner(member, operation_name, scope);
        if (!operation) {
            std::cerr << "No operation " << operation_name << " in " << get_typespec(member) << "!\n";
            throw INTERNAL_ERROR;
        }
        
        return operation;
    }
};


class Yield: public Identifier {
public:
    YieldableValue *yieldable_value;
    
    Yield(std::string n, YieldableValue *yv)
        :Identifier(n, NO_TS) {
        yieldable_value = yv;
    }
    
    virtual Value *matched(Value *cpivot, Scope *scope, TypeMatch &match) {
        return make<YieldValue>(yieldable_value);
    }
};

