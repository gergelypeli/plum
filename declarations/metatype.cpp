
class HyperType: public Type {
public:
    HyperType()
        :Type("", TTs { META_TYPE }, HYPER_TYPE, NULL) {
    }
};


class MetaType: public Type {
public:
    // Plain Type-s get their parameters in the form of pre-evaluated type names, and
    // produce variables by declaring them to those types.
    // MetaTypes get their parameters in the form of keyword arguments to be evaluated
    // later, and produce types by declaring them to the resulting type.
    
    typedef Value *(*TypeDefinitionFactory)();
    TypeDefinitionFactory factory;
    
    MetaType(std::string n, TypeType tt, TypeDefinitionFactory f)
        :Type(n, TTs { tt }, META_TYPE, NULL) {
        factory = f;
        
        // Allow the alpha metatype not to have an inner scope
        // FIXME: using any_type only allows metascopes for value types now!
        if (factory)
            make_inner_scope(TypeSpec { any_type });
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
};


class TypeMetaType: public MetaType {
public:
    TypeMetaType(std::string name)
        :MetaType(name, GENERIC_TYPE, NULL) {
    }
};


class IntegerMetaType: public MetaType {
public:
    IntegerMetaType(std::string name)
        :MetaType(name, VALUE_TYPE, make_integer_definition_value) {
    }
};


class EnumerationMetaType: public MetaType {
public:
    EnumerationMetaType(std::string name)
        :MetaType(name, VALUE_TYPE, make_enumeration_definition_value) {
    }
};


class TreenumerationMetaType: public MetaType {
public:
    TreenumerationMetaType(std::string name)
        :MetaType(name, VALUE_TYPE, make_treenumeration_definition_value) {
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
    RecordMetaType(std::string name)
        :MetaType(name, VALUE_TYPE, make_record_definition_value) {
    }
};


class ClassMetaType: public MetaType {
public:
    ClassMetaType(std::string name)
        :MetaType(name, IDENTITY_TYPE, make_class_definition_value) {
    }
};


class InterfaceMetaType: public MetaType {
public:
    InterfaceMetaType(std::string name)
        :MetaType(name, GENERIC_TYPE, make_interface_definition_value) {
    }
};


class ImplementationMetaType: public MetaType {
public:
    ImplementationMetaType(std::string name)
        :MetaType(name, GENERIC_TYPE, make_implementation_definition_value) {
    }
};
