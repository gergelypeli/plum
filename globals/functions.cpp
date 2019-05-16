
// Value operations

TypeSpec get_typespec(Value *value) {
    return value ? value->ts : NO_TS;
}


void set_typespec(Value *value, TypeSpec ts) {
    value->ts = ts;
}


void function_call_be_static(Value *v, Associable *sr) {
    ptr_cast<FunctionCallValue>(v)->be_static(sr);
}


Declaration *declaration_get_decl(DeclarationValue *dv) {
    return dv->get_decl();
}


PartialInfo *partial_variable_get_info(Value *v) {
    PartialVariableValue *pvv = dynamic_cast<PartialVariableValue *>(v);
    if (pvv)
        return pvv->partial_info;
        
    throw INTERNAL_ERROR;
}


bool is_initializer_function_call(Value *value) {
    FunctionCallValue *fcv = dynamic_cast<FunctionCallValue *>(value);
                
    return fcv && fcv->function->type == INITIALIZER_FUNCTION;
}


TypeSpec type_value_represented_ts(Value *v) {
    return ptr_cast<TypeValue>(v)->represented_ts;
}


TSs type_tuple_value_represented_tss(Value *v) {
    TypeTupleValue *ttv = ptr_cast<TypeTupleValue>(v);
    
    return (ttv ? ttv->represented_tss : TSs());
}


Value *value_lookup_inner(Value *value, std::string name, Scope *scope) {
    return value->lookup_inner(name, scope);
}


bool value_check(Value *v, Args &a, Kwargs &k, Scope *s) {
    return v->check(a, k, s);
}


void value_need_lvalue(Value *v) {
    v->need_lvalue();
}


const char *typeidname(Value *v) {
    return typeid(*v).name();
}


// Declaration operations

Declaration *make_record_compare() {
    return new TemplateOperation<RecordOperationValue>("compare", COMPARE);
}


void associable_override_virtual_entry(Associable *a, int vi, VirtualEntry *ve) {
    a->override_virtual_entry(vi, ve);
}


bool associable_is_or_is_in_requiring(Associable *a) {
    return a && (a->is_requiring() || a->is_in_requiring());
}


std::string treenumeration_get_name(TreenumerationType *t) {
    return t->name;
}


std::string function_get_name(Function *f) {
    return f->name;
}


Label function_get_label(Function *f, X64 *x64) {
    return f->get_label(x64);
}


std::string print_exception_type(TreenumerationType *t) {
    return t ? t->name : "-";
}


TreenumerationType *make_treenum(const char *name, const char *kw1) {
    TreenumerationType *t = new TreenumerationType(name, { "", kw1 }, { 0, 0 });
    t->make_inner_scope()->leave();
    return t;
}


TreenumerationType *make_treenum(const char *name, TreenumInput *x) {
    std::vector<std::string> keywords = { "" };
    std::vector<unsigned> parents = { 0 };

    for (unsigned i = 0; x[i].kw; i++) {
        keywords.push_back(x[i].kw);
        parents.push_back(x[i].p);
    }

    TreenumerationType *t = new TreenumerationType(name, keywords, parents);
    t->make_inner_scope()->leave();
    return t;
}




// Checking

void check_retros(unsigned i, Scope *scope, const std::vector<ArgInfo> &arg_infos, CodeScope *code_scope) {
    // Grab all preceding Dvalue var declarations, and put them in this scope.
    // Retro variables must only be accessible from the following Code argument's
    // scope, because their initialization is only guaranteed while that Code
    // is being evaluated.
    std::vector<RetroArgumentScope *> retros;
    
    for (unsigned j = i - 1; j < i; j--) {
        RetroArgumentValue *rav = ptr_cast<RetroArgumentValue>(arg_infos[j].target->get());
        
        if (rav) {
            RetroArgumentScope *ras = rav->retro_argument_scope;
            ras->outer_scope->remove(ras);
            retros.push_back(ras);
            std::cerr << "Moving retro argument " << arg_infos[j].name << " to code scope.\n";
        }
    }

    scope->add(code_scope);
    code_scope->enter();

    for (auto ras : retros) {
        code_scope->add(ras);
    }
}


