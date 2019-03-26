
// Stage 4

bool is_typedefinition(Expr *expr) {
    if (expr->type != Expr::DECLARATION)
        return false;
        
    if (expr->args.size() != 1)
        return false;
        
    Expr *e = expr->args[0].get();
    
    if (e->type != Expr::CONTROL)
        return false;
        
    return colon_scope->is_typedefinition(e->text);
}


Value *lookup_unchecked(std::string name, Value *pivot, Scope *scope) {
    if (pivot && pivot->ts[0] == multi_type) {
        // Conversions from Multi to a scalar can only be attempted once (see typematch),
        // but it's not a problem, since we don't want Multi pivots anyway. But for correct
        // operation we should catch this case here.
        // Note that Multilvalue and Multitype passes this test.
        return NULL;
    }
    
    //std::cerr << "Looking up  " << get_typespec(pivot) << " " << name << " definition.\n";
    Value *value = NULL;
    
    // Types are not pivot-scoped, but explicit scoped.
    
    if (pivot == colon_value) {  // I hope this is a dummy (TM)
        // Implicitly scoped identifier
        std::cerr << "Looking up in Colon.\n";
        value = colon_scope->lookup(name, NULL, scope);
    }
    //else if (name[0] == '.' && isupper(name[1])) {
    //    // Local type, look up in enclosing data scope
    //    for (Scope *s = scope; s; s = s->outer_scope) {
    //        if (s->type == DATA_SCOPE || s->type == SINGLETON_SCOPE) {
    //            value = s->lookup(name.substr(1), pivot, scope);
    //            break;
    //        }
    //    }
    //}
    else if (isupper(name[0])) {
        // Global type, look up in module level
        //Scope *module_scope = scope->get_module_scope();
        //value = module_scope->lookup(name, pivot, scope);
        Scope *start = (scope->type == CODE_SCOPE || scope->type == ARGUMENT_SCOPE ? scope->get_function_scope()->outer_scope : scope);
        
        for (Scope *s = start; s; s = s->outer_scope) {
            value = s->lookup(name, pivot, scope);
        
            if (value)
                break;
        }
        
    }
    else if (pivot) {
        // Pivoted value
        std::cerr << "Looking up in pivot scope.\n";
        value = pivot->lookup_inner(name, scope);
    }
    else if ((name[0] == '.' && islower(name[1])) || (islower(name[0]) && name.find(".") != std::string::npos)) {
        // Module qualified identifier, look up in module scope
        Scope *module_scope = scope->get_module_scope();
        value = module_scope->lookup(name, pivot, scope);
    }
    else if (islower(name[0]) || name[0] == '$' || name[0] == '<') {
        // Local variable, look up in function body and head
        for (Scope *s = scope; s && (s->type == CODE_SCOPE || s->type == FUNCTION_SCOPE); s = s->outer_scope) {
            value = s->lookup(name, pivot, scope);
        
            if (value)
                break;
        }
    }
    else {
        std::cerr << "Weird identifier: " << name << "!\n";
        throw INTERNAL_ERROR;
    }

    if (!value) {
        if (name == "is_equal" || name == "not_equal") {
            std::cerr << "Trying equal fallback for missing " << name << ".\n";
            Value *fallback = lookup_unchecked("equal", pivot, scope);
            
            if (fallback) {
                bool no = (name == "not_equal");
                value = make<EqualityValue>(no, fallback);
            }
        }
    }
    
    if (!value) {
        if (name == "is_equal" || name == "not_equal" ||
            name == "is_less" || name == "is_greater" ||
            name == "not_less" || name == "not_greater"
        ) {
            std::cerr << "Trying compare fallback for missing " << name << ".\n";
            Value *fallback = lookup_unchecked("compare", pivot, scope);
            
            if (fallback) {
                // Comparison results are signed integers
                ConditionCode cc = (
                    name == "is_equal" ? CC_EQUAL :
                    name == "not_equal" ? CC_NOT_EQUAL :
                    name == "is_less" ? CC_LESS :
                    name == "is_greater" ? CC_GREATER :
                    name == "not_less" ? CC_GREATER_EQUAL :
                    name == "not_greater" ? CC_LESS_EQUAL :
                    throw INTERNAL_ERROR
                );
                
                value = make<ComparisonValue>(cc, fallback);
            }
        }
    }
    
    // TODO: we should print the definition pivot type, not the value type
    if (value) {
        std::cerr << "Found       " << get_typespec(pivot) << " " << name << " returning " << value->ts << ".\n";
        //std::cerr << "... type " << typeid(*value).name() << ".\n";
    }
        
    return value;
}


