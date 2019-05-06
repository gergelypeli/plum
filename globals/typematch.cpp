
// Matching

TypeSpec typesubst(TypeSpec &tt, TypeMatch &match) {
    TypeSpec ts;
    
    for (TypeSpecIter tti = tt.begin(); tti != tt.end(); tti++) {
        if (*tti == same_type || *tti == same2_type || *tti == same3_type || *tti == sameid_type || *tti == sameid2_type || *tti == sameid3_type) {
            int mi = (*tti == same_type || *tti == sameid_type ? 1 : *tti == same2_type || *tti == sameid2_type ? 2 : *tti == same3_type || *tti == sameid3_type ? 3 : throw INTERNAL_ERROR);
            
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

bool is_any(Type *t) {
    return t == any_type || t == any2_type || t == any3_type || t == anyid_type || t == anyid2_type || t == anyid3_type;
}


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
            value = new CastValue(value, value->ts.rvalue().reprefix(ref_type, ptr_type));
        }
        else if (*s == ptr_type && *t == ref_type) {
            if (matchlog) std::cerr << "No match, ptr for ref!\n";
            return false;
        }
    }
    
    if (*s == *t || is_any(*t)) {
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


bool match_anymulti_type(TypeSpecIter s, TypeSpecIter t, TypeMatch &match, Value *&value, bool require_lvalue) {
    // Allow any_type match references
    
    if (is_any(*t)) {
        if (*s == void_type) {
            if (matchlog) std::cerr << "No match, Void for Any!\n";
            return false;
        }
        else if (*s == multi_type || *s == multilvalue_type || *s == multitype_type) {
            if (matchlog) std::cerr << "No match, Multi for Any!\n";
            return false;
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
    
    if (*t == multi_type || *t == multilvalue_type || *t == multitype_type) {
        // Expecting a multi, only the same multi can suffice
        
        if (*s != *t) {
            if (matchlog) std::cerr << "No match, scalar for Multi!\n";
            return false;
        }
        
        // Match Multi to Multi
        return match_special_type(s, t, match, value, require_lvalue);
    }
    else if (*t == multicode_type) {
        // Expecting a plain multi, just in a Code context
        
        if (*s == multi_type) {
            match[0].push_back(*t);
            return true;
        }
        else {
            if (matchlog) std::cerr << "No match, non-Multivalue for Multicode!\n";
            return false;
        }
    }
    else {
        // Expecting a scalar, only a plain multi can be scalarized, the others not so much
        
        if (*s == multilvalue_type || *s == multitype_type) {
            if (*t == void_type) {
                // This is not allowed because a Multi may contain uninitialized values
                if (matchlog) std::cerr << "No match, Multi for Void!\n";
                std::cerr << "Multi value dropped on the floor!\n";
                return false;
            }
            else {
                if (matchlog) std::cerr << "No match, non-Multivalue for scalar!\n";
                return false;
            }
        }
        else if (*s == multi_type) {
            // A Multi is being converted to something non-Multi.
            // Since a Multi can never be in a pivot position, this value must be a plain
            // argument, so if converting it fails, then it will be a fatal error. So
            // it's OK to wrap it in a scalarization, because this is our only conversion chance.
            std::vector<TypeSpec> tss;
        
            if (!unpack_value(value, tss))
                throw INTERNAL_ERROR;
        
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
        
        return match_anymulti_type(s, t, match, value, true);
    }
    else if (*t == code_type) {  // evalue
        match[0].push_back(*t);
        t++;

        int pc = (*t)->get_parameter_count();  // a Tuple must follow the Code
        t++;
        
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

            if (!match_anymulti_type(s, t, match, value, false))
                return false;

            return true;
        }
        else {
            if (matchlog) std::cerr << "Sorry, nothing converts to a nonscalar tuple yet!\n";
            return false;
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

        return match_anymulti_type(s, t, match, value, false);
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

bool typematch(TypeSpec tt, Value *&value, TypeMatch &match) {
    if (tt == NO_TS)
        throw INTERNAL_ERROR;  // Mustn't be called with NO_TS

    if (matchlog) std::cerr << "Matching " << get_typespec(value) << " to pattern " << tt << "...\n";

    match[0] = NO_TS;
    match[1] = NO_TS;
    match[2] = NO_TS;
    match[3] = NO_TS;
    
    TypeSpec ss = get_typespec(value);
    TypeSpecIter s(ss.begin());
    TypeSpecIter t(tt.begin());
    
    if (!match_attribute_type(s, t, match, value)) {
        //std::cerr << "Typematch failed with " << get_typespec(value) << " to " << tt << "!\n";
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