bool check_argument(unsigned i, Expr *e, const std::vector<ArgInfo> &arg_infos, bool is_function_call) {
    if (i >= arg_infos.size()) {
        std::cerr << "Too many arguments!\n";
        return false;
    }

    if (!e && arg_infos[i].context) {
        // Some arguments may be omitted
        TypeSpec cts = *arg_infos[i].context;
        
        if (cts[0] == dvalue_type)
            ;  // continue here
        else if (cts[0] == ovalue_type || cts == TUPLE0_CODE_TS || cts[0] == unit_type)
            return true;  // leave target empty
        else {
            std::cerr << "Missing mandatory argument " << arg_infos[i].name << "!\n";
            return false;
        }
    }

    std::unique_ptr<Value> *target = arg_infos[i].target;
    TypeSpec *context = arg_infos[i].context;
    Scope *scope = arg_infos[i].scope;

    if (*target) {
        std::cerr << "Argument " << i << " already supplied!\n";
        return false;
    }

    // Some context types request a certain scope around the value
    Type *ctx0 = (context ? (*context)[0] : NULL);
    
    CodeScope *code_scope = NULL;
    RetroScope *retro_scope = NULL;
    RetroArgumentScope *retro_argument_scope = NULL;
    Scope *innermost_scope = scope;
    
    if (ctx0 == code_type && !is_function_call) {
        code_scope = new CodeScope;
        check_retros(i, scope, arg_infos, code_scope);  // FIXME: move to RetroScopeValue?
        innermost_scope = code_scope;
    }
    else if (ctx0 == code_type && is_function_call) {
        retro_scope = new RetroScope;
        check_retros(i, scope, arg_infos, retro_scope);  // FIXME: move to RetroScopeValue?
        innermost_scope = retro_scope;
    }
    else if (ctx0 == dvalue_type) {
        retro_argument_scope = new RetroArgumentScope((*context).unprefix(dvalue_type));
        scope->add(retro_argument_scope);
        retro_argument_scope->enter();
        innermost_scope = retro_argument_scope;
    }

    // The argument type being an Interface can't be a context for initializers
    TypeSpec *constructive_context = context;
    
    if (context && (*context).rvalue().has_meta(interface_metatype))
        constructive_context = NULL;

    // NULL expr is now allowed for omitted value in Dvalue context
    Value *v = (e ? typize(e, innermost_scope, constructive_context) : NULL);
    
    TypeMatch match;
    
    if (context && (*context)[0] != dvalue_type && !typematch(*context, v, match)) {
        // Make an effort to print meaningful error messages
        if (*context == WHATEVER_TUPLE1_CODE_TS)
            std::cerr << "Expression must transfer control, not return " << get_typespec(v) << " at " << e->token << "!\n";
        else
            std::cerr << "Argument type mismatch, " << get_typespec(v) << " is not a " << *context << " at " << e->token << "!\n";

        return false;
    }

    if (ctx0 == lvalue_type)
        v->need_lvalue();

    if (code_scope) {
        v = make<CodeScopeValue>(v, code_scope, v->ts);
        code_scope->leave();
    }
    else if (retro_scope) {
        v = make<RetroScopeValue>(v, retro_scope, match[0]);
        retro_scope->leave();
    }
    else if (retro_argument_scope) {
        v = make<RetroArgumentValue>(v, retro_argument_scope);
        
        if (!ptr_cast<RetroArgumentValue>(v)->check())
            return false;
            
        retro_argument_scope->leave();
    }

    target->reset(v);
    return true;
}


bool check_arguments(Args &args, Kwargs &kwargs, const ArgInfos &arg_infos, bool is_function_call) {
    //std::cerr << "Checking arguments: ";
    
    //for (auto &ai : arg_infos)
    //    std::cerr << ai.name << ":" << (ai.context ? *ai.context : NO_TS) << " ";
        
    //std::cerr << "\n";

    unsigned n = arg_infos.size();
    Expr *exprs[n];

    for (unsigned i = 0; i < n; i++)
        exprs[i] = NULL;

    if (args.size() > n) {
        std::cerr << "Too many arguments!\n";
        return false;
    }

    for (unsigned i = 0; i < args.size(); i++) {
        exprs[i] = args[i].get();
        
        //if (!check_argument(i, e, arg_infos))
        //    return false;
    }
            
    for (auto &kv : kwargs) {
        unsigned i = (unsigned)-1;
        
        for (unsigned j = 0; j < n; j++) {
            if (arg_infos[j].name == kv.first) {
                i = j;
                break;
            }
        }
            
        if (i == (unsigned)-1) {
            std::cerr << "No argument named " << kv.first << "!\n";
            return false;
        }
        
        if (exprs[i]) {
            std::cerr << "Argument " << i << " duplicated as " << kv.first << "!\n";
            return false;
        }
        
        exprs[i] = kv.second.get();
        //std::cerr << "Checking keyword argument " << kv.first << ".\n";
        
        //if (!check_argument(i, e, arg_infos))
        //    return false;
    }

    for (unsigned i = 0; i < n; i++) {
        if (!check_argument(i, exprs[i], arg_infos, is_function_call))
            return false;
    }
    
    return true;
}


bool check_exprs(Args &args, Kwargs &kwargs, const ExprInfos &expr_infos) {
    if (args.size() > 1)
        throw INTERNAL_ERROR;
    else if (args.size() == 1) {
        unsigned i = (unsigned)-1;
        
        for (unsigned j = 0; j < expr_infos.size(); j++) {
            if (expr_infos[j].name == "") {
                i = j;
                break;
            }
        }

        if (i == (unsigned)-1) {
            std::cerr << "No positional argument allowed!\n";
            return false;
        }
        
        *expr_infos[i].target = std::move(args[0]);
    }

    for (auto &kv : kwargs) {
        unsigned i = (unsigned)-1;
        
        for (unsigned j = 0; j < expr_infos.size(); j++) {
            if (expr_infos[j].name == kv.first) {
                i = j;
                break;
            }
        }
            
        if (i == (unsigned)-1) {
            std::cerr << "No keyword argument named " << kv.first << "!\n";
            return false;
        }
        
        *expr_infos[i].target = std::move(kv.second);
    }

    return true;
}
