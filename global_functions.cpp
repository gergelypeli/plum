
// Value operations

TypeSpec get_typespec(Value *value) {
    return value ? value->ts : NO_TS;
}


Value *make_variable_value(Variable *decl, Value *pivot, TypeMatch &match) {
    return new VariableValue(decl, pivot, match);
}


Value *make_partial_variable_value(PartialVariable *decl, Value *pivot, TypeMatch &match) {
    return new PartialVariableValue(decl, pivot, match);
}


bool partial_variable_is_initialized(std::string name, Value *pivot) {
    PartialVariableValue *pv = dynamic_cast<PartialVariableValue *>(pivot);
    if (!pv)
        throw INTERNAL_ERROR;
        
    return pv->is_initialized(name);
}


Value *make_role_value(Variable *decl, Value *pivot) {
    return new RoleValue(decl, pivot);
}


Value *make_function_call_value(Function *decl, Value *pivot, TypeMatch &match) {
    return new FunctionCallValue(decl, pivot, match);
}


Value *make_type_value(TypeSpec ts) {
    return new TypeValue(ts);
}


Value *make_code_block_value(TypeSpec *context) {
    return new CodeBlockValue(context);
}


Value *make_multi_value() {
    return new MultiValue();
}


Value *make_eval_value(std::string en) {
    return new EvalValue(en);
}


Value *make_yield_value(EvalScope *es) {
    return new YieldValue(es);
}


Value *make_declaration_value(std::string name, TypeSpec *context) {
    return new DeclarationValue(name, context);
}


Value *make_partial_declaration_value(std::string name, PartialVariableValue *pivot) {
    return new PartialDeclarationValue(name, pivot);
}


Value *make_basic_value(TypeSpec ts, int number) {
    return new BasicValue(ts, number);
}


Value *make_string_literal_value(std::string text) {
    return new StringLiteralValue(text);
}


Value *make_code_scope_value(Value *value, CodeScope *code_scope) {
    return new CodeScopeValue(value, code_scope);
}


Value *make_scalar_conversion_value(Value *p) {
    return new ScalarConversionValue(p);
}


Value *make_void_conversion_value(Value *p) {
    return new VoidConversionValue(p);
}


Value *peek_void_conversion_value(Value *v) {
    VoidConversionValue *vcv = dynamic_cast<VoidConversionValue *>(v);
    
    return vcv ? vcv->orig.get() : v;
}


Value *make_boolean_conversion_value(Value *p) {
    return new BooleanConversionValue(p);
}


Value *make_implementation_conversion_value(ImplementationType *imt, Value *p, TypeMatch &match) {
    return new ImplementationConversionValue(imt, p, match);
}


Value *make_boolean_not_value(Value *p) {
    TypeMatch match;
    
    if (!typematch(BOOLEAN_TS, p, match))
        throw INTERNAL_ERROR;
        
    return new BooleanOperationValue(COMPLEMENT, p, match);
}


Value *make_null_reference_value(TypeSpec ts) {
    return new NullReferenceValue(ts);
}


Value *make_null_string_value() {
    return new NullStringValue();
}


Value *make_array_empty_value(TypeSpec ts) {
    return new ArrayEmptyValue(ts);
}


Value *make_array_initializer_value(TypeSpec ts) {
    return new ArrayInitializerValue(ts);
}


Value *make_circularray_empty_value(TypeSpec ts) {
    return new CircularrayEmptyValue(ts);
}


Value *make_circularray_initializer_value(TypeSpec ts) {
    return new CircularrayInitializerValue(ts);
}


Value *make_rbtree_empty_value(TypeSpec ts) {
    return new RbtreeEmptyValue(ts);
}


Value *make_rbtree_reserved_value(TypeSpec ts) {
    return new RbtreeReservedValue(ts);
}


Value *make_unicode_character_value() {
    return new UnicodeCharacterValue();
}


Value *make_integer_definition_value() {
    return new IntegerDefinitionValue();
}


Value *make_enumeration_definition_value() {
    return new EnumerationDefinitionValue();
}


Value *make_treenumeration_definition_value() {
    return new TreenumerationDefinitionValue();
}


