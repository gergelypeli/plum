
class Type: public Declaration {
public:
    std::string name;
    unsigned parameter_count;
    
    Type(std::string n, unsigned pc) {
        name = n;
        parameter_count = pc;
    }
    
    virtual unsigned get_parameter_count() {
        return parameter_count;
    }
    
    virtual int get_length() {
        return 1;
    }
    
    virtual std::string get_label() {
        return "";
    }
    
    virtual Value *match(std::string name, Value *pivot) {
        if (name != this->name)
            return NULL;
        else if (parameter_count == 0) {
            if (pivot != NULL)
                return NULL;
                
            TypeSpec ts;
            ts.push_back(type_type);
            ts.push_back(this);
            
            return make_type_value(ts);
        }
        else if (parameter_count == 1 && pivot != NULL) {
            TypeSpec pts = get_typespec(pivot);
            
            if (pts[0] != type_type)
                return NULL;
                
            TypeSpec ts = pts.unprefix(type_type).prefix(this).prefix(type_type);
            // FIXME: do something with pivot!
            
            return make_type_value(ts);
        }
        else
            throw INTERNAL_ERROR;
    }
    
    virtual Value *convertible(TypeSpecIter this_tsi, TypeSpecIter that_tsi, Value *orig) {
        if (equalish(this_tsi, that_tsi))
            return orig;
        else if (*that_tsi == any_type)
            return orig;
        else if (*that_tsi == void_type)
            return make_converted_value(VOID_TS, orig);
        else if (*that_tsi == code_type) {
            that_tsi++;
            Value *c = convertible(this_tsi, that_tsi, orig);
            return make_code_value(c);
        }
        else
            return NULL;
    }

    virtual Storage convert(TypeSpecIter this_tsi, TypeSpecIter that_tsi, Storage s, X64 *x64) {
        if (*that_tsi == void_type) {
            store(this_tsi, s, Storage(), x64);
            return Storage();
        }
        else {
            std::cerr << "Unconvertable type: " << name << "!\n";
            throw INTERNAL_ERROR;
        }
    }
    
