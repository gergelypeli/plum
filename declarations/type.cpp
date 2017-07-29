
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
                
            TypeSpec ts = { type_type, this };
            
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
    
    virtual StorageWhere where(TypeSpecIter tsi) {
        std::cerr << "Nowhere type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }

    virtual Storage boolval(TypeSpecIter tsi, Storage, X64 *, bool probe) {
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
    
    virtual Value *initializer(TypeSpecIter tsi, std::string n) {
        std::cerr << "Uninitializable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }
    
    virtual Scope *get_inner_scope() {
        return NULL;
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

    virtual Storage boolval(TypeSpecIter tsi, Storage s, X64 *x64, bool probe) {
        // None of these cases destroy the original value, so they all pass for probing
        
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
    ReferenceType(std::string name)
        :Type(name, 1) {
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

    virtual Storage boolval(TypeSpecIter , Storage s, X64 *x64, bool probe) {
        switch (s.where) {
        case CONSTANT:
            return Storage(CONSTANT, s.value != 0);
        case REGISTER:
            if (!probe)
                x64->decref(s.reg);
                
            x64->op(CMPQ, s.reg, 0);
            return Storage(FLAGS, SETNE);
        case MEMORY:
            x64->op(CMPQ, s.address, 0);
            return Storage(FLAGS, SETNE);
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Value *initializer(TypeSpecIter tsi, std::string name) {
        if (name == "null") {
            return make_null_reference_value(TypeSpec(tsi));
        }
        else {
            std::cerr << "No reference initializer called " << name << "!\n";
            return NULL;
        }
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

    virtual Storage boolval(TypeSpecIter this_tsi, Storage s, X64 *x64, bool probe) {
        this_tsi++;
        return (*this_tsi)->boolval(this_tsi, s, x64, probe);
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


class EnumerationType: public BasicType {
public:
    std::vector<std::string> keywords;

    EnumerationType(std::string n, std::vector<std::string> kw)
        :BasicType(n, 1) {  // TODO: different sizes based on the keyword count!
        
        keywords = kw;
    }
    
    virtual Value *initializer(TypeSpecIter tsi, std::string n) {
        for (unsigned i = 0; i < keywords.size(); i++)
            if (keywords[i] == n)
                return make_enumeration_value(TypeSpec(tsi), i);
        
        return NULL;
    }
    
    virtual Scope *get_inner_scope() {
        return enumeration_metatype->get_inner_scope();
    }
};


class EnumerationMetaType: public Type {
public:
    std::unique_ptr<Scope> inner_scope;
    
    EnumerationMetaType(std::string name)
        :Type(name, 0) {
        
        inner_scope.reset(new Scope);
    }
    
    virtual Value *match(std::string name, Value *pivot) {
        if (name != this->name)
            return NULL;
            
        return make_enumeration_type_value();
    }
    
    virtual Scope *get_inner_scope() {
        return inner_scope.get();
    }
};
