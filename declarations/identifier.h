enum PivotRequirement {
    RVALUE_PIVOT, LVALUE_PIVOT, INITIALIZABLE_PIVOT, VARIABLE_PIVOT, NO_PIVOT, CUSTOM_PIVOT
};

class Identifier: public Declaration {
public:
    std::string name;
    PivotRequirement pivot_requirement;

    Identifier(std::string n, PivotRequirement pr);

    virtual std::string get_fully_qualified_name();
    virtual TypeSpec get_pivot_ts();
    virtual Value *matched(Value *pivot, Scope *scope, TypeMatch &match);
    virtual Value *match(std::string n, Value *pivot, Scope *scope);
};

class Identity: public Identifier {
public:
    Identity(std::string name);

    virtual Value *matched(Value *cpivot, Scope *scope, TypeMatch &match);
};

class Cast: public Identifier {
public:
    TypeSpec cast_ts;

    Cast(std::string name, TypeSpec cts);

    virtual Value *matched(Value *cpivot, Scope *scope, TypeMatch &match);
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
    Unpacking(std::string n);

    virtual Value *match(std::string n, Value *pivot, Scope *scope);
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

    RecordWrapperIdentifier(std::string n, PivotRequirement pr, TypeSpec pcts, TypeSpec rts, std::string on, std::string aon = "");

    virtual Value *matched(Value *pivot, Scope *scope, TypeMatch &match);
};

class ClassWrapperIdentifier: public Identifier {
public:
    TypeSpec pivot_cast_ts;
    std::string operation_name;
    bool autogrow;

    ClassWrapperIdentifier(std::string n, PivotRequirement pr, TypeSpec pcts, std::string on, bool ag = false);

    virtual Value *matched(Value *pivot, Scope *scope, TypeMatch &match);
};

class Yield: public Identifier {
public:
    YieldableValue *yieldable_value;

    Yield(std::string n, YieldableValue *yv);

    virtual Value *matched(Value *cpivot, Scope *scope, TypeMatch &match);
};
