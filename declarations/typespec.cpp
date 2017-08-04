
std::ostream &operator<<(std::ostream &os, const TypeSpec &ts) {
    os << "[";
    
    bool start = true;
    
    for (auto type : ts) {
        if (start)
            start = false;
        else
            os << ",";
            
        os << type->name;
    }
    
    os << "]";
    
    return os;
}


TypeSpec::TypeSpec() {
}


TypeSpec::TypeSpec(TypeSpecIter tsi) {
    unsigned counter = 1;
    
    while (counter--) {
        push_back(*tsi);
        counter += (*tsi)->parameter_count;
        tsi++;
    }
}


StorageWhere TypeSpec::where() {
    TypeSpecIter this_tsi(begin());
    
    return (*this_tsi)->where(this_tsi);
}


bool TypeSpec::pass_alias() {
    TypeSpecIter this_tsi(begin());
    
    return (*this_tsi)->pass_alias(this_tsi, false);  // assume not lvalue
}


void TypeSpec::push_alias(Storage s, X64 *x64) {
    TypeSpecIter this_tsi(begin());
    
    return (*this_tsi)->push_alias(this_tsi, s, x64);
}


void TypeSpec::pop_alias(Storage s, X64 *x64) {
    TypeSpecIter this_tsi(begin());
    
    return (*this_tsi)->pop_alias(this_tsi, s, x64);
}


Storage TypeSpec::boolval(Storage s, X64 *x64, bool probe) {
    TypeSpecIter this_tsi(begin());
    
    return (*this_tsi)->boolval(this_tsi, s, x64, probe);
}


TypeSpec TypeSpec::prefix(Type *t) {
    TypeSpec ts;
    ts.push_back(t);
        
    for (unsigned i = 0; i < size(); i++)
        ts.push_back(at(i));
        
    return ts;
}


TypeSpec TypeSpec::unprefix(Type *t) {
    if (at(0) != t) {
        std::cerr << "TypeSpec doesn't start with " << t->name << ": " << *this << "!\n";
        throw INTERNAL_ERROR;
    }

    if (t->parameter_count != 1) {
        std::cerr << "Can't unprefix Type with " << t->parameter_count << " parameters: " << *this << "!\n";
        throw INTERNAL_ERROR;
    }
        
    TypeSpec ts;

    for (unsigned i = 1; i < size(); i++)
        ts.push_back(at(i));
            
    return ts;
}


TypeSpec TypeSpec::rvalue() {
    return at(0) == lvalue_type ? unprefix(lvalue_type) : at(0) == ovalue_type ? unprefix(ovalue_type) : *this;
}


TypeSpec TypeSpec::lvalue() {
    return at(0) == lvalue_type ? *this : at(0) == ovalue_type ? unprefix(ovalue_type).prefix(lvalue_type) : prefix(lvalue_type);
}


TypeSpec TypeSpec::nonlvalue() {
    return at(0) == lvalue_type ? unprefix(lvalue_type) : *this;
}


TypeSpec TypeSpec::nonrvalue() {
    return at(0) != lvalue_type && at(0) != ovalue_type ? prefix(lvalue_type) : *this;
}


unsigned TypeSpec::measure() {
    TypeSpecIter tsi(begin());
    return (*tsi)->measure(tsi);
}


void TypeSpec::store(Storage s, Storage t, X64 *x64) {
    TypeSpecIter tsi(begin());
    return (*tsi)->store(tsi, s, t, x64);
}


void TypeSpec::create(Storage s, X64 *x64) {
    TypeSpecIter tsi(begin());
    return (*tsi)->create(tsi, s, x64);
}


void TypeSpec::destroy(Storage s, X64 *x64) {
    TypeSpecIter tsi(begin());
    return (*tsi)->destroy(tsi, s, x64);
}


Value *TypeSpec::initializer(std::string name) {
    TypeSpecIter tsi(begin());
    return (*tsi)->initializer(tsi, name);
}


// New-style type matching

Value *rolematch(Value *v, TypeSpecIter s, Type *t) {
    // Return a role of v with an unprefixed type of s, with the front type
    // being is equal to t, but with arbitrary type parameters, potentially
    // derived from the type parameters of s. Or NULL, if it can't be done.
    // May call itself recursively.
    return NULL;
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
#define MATCHLOG if (false)

    // This may overwrite an older match, but that's OK, we just need to
    // preserve the type parameters.
    if (match.size() > 0)
        match[0].clear();
    else
        match.push_back(TypeSpec());

    for (TypeSpecIter tti = tt.begin(); tti != tt.end(); tti++) {
        if (*tti == same_type) {
            tt.erase(tti);
            std::copy(match.at(1).begin(), match.at(1).end(), tti);
        }
    }

    //std::cerr << "Matching " << get_typespec(value) << " to pattern " << tt << "...\n";

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
    
    if (tt[0] == void_type) {
        MATCHLOG std::cerr << "No match, something for Void.\n";
        return false;
    }
    
    TypeSpec ss = get_typespec(value);
    
    // Checking Void value
    if (ss[0] == void_type) {
        if (tt[0] == code_type && tt[1] == void_type) {
            MATCHLOG std::cerr << "Matched Void for Void Code.\n";
            match[0] = tt;
            value = make_code_value(value);
            return true;
        }
        else {
            MATCHLOG std::cerr << "No match, Void for something.\n";
            return false;
        }
    }

    TypeSpecIter s(ss.begin());
    TypeSpecIter t(tt.begin());
    
    bool strict = false;

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
            value = make_code_value(value);
            return true;
        }
    }
    else if (*t == ovalue_type) {
        t++;
    }
    
    // Drop unnecessary attribute
    if (*s == lvalue_type || *s == ovalue_type) {
        s++;
    }

    // Checking references
    if (*t == any_type)
        ;
    else if (*s == reference_type && *t == reference_type) {
        match[0].push_back(*t);
        s++;
        t++;
    }
    else if (*s == reference_type || *t == reference_type) {
        MATCHLOG std::cerr << "No match, reference mismatch!\n";
        return false;
    }

    // Checking main type
    if (*t == any_type) {
        // Maybe call this nonvoid?
    }
    else if (*s == *t) {
        // Exact match is always OK
    }
    else if (strict) {
        // For conversion to lvalue, only an exact match was acceptable
        MATCHLOG std::cerr << "No match, lvalue types differ!\n";
        return false;
    }
    else if (*t == boolean_type) {
        match[0].push_back(*t);
        MATCHLOG std::cerr << "Matched as " << match[0] << ".\n";
        value = make_boolean_conversion_value(value);
        if (tt[0] == code_type)
            value = make_code_value(value);
        return true;
    }
    else {
        Value *role = rolematch(value, s, *t);
        
        if (!role) {
            MATCHLOG std::cerr << "No match, unconvertable types!\n";
            return false;
        }
        
        value = role;
        ss = get_typespec(value);
        s = ss.begin();
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
            MATCHLOG std::cerr << "No match, type parameters differ!\n";
            return false;
        }
    }
    
    MATCHLOG std::cerr << "Matched as " << match[0];
    MATCHLOG if (match.size() > 1) { std::cerr << ", parameters"; for (unsigned i = 1; i < match.size(); i++) std::cerr << " " << match[i]; }
    MATCHLOG std::cerr << ".\n";

    if (tt[0] == code_type)
        value = make_code_value(value);

    return true;
}
