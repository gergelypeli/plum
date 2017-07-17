
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
        if (name == this->name && pivot == NULL && parameter_count == 0) {
            TypeSpec ts;
            ts.push_back(type_type);
            ts.push_back(this);
            
            return make_type_value(ts);
        }
        else
            return NULL;
    }
    
    virtual bool is_equal(TypeSpecIter &this_tsi, TypeSpecIter &that_tsi) {
        if (*this_tsi != *that_tsi)
            return false;
            
        this_tsi++;
        that_tsi++;
        
        for (unsigned p = 0; p < parameter_count; p++) {
            if (!(*this_tsi)->is_equal(this_tsi, that_tsi))
                return false;
        }
        
        return true;
    }

    virtual Value *convertible(TypeSpecIter &this_tsi, TypeSpecIter &that_tsi, Value *orig) {
        return (*this_tsi)->is_equal(this_tsi, that_tsi) ? orig : NULL;
    }

    virtual Storage convert(TypeSpecIter &, TypeSpecIter &, Storage, X64 *) {
        std::cerr << "Unconvertable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }
    
    virtual unsigned measure(TypeSpecIter &) {
        std::cerr << "Unmeasurable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }

    virtual void store(TypeSpecIter &, Storage, Storage, X64 *) {
        std::cerr << "Unstorable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }
    
    virtual void create(TypeSpecIter &, Storage, X64 *) {
        std::cerr << "Uncreatable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }

    virtual void destroy(TypeSpecIter &, Storage, X64 *) {
        std::cerr << "Undestroyable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }
};


class SpecialType: public Type {
public:
    SpecialType(std::string name, unsigned pc):Type(name, pc) {}
    
    virtual unsigned measure(TypeSpecIter &) {
        return 0;
    }

    virtual void store(TypeSpecIter &, Storage s, Storage t, X64 *) {
        if (s.where != NOWHERE || t.where != NOWHERE) {
            std::cerr << "Invalid special store from " << s << " to " << t << "!\n";
            throw INTERNAL_ERROR;
        }
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
    
    virtual unsigned measure(TypeSpecIter &) {
        return size;
    }

    virtual void store(TypeSpecIter &, Storage s, Storage t, X64 *x64) {
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
            x64->op(PUSHQ, 0);
            x64->op(s.bitset, Address(RSP, 0));
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
            x64->op(ADDQ, RSP, 8);
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
                x64->op(XCHGQ, RAX, Address(RSP, 0));
                x64->op(mov, t.address, RAX);
                x64->op(POPQ, RAX);
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
                x64->op(PUSHQ, RAX);
                x64->op(mov, RAX, s.address);
                x64->op(XCHGQ, RAX, Address(RSP, 0));
            }
            return;
        case MEMORY_MEMORY:
            if (size == 8) {
                x64->op(PUSHQ, s.address);
                x64->op(POPQ, t.address);
            }
            else {
                x64->op(PUSHQ, RAX);
                x64->op(mov, RAX, s.address);
                x64->op(mov, t.address, RAX);
                x64->op(POPQ, RAX);
            }
            return;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual void create(TypeSpecIter &, Storage s, X64 *x64) {
        BinaryOp mov = (
            size == 1 ? MOVB :
            size == 2 ? MOVW :
            size == 4 ? MOVD :
            size == 8 ? MOVQ :
            throw INTERNAL_ERROR
        );

        if (s.where == MEMORY)
            x64->op(mov, s.address, 0);
        else
            throw INTERNAL_ERROR;
    }

    virtual void destroy(TypeSpecIter &, Storage, X64 *) {
        return;
    }

    virtual Value *convertible(TypeSpecIter &this_tsi, TypeSpecIter &that_tsi, Value *orig) {
        return (*this_tsi)->is_equal(this_tsi, that_tsi) ? orig : *that_tsi == boolean_type ? make_converted_value(BOOLEAN_TS, orig) : NULL;
    }

    virtual Storage convert(TypeSpecIter &, TypeSpecIter &, Storage s, X64 *x64) {
        switch (s.where) {
        case CONSTANT:
            return Storage(CONSTANT, s.value != 0);
        case REGISTER:
            x64->op(CMPQ % os, s.reg, 0);
            return Storage(FLAGS, SETNE);
        case STACK:
            x64->op(CMPQ % os, Address(RSP, 0), 0);
            x64->op(SETNE, Address(RSP, 0));
            return Storage(STACK);
        case MEMORY:
            x64->op(CMPQ % os, s.address, 0);
            return Storage(FLAGS, SETNE);
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class ArrayType: public Type {
public:
    ArrayType()
        :Type("Array", 1) {
    }
    
    virtual unsigned measure(TypeSpecIter &) {
        return 8;
    }

    virtual void store(TypeSpecIter &, Storage s, Storage t, X64 *x64) {
        // We can't use any register, unless saved and restored
        // Constants must be static arrays linked such that their offset
        // fits in 32-bit relocations.
        // No lifetime management yet.
        
        switch (s.where * t.where) {
        case NOWHERE_NOWHERE:
            return;
            
        case CONSTANT_NOWHERE:
            return;
        case CONSTANT_CONSTANT:
            return;
        case CONSTANT_REGISTER:
            x64->op(MOVQ, t.reg, s.value);
            return;
        case CONSTANT_STACK:
            x64->op(PUSHQ, s.value);
            return;
        case CONSTANT_MEMORY:
            x64->op(MOVQ, t.address, s.value);
            return;
            
        case REGISTER_NOWHERE:
            return;
        case REGISTER_REGISTER:
            if (s.reg != t.reg)
                x64->op(MOVQ, t.reg, s.reg);
            return;
        case REGISTER_STACK:
            x64->op(PUSHQ, s.reg);
            return;
        case REGISTER_MEMORY:
            x64->op(MOVQ, t.address, s.reg);
            return;

        case STACK_NOWHERE:
            x64->op(ADDQ, RSP, 8);
            return;
        case STACK_REGISTER:
            x64->op(POPQ, t.reg);
            return;
        case STACK_STACK:
            return;
        case STACK_MEMORY:
            x64->op(POPQ, t.address);
            return;

        case MEMORY_NOWHERE:
            return;
        case MEMORY_REGISTER:
            x64->op(MOVQ, t.reg, s.address);
            return;
        case MEMORY_STACK:
            x64->op(PUSHQ, s.address);
            return;
        case MEMORY_MEMORY:
            x64->op(PUSHQ, s.address);
            x64->op(POPQ, t.address);
            return;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual void create(TypeSpecIter &, Storage s, X64 *x64) {
        if (s.where == MEMORY)
            x64->op(MOVQ, s.address, 0);
        else
            throw INTERNAL_ERROR;
    }

    virtual void destroy(TypeSpecIter &, Storage, X64 *) {
        return;
    }

    virtual Value *convertible(TypeSpecIter &this_tsi, TypeSpecIter &that_tsi, Value *orig) {
        return (*this_tsi)->is_equal(this_tsi, that_tsi) ? orig : *that_tsi == boolean_type ? make_converted_value(BOOLEAN_TS, orig) : NULL;
    }

    virtual Storage convert(TypeSpecIter &, TypeSpecIter &, Storage s, X64 *x64) {
        switch (s.where) {
        case CONSTANT:
            return Storage(CONSTANT, s.value != 0);
        case REGISTER:
            x64->op(CMPQ, s.reg, 0);
            return Storage(FLAGS, SETNE);
        case STACK:
            x64->op(CMPQ, Address(RSP, 0), 0);
            x64->op(SETNE, Address(RSP, 0));
            return Storage(STACK);
        case MEMORY:
            x64->op(CMPQ, s.address, 0);
            return Storage(FLAGS, SETNE);
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class LvalueType: public Type {
public:
    LvalueType()
        :Type("<Lvalue>", 1) {
    }
    
    virtual Value *convertible(TypeSpecIter &this_tsi, TypeSpecIter &that_tsi, Value *orig) {
        if (*this_tsi == *that_tsi) {
            // When an lvalue is required, only the same type suffices
            this_tsi++;
            that_tsi++;
            
            return (*this_tsi)->is_equal(this_tsi, that_tsi) ? orig : NULL;
        }
        else {
            // When an rvalue is required, subtypes are also fine
            this_tsi++;
            
            return (*this_tsi)->convertible(this_tsi, that_tsi, orig);
        }
    }

    virtual Storage convert(TypeSpecIter &this_tsi, TypeSpecIter &that_tsi, Storage s, X64 *x64) {
        // Converting to an rvalue
        this_tsi++;
        return (*this_tsi)->convert(this_tsi, that_tsi, s, x64);
    }
    
    virtual unsigned measure(TypeSpecIter &tsi) {
        tsi++;
        return (*tsi)->measure(tsi);
    }

    virtual void store(TypeSpecIter &tsi, Storage s, Storage t, X64 *x64) {
        tsi++;
        return (*tsi)->store(tsi, s, t, x64);
    }

    virtual void create(TypeSpecIter &tsi, Storage s, X64 *x64) {
        tsi++;
        (*tsi)->create(tsi, s, x64);
    }

    virtual void destroy(TypeSpecIter &tsi, Storage s, X64 *x64) {
        tsi++;
        (*tsi)->destroy(tsi, s, x64);
    }
};


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
    if (at(0) != t || t->parameter_count != 1)
        throw INTERNAL_ERROR;
        
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


std::ostream &operator<<(std::ostream &os, TypeSpec &ts) {
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