Value *make_record_definition_value() {
    return new RecordDefinitionValue();
}


Value *make_record_initializer_value(TypeMatch &match) {
    return new RecordInitializerValue(match);
}


Value *make_record_preinitializer_value(TypeSpec ts) {
    return new RecordPreinitializerValue(ts);
}


Value *make_record_postinitializer_value(Value *v) {
    return new RecordPostinitializerValue(v);
}


Value *make_class_definition_value() {
    return new ClassDefinitionValue();
}


Value *make_class_preinitializer_value(TypeSpec ts) {
    return new ClassPreinitializerValue(ts);
}


Value *make_interface_definition_value() {
    return new InterfaceDefinitionValue();
}


Value *make_implementation_definition_value() {
    return new ImplementationDefinitionValue();
}


Value *make_cast_value(Value *v, TypeSpec ts) {
    return new CastValue(v, ts);
}


Value *make_equality_value(bool no, Value *v) {
    return new EqualityValue(no, v);
}


Value *make_comparison_value(BitSetOp bs, Value *v) {
    return new ComparisonValue(bs, v);
}


DeclarationValue *make_declaration_by_value(std::string name, Value *v, Scope *scope) {
    DeclarationValue *dv = new DeclarationValue(name);
    bool ok = dv->use(v, scope);
    if (!ok)
        throw INTERNAL_ERROR;
    return dv;
}

DeclarationValue *declaration_value_cast(Value *value) {
    return dynamic_cast<DeclarationValue *>(value);
}


Declaration *declaration_get_decl(DeclarationValue *dv) {
    return dv->get_decl();
}


bool unpack_value(Value *v, std::vector<TypeSpec> &tss) {
    return v->unpack(tss);
}


Value *make_record_unwrap_value(TypeSpec cast_ts, Value *v) {
    return new RecordUnwrapValue(cast_ts, v);
}


Value *make_record_wrapper_value(Value *pivot, TypeSpec pivot_cast_ts, TypeSpec arg_ts, TypeSpec arg_cast_ts, TypeSpec result_ts, std::string operation_name) {
    return new RecordWrapperValue(pivot, pivot_cast_ts, arg_ts, arg_cast_ts, result_ts, operation_name);
}


Value *make_class_wrapper_initializer_value(Value *object, Value *value) {
    return new ClassWrapperInitializerValue(object, value);
}


// Declaration operations

Variable *variable_cast(Declaration *decl) {
    return dynamic_cast<Variable *>(decl);
}


HeapType *heap_type_cast(Type *t) {
    return dynamic_cast<HeapType *>(t);
}


// TypeSpec operations

bool is_implementation(Type *t, TypeMatch &match, TypeSpecIter target, TypeSpec &ifts) {
    ImplementationType *imp = dynamic_cast<ImplementationType *>(t);

    if (imp) {
        ifts = imp->get_interface_ts(match);
        
        if (ifts[0] == *target)
            return true;
    }

    return false;
}


Value *find_implementation(Scope *inner_scope, TypeMatch &match, TypeSpecIter target, Value *orig, TypeSpec &ifts) {
    if (!inner_scope)
        return NULL;

    for (auto &d : inner_scope->contents) {
        ImplementationType *imp = dynamic_cast<ImplementationType *>(d.get());

        if (imp) {
            ifts = imp->get_interface_ts(match);

            // FIXME: check for proper type match!
            if (ifts[0] == *target) {
                // Direct implementation
                return make_implementation_conversion_value(imp, orig, match);
            }
            else {
                // Maybe indirect implementation
                Scope *ifscope = ifts.get_inner_scope();
                TypeMatch ifmatch = type_parameters_to_match(ifts);
                Value *v = find_implementation(ifscope, ifmatch, target, orig, ifts);

                if (v)
                    return v;
            }
        }
    }

    return NULL;
}


TypeMatch type_parameters_to_match(TypeSpec ts) {
    TypeMatch fake_match;
    fake_match.push_back(ts);
    TypeSpecIter tsi(ts.begin());
    tsi++;
    
    for (unsigned i = 0; i < ts[0]->parameter_count; i++) {
        fake_match.push_back(TypeSpec(tsi));
        tsi += fake_match.back().size();
    }
    
    return fake_match;
}


