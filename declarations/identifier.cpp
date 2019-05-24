
Identifier::Identifier(std::string n, PivotRequirement pr) {
    name = n;
    pivot_requirement = pr;
}

std::string Identifier::get_fully_qualified_name() {
    // If the name begins with an associable prefix, include it in the qualified one,
    // but if the associable name is empty, then don't keep an empty component.
    
    return outer_scope->fully_qualify(name[0] == '.' ? name.substr(1) : name);
}

TypeSpec Identifier::get_pivot_ts() {
    DataScope *ds = ptr_cast<DataScope>(outer_scope);
    
    if (!ds)
        return NO_TS;  // TODO: make it only acceptable for some pr values?
        
    switch (pivot_requirement) {
    case RVALUE_PIVOT:
    case VARIABLE_PIVOT:
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

Value *Identifier::matched(Value *pivot, Scope *scope, TypeMatch &match) {
    std::cerr << "Unmatchable identifier!\n";
    throw INTERNAL_ERROR;
}

Value *Identifier::match(std::string n, Value *pivot, Scope *scope) {
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
        else if (get_typespec(pivot)[0] == lvalue_type && pivot_requirement != VARIABLE_PIVOT)
            pivot = make<RvalueCastValue>(pivot);

        return matched(pivot, scope, match);
    }
    else {
        //std::cerr << "Identifier pivot " << get_typespec(pivot) << " did not match " << pivot_ts << "!\n";
        return NULL;
    }
}




Identity::Identity(std::string name)
    :Identifier(name, RVALUE_PIVOT) {
}

Value *Identity::matched(Value *cpivot, Scope *scope, TypeMatch &match) {
    return cpivot;
}




Cast::Cast(std::string name, TypeSpec cts)
    :Identifier(name, RVALUE_PIVOT) {
    cast_ts = cts;
}

Value *Cast::matched(Value *cpivot, Scope *scope, TypeMatch &match) {
    return make<CastValue>(cpivot, typesubst(cast_ts, match));
}




Unpacking::Unpacking(std::string n)
    :Identifier(n, CUSTOM_PIVOT) {
}

Value *Unpacking::match(std::string n, Value *pivot, Scope *scope) {
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




NosytreeIdentifier::NosytreeIdentifier(std::string n, PivotRequirement pr, TypeSpec ets)
    :Identifier(n, pr) {
    elem_ts = ets;
}

Value *NosytreeIdentifier::create(Value *pivot, TypeSpec ets) {
    throw INTERNAL_ERROR;
}

Value *NosytreeIdentifier::matched(Value *cpivot, Scope *scope, TypeMatch &match) {
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
    
    return create(pivot, ets);
}




RecordWrapperIdentifier::RecordWrapperIdentifier(std::string n, PivotRequirement pr, TypeSpec pcts, TypeSpec rts, std::string on, std::string aon)
    :Identifier(n, pr) {
    result_ts = rts;
    pivot_cast_ts = pcts;
    operation_name = on;
    arg_operation_name = aon;
}

Value *RecordWrapperIdentifier::matched(Value *pivot, Scope *scope, TypeMatch &match) {
    if (!pivot)
        throw INTERNAL_ERROR;
    
    TypeSpec rts = typesubst(result_ts, match);
    TypeSpec pcts = typesubst(pivot_cast_ts, match);
        
    Value *wrapper = make<RecordWrapperValue>(pivot, pcts, rts, operation_name, arg_operation_name, scope);
    
    return wrapper;
}



ClassWrapperIdentifier::ClassWrapperIdentifier(std::string n, PivotRequirement pr, TypeSpec pcts, std::string on, bool ag)
    :Identifier(n, pr) {
    pivot_cast_ts = pcts;
    operation_name = on;
    autogrow = ag;
}

Value *ClassWrapperIdentifier::matched(Value *pivot, Scope *scope, TypeMatch &match) {
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




Yield::Yield(std::string n, YieldableValue *yv)
    :Identifier(n, RVALUE_PIVOT) {
    yieldable_value = yv;
}

Value *Yield::matched(Value *cpivot, Scope *scope, TypeMatch &match) {
    return make<YieldValue>(yieldable_value);
}

