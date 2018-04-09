
class HyperType: public Type {
public:
    HyperType()
        :Type("", Metatypes { }, NULL) {
    }
    
    virtual Value *match(std::string name, Value *pivot) {
        return NULL;
    }
};


class MetaType: public Type {
public:
    // Plain Type-s get their parameters in the form of pre-evaluated type names, and
    // produce variables by declaring them to those types.
    // MetaTypes get their parameters in the form of keyword arguments to be evaluated
    // later, and produce types by declaring them to the resulting type.

    Type *super_type;
    typedef Value *(*TypeDefinitionFactory)();
    TypeDefinitionFactory factory;
    
    MetaType(std::string n, Type *st, TypeDefinitionFactory f)
        :Type(n, Metatypes { }, metatype_hypertype) {
        super_type = st;
        factory = f;
    }

    virtual Value *match(std::string name, Value *pivot) {
        if (name != this->name)
            return NULL;
            
        if (pivot)
            return NULL;
            
        if (!factory)
            throw INTERNAL_ERROR;
            
        return factory();
    }
    
    virtual void store(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        if (s.where != NOWHERE || t.where != NOWHERE) {
            std::cerr << "Invalid metatype store from " << s << " to " << t << "!\n";
            throw INTERNAL_ERROR;
        }
    }
};


class TypeMetaType: public MetaType {
public:
    TypeMetaType(std::string name, Type *st)
        :MetaType(name, st, NULL) {
    }
};


class ValueMetaType: public MetaType {
public:
    ValueMetaType(std::string name, Type *st)
        :MetaType(name, st, NULL) {
    }
};


class IdentityMetaType: public MetaType {
public:
    IdentityMetaType(std::string name, Type *st)
        :MetaType(name, st, NULL) {
    }
};


class AttributeMetaType: public MetaType {
public:
    AttributeMetaType(std::string name, Type *st)
        :MetaType(name, st, NULL) {
    }
};


class IntegerMetaType: public MetaType {
public:
    IntegerMetaType(std::string name, Type *st)
        :MetaType(name, st, make_integer_definition_value) {
    }
};


class EnumerationMetaType: public MetaType {
public:
    EnumerationMetaType(std::string name, Type *st)
        :MetaType(name, st, make_enumeration_definition_value) {
    }
};


class TreenumerationMetaType: public MetaType {
public:
    TreenumerationMetaType(std::string name, Type *st)
        :MetaType(name, st, make_treenumeration_definition_value) {
    }

    // NOTE: experimental thing for exception specifications
    virtual Value *lookup_initializer(TypeMatch tm, std::string n) {
        if (n == "{}")
            return make_treenumeration_definition_value();
        
        return NULL;
    }
};


class RecordMetaType: public MetaType {
public:
    RecordMetaType(std::string name, Type *st)
        :MetaType(name, st, make_record_definition_value) {
    }
};


class ClassMetaType: public MetaType {
public:
    ClassMetaType(std::string name, Type *st)
        :MetaType(name, st, make_class_definition_value) {
    }
};


class InterfaceMetaType: public MetaType {
public:
    InterfaceMetaType(std::string name, Type *st)
        :MetaType(name, st, make_interface_definition_value) {
    }
};


class ImplementationMetaType: public MetaType {
public:
    ImplementationMetaType(std::string name, Type *st)
        :MetaType(name, st, make_implementation_definition_value) {
    }
};
