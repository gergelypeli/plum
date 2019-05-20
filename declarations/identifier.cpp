
enum PivotRequirement {
    RVALUE_PIVOT, LVALUE_PIVOT, INITIALIZABLE_PIVOT, NO_PIVOT, CUSTOM_PIVOT
};

class Identifier: public Declaration {
public:
    std::string name;
    PivotRequirement pivot_requirement;

    Identifier(std::string n, PivotRequirement pr) {
        name = n;
        pivot_requirement = pr;
    }
    
    //virtual void set_pivot_requirement(PivotRequirement pr) {
    //    pivot_requirement = pr;
    //}

    virtual std::string get_fully_qualified_name() {
        // If the name begins with an associable prefix, include it in the qualified one,
        // but if the associable name is empty, then don't keep an empty component.
        
        return outer_scope->fully_qualify(name[0] == '.' ? name.substr(1) : name);
    }

    virtual TypeSpec get_pivot_ts() {
        DataScope *ds = ptr_cast<DataScope>(outer_scope);
        
        if (!ds)
            return NO_TS;  // TODO: make it only acceptable for some pr values?
            
        switch (pivot_requirement) {
        case RVALUE_PIVOT:
            return ds->get_pivot_ts();
        case LVALUE_PIVOT:
            return ds->get_pivot_ts().lvalue();
        case INITIALIZABLE_PIVOT:
            return ds->get_pivot_ts().prefix(initializable_type);
        case NO_PIVOT:
            return NO_TS;
        case CUSTOM_PIVOT:
            throw INTERNAL_ERROR;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual bool disable_rvalue_pivot_cast() {
        // This is a hack for the sake of Variable, because it works with both
        // rvalue and lvalue pivots.
        return false;
    }
    
    virtual Value *matched(Value *pivot, Scope *scope, TypeMatch &match) {
        std::cerr << "Unmatchable identifier!\n";
        throw INTERNAL_ERROR;
    }
    
    virtual Value *match(std::string n, Value *pivot, Scope *scope) {
        if (n != name) {
            //std::cerr << "Nope, this is " << name << " not " << n << "\n";
            return NULL;
        }

        TypeSpec pivot_ts = get_pivot_ts();
        
        std::cerr << "Identifier match " << name << " from " << get_typespec(pivot) << " to " << pivot_ts << "\n";

        TypeMatch match;
            
        if (pivot_ts == NO_TS) {
            if (!pivot)
                return matched(NULL, scope, match);
            else
                return NULL;
        }

        if (typematch(pivot_ts, pivot, match)) {
            if (pivot_ts[0] == lvalue_type || pivot_ts[0] == uninitialized_type)
                value_need_lvalue(pivot);
            else if (get_typespec(pivot)[0] == lvalue_type && !disable_rvalue_pivot_cast())
                pivot = make<RvalueCastValue>(pivot);

            return matched(pivot, scope, match);
        }
        else {
            //std::cerr << "Identifier pivot " << get_typespec(pivot) << " did not match " << pivot_ts << "!\n";
            return NULL;
        }
    }
};


class Identity: public Identifier {
public:
    Identity(std::string name)
        :Identifier(name, RVALUE_PIVOT) {
    }

    virtual Value *matched(Value *cpivot, Scope *scope, TypeMatch &match) {
        return cpivot;
    }
};


class Cast: public Identifier {
public:
    TypeSpec cast_ts;
    
    Cast(std::string name, TypeSpec cts)
        :Identifier(name, RVALUE_PIVOT) {
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

    TemplateOperation(std::string n, OperationType o)
        :Identifier(n, is_assignment(o) ? LVALUE_PIVOT : RVALUE_PIVOT) {
        operation = o;
    }
    
    virtual Value *matched(Value *cpivot, Scope *scope, TypeMatch &match) {
        return new T(operation, cpivot, match);
    }
};


template <typename T>
class TemplateIdentifier: public Identifier {
public:
    TemplateIdentifier(std::string n, PivotRequirement pr)
        :Identifier(n, pr) {
    }
    
    virtual Value *matched(Value *cpivot, Scope *scope, TypeMatch &match) {
        return new T(cpivot, match);
    }
};


class Unpacking: public Identifier {
public:
    Unpacking(std::string n)
        :Identifier(n, CUSTOM_PIVOT) {
    }

    virtual Value *match(std::string n, Value *pivot, Scope *scope) {
        // This operation is in the tuple metascope.
        // If anything finds it here, the pivot argument must be a tuple.
        // But we can't represent a generic type for any tuple, so the official pivot type
        // of the metascope is useless, so don't check that.
        if (n != name) {
            //std::cerr << "Nope, this is " << name << " not " << n << "\n";
            return NULL;
        }

        TypeSpec pts = get_typespec(pivot);
        
        if (!pts.has_meta(tuple_metatype))
            return NULL;
        
        // TODO: check for all lvalues!
        return make<UnpackingValue>(pivot);
    }
};


template <typename T>
class NosytreeTemplateIdentifier: public Identifier {
public:
    TypeSpec elem_ts;
    
    NosytreeTemplateIdentifier(std::string n, PivotRequirement pr, TypeSpec ets)
        :Identifier(n, pr) {
        elem_ts = ets;
    }
    
    virtual Value *matched(Value *cpivot, Scope *scope, TypeMatch &match) {
        // Take the Rbtree Ref from the Nosytree before instantiating
        TypeSpec ets = typesubst(elem_ts, match);
        TypeSpec pivot_ts = get_pivot_ts();

        TypeSpec member_ts = ets.prefix(rbtree_type).prefix(ref_type);
        if (pivot_ts[0] == lvalue_type)
            member_ts = member_ts.lvalue();
        
        Value *pivot = make<NosytreeMemberValue>(cpivot, ets, member_ts);
        
        if (pivot_ts[0] == lvalue_type) {
            value_need_lvalue(pivot);
        }
        else if (get_typespec(pivot)[0] == lvalue_type)
            pivot = make<RvalueCastValue>(pivot);
        
        Args fake_args;
        Kwargs fake_kwargs;

        if (!value_check(pivot, fake_args, fake_kwargs, scope))
            throw INTERNAL_ERROR;
        
        return new T(pivot, ets);
    }
};


class RecordWrapperIdentifier: public Identifier {
public:
    TypeSpec result_ts;
    TypeSpec pivot_cast_ts;
    std::string operation_name;
    std::string arg_operation_name;
    
    RecordWrapperIdentifier(std::string n, PivotRequirement pr, TypeSpec pcts, TypeSpec rts, std::string on, std::string aon = "")
        :Identifier(n, pr) {
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
    
    ClassWrapperIdentifier(std::string n, PivotRequirement pr, TypeSpec pcts, std::string on, bool ag = false)
        :Identifier(n, pr) {
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
        :Identifier(n, RVALUE_PIVOT) {
        yieldable_value = yv;
    }
    
    virtual Value *matched(Value *cpivot, Scope *scope, TypeMatch &match) {
        return make<YieldValue>(yieldable_value);
    }
};

