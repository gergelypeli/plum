
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


Value *make_role_value(Variable *decl, Value *pivot, TypeMatch &tm) {
    return new RoleValue(decl, pivot, tm);
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


Value *make_rbtree_initializer_value(TypeSpec ts) {
    return new RbtreeInitializerValue(ts);
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


Value *make_record_wrapper_value(Value *pivot, TypeSpec pivot_cast_ts, TypeSpec result_ts, std::string operation_name, std::string arg_operation_name) {
    return new RecordWrapperValue(pivot, pivot_cast_ts, result_ts, operation_name, arg_operation_name);
}


Value *make_class_wrapper_initializer_value(Value *object, Value *value) {
    return new ClassWrapperInitializerValue(object, value);
}


Value *make_option_none_value(TypeSpec ts) {
    return new OptionNoneValue(ts);
}


Value *make_option_some_value(TypeSpec ts) {
    return new OptionSomeValue(ts);
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
        if (*tti == same_type || *tti == same2_type || *tti == same3_type) {
            int mi = (*tti == same_type ? 1 : *tti == same2_type ? 2 : 3);
            
            if (match[mi] == NO_TS) {
                std::cerr << "No matched Any type while substituting Same!\n";
                throw INTERNAL_ERROR;
            }
            
            for (TypeSpecIter si = match[mi].begin(); si != match[mi].end(); si++)
                ts.push_back(*si);
        }
        else
            ts.push_back(*tti);
    }
    
    return ts;
}


Value *rolematch(Value *v, TypeSpec s, TypeSpecIter target, TypeSpec &ifts) {
    // Return a role of v with an unprefixed type of s, with the front type
    // being is equal to t, but with arbitrary type parameters, potentially
    // derived from the type parameters of s. Or NULL, if it can't be done.
    // May call itself recursively.
    std::cerr << "Trying rolematch from " << s << " to " << target << ".\n";
    return s.autoconv(target, v, ifts);
}


// *******

#define MATCHLOG if (false)

bool match_type_parameters(TypeSpecIter s, TypeSpecIter t, TypeMatch &match) {
    unsigned counter = 1;
    
    while (counter--) {
        if (*s == *t) {
            match[0].push_back(*t);
            counter += (*s)->parameter_count;
            s++;
            t++;
        }
        else if (*t == any_type || *t == any2_type || *t == any3_type) {
            int mi = (*t == any_type ? 1 : *t == any2_type ? 2 : 3);
            unsigned c = 1;
    
            while (c--) {
                c += (*s)->parameter_count;
                match[mi].push_back(*s);
                match[0].push_back(*s);
                s++;
            }

            t++;
        }
        else {
            MATCHLOG std::cerr << "No match, type parameters differ!\n";
            return false;
        }
    }
    
    return true;
}


bool match_regular_type(TypeSpecIter s, TypeSpecIter t, TypeMatch &match, Value *&value) {
    if (*t == boolean_type) {
        match[0].push_back(*t);
        MATCHLOG std::cerr << "Matched as " << match[0] << ".\n";
        value = make_boolean_conversion_value(value);
        return true;
    }
    
    TypeSpec ifts;
    Value *role = rolematch(value, TypeSpec(s), t, ifts);
        
    if (role) {
        value = role;
        TypeSpec ss = ifts;
        s = ss.begin();
        return match_type_parameters(s, t, match);
    }
    
    MATCHLOG std::cerr << "No match, unconvertible types!\n";
    return false;
}


bool match_special_type(TypeSpecIter s, TypeSpecIter t, TypeMatch &match, Value *&value, bool strict) {
    if (*s == reference_type && *t == reference_type) {
        match[0].push_back(*t);
        s++;
        t++;
    }

    if (*s == *t) {
        return match_type_parameters(s, t, match);
    }
    
    if (strict) {
        // For conversion to lvalue, only an exact match was acceptable
        MATCHLOG std::cerr << "No match, lvalue types differ!\n";
        return false;
    }

    if (*t == void_type) {
        if (*s == void_type)
            return true;
            
        value = make_void_conversion_value(value);
        return true;
    }
    
    return match_regular_type(s, t, match, value);
}


bool match_anymulti_type(TypeSpecIter s, TypeSpecIter t, TypeMatch &match, Value *&value, bool strict) {
    // Allow any_type match references
    
    if (*t == any_type) {
        if (*s == void_type) {
            MATCHLOG std::cerr << "No match, Void for Any!\n";
            return false;
        }
        else if (*s == multi_type) {
            MATCHLOG std::cerr << "No match, Multi for Any!\n";
            return false;
        }
        
        return match_type_parameters(s, t, match);
    }
    
    if (*t == multi_type) {
        if (*s != multi_type) {
            MATCHLOG std::cerr << "No match, scalar for Multi!\n";
            return false;
        }
        
        // Match Multi to Multi
        return match_special_type(s, t, match, value, strict);
    }
    else {
        if (*s == multi_type) {
            // A Multi is being converted to something non-Multi.
            // Since a Multi can never be in a pivot position, this value must be a plain
            // argument, so if converting it fails, then it will be a fatal error. So
            // it's OK to wrap it in a scalarization, because this is our only conversion chance.
            std::vector<TypeSpec> tss;
        
            if (!unpack_value(value, tss))
                throw INTERNAL_ERROR;
        
            TypeSpec ss = tss[0];
            s = ss.begin();
            MATCHLOG std::cerr << "Unpacking Multi to " << ss << ".\n";
            value = make_scalar_conversion_value(value);

            // ss is a local variable, so call this in the scope
            return match_special_type(s, t, match, value, strict);
        }

        // Match scalar to scalar
        return match_special_type(s, t, match, value, strict);
    }
}


bool match_attribute_type(TypeSpecIter s, TypeSpecIter t, TypeMatch &match, Value *&value, CodeScope *code_scope) {
    // Checking attribute templates
    
    if (*t == lvalue_type) {
        if (!value) {
            MATCHLOG std::cerr << "No match, nothing for Lvalue!\n";
            return false;
        }
        else if (*s != lvalue_type) {
            MATCHLOG std::cerr << "No match, rvalue for Lvalue!\n";
            return false;
        }
        
        match[0].push_back(*t);
        s++;
        t++;

        if (*s == void_type || *t == void_type)
            throw INTERNAL_ERROR;
        
        return match_anymulti_type(s, t, match, value, true);
    }
    else if (*t == code_type) {  // evalue
        match[0].push_back(*t);
        t++;

        if (!value) {
            if (*t == void_type) {
                MATCHLOG std::cerr << "Matched nothing for Void Code.\n";
                match[0].push_back(void_type);
                return true;
            }
            else {
                MATCHLOG std::cerr << "No match, nothing for nonvoid Code!\n";
                return false;
            }
        }

        // value is non-NULL from here

        // Drop unnecessary attribute
        if (*s == lvalue_type || *s == ovalue_type) {
            s++;
        }

        if (!match_anymulti_type(s, t, match, value, false))
            return false;

        value = make_code_scope_value(value, code_scope);
        return true;
    }
    else if (*t == ovalue_type) {
        if (!value) {
            MATCHLOG std::cerr << "Matched nothing for Ovalue.\n";
            return true;
        }
        
        t++;
        
        if (*s == lvalue_type || *s == ovalue_type) {
            s++;
        }

        return match_anymulti_type(s, t, match, value, false);
    }
    else {
        if (!value) {
            MATCHLOG std::cerr << "No match, nothing for something!\n";
            return false;
        }
        
        if (*s == lvalue_type || *s == ovalue_type) {
            s++;
        }

        return match_anymulti_type(s, t, match, value, false);
    }
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
    if (tt == NO_TS)
        throw INTERNAL_ERROR;  // Mustn't be called with NO_TS

    MATCHLOG std::cerr << "Matching " << get_typespec(value) << " to pattern " << tt << "...\n";

    if (match.size() != 0)
        throw INTERNAL_ERROR;

    match.push_back(NO_TS);
    match.push_back(NO_TS);
    match.push_back(NO_TS);
    match.push_back(NO_TS);
    
    TypeSpec ss = get_typespec(value);
    TypeSpecIter s(ss.begin());
    TypeSpecIter t(tt.begin());
    
    if (!match_attribute_type(s, t, match, value, code_scope)) {
        //std::cerr << "Typematch failed with " << get_typespec(value) << " to " << tt << "!\n";
        return false;
    }
        
    MATCHLOG std::cerr << "Matched as " << match[0];
    MATCHLOG if (match.size() > 1) { std::cerr << ", parameters"; for (unsigned i = 1; i < match.size(); i++) std::cerr << " " << match[i]; }
    MATCHLOG std::cerr << ".\n";

    return true;
}
