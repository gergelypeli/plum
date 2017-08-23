
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
    
    virtual void set_name(std::string n) {
        name = n;
    }
    
    virtual Value *match(std::string name, Value *pivot, TypeMatch &match) {
        if (name != this->name)
            return NULL;
            
        if (parameter_count == 0) {
            if (!typematch(VOID_TS, pivot, match))
                return NULL;
                
            TypeSpec ts = { type_type, this };
            
            return make_type_value(ts);
        }
        else if (parameter_count == 1) {
            if (!typematch(ANY_TYPE_TS, pivot, match))
                return NULL;
                
            TypeSpec ts = match[1].prefix(this).prefix(type_type);
            // FIXME: do something with pivot!
            
            return make_type_value(ts);
        }
        else
            throw INTERNAL_ERROR;
    }
    
    virtual StorageWhere where(TypeSpecIter tsi, bool is_arg, bool is_lvalue) {
        std::cerr << "Nowhere type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }

    virtual Storage boolval(TypeSpecIter tsi, Storage, X64 *, bool probe) {
        std::cerr << "Unboolable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }

    virtual unsigned measure(TypeSpecIter tsi, StorageWhere where) {
        switch (where) {
        case ALISTACK:
            return 8;
        case ALIAS:
            return 8;
        default:
            std::cerr << "Unmeasurable type: " << name << "!\n";
            throw INTERNAL_ERROR;
        }
    }

    virtual void store(TypeSpecIter this_tsi, Storage s, Storage t, X64 *x64) {
        switch (s.where * t.where) {
        case MEMORY_ALISTACK:
            x64->op(LEA, RBX, s.address);
            x64->op(PUSHQ, RBX);
            return;
        case MEMORY_ALIAS:
            x64->op(LEA, RBX, s.address);
            x64->op(MOVQ, t.address, RBX);
            return;
            
        case ALISTACK_NOWHERE:
            x64->op(ADDQ, RSP, 8);
            return;
        case ALISTACK_MEMORY:
            if (t.address.base == NOREG || t.address.base == RBP || t.address.base == RSP || t.address.index != NOREG || t.address.offset != 0)
                throw INTERNAL_ERROR;
                
            x64->op(POPQ, t.address.base);
            return;
        case ALISTACK_ALISTACK:
            return;
        case ALISTACK_ALIAS:
            x64->op(POPQ, t.address);
            return;
            
        case ALIAS_NOWHERE:
            return;
        case ALIAS_MEMORY:
            if (t.address.base == NOREG || t.address.base == RBP || t.address.base == RSP || t.address.index != NOREG || t.address.offset != 0)
                throw INTERNAL_ERROR;
                
            x64->op(MOVQ, t.address.base, s.address);
            return;
        case ALIAS_ALISTACK:
            x64->op(PUSHQ, s.address);
            return;
        case ALIAS_ALIAS:
            x64->op(MOVQ, RBX, s.address);
            x64->op(MOVQ, t.address, RBX);
            return;
            
        default:
            std::cerr << "Unstorable type: " << name << "!\n";
            throw INTERNAL_ERROR;
        }
    }
    
    virtual void create(TypeSpecIter tsi, Storage s, Storage t, X64 *x64) {
        std::cerr << "Uncreatable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }

    virtual void destroy(TypeSpecIter tsi, Storage s, X64 *x64) {
        std::cerr << "Undestroyable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }

    virtual Value *lookup_initializer(TypeSpecIter tsi, std::string n, Scope *scope) {
        std::cerr << "Uninitializable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }
    
    virtual Scope *get_inner_scope() {
        return NULL;
    }
};


class SpecialType: public Type {
public:
    SpecialType(std::string name, unsigned pc)
        :Type(name, pc) {
    }
    
    //virtual unsigned measure(TypeSpecIter, StorageWhere) {
    //    return 0;
    //}

    //virtual StorageWhere where(TypeSpecIter tsi) {
    //    return NOWHERE;
    //}

    virtual void store(TypeSpecIter tsi, Storage s, Storage t, X64 *x64) {
        if (s.where != NOWHERE || t.where != NOWHERE) {
            std::cerr << "Invalid special store from " << s << " to " << t << "!\n";
            throw INTERNAL_ERROR;
        }
    }
};


class AttributeType: public Type {
public:
    AttributeType(std::string n)
        :Type(n, 1) {
    }

    virtual StorageWhere where(TypeSpecIter this_tsi, bool is_arg, bool is_lvalue) {
        this_tsi++;
        return (*this_tsi)->where(this_tsi, is_arg, is_lvalue || this == lvalue_type);
    }

    virtual Storage boolval(TypeSpecIter this_tsi, Storage s, X64 *x64, bool probe) {
        this_tsi++;
        return (*this_tsi)->boolval(this_tsi, s, x64, probe);
    }
    
    virtual unsigned measure(TypeSpecIter tsi, StorageWhere where) {
        tsi++;
        return (*tsi)->measure(tsi, where);
    }

    virtual void store(TypeSpecIter tsi, Storage s, Storage t, X64 *x64) {
        tsi++;
        return (*tsi)->store(tsi, s, t, x64);
    }

    virtual void create(TypeSpecIter tsi, Storage s, Storage t, X64 *x64) {
        tsi++;
        (*tsi)->create(tsi, s, t, x64);
    }

    virtual void destroy(TypeSpecIter tsi, Storage s, X64 *x64) {
        tsi++;
        (*tsi)->destroy(tsi, s, x64);
    }

    virtual Value *lookup_initializer(TypeSpecIter tsi, std::string n, Scope *scope) {
        tsi++;
        return (*tsi)->lookup_initializer(tsi, n, scope);
    }
};


class MetaType: public Type {
public:
    std::unique_ptr<Scope> inner_scope;
    
    MetaType(std::string name)
        :Type(name, 0) {
        
        inner_scope.reset(new Scope);
    }
    
    virtual Scope *get_inner_scope() {
        return inner_scope.get();
    }
};


class IntegerMetaType: public MetaType {
public:
    IntegerMetaType(std::string name)
        :MetaType(name) {
    }
    
    virtual Value *match(std::string name, Value *pivot, TypeMatch &match) {
        if (name != this->name)
            return NULL;

        if (!typematch(VOID_TS, pivot, match))
            return NULL;

        return make_integer_definition_value();
    }
};


class EnumerationMetaType: public MetaType {
public:
    EnumerationMetaType(std::string name)
        :MetaType(name) {
    }
    
    virtual Value *match(std::string name, Value *pivot, TypeMatch &match) {
        if (name != this->name)
            return NULL;
            
        if (!typematch(VOID_TS, pivot, match))
            return NULL;
            
        return make_enumeration_definition_value();
    }
};


class TreenumerationMetaType: public MetaType {
public:
    TreenumerationMetaType(std::string name)
        :MetaType(name) {
    }
    
    virtual Value *match(std::string name, Value *pivot, TypeMatch &match) {
        if (name != this->name)
            return NULL;
            
        if (!typematch(VOID_TS, pivot, match))
            return NULL;
            
        return make_treenumeration_definition_value();
    }
};


class RecordMetaType: public MetaType {
public:
    RecordMetaType(std::string name)
        :MetaType(name) {
    }
    
    virtual Value *match(std::string name, Value *pivot, TypeMatch &match) {
        if (name != this->name)
            return NULL;
            
        if (!typematch(VOID_TS, pivot, match))
            return NULL;
            
        return make_record_definition_value();
    }
};


#include "basic.cpp"
#include "reference.cpp"
#include "record.cpp"