TypeSpec typesubst(TypeSpec &tt, TypeMatch &match) {
    TypeSpec ts;
    
    for (TypeSpecIter tti = tt.begin(); tti != tt.end(); tti++) {
        if (*tti == same_type) {
            if (match.size() < 2) {
                std::cerr << "No TypeMatch parameters while substituting Same!\n";
                throw INTERNAL_ERROR;
            }
                
            for (TypeSpecIter si = match.at(1).begin(); si != match.at(1).end(); si++)
                ts.push_back(*si);
        }
        else
            ts.push_back(*tti);
    }
    
    return ts;
}


Value *rolematch(Value *v, TypeSpecIter tsi, TypeSpecIter target, TypeSpec &ifts) {
    // Return a role of v with an unprefixed type of s, with the front type
    // being is equal to t, but with arbitrary type parameters, potentially
    // derived from the type parameters of s. Or NULL, if it can't be done.
    // May call itself recursively.
    std::cerr << "Trying rolematch from " << tsi << " to " << target << ".\n";
    return (*tsi)->autoconv(tsi, target, v, ifts);
}


// Tries to convert value to match tt. The value argument may be overwritten with the
// converted one. The match argument just returns some information gained during
// the conversion.
// There's a bit of a mess around the Void type that probably won't be cleaned up.
// When used in a pattern, it is interpreted as follows:
//  * Void - accepts only NULL value. Not even Void.
//  * Void Lvalue - just plain illegal.
//  * Void Code - accepts everything, even Void and NULL.
// No other type accepts Void, not even Any.

