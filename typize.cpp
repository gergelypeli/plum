
// Stage 4

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
    
    for (Scope *s = scope; s; s = s->outer_scope) {
        value = s->lookup(name, pivot);
        
        if (value)
            break;
    }

    if (!value) {
        if (pivot) {
            //std::cerr << "Looking up in inner scopes, too.\n";
            value = pivot->lookup_inner(name);
        }
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


Value *lookup(std::string name, Value *pivot, Expr *expr, Scope *scope, TypeSpec *context) {
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


Value *lookup_fake(std::string name, Value *pivot, Token token, Scope *scope, TypeSpec *context, Variable *arg_var) {
    Expr fake_expr(Expr::IDENTIFIER, token, name);
    
    if (arg_var) {
        Expr *fake_arg_expr = new Expr(Expr::IDENTIFIER, token, arg_var->name);
        fake_expr.args.push_back(std::unique_ptr<Expr>(fake_arg_expr));
    }
    
    return lookup(name, pivot, &fake_expr, scope, context);
}


Value *lookup_switch(Scope *scope, Token token) {
    SwitchScope *ss = scope->get_switch_scope();
    
    if (!ss) {
        std::cerr << "Pivotless matcher outside of :switch at " << token << "!\n";
        throw TYPE_ERROR;
    }
    
    Variable *v = ss->get_variable();
    Value *p = v->match(v->name, NULL);
    
    if (!p)
        throw INTERNAL_ERROR;
        
    return p;
}


Value *typize(Expr *expr, Scope *scope, TypeSpec *context) {
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
            value->set_token(expr->token);
        
            bool ok = value->check(expr->args, expr->kwargs, scope);
            if (!ok) {
                std::cerr << "Code block error!\n";
                throw TYPE_ERROR;
            }
        }
        else {
            value = make<MultiValue>();
            value->set_token(expr->token);
        
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
            std::cerr << "Invalid declaration!\n";
            throw TYPE_ERROR;
        }
        else {
            value = make<DeclarationValue>(name, context);
            value->set_token(expr->token);
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

        if (!p && name.find(".") != std::string::npos) {
            std::string::size_type i = 0;
            
            while (islower(name[i])) {
                i = name.find_first_of(".", i);
                
                if (i == std::string::npos) {
                    i = name.size() + 1;
                    break;
                }
                    
                i += 1;
            }
            
            if (i > 0) {
                std::string module_name = name.substr(0, i - 1);
                name = (i < name.size() ? name.substr(i) : "");

                if (name.size() > 0) {
                    std::cerr << "Will lookup symbol " << name << " in module " << module_name << "\n";
                    scope = lookup_module(module_name, scope->get_module_scope());
                }
                else {
                    std::cerr << "Unexpected bare module name " << module_name << "!\n";
                    throw TYPE_ERROR;
                }
            }
        }
        
        value = lookup(name, p, expr, scope);
        
        if (!value)
            throw TYPE_ERROR;
    }
    else if (expr->type == Expr::CONTROL) {
        std::string name = ":" + expr->text;
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
        
        value = lookup(name, NULL, expr, scope, context);

        if (!value)
            throw TYPE_ERROR;
    }
    else if (expr->type == Expr::EVAL) {
        value = make<EvalValue>(expr->text);
        value->set_token(expr->token);
        value->set_context_ts(context);
        
        if (!value->check(expr->args, expr->kwargs, scope))
            throw TYPE_ERROR;
    }
    else if (expr->type == Expr::INITIALIZER) {
        std::string name = expr->text;
        Value *p = expr->pivot ? typize(expr->pivot.get(), scope) : NULL;
        //if (p)
        //    p->set_marker(marker);  // just in case

        StringLiteralValue *s = ptr_cast<StringLiteralValue>(p);
        
        if (s) {
            if (name.size()) {
                std::cerr << "No named initializers for string literals!\n";
                throw TYPE_ERROR;
            }
            
            value = interpolate(s->text, expr, scope);
        }
        else {
            TypeSpec ts;
            
            if (p) {
                if (p->ts.is_meta())
                    ts = ptr_cast<TypeValue>(p)->represented_ts;
                else {
                    std::cerr << "Initializer with nontype context!\n";
                    throw TYPE_ERROR;
                }
            }
            else if (context)
                ts = *context;
            else {
                std::cerr << "Initializer without type context: " << expr->token << "\n";
                throw TYPE_ERROR;
            }

            // TODO: strip some prefixes
            if (ts[0] == code_type)
                ts = ts.unprefix(code_type);
            
            // We must have checked this.
            if (!ts.has_meta(value_metatype) && !ts.has_meta(metatype_hypertype)) {
                std::cerr << "Initializer with nonvalue type context: " << ts << " at " << expr->token << "!\n";
                throw TYPE_ERROR;
            }
            
            if (name.size() == 0)
                name = "{}";
            
            value = ts.lookup_initializer(name);
            
            if (!value) {
                std::cerr << "No initializer " << ts << " `" << name << "!\n";
                throw TYPE_ERROR;
            }
            
            value->set_token(expr->token);
            bool ok = value->check(expr->args, expr->kwargs, scope);
        
            if (!ok) {
                std::cerr << "Initializer argument problem for " << expr->token << "!\n";
                throw TYPE_ERROR;
            }
            
            std::cerr << "Using initializer " << ts << " `" << name << ".\n";
            //std::cerr << "... with type " << value->ts << "\n";
        }
    }
    else if (expr->type == Expr::PARTINITIALIZER) {
        std::string name = expr->text;
        Value *p = expr->pivot ? typize(expr->pivot.get(), scope) : NULL;

        if (!p) {
            std::cerr << "Partinitializer without pivot!\n";
            throw TYPE_ERROR;
        }

        value = p->ts.lookup_partinitializer(name, p);
            
        if (!value) {
            std::cerr << "No partinitializer " << p->ts << " `" << name << "!\n";
            throw TYPE_ERROR;
        }
            
        value->set_token(expr->token);
        bool ok = value->check(expr->args, expr->kwargs, scope);
        
        if (!ok) {
            std::cerr << "Partinitializer argument problem for " << expr->token << "!\n";
            throw TYPE_ERROR;
        }
            
        std::cerr << "Using partinitializer " << p->ts << " `" << name << ".\n";
        //std::cerr << "... with type " << value->ts << "\n";
    }
    else if (expr->type == Expr::MATCHER) {
        std::string name = expr->text;
        Value *p = expr->pivot ? typize(expr->pivot.get(), scope) : NULL;

        if (!p)
            p = lookup_switch(scope, expr->token);

        value = p->ts.lookup_matcher(name, p);
    
        if (!value) {
            std::cerr << "No matcher " << p->ts << " ~" << name << "!\n";
            throw TYPE_ERROR;
        }

        value->set_token(expr->token);
        bool ok = value->check(expr->args, expr->kwargs, scope);
    
        if (!ok) {
            std::cerr << "Matcher argument problem for " << expr->token << "!\n";
            throw TYPE_ERROR;
        }
        
        std::cerr << "Using matcher " << p->ts << " `" << name << ".\n";
    }
    else if (expr->type == Expr::UNSIGNED_NUMBER || expr->type == Expr::NEGATIVE_NUMBER) {
        bool is_negative = (expr->type == Expr::NEGATIVE_NUMBER);
        std::string text = expr->text;
        bool is_float = false;
        
        for (unsigned i = 0; i < text.size(); i++) {
            char c = text[i];
            
            if (c == '.' || c == '+' || c == '-') {
                is_float = true;
                break;
            }
        }
        
        if (is_float) {
            double x = parse_float(text);
            value = make<FloatValue>(FLOAT_TS, is_negative ? -x : x);
        }
        else {
            IntegerType *t = ptr_cast<IntegerType>(
                desuffix(text, "s32") ? integer32_type :
                desuffix(text, "s16") ? integer16_type :
                desuffix(text, "s8") ? integer8_type :
                desuffix(text, "u32") ? unsigned_integer32_type :
                desuffix(text, "u16") ? unsigned_integer16_type :
                desuffix(text, "u8") ? unsigned_integer8_type :
                desuffix(text, "u") ? unsigned_integer_type :
                integer_type
            );
        
            if (context && *context != ANY_TS && t == integer_type) {
                Type *x = (*context)[0];
                x = (x == code_type || x == ovalue_type ? (*context)[1] : x);
                t = ptr_cast<IntegerType>(x);
        
                if (!t) {
                    std::cerr << "Literal number in a noninteger " << *context << " context: " << expr->token << "\n";
                    throw TYPE_ERROR;
                }
            }

        
            // parse the part without sign into a 64-bit unsigned
            unsigned long x = parse_unsigned_integer(text);

            if (t->is_unsigned) {
                if (
                    is_negative ||
                    (t->size == 1 && x > 255) ||
                    (t->size == 2 && x > 65535) ||
                    (t->size == 4 && x > 4294967295)
                ) {
                    std::cerr << "Unsigned integer literal out of range: " << expr->token << "\n";
                    throw TYPE_ERROR;
                }
            }
            else {
                if (
                    (t->size == 1 && x > (is_negative ? 128 : 127)) ||
                    (t->size == 2 && x > (is_negative ? 32768 : 32767)) ||
                    (t->size == 4 && x > (is_negative ? 2147483648 : 2147483647)) ||
                    (t->size == 8 && x > (is_negative ? 9223372036854775808U : 9223372036854775807U))
                ) {
                    std::cerr << "Signed integer literal out of range: " << expr->token << "\n";
                    throw TYPE_ERROR;
                }
            }

            value = make<BasicValue>(TypeSpec { t }, is_negative ? -x : x);
        }
        
        value->set_token(expr->token);
    }
    else if (expr->type == Expr::STRING) {
        value = make<StringLiteralValue>(expr->text);
        value->set_token(expr->token);
    }
    else {
        std::cerr << "Can't typize this now: " << expr->token << "!\n";
        throw INTERNAL_ERROR;
    }
    
    return value;
}