Value *lookup(std::string name, Value *pivot, Scope *scope, Expr *expr, TypeSpec *context) {
    //std::cerr << "Looking up  " << pts << " " << name << " definition.\n";
    Value *value = lookup_unchecked(name, pivot, scope);
    
    if (!value) {
        std::cerr << "No match for " << get_typespec(pivot) << " " << name << " at " << expr->token << "!\n";
        throw TYPE_ERROR;
    }

    value->set_token(expr->token);
    value->set_context_ts(context);
    
    bool ok = value->check(expr->args, expr->kwargs, scope);

    if (!ok) {
        std::cerr << "Argument problem for " << expr->token << "!\n";
        throw TYPE_ERROR;
    }
    
    return value;
}


Value *lookup_fake(std::string name, Value *pivot, Scope *scope, Token token, TypeSpec *context, Variable *arg_var) {
    Expr fake_expr(Expr::IDENTIFIER, token, name);
    
    if (arg_var) {
        Expr *fake_arg_expr = new Expr(Expr::IDENTIFIER, token, arg_var->name);
        fake_expr.args.push_back(std::unique_ptr<Expr>(fake_arg_expr));
    }
    
    return lookup(name, pivot, scope, &fake_expr, context);
}


Value *lookup_switch(Scope *scope, Token token) {
    SwitchScope *ss = scope->get_switch_scope();
    
    if (!ss) {
        std::cerr << "Pivotless matcher outside of :switch at " << token << "!\n";
        throw TYPE_ERROR;
    }
    
    Variable *v = ss->get_variable();
    Value *p = v->match(v->name, NULL, scope);
    
    if (!p)
        throw INTERNAL_ERROR;
        
    return p;
}


TypeSpec initializer_ts(Value *p, TypeSpec *context, Token token) {
    TypeSpec ts;
    
    if (p) {
        TypeValue *tv = ptr_cast<TypeValue>(p);
        
        if (tv)
            ts = tv->represented_ts;
        else {
            std::cerr << "Initializer with nontype context: " << token << "!\n";
            throw TYPE_ERROR;
        }
    }
    else if (context)
        ts = *context;
    else {
        std::cerr << "Initializer without type context: " << token << "\n";
        throw TYPE_ERROR;
    }

    // TODO: strip some prefixes
    if (ts[0] == code_type)
        ts = ts.unprefix(code_type);
    else if (ts[0] == ovalue_type)
        ts = ts.unprefix(ovalue_type);
    
    // We must have checked this.
    if (!ts.has_meta(value_metatype) && !ts.has_meta(metatype_hypertype)) {
        std::cerr << "Initializer with nonvalue type context: " << ts << " at " << token << "!\n";
        throw TYPE_ERROR;
    }
    
    return ts;
}


