

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
    
    virtual Value *match(std::string name, Value *pivot) {
        if (name != this->name)
            return NULL;
            
        if (parameter_count == 0) {
            if (pivot)
                return NULL;
                
            TypeSpec ts = { type_type, this };
            
            return make_type_value(ts);
        }
        else if (parameter_count == 1) {
            TypeMatch match;
            
            if (!typematch(ANY_TYPE_TS, pivot, match))
                return NULL;
                
            TypeSpec param = match[1];
            if (param[0] == lvalue_type || param[0] == ovalue_type || param[0] == code_type) {
                std::cerr << "Invalid type parameter: " << param << "!\n";
                throw TYPE_ERROR;
            }

            TypeSpec ts = param.prefix(this).prefix(type_type);
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
            std::cerr << "Unstorable type: " << name << " from " << s << " to " << t << "!\n";
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

    virtual void compare(TypeSpecIter tsi, Storage s, Storage t, X64 *x64, Label less, Label greater) {
        std::cerr << "Uncomparable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }

    virtual Value *lookup_initializer(TypeSpecIter tsi, std::string n, Scope *scope) {
        std::cerr << "Uninitializable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }

    virtual Value *lookup_inner(TypeSpecIter tsi, std::string n, Value *v) {
        Scope *scope = get_inner_scope(tsi);
        
        if (scope)
            return scope->lookup(n, v);
        
        return NULL;
    }
    
    virtual Scope *get_inner_scope(TypeSpecIter tsi) {
        return NULL;
    }
    
    virtual std::vector<Function *> get_virtual_table(TypeSpecIter tsi) {
        throw INTERNAL_ERROR;
    }

    virtual Label get_virtual_table_label(TypeSpecIter tsi) {
        throw INTERNAL_ERROR;
    }
    
    virtual Value *autoconv(TypeSpecIter tsi, TypeSpecIter target, Value *orig, TypeSpec &ifts) {
        TypeMatch match = type_parameters_to_match(TypeSpec(tsi));
        
        if (is_implementation(*tsi, match, target, ifts))
            return orig;
            
        Scope *inner_scope = get_inner_scope(tsi);
        return find_implementation(inner_scope, match, target, orig, ifts);
    }
};


class SpecialType: public Type {
public:
    SpecialType(std::string name, unsigned pc)
        :Type(name, pc) {
    }
    
    virtual void store(TypeSpecIter tsi, Storage s, Storage t, X64 *x64) {
        if (s.where != NOWHERE || t.where != NOWHERE) {
            std::cerr << "Invalid special store from " << s << " to " << t << "!\n";
            throw INTERNAL_ERROR;
        }
    }
};


class SameType: public Type {
public:
    SameType(std::string name)
        :Type(name, 0) {
    }
    
    virtual StorageWhere where(TypeSpecIter tsi, bool is_arg, bool is_lvalue) {
        if (!is_arg && is_lvalue)
            return MEMORY;  // TODO: all types must be MEMORY for this combination!
        else
            throw INTERNAL_ERROR;
    }

    virtual unsigned measure(TypeSpecIter tsi, StorageWhere where) {
        if (where == MEMORY)
            return SAME_SIZE;
        else
            throw INTERNAL_ERROR;
    }

    virtual void store(TypeSpecIter tsi, Storage s, Storage t, X64 *x64) {
        if (s.where != NOWHERE || t.where != NOWHERE) {
            std::cerr << "Invalid Same store from " << s << " to " << t << "!\n";
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

    virtual void compare(TypeSpecIter tsi, Storage s, Storage t, X64 *x64, Label less, Label greater) {
        tsi++;
        return (*tsi)->compare(tsi, s, t, x64, less, greater);
    }

    virtual Value *lookup_initializer(TypeSpecIter tsi, std::string n, Scope *scope) {
        tsi++;
        return (*tsi)->lookup_initializer(tsi, n, scope);
    }

    virtual std::vector<Function *> get_virtual_table(TypeSpecIter tsi) {
        tsi++;
        return (*tsi)->get_virtual_table(tsi);
    }

    virtual Label get_virtual_table_label(TypeSpecIter tsi) {
        tsi++;
        return (*tsi)->get_virtual_table_label(tsi);
    }

    virtual Value *lookup_inner(TypeSpecIter tsi, std::string n, Value *v) {
        tsi++;
        return (*tsi)->lookup_inner(tsi, n, v);
    }
    
    virtual Scope *get_inner_scope(TypeSpecIter tsi) {
        tsi++;
        return (*tsi)->get_inner_scope(tsi);
    }
};


class MetaType: public Type {
public:
    typedef Value *(*TypeDefinitionFactory)();
    TypeDefinitionFactory factory;
    std::unique_ptr<DataScope> inner_scope;
    
    MetaType(std::string name, TypeDefinitionFactory f)
        :Type(name, 0) {
        factory = f;
        inner_scope.reset(new DataScope);
        inner_scope->set_pivot_type_hint(TypeSpec { any_type });
    }

    virtual Value *match(std::string name, Value *pivot) {
        if (name != this->name)
            return NULL;

        if (pivot)
            return NULL;

        return factory();
    }

    virtual void allocate() {
        inner_scope->allocate();
    }
    
    virtual Scope *get_inner_scope(TypeSpecIter tsi) {
        return inner_scope.get();
    }
};


class IntegerMetaType: public MetaType {
public:
    IntegerMetaType(std::string name)
        :MetaType(name, make_integer_definition_value) {
    }
};


class EnumerationMetaType: public MetaType {
public:
    EnumerationMetaType(std::string name)
        :MetaType(name, make_enumeration_definition_value) {
    }
};


class TreenumerationMetaType: public MetaType {
public:
    TreenumerationMetaType(std::string name)
        :MetaType(name, make_treenumeration_definition_value) {
    }

    // NOTE: experimental thing for exception specifications
    virtual Value *lookup_initializer(TypeSpecIter tsi, std::string n, Scope *scope) {
        if (n == "{}")
            return make_treenumeration_definition_value();
        
        return NULL;
    }
};


class RecordMetaType: public MetaType {
public:
    RecordMetaType(std::string name)
        :MetaType(name, make_record_definition_value) {
    }
};


class ClassMetaType: public MetaType {
public:
    ClassMetaType(std::string name)
        :MetaType(name, make_class_definition_value) {
    }
};


class InterfaceMetaType: public MetaType {
public:
    InterfaceMetaType(std::string name)
        :MetaType(name, make_interface_definition_value) {
    }
};


class ImplementationMetaType: public MetaType {
public:
    ImplementationMetaType(std::string name)
        :MetaType(name, make_implementation_definition_value) {
    }
};


#include "interface.cpp"
#include "basic.cpp"
#include "reference.cpp"
#include "record.cpp"
#include "class.cpp"
