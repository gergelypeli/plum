#include "../plum.h"


// Matching

int any_index(Type *t) {
    return (
        t == any_type  || t == anyid_type  || t == anytuple_type  ? 1 :
        t == any2_type || t == anyid2_type || t == anytuple2_type ? 2 :
        t == any3_type || t == anyid3_type || t == anytuple3_type ? 3 :
        0
    );
}

int same_index(Type *t) {
    return (
        t == same_type  || t == sameid_type  || t == sametuple_type  ? 1 :
        t == same2_type || t == sameid2_type || t == sametuple2_type ? 2 :
        t == same3_type || t == sameid3_type || t == sametuple3_type ? 3 :
        0
    );
}

TypeSpec typesubst(TypeSpec &tt, TypeMatch &match) {
    TypeSpec ts;
    
    for (TypeSpecIter tti = tt.begin(); tti != tt.end(); tti++) {
        int mi = same_index(*tti);
        
        if (mi > 0) {
            if (match[mi] == NO_TS) {
                std::cerr << "No matched Any type while substituting Same!\n";
                std::cerr << tt << " - " << match << "\n";
                throw INTERNAL_ERROR;
            }
            
            if (!match[mi].has_meta((*tti)->meta_type)) {
                std::cerr << "Wrong matched Any type while substituting Same!\n";
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


Allocation allocsubst(Allocation a, TypeMatch &match) {
    // Concretization always rounds size up, because unknown types are 8-bytes aligned.
    // Only evaluate measure for parameters that actually matter, because some parameters
    // may be unmeasurable, but if they're not needed, then it's OK.
    
    Allocation result = { a.bytes, 0, 0, 0 };
    
    if (a.count1)
        result = result + match[1].measure().stack_size() * a.count1;
        
    if (a.count2)
        result = result + match[2].measure().stack_size() * a.count2;

    if (a.count3)
        result = result + match[3].measure().stack_size() * a.count3;
    
    return result;
}


// *******
extern bool matchlog;


bool match_type_parameter(TypeSpecIter &s, TypeSpecIter &t, TypeMatch &match, int mi, MetaType *metatype) {
    if (!TypeSpec(s).has_meta(metatype)) {
        if (matchlog) std::cerr << "No match, type parameter not a " << metatype->name << "!\n";
        return false;
    }
    
    if (match[mi].size()) {
        std::cerr << "Duplicate Any usage!\n";
        throw INTERNAL_ERROR;
    }
    
    unsigned c = 1;

    while (c--) {
        c += (*s)->get_parameter_count();
        match[mi].push_back(*s);
        match[0].push_back(*s);
        s++;
    }

    t++;
    return true;
}


bool match_type_parameters(TypeSpecIter s, TypeSpecIter t, TypeMatch &match) {
    unsigned counter = 1;
    
    while (counter--) {
        if (*s == *t) {
            match[0].push_back(*t);
            counter += (*s)->get_parameter_count();
            s++;
            t++;
        }
        else if (*t == any_type) {
            if (!match_type_parameter(s, t, match, 1, value_metatype))
                return false;
        }
        else if (*t == any2_type) {
            if (!match_type_parameter(s, t, match, 2, value_metatype))
                return false;
        }
        else if (*t == any3_type) {
            if (!match_type_parameter(s, t, match, 3, value_metatype))
                return false;
        }
        else if (*t == anyid_type) {
            if (!match_type_parameter(s, t, match, 1, identity_metatype))
                return false;
        }
        else if (*t == anyid2_type) {
            if (!match_type_parameter(s, t, match, 2, identity_metatype))
                return false;
        }
        else if (*t == anyid3_type) {
            if (!match_type_parameter(s, t, match, 3, identity_metatype))
                return false;
        }
        else if (*t == anytuple_type) {
            if (!match_type_parameter(s, t, match, 1, tuple_metatype))
                return false;
        }
        else if (*t == anytuple2_type) {
            if (!match_type_parameter(s, t, match, 2, tuple_metatype))
                return false;
        }
        else if (*t == anytuple3_type) {
            if (!match_type_parameter(s, t, match, 3, tuple_metatype))
                return false;
        }
        else {
            if (matchlog) std::cerr << "No match, type parameters differ!\n";
            return false;
        }
    }
    
    return true;
}


bool match_regular_type(TypeSpecIter s, TypeSpecIter t, TypeMatch &match, Value *&value) {
    TypeSpec ifts;
    
    // Return a role of v with an unprefixed type of s, with the front type
    // being is equal to t, but with arbitrary type parameters, potentially
    // derived from the type parameters of s. Or NULL, if it can't be done.
    // May call itself recursively.
    std::cerr << "Trying autoconv from " << s << " to " << t << ".\n";
    Value *role = TypeSpec(s).autoconv(*t, value, ifts);
        
    if (role) {
        value = role;
        TypeSpec ss = ifts;
        s = ss.begin();
        return match_type_parameters(s, t, match);
    }
    
    if (matchlog) std::cerr << "No match, unconvertible types!\n";
    return false;
}


bool match_special_type(TypeSpecIter s, TypeSpecIter t, TypeMatch &match, Value *&value, bool require_lvalue) {
    if (*s == ref_type || *s == ptr_type || *t == ref_type || *t == ptr_type) {
        if (*s == *t) {
            match[0].push_back(*t);
            s++;
            t++;
        }
        else if (*s == ref_type && *t == ptr_type && !require_lvalue) {
            match[0].push_back(*t);
            s++;
            t++;
            
            if (value->ts[0] == lvalue_type)
                value = new RvalueCastValue(value);
                
            value = new CastValue(value, value->ts.reprefix(ref_type, ptr_type));
        }
        else if (*s == ptr_type && *t == ref_type) {
            if (matchlog) std::cerr << "No match, ptr for ref!\n";
            return false;
        }
    }
    
    if (*s == *t || any_index(*t)) {
        bool ok = match_type_parameters(s, t, match);
        
        return ok;
    }
    
    if (require_lvalue) {
        // For conversion to lvalue, only an exact match was acceptable
        if (matchlog) std::cerr << "No match, lvalue types differ!\n";
        return false;
    }

    // FIXME: we duplicate this check with Tuple0 Code as well, below
    if (*t == void_type) {
        // These can't interfere with references
        if (*s == void_type)
            return true;

        if (*s == uninitialized_type || *s == stringtemplate_type) {
            if (matchlog) std::cerr << "No match, " << s << " for Void!\n";
            std::cerr << s << " value dropped on the floor!\n";
            return false;
        }

        value = make<VoidConversionValue>(value);
        return true;
    }

    bool ok = match_regular_type(s, t, match, value);
    
    return ok;
}


bool match_anytuple_type(TypeSpecIter s, TypeSpecIter t, TypeMatch &match, Value *&value, bool require_lvalue) {
    // Allow any_type match references
    
    if (any_index(*t)) {
        if (*s == void_type) {
            if (matchlog) std::cerr << "No match, Void for Any!\n";
            return false;
        }
        else if ((*s)->meta_type == tuple_metatype) {
            if ((*s)->get_parameter_count() == 1) {
                if (matchlog) std::cerr << "Matched single tuple for Any.\n";
                s++;
            }
            else {
                if (matchlog) std::cerr << "No match, Tuple for Any!\n";
                return false;
            }
        }
        else if (*s == whatever_type) {
            if (matchlog) std::cerr << "No match, Whatever for Any!\n";
            return false;
        }
        
        return match_type_parameters(s, t, match);
    }
    
    if (*s == whatever_type) {
        value->ts = TypeSpec(t);
        return true;
    }
    
    if ((*t)->meta_type == tuple_metatype) {
        // Expecting a multi, only the same multi can suffice.
        
        if ((*s)->meta_type != tuple_metatype) {
            if (matchlog) std::cerr << "No match, scalar for Multi!\n";
            return false;
        }
        
        if ((*s)->get_parameter_count() < (*t)->get_parameter_count()) {
            if (matchlog) std::cerr << "No match, short tuple for long!\n";
            return false;
        }
        
        // Leave the rest of the test to the Value, because we can't process multiple
        // type parameters.
        
        return true;
    }
    else {
        // Expecting a scalar, only a plain multi can be scalarized, the others not so much
        
        if ((*s)->meta_type == tuple_metatype) {
            if (*t == void_type) {
                // This is not allowed because a Multi may contain uninitialized values
                if (matchlog) std::cerr << "No match, Multi for Void!\n";
                std::cerr << "Multi value dropped on the floor!\n";
                return false;
            }
        
            // A Multi is being converted to something non-Multi.
            // Since a Multi can never be in a pivot position, this value must be a plain
            // argument, so if converting it fails, then it will be a fatal error. So
            // it's OK to wrap it in a scalarization, because this is our only conversion chance.
            std::vector<TypeSpec> tss;
            TypeSpec ts(s);
            ts.unpack_tuple(tss);
        
            TypeSpec ss = tss[0];
            s = ss.begin();
            if (matchlog) std::cerr << "Unpacking Multi to " << ss << ".\n";
            value = make<ScalarConversionValue>(value);

            return match_special_type(s, t, match, value, require_lvalue);
        }

        // Match scalar to scalar
        return match_special_type(s, t, match, value, require_lvalue);
    }
}


bool match_attribute_type(TypeSpecIter s, TypeSpecIter t, TypeMatch &match, Value *&value) {
    // Checking attribute templates
    
    if (*t == lvalue_type) {
        if (!value) {
            if (matchlog) std::cerr << "No match, nothing for Lvalue!\n";
            return false;
        }
        else if (*s != lvalue_type) {
            if (matchlog) std::cerr << "No match, rvalue for Lvalue!\n";
            return false;
        }
        
        match[0].push_back(*t);
        s++;
        t++;

        if (*s == void_type || *t == void_type)
            throw INTERNAL_ERROR;
        
        return match_anytuple_type(s, t, match, value, true);
    }
    else if (*t == code_type) {  // evalue
        match[0].push_back(*t);
        t++;

        int pc = (*t)->get_parameter_count();  // a Tuple must follow the Code
        
        if (pc == 0) {
            // FIXME: we duplicate this check with Void, above
            if (!value) {
                if (matchlog) std::cerr << "Matched nothing for Void Code.\n";
                match[0].push_back(tuple0_type);
                return true;
            }
            
            if (*s == void_type)  // TODO: remove void_type?
                return true;

            if (*s == uninitialized_type || *s == stringtemplate_type) {
                if (matchlog) std::cerr << "No match, " << s << " for Void!\n";
                std::cerr << s << " value dropped on the floor!\n";
                return false;
            }

            value = make<VoidConversionValue>(value);
            return true;
        }
        else if (pc == 1) {
            t++;
            
            if (!value) {
                if (matchlog) std::cerr << "No match, nothing for nonvoid Code!\n";
                return false;
            }

            // value is non-NULL from here

            // Drop unnecessary attribute
            if (*s == lvalue_type || *s == ovalue_type) {
                s++;
            }

            match[0].push_back(tuple1_type);

            if (!match_anytuple_type(s, t, match, value, false))
                return false;

            return true;
        }
        else {
            // This is a bit lame
            if (!match_anytuple_type(s, t, match, value, false))
                return false;

            return true;
        }
    }
    else if (*t == ovalue_type) {
        if (!value) {
            if (matchlog) std::cerr << "Matched nothing for Ovalue.\n";
            return true;
        }
        
        t++;
        
        if (*s == lvalue_type || *s == ovalue_type) {
            s++;
        }

        return match_anytuple_type(s, t, match, value, false);
    }
    else if (*t == uninitialized_type) {
        // This is a special case, Uninitialized only occurs with Any.
        // Maybe this should be checked in an identifier directly.
        if (!value) {
            if (matchlog) std::cerr << "No match, nothing for Uninitialized!\n";
            return false;
        }
        else if (*s != uninitialized_type) {
            if (matchlog) std::cerr << "No match, initialized for Uninitialized!\n";
            return false;
        }
        
        match[0].push_back(*t);
        s++;
        t++;

        if (*t != any_type)
            throw INTERNAL_ERROR;
        
        return match_type_parameters(s, t, match);
    }
    else {
        if (!value) {
            if (matchlog) std::cerr << "No match, nothing for something!\n";
            return false;
        }
        
        if (*s == lvalue_type || *s == ovalue_type || *s == code_type) {
            s++;
        }

        return match_anytuple_type(s, t, match, value, false);
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

bool typematch(TypeSpec tt, Value *&value, TypeMatch &match) {
    if (tt == NO_TS)
        throw INTERNAL_ERROR;  // Mustn't be called with NO_TS

    if (matchlog) std::cerr << "Matching " << value->ts << " to pattern " << tt << "...\n";

    match[0] = NO_TS;
    match[1] = NO_TS;
    match[2] = NO_TS;
    match[3] = NO_TS;
    
    TypeSpec ss = value->ts;
    TypeSpecIter s(ss.begin());
    TypeSpecIter t(tt.begin());
    
    if (!match_attribute_type(s, t, match, value)) {
        //std::cerr << "Typematch failed with " << value->ts << " to " << tt << "!\n";
        return false;
    }
        
    if (matchlog) std::cerr << "Matched as " << match[0];
    if (matchlog) if (match.size() > 1) { std::cerr << ", parameters"; for (unsigned i = 1; i < match.size(); i++) std::cerr << " " << match[i]; }
    if (matchlog) std::cerr << ".\n";

    return true;
}


bool converts(TypeSpec sts, TypeSpec tts) {
    Value *dummy_value = new Value(sts);
    TypeMatch dummy_match;
        
    bool ok = typematch(tts, dummy_value, dummy_match);
    
    delete dummy_value;
    
    return ok;
}


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
            std::cerr << "Expression must transfer control, not return " << v->ts << " at " << e->token << "!\n";
        else
            std::cerr << "Argument type mismatch, " << v->ts << " is not a " << *context << " at " << e->token << "!\n";

        return false;
    }

    if (ctx0 != lvalue_type && v && v->ts[0] == lvalue_type && !(ctx0 && ctx0->meta_type == interface_metatype))
        v = new RvalueCastValue(v);

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