Value *typize(Expr *expr, Scope *scope, TypeSpec *context) {
    // Sanity check, interface types can't be contexts, as they're not concrete types,
    // so they're useless for initializers, but would allow a multi-tailed control to
    // have multiple concrete result types.
    if (context && (*context).rvalue().has_meta(interface_metatype))
        throw INTERNAL_ERROR;

    // NULL context is used for pivot position expressions.
    // &ANY_TS context may be used for arguments, this also make tuples into code blocks.

    Value *value = NULL;

    if (!expr)
        throw INTERNAL_ERROR;
    else if (expr->type == Expr::TUPLE) {
        if (expr->pivot) {
            std::cerr << "A TUPLE had a pivot argument!\n";
            throw INTERNAL_ERROR;
        }

        if (context) {
            value = make<CodeBlockValue>(context);
        
            bool ok = value->check(expr->args, expr->kwargs, scope);
            if (!ok) {
                std::cerr << "Code block error!\n";
                throw TYPE_ERROR;
            }
        }
        else {
            value = make<MultiValue>();
        
            bool ok = value->check(expr->args, expr->kwargs, scope);
            if (!ok) {
                std::cerr << "Multi value error: " << expr->token << "\n";
                throw TYPE_ERROR;
            }
        }
    }
    else if (expr->type == Expr::DECLARATION) {
        std::string name = expr->text;
        if (name == "")
            name = "<anonymous>";
        Value *p = expr->pivot ? typize(expr->pivot.get(), scope) : NULL;
        
        if (p) {
            std::cerr << "Invalid declaration with pivot: " << expr->token << "!\n";
            throw TYPE_ERROR;
        }
        else {
            value = make<DeclarationValue>(name, context);
            bool ok = value->check(expr->args, expr->kwargs, scope);
        
            if (!ok) {
                std::cerr << "Couldn't declare " << name << "!\n";
                throw TYPE_ERROR;
            }

            std::cerr << "Declared " << name << ".\n";
        }
    }
    else if (expr->type == Expr::IDENTIFIER) {
        std::string name = expr->text;
        Value *p = expr->pivot ? typize(expr->pivot.get(), scope) : NULL;

        value = lookup(name, p, scope, expr);
        
        if (!value)
            throw TYPE_ERROR;
    }
    else if (expr->type == Expr::CONTROL) {
        std::string name = expr->text;
        Value *p = expr->pivot ? typize(expr->pivot.get(), scope) : NULL;
        //if (p)
        //    p->set_marker(marker);

        TypeSpec ts;
        
        if (p) {
            if (p->ts.is_meta()) {
                ts = ptr_cast<TypeValue>(p)->represented_ts;
                context = &ts;
            }
            else {
                std::cerr << "Control with nontype context!\n";
                throw TYPE_ERROR;
            }
        }

        value = lookup(name, colon_value, scope, expr, context);  // dummy value

        if (!value)
            throw TYPE_ERROR;
    }
    else if (expr->type == Expr::EVAL) {
        value = make<EvalValue>(expr->text);
        value->set_context_ts(context);
        
        if (!value->check(expr->args, expr->kwargs, scope))
            throw TYPE_ERROR;
    }
    else if (expr->type == Expr::INITIALIZER) {
        std::string name = expr->text;
        Value *p = expr->pivot ? typize(expr->pivot.get(), scope) : NULL;
        //if (p)
        //    p->set_marker(marker);  // just in case

        StringTemplateValue *stv = ptr_cast<StringTemplateValue>(p);
        
        if (stv) {
            if (name.size()) {
                std::cerr << "No named initializers for string templates!\n";
                throw TYPE_ERROR;
            }
            
            value = new InterpolationValue(stv->fragments, expr->token);

            bool ok = value->check(expr->args, expr->kwargs, scope);
        
            if (!ok) {
                std::cerr << "Interpolation problem for " << expr->token << "!\n";
                throw TYPE_ERROR;
            }
        }
        else {
            TypeSpec ts = initializer_ts(p, context, expr->token);
            
            if (name.size() == 0)
                name = "{}";
            
            value = ts.lookup_initializer(name, scope);
            
            if (!value) {
                std::cerr << "No initializer " << ts << " `" << name << "!\n";
                throw TYPE_ERROR;
            }
            
            bool ok = value->check(expr->args, expr->kwargs, scope);
        
            if (!ok) {
                std::cerr << "Initializer argument problem for " << expr->token << "!\n";
                throw TYPE_ERROR;
            }
            
            std::cerr << "Using initializer " << ts << " `" << name << ".\n";
            //std::cerr << "... with type " << value->ts << "\n";
        }
    }
    else if (expr->type == Expr::MATCHER) {
        std::string name = expr->text;
        Value *p = expr->pivot ? typize(expr->pivot.get(), scope) : NULL;

        if (!p)
            p = lookup_switch(scope, expr->token);

        value = p->ts.lookup_matcher(name, p, scope);
    
        if (!value) {
            std::cerr << "No matcher " << p->ts << " ~" << name << "!\n";
            throw TYPE_ERROR;
        }

        bool ok = value->check(expr->args, expr->kwargs, scope);
    
        if (!ok) {
            std::cerr << "Matcher argument problem for " << expr->token << "!\n";
            throw TYPE_ERROR;
        }
        
        std::cerr << "Using matcher " << p->ts << " `" << name << ".\n";
    }
    else if (expr->type == Expr::UNSIGNED_NUMBER || expr->type == Expr::NEGATIVE_NUMBER) {
        Value *p = expr->pivot ? typize(expr->pivot.get(), scope) : NULL;

        bool is_negative = (expr->type == Expr::NEGATIVE_NUMBER);
        std::string text = expr->text;
        bool looks_float = false;
        
        for (unsigned i = 0; i < text.size(); i++) {
            char c = text[i];
            
            if (c == '.' || c == '+' || c == '-') {
                looks_float = true;
                break;
            }
        }
        
        TypeSpec *ctx = (context && *context != ANY_TS ? context : looks_float ? &FLOAT_TS : &INTEGER_TS);
        TypeSpec value_ts = initializer_ts(p, ctx, expr->token);
        Type *t = value_ts[0];
        IntegerType *it = ptr_cast<IntegerType>(t);
        
        if (t == float_type) {
            // Our parser works on character arrays
            int64 character_length = text.size();
            unsigned16 characters[character_length];
            
            for (unsigned i = 0; i < character_length; i++)
                characters[i] = text[i];
                
            double result;
            int64 character_count;
            
            bool ok = parse_float(characters, character_length, &result, &character_count);
            
            if (!ok || character_count < character_length) {
                std::cerr << "Invalid float literal '" << text << "' at " << expr->token << "!\n";
                throw TYPE_ERROR;
            }
                
            value = make<FloatValue>(value_ts, is_negative ? -result : result);
        }
        else if (it) {
            if (looks_float) {
                std::cerr << "Noninteger literal in a " << value_ts << " context: " << expr->token << "\n";
                throw TYPE_ERROR;
            }

            // parse the part without sign into a 64-bit unsigned
            // Our parser works on character arrays
            int64 character_length = text.size();
            unsigned16 characters[character_length];
            
            for (unsigned i = 0; i < character_length; i++)
                characters[i] = text[i];
                
            unsigned64 result;
            int64 character_count;

            bool ok = parse_unteger(characters, character_length, &result, &character_count);
            
            if (!ok || character_count < character_length) {
                std::cerr << "Invalid integer literal '" << text << "' at " << expr->token << "!\n";
                throw TYPE_ERROR;
            }

            if (it->is_unsigned) {
                if (
                    is_negative ||
                    (it->size == 1 && result > 255) ||
                    (it->size == 2 && result > 65535) ||
                    (it->size == 4 && result > 4294967295)
                ) {
                    std::cerr << "A " << value_ts << " literal out of range: " << expr->token << "\n";
                    throw TYPE_ERROR;
                }
            }
            else {
                if (
                    (it->size == 1 && result > (is_negative ? 128 : 127)) ||
                    (it->size == 2 && result > (is_negative ? 32768 : 32767)) ||
                    (it->size == 4 && result > (is_negative ? 2147483648 : 2147483647)) ||
                    (it->size == 8 && result > (is_negative ? 9223372036854775808U : 9223372036854775807U))
                ) {
                    std::cerr << "A " << value_ts << " literal out of range: " << expr->token << "\n";
                    throw TYPE_ERROR;
                }
            }

            value = make<BasicValue>(value_ts, is_negative ? -result : result);
        }
        else {
            std::cerr << "Numeric literal in a " << value_ts << " context: " << expr->token << "\n";
            throw TYPE_ERROR;
        }
    }
    else if (expr->type == Expr::STRING) {
        // String contents are not ASCII-fied, and left in the token only
        std::ustring utext = expr->token.utext.substr(1, expr->token.utext.size() - 2);
        
        Value *p = expr->pivot ? typize(expr->pivot.get(), scope) : NULL;

        TypeSpec *ctx = (context && *context != ANY_TS ? context : &STRING_TS);
        TypeSpec value_ts = initializer_ts(p, ctx, expr->token);

        if (value_ts == STRING_TS) {
            std::vector<std::ustring> fragments = interpolate_characters(brace_split(utext));
        
            if (fragments.size() == 1)
                value = make<StringLiteralValue>(fragments[0]);
            else
                value = make<StringTemplateValue>(fragments);
        }
        else if (value_ts == CHARACTER_TS) {
            if (utext.size() != 1) {
                std::cerr << "Invalid Character literal at " << expr->token << "\n";
                throw TYPE_ERROR;
            }
            
            value = make<BasicValue>(value_ts, utext[0]);
        }
        else {
            std::cerr << "Text literal in a " << value_ts << " context: " << expr->token << "\n";
            throw TYPE_ERROR;
        }
    }
    else {
        std::cerr << "Can't typize this now: " << expr->token << "!\n";
        throw INTERNAL_ERROR;
    }
    
    value->set_token(expr->token);
    return value;
}

