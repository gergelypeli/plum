
// Value operations

TypeSpec get_typespec(Value *value) {
    return value ? value->ts : NO_TS;
}


void set_typespec(Value *value, TypeSpec ts) {
    value->ts = ts;
}


void role_value_be_static(RoleValue *rv) {
    rv->be_static();
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


bool unpack_value(Value *v, std::vector<TypeSpec> &tss) {
    return v->unpack(tss);
}


Value *value_lookup_inner(Value *value, std::string name, Scope *scope) {
    return value->lookup_inner(name, scope);
}


bool value_check(Value *v, Args &a, Kwargs &k, Scope *s) {
    return v->check(a, k, s);
}


const char *typeidname(Value *v) {
    return typeid(*v).name();
}


// Declaration operations

Declaration *make_record_compare() {
    return new TemplateOperation<RecordOperationValue>("compare", ANY_TS, COMPARE);
}


void associable_override_virtual_entry(Associable *a, int vi, VirtualEntry *ve) {
    a->override_virtual_entry(vi, ve);
}


std::string function_get_name(Function *f) {
    return f->name;
}


bool function_is_abstract(Function *f) {
    return f->type == ABSTRACT_FUNCTION;
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

CodeScope *check_retros(unsigned i, Scope *scope, const std::vector<ArgInfo> &arg_infos) {
    // Grab all preceding Dvalue bar declarations, and put them in this scope.
    // Retro variables must only be accessible from the following Code argument's
    // scope, because their initialization is only guaranteed while that Code
    // is being evaluated.
    std::vector<Variable *> retros;
    
    for (unsigned j = i - 1; j < i; j--) {
        DeclarationValue *dv = ptr_cast<DeclarationValue>(arg_infos[j].target->get());
        
        if (dv) {
            if (dv->ts[0] != dvalue_type)
                break;
                
            Declaration *decl = declaration_get_decl(dv);
            Variable *var = ptr_cast<Variable>(decl);
            if (!var)
                throw INTERNAL_ERROR;
                
            var->outer_scope->remove(var);
            retros.push_back(var);
        }
    }

    CodeScope *code_scope = new CodeScope;
    scope->add(code_scope);
    code_scope->enter();

    for (auto var : retros) {
        std::cerr << "Moving retro variable " << var->name << " to code scope.\n";
        code_scope->add(var);
    }
    
    return code_scope;
}


bool check_argument(unsigned i, Expr *e, const std::vector<ArgInfo> &arg_infos) {
    if (i >= arg_infos.size()) {
        std::cerr << "Too many arguments!\n";
        return false;
    }

    std::unique_ptr<Value> *target = arg_infos[i].target;
    TypeSpec *context = arg_infos[i].context;
    Scope *scope = arg_infos[i].scope;

    if (*target) {
        std::cerr << "Argument " << i << " already supplied!\n";
        return false;
    }

    CodeScope *code_scope = NULL;
    
    if (context && (*context)[0] == code_type) {
        code_scope = check_retros(i, scope, arg_infos);
    }

    TypeSpec *constructive_context = context;
    
    if (context && (*context).rvalue().has_meta(interface_metatype))
        constructive_context = NULL;

    Value *v = typize(e, code_scope ? code_scope : scope, constructive_context);
    
    // Hack for omitting strict checking in :is controls
    if (context && (*context)[0] == equalitymatcher_type)
        context = NULL;

    TypeMatch match;
    
    if (context && !typematch(*context, v, match)) {
        // Make an effort to print meaningful error messages
        if (*context == WHATEVER_CODE_TS)
            std::cerr << "Expression must transfer control, not return " << get_typespec(v) << " at " << e->token << "!\n";
        else
            std::cerr << "Argument type mismatch, " << get_typespec(v) << " is not a " << *context << " at " << e->token << "!\n";

        return false;
    }

    if (code_scope) {
        v = make<CodeScopeValue>(v, code_scope);
        code_scope->leave();
    }

    target->reset(v);
    return true;
}


bool check_arguments(Args &args, Kwargs &kwargs, const ArgInfos &arg_infos) {
    //std::cerr << "Checking arguments: ";
    
    //for (auto &ai : arg_infos)
    //    std::cerr << ai.name << ":" << (ai.context ? *ai.context : NO_TS) << " ";
        
    //std::cerr << "\n";

    unsigned n = arg_infos.size();
    Expr *exprs[n];

    for (unsigned i = 0; i < n; i++)
        exprs[i] = NULL;

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
        if (exprs[i]) {
            if (!check_argument(i, exprs[i], arg_infos))
                return false;
        }
        else {
            if (arg_infos[i].context) {
                TypeSpec &ts = *arg_infos[i].context;
            
                if (ts[0] != ovalue_type && !(ts[0] == code_type && ts[1] == void_type)) {
                    std::cerr << "Missing mandatory argument " << arg_infos[i].name << "!\n";
                    return false;
                }
            }
            else {
                std::cerr << "XXX is this used somewhere?\n";
            }
        }
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
        
        *expr_infos[i].target = args[0].get();
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
        
        *expr_infos[i].target = kv.second.get();
    }

    return true;
}