bool typematch(TypeSpec tt, Value *&value, TypeMatch &match, CodeScope *code_scope) {
#define MATCHLOG if (false)

    //if (tt == VOID_TS)
    //    throw INTERNAL_ERROR;  // FIXME: to be removed after debugging, needed for :repeat
        
    if (tt == NO_TS)
        throw INTERNAL_ERROR;  // Mustn't be called with NO_TS

    // This may overwrite an older match, but that's OK, we just need to
    // preserve the type parameters.
    if (match.size() > 0)
        match[0].clear();
    else
        match.push_back(TypeSpec());

    tt = typesubst(tt, match);

    MATCHLOG std::cerr << "Matching " << get_typespec(value) << " to pattern " << tt << "...\n";

    // Checking NULL value
    if (!value) {
        if (tt[0] == void_type) {
            MATCHLOG std::cerr << "Matched nothing for Void.\n";
            match[0] = tt;
            return true;
        }
        else if (tt[0] == code_type && tt[1] == void_type) {
            MATCHLOG std::cerr << "Matched nothing for Void Code.\n";
            match[0] = tt;
            value = make_code_scope_value(NULL, code_scope);
            return true;
        }
        else if (tt[0] == ovalue_type) {
            MATCHLOG std::cerr << "Matched nothing for Ovalue.\n";
            match[0] == VOID_TS;
            return true;
        }
        else {
            MATCHLOG std::cerr << "No match, nothing for something.\n";
            return false;
        }
    }
    
    //if (tt[0] == void_type) {
    //    MATCHLOG std::cerr << "No match, something for Void.\n";
    //    return false;
    //}
    
    TypeSpec ss = get_typespec(value);
    
    // Checking Void value
    if (ss[0] == void_type) {
        if (tt[0] == void_type) {
            MATCHLOG std::cerr << "Matched Void for Void.\n";
            match[0] = tt;
            return true;
        }
        else if (tt[0] == code_type && tt[1] == void_type) {
            MATCHLOG std::cerr << "Matched Void for Void Code.\n";
            match[0] = tt;
            value = make_code_scope_value(value, code_scope);
            return true;
        }
        else {
            MATCHLOG std::cerr << "No match, Void for something.\n";
            return false;
        }
    }

    if (tt[0] == void_type) {
        MATCHLOG std::cerr << "Matched something for Void.\n";
        match[0] = tt;
        value = make_void_conversion_value(value);
        return true;
    }

    TypeSpecIter s(ss.begin());
    TypeSpecIter t(tt.begin());
    
    bool strict = false;
    bool need_code_conversion = false;
    bool need_scalar_conversion = false;
    bool need_boolean_conversion = false;

    // Checking attribute templates
    if (*t == lvalue_type) {
        if (*s != lvalue_type) {
            MATCHLOG std::cerr << "No match, lvalue expected!\n";
            return false;
        }
        
        strict = true;
        match[0].push_back(*t);
        s++;
        t++;

        if (*s == void_type || *t == void_type)
            throw INTERNAL_ERROR;
    }
    else if (*t == code_type) {  // evalue
        match[0].push_back(*t);
        t++;

        if (*t == void_type) {
            match[0].push_back(*t);
            MATCHLOG std::cerr << "Matched something for Void Code.\n";
            value = make_void_conversion_value(value);
            value = make_code_scope_value(value, code_scope);
            return true;
        }
        
        need_code_conversion = true;
    }
    else if (*t == ovalue_type) {
        t++;
    }
    
    // Drop unnecessary attribute
    if (*s == lvalue_type || *s == ovalue_type) {
        s++;
    }

    bool ok = false;

    // Allow any_type match references
    if (*t == any_type) {
        ok = true;
    }
    
    // Checking references
    if (ok)
        ;
    else if (*s == reference_type && *t == reference_type) {
        match[0].push_back(*t);
        s++;
        t++;
    }
    else if (*s == reference_type) {
        s++;
        //MATCHLOG std::cerr << "No match, reference mismatch!\n";
        //return false;
    }

    // Checking main type
    if (!ok && *t == any_type) {
        ok = true;
    }

    if (!ok && *s == *t) {
        ok = true;
    }
    
    if (!ok && strict) {
        // For conversion to lvalue, only an exact match was acceptable
        MATCHLOG std::cerr << "No match, lvalue types differ!\n";
        return false;
    }

    if (!ok && *s == multi_type) {
        std::vector<TypeSpec> tss;
        
        if (!unpack_value(value, tss))
            throw INTERNAL_ERROR;
        
        ss = tss[0];
        s = ss.begin();
        need_scalar_conversion = true;
        // Not yet ok, keep on matching
        std::cerr << "Trying unpacking to " << ss << ".\n";
    }
    
    if (!ok && *t == boolean_type) {
        //match[0].push_back(*t);
        MATCHLOG std::cerr << "Matched as " << match[0] << ".\n";
        //value = make_boolean_conversion_value(value);
        //if (tt[0] == code_type)
        //    value = make_code_scope_value(value);
        //return true;
        s = BOOLEAN_TS.begin();
        ok = true;
        need_boolean_conversion = true;
    }
    
    if (!ok) {
        TypeSpec ifts;
        Value *role = rolematch(value, s, t, ifts);
        
        if (role) {
            value = role;
            ss = ifts;
            s = ss.begin();
            ok = true;
        }
    }
    
    unsigned counter = 1;
    
    while (counter--) {
        if (*s == *t) {
            match[0].push_back(*t);
            counter += (*s)->parameter_count;
            s++;
            t++;
        }
        else if (*t == any_type) {
            match.push_back(TypeSpec());
            unsigned c = 1;
    
            while (c--) {
                c += (*s)->parameter_count;
                match.back().push_back(*s);
                match[0].push_back(*s);
                s++;
            }

            t++;
        }
        else {
            MATCHLOG std::cerr << "No match, types differ!\n";
            return false;
        }
    }
    
    MATCHLOG std::cerr << "Matched as " << match[0];
    MATCHLOG if (match.size() > 1) { std::cerr << ", parameters"; for (unsigned i = 1; i < match.size(); i++) std::cerr << " " << match[i]; }
    MATCHLOG std::cerr << ".\n";

    if (need_scalar_conversion)
        value = make_scalar_conversion_value(value);
        
    if (need_boolean_conversion)
        value = make_boolean_conversion_value(value);

    if (need_code_conversion)
        value = make_code_scope_value(value, code_scope);
        
    return true;
}