    virtual StorageWhere where(TypeSpecIter tsi) {
        std::cerr << "Nowhere type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }

    virtual Storage boolval(TypeSpecIter tsi, Storage, X64 *) {
        std::cerr << "Unboolable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }

    virtual unsigned measure(TypeSpecIter tsi) {
        std::cerr << "Unmeasurable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }

    virtual bool is_unsigned(TypeSpecIter tsi) {
        std::cerr << "Unsignable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }

    virtual void store(TypeSpecIter this_tsi, Storage s, Storage t, X64 *x64) {
        std::cerr << "Unstorable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }
    
    virtual void create(TypeSpecIter tsi, Storage s, X64 *x64) {
        std::cerr << "Uncreatable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }

    virtual void destroy(TypeSpecIter tsi, Storage s, X64 *x64) {
        std::cerr << "Undestroyable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }
};


class SpecialType: public Type {
public:
    SpecialType(std::string name, unsigned pc):Type(name, pc) {}
    
    virtual unsigned measure(TypeSpecIter ) {
        return 0;
    }

    virtual StorageWhere where(TypeSpecIter tsi) {
        return NOWHERE;
    }

    virtual void store(TypeSpecIter tsi, Storage s, Storage t, X64 *x64) {
        if (s.where != NOWHERE || t.where != NOWHERE) {
            std::cerr << "Invalid special store from " << s << " to " << t << "!\n";
            throw INTERNAL_ERROR;
        }
    }
};


class HeapType: public Type {
public:
    HeapType(std::string name, unsigned pc)
        :Type(name, pc) {
    }
};


class BasicType: public Type {
public:
    unsigned size;
    int os;

    BasicType(std::string n, unsigned s)
        :Type(n, 0) {
        size = s;
        os = (s == 1 ? 0 : s == 2 ? 1 : s == 4 ? 2 : s == 8 ? 3 : throw INTERNAL_ERROR);        
    }
    
    virtual unsigned measure(TypeSpecIter tsi) {
        return size;
    }

    virtual void store(TypeSpecIter tsi, Storage s, Storage t, X64 *x64) {
        // We can't use any register, unless saved and restored
        BinaryOp mov = MOVQ % os;
        
        switch (s.where * t.where) {
        case NOWHERE_NOWHERE:
            return;
            
        case CONSTANT_NOWHERE:
            return;
        case CONSTANT_CONSTANT:
            return;
        case CONSTANT_REGISTER:
            x64->op(mov, t.reg, s.value);
            return;
        case CONSTANT_STACK:
            x64->op(PUSHQ, s.value);
            return;
        case CONSTANT_MEMORY:
            x64->op(mov, t.address, s.value);
            return;
            
        case FLAGS_NOWHERE:
            return;
        case FLAGS_FLAGS:
            return;
        case FLAGS_REGISTER:
            x64->op(s.bitset, t.reg);
            return;
        case FLAGS_STACK:
            x64->op(s.bitset, BL);
            x64->op(PUSHQ, RBX);
            return;
        case FLAGS_MEMORY:
            x64->op(s.bitset, t.address);
            return;

        case REGISTER_NOWHERE:
            return;
        case REGISTER_REGISTER:
            if (s.reg != t.reg)
                x64->op(mov, t.reg, s.reg);
            return;
        case REGISTER_STACK:
            x64->op(PUSHQ, s.reg);
            return;
        case REGISTER_MEMORY:
            x64->op(mov, t.address, s.reg);
            return;

        case STACK_NOWHERE:
            x64->op(POPQ, RBX);
            return;
        case STACK_REGISTER:
            x64->op(POPQ, t.reg);
            return;
        case STACK_STACK:
            return;
        case STACK_MEMORY:
            if (size == 8)
                x64->op(POPQ, t.address);
            else {
                x64->op(POPQ, RBX);
                x64->op(mov, t.address, RBX);
            }
            return;

        case MEMORY_NOWHERE:
            return;
        case MEMORY_REGISTER:
            x64->op(mov, t.reg, s.address);
            return;
        case MEMORY_STACK:
            if (size == 8)
                x64->op(PUSHQ, s.address);
            else {
                x64->op(mov, RBX, s.address);
                x64->op(PUSHQ, RBX);
            }
            return;
        case MEMORY_MEMORY:
            x64->op(mov, RBX, s.address);
            x64->op(mov, t.address, RBX);
            return;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual void create(TypeSpecIter tsi, Storage s, X64 *x64) {
        BinaryOp mov = (
            size == 1 ? MOVB :
            size == 2 ? MOVW :
            size == 4 ? MOVD :
            size == 8 ? MOVQ :
            throw INTERNAL_ERROR
        );

        if (s.where == REGISTER)
            x64->op(mov, s.reg, 0);
        else if (s.where == MEMORY)
            x64->op(mov, s.address, 0);
        else
            throw INTERNAL_ERROR;
    }

    virtual void destroy(TypeSpecIter tsi, Storage s, X64 *x64) {
        return;
    }

    virtual StorageWhere where(TypeSpecIter tsi) {
        return REGISTER;
    }

    virtual Storage boolval(TypeSpecIter tsi, Storage s, X64 *x64) {
        switch (s.where) {
        case CONSTANT:
            return Storage(CONSTANT, s.value != 0);
        case FLAGS:
            return Storage(FLAGS, s.bitset);
        case REGISTER:
            x64->op(CMPQ % os, s.reg, 0);
            return Storage(FLAGS, SETNE);
        case MEMORY:
            x64->op(CMPQ % os, s.address, 0);
            return Storage(FLAGS, SETNE);
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Value *convertible(TypeSpecIter this_tsi, TypeSpecIter that_tsi, Value *orig) {
        if (*that_tsi == boolean_type)
            return make_converted_value(BOOLEAN_TS, orig);
        else
            return Type::convertible(this_tsi, that_tsi, orig);
    }

    virtual Storage convert(TypeSpecIter this_tsi, TypeSpecIter that_tsi, Storage s, X64 *x64) {
        if (*that_tsi == boolean_type)
            return boolval(this_tsi, s, x64);  // Fortunately basic types need no cleanup
        else
            return Type::convert(this_tsi, that_tsi, s, x64);
    }
};


class SignedIntegerType: public BasicType {
public:
    SignedIntegerType(std::string n, unsigned s)
        :BasicType(n, s) {
    }
    
    virtual bool is_unsigned(TypeSpecIter tsi) {
        return false;
    }
};


class UnsignedIntegerType: public BasicType {
public:
    UnsignedIntegerType(std::string n, unsigned s)
        :BasicType(n, s) {
    }
    
    virtual bool is_unsigned(TypeSpecIter tsi) {
        return true;
    }
};


class ReferenceType: public Type {
public:
    ReferenceType()
        :Type("<Reference>", 1) {
    }
    
    virtual unsigned measure(TypeSpecIter ) {
        return 8;
    }

    virtual void store(TypeSpecIter , Storage s, Storage t, X64 *x64) {
        // Constants must be static arrays linked such that their offset
        // fits in 32-bit relocations.
        
        switch (s.where * t.where) {
        case NOWHERE_NOWHERE:
            return;
            
        case REGISTER_NOWHERE:
            x64->decref(s.reg);
            return;
        case REGISTER_REGISTER:
            if (s.reg != t.reg)
                x64->op(MOVQ, t.reg, s.reg);
            return;
        case REGISTER_STACK:
            x64->op(PUSHQ, s.reg);
            return;
        case REGISTER_MEMORY:
            x64->op(XCHGQ, t.address, s.reg);
            x64->decref(s.reg);
            return;

        case STACK_NOWHERE:
            x64->op(POPQ, RBX);
            x64->decref(RBX);
            return;
        case STACK_REGISTER:
            x64->op(POPQ, t.reg);
            return;
        case STACK_STACK:
            return;
        case STACK_MEMORY:
            x64->op(POPQ, RBX);
            x64->op(XCHGQ, RBX, t.address);
            x64->decref(RBX);
            return;

        case MEMORY_NOWHERE:
            return;
        case MEMORY_REGISTER:
            x64->op(MOVQ, t.reg, s.address);
            x64->incref(t.reg);
            return;
        case MEMORY_STACK:
            x64->op(MOVQ, RBX, s.address);
            x64->incref(RBX);
            x64->op(PUSHQ, RBX);
            return;
        case MEMORY_MEMORY:
            x64->op(MOVQ, RBX, s.address);
            x64->incref(RBX);
            x64->op(XCHGQ, RBX, t.address);
            x64->decref(RBX);
            return;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual void create(TypeSpecIter , Storage s, X64 *x64) {
        if (s.where == REGISTER)
            x64->op(MOVQ, s.reg, 0);
        else if (s.where == MEMORY)
            x64->op(MOVQ, s.address, 0);
        else
            throw INTERNAL_ERROR;
    }

    virtual void destroy(TypeSpecIter , Storage s, X64 *x64) {
        if (s.where == MEMORY) {
            x64->op(MOVQ, RBX, s.address);
            x64->decref(RBX);
        }
        else
            throw INTERNAL_ERROR;
    }

    virtual StorageWhere where(TypeSpecIter ) {
        return REGISTER;
    }

    virtual Storage boolval(TypeSpecIter , Storage s, X64 *x64) {
        switch (s.where) {
        case CONSTANT:
            return Storage(CONSTANT, s.value != 0);
        case REGISTER:
            x64->op(CMPQ, s.reg, 0);
            return Storage(FLAGS, SETNE);
        case MEMORY:
            x64->op(CMPQ, s.address, 0);
            return Storage(FLAGS, SETNE);
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Value *convertible(TypeSpecIter this_tsi, TypeSpecIter that_tsi, Value *orig) {
        if (*that_tsi == boolean_type)
            return make_converted_value(BOOLEAN_TS, orig);
        else
            return Type::convertible(this_tsi, that_tsi, orig);
    }

    virtual Storage convert(TypeSpecIter this_tsi, TypeSpecIter that_tsi, Storage s, X64 *x64) {
        if (*that_tsi == boolean_type) {
            // First we have to destroy, then compute the boolval, making sure that the
            // destruction left the value still accessible for that.
            
            switch (s.where) {
            case CONSTANT:
                break;
            case REGISTER:
                x64->decref(s.reg);
                break;
            case MEMORY:
                break;
            default:
                throw INTERNAL_ERROR;
            }
        
            return boolval(this_tsi, s, x64);
        }
        else
            return Type::convert(this_tsi, that_tsi, s, x64);
    }
};


class AttributeType: public Type {
public:
    AttributeType(std::string n)
        :Type(n, 1) {
    }

    virtual StorageWhere where(TypeSpecIter this_tsi) {
        this_tsi++;
        return (*this_tsi)->where(this_tsi);
    }

    virtual Storage boolval(TypeSpecIter this_tsi, Storage s, X64 *x64) {
        this_tsi++;
        return (*this_tsi)->boolval(this_tsi, s, x64);
    }
    
    virtual Value *convertible(TypeSpecIter this_tsi, TypeSpecIter that_tsi, Value *orig) {
        if (*this_tsi == *that_tsi) {
            // When the same attribute is required, accept only the exact same type.
            // This is necessary for lvalues.
            this_tsi++;
            that_tsi++;
            
            return equalish(this_tsi, that_tsi) ? orig : NULL;
        }
        else {
            // When the same attribute is not required, remove it and try regular conversions.
            this_tsi++;
            
            return (*this_tsi)->convertible(this_tsi, that_tsi, orig);
        }
    }

    virtual Storage convert(TypeSpecIter this_tsi, TypeSpecIter that_tsi, Storage s, X64 *x64) {
        // Must be a conversion as above
        this_tsi++;
        return (*this_tsi)->convert(this_tsi, that_tsi, s, x64);
    }
    
    virtual unsigned measure(TypeSpecIter tsi) {
        tsi++;
        return (*tsi)->measure(tsi);
    }

    virtual void store(TypeSpecIter tsi, Storage s, Storage t, X64 *x64) {
        tsi++;
        return (*tsi)->store(tsi, s, t, x64);
    }

    virtual void create(TypeSpecIter tsi, Storage s, X64 *x64) {
        tsi++;
        (*tsi)->create(tsi, s, x64);
    }

    virtual void destroy(TypeSpecIter tsi, Storage s, X64 *x64) {
        tsi++;
        (*tsi)->destroy(tsi, s, x64);
    }
};


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


void fill_typespec(TypeSpec &ts, TypeSpecIter &tsi) {
    unsigned pc = (*tsi)->parameter_count;
    
    ts.push_back(*tsi++);
        
    for (unsigned i = 0; i < pc; i++)
        fill_typespec(ts, tsi);
}


TypeSpec::TypeSpec() {
}


TypeSpec::TypeSpec(TypeSpecIter tsi) {
    fill_typespec(*this, tsi);
}


void skip(TypeSpecIter &tsi) {
    unsigned counter = 1;
    
    while (counter--)
        counter += (*tsi++)->parameter_count;
}


bool equalish(TypeSpecIter this_tsi, TypeSpecIter that_tsi) {
    // Almost exact equality, except conversion to ANY is always accepted for
    // any type parameter
    
    if (*that_tsi == any_type)
        return true;
    else if (*this_tsi != *that_tsi)
        return false;
        
    unsigned parameter_count = (*this_tsi)->parameter_count;
    this_tsi++;
    that_tsi++;
    
    for (unsigned p = 0; p < parameter_count; p++) {
        if (!equalish(this_tsi, that_tsi))
            return false;
            
        skip(this_tsi);
        skip(that_tsi);
    }
    
    return true;
}


StorageWhere TypeSpec::where() {
    TypeSpecIter this_tsi(begin());
    
    return (*this_tsi)->where(this_tsi);
}


Storage TypeSpec::boolval(Storage s, X64 *x64) {
    TypeSpecIter this_tsi(begin());
    
    return (*this_tsi)->boolval(this_tsi, s, x64);
}


Value *TypeSpec::convertible(TypeSpec &other, Value *orig) {
    TypeSpecIter this_tsi(begin());
    TypeSpecIter that_tsi(other.begin());
    
    return (*this_tsi)->convertible(this_tsi, that_tsi, orig);
}


Storage TypeSpec::convert(TypeSpec &other, Storage s, X64 *x64) {
    TypeSpecIter this_tsi(begin());
    TypeSpecIter that_tsi(other.begin());
    
    return (*this_tsi)->convert(this_tsi, that_tsi, s, x64);
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
    return at(0) == lvalue_type ? unprefix(lvalue_type) : *this;
}


TypeSpec TypeSpec::lvalue() {
    return at(0) == lvalue_type ? *this : prefix(lvalue_type);
}


unsigned TypeSpec::measure() {
    TypeSpecIter tsi(begin());
    return (*tsi)->measure(tsi);
}


bool TypeSpec::is_unsigned() {
    TypeSpecIter tsi(begin());
    return (*tsi)->is_unsigned(tsi);
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
