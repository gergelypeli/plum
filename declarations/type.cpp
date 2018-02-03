

class Type: public Declaration {
public:
    std::string name;
    unsigned parameter_count;
    DataScope *inner_scope;  // Will be owned by the outer scope
    
    Type(std::string n, unsigned pc) {
        name = n;
        parameter_count = pc;
        inner_scope = NULL;
    }
    
    virtual unsigned get_parameter_count() {
        return parameter_count;
    }
    
    virtual void set_name(std::string n) {
        name = n;
    }
    
    virtual void set_outer_scope(Scope *os) {
        // Make sure the inner scope is added first, because the removal only works
        // on the type, when it is the last declaration is a scope.
        // FIXME: this can't be true for the builtin types, where the Type is added
        // first, and the inner scope much later...
        
        if (inner_scope && os)
            os->add(inner_scope);
            
        Declaration::set_outer_scope(os);
        
        if (inner_scope && !os)
            inner_scope->outer_scope->remove(inner_scope);
    }
    
    virtual DataScope *make_inner_scope(TypeSpec pts) {
        if (inner_scope)
            throw INTERNAL_ERROR;
            
        //if (outer_scope)
        //    throw INTERNAL_ERROR;  // Just to make sure we can keep the right order
            
        inner_scope = new DataScope;
        inner_scope->set_pivot_type_hint(pts);
        
        // TODO: awkward ordering may be an issue, the inner scope may be added much later
        // as the Type to the outer scope. Probably Type itself should be a Scope.
        // Maybe its own inner?
        if (outer_scope)
            outer_scope->add(inner_scope);
            
        return inner_scope;
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
    
    virtual StorageWhere where(TypeMatch tm, bool is_arg, bool is_lvalue) {
        std::cerr << "Nowhere type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }

    virtual Storage boolval(TypeMatch tm, Storage, X64 *, bool probe) {
        std::cerr << "Unboolable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }

    virtual Allocation measure(TypeMatch tm) {
        std::cerr << "Unmeasurable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }

    virtual void store(TypeMatch tm, Storage s, Storage t, X64 *x64) {
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
            x64->op(ADDQ, RSP, ALIAS_SIZE);
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
    
    virtual void create(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        std::cerr << "Uncreatable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }

    virtual void destroy(TypeMatch tm, Storage s, X64 *x64) {
        std::cerr << "Undestroyable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }

    virtual void compare(TypeMatch tm, Storage s, Storage t, X64 *x64, Label less, Label greater) {
        std::cerr << "Uncomparable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }

    virtual Value *lookup_initializer(TypeMatch tm, std::string n, Scope *scope) {
        std::cerr << "Uninitializable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }

    virtual Value *lookup_inner(TypeMatch tm, std::string n, Value *v) {
        Scope *scope = get_inner_scope(tm);
        
        if (scope)
            return scope->lookup(n, v);
        
        return NULL;
    }
    
    virtual DataScope *get_inner_scope(TypeMatch tm) {
        return inner_scope;
    }
    
    virtual std::vector<Function *> get_virtual_table(TypeMatch tm) {
        throw INTERNAL_ERROR;
    }

    virtual Label get_virtual_table_label(TypeMatch tm, X64 *x64) {
        throw INTERNAL_ERROR;
    }

    virtual Label get_finalizer_label(TypeMatch tm, X64 *x64) {
        throw INTERNAL_ERROR;
    }
    
    virtual Value *autoconv(TypeMatch tm, TypeSpecIter target, Value *orig, TypeSpec &ifts) {
        //TypeMatch match = type_parameters_to_match(TypeSpec(tsi));
        
        if (is_implementation(this, tm, target, ifts))
            return orig;
            
        Scope *inner_scope = get_inner_scope(tm);
        return find_implementation(inner_scope, tm, target, orig, ifts);
    }
    
    virtual void complete_type() {
    }
};


class SpecialType: public Type {
public:
    SpecialType(std::string name, unsigned pc)
        :Type(name, pc) {
    }
    
    virtual void store(TypeMatch tm, Storage s, Storage t, X64 *x64) {
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
    
    virtual StorageWhere where(TypeMatch tm, bool is_arg, bool is_lvalue) {
        if (!is_arg && is_lvalue)
            return MEMORY;  // TODO: all types must be MEMORY for this combination!
        else
            throw INTERNAL_ERROR;
    }

    virtual Allocation measure(TypeMatch tm) {
        return Allocation(0, 1, 0, 0);  // TODO: Same increases the count1
    }

    virtual void store(TypeMatch tm, Storage s, Storage t, X64 *x64) {
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

    virtual StorageWhere where(TypeMatch tm, bool is_arg, bool is_lvalue) {
        return tm[1].where(is_arg, is_lvalue || this == lvalue_type);
    }

    virtual Storage boolval(TypeMatch tm, Storage s, X64 *x64, bool probe) {
        return tm[1].boolval(s, x64, probe);
    }
    
    virtual Allocation measure(TypeMatch tm) {
        return tm[1].measure();
    }

    virtual void store(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        return tm[1].store(s, t, x64);
    }

    virtual void create(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        tm[1].create(s, t, x64);
    }

    virtual void destroy(TypeMatch tm, Storage s, X64 *x64) {
        tm[1].destroy(s, x64);
    }

    virtual void compare(TypeMatch tm, Storage s, Storage t, X64 *x64, Label less, Label greater) {
        tm[1].compare(s, t, x64, less, greater);
    }

    virtual Value *lookup_initializer(TypeMatch tm, std::string n, Scope *scope) {
        return tm[1].lookup_initializer(n, scope);
    }

    virtual std::vector<Function *> get_virtual_table(TypeMatch tm) {
        return tm[1].get_virtual_table();
    }

    virtual Label get_virtual_table_label(TypeMatch tm, X64 *x64) {
        return tm[1].get_virtual_table_label(x64);
    }

    virtual Value *lookup_inner(TypeMatch tm, std::string n, Value *v) {
        return tm[1].lookup_inner(n, v);
    }
    
    virtual DataScope *get_inner_scope(TypeMatch tm) {
        return tm[1].get_inner_scope();
    }
};


class PartialType: public Type {
public:
    PartialType(std::string name)
        :Type(name, 1) {
    }

    virtual StorageWhere where(TypeMatch tm, bool is_arg, bool is_lvalue) {
        return tm[1].where(is_arg, is_lvalue);
    }

    virtual Allocation measure(TypeMatch tm) {
        return tm[1].measure();
    }

    virtual void store(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        return tm[1].store(s, t, x64);
    }

    virtual void create(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        tm[1].create(s, t, x64);
    }

    virtual Value *lookup_inner(TypeMatch tm, std::string n, Value *v) {
        std::cerr << "Partial inner lookup " << n << ".\n";
        
        if (!partial_variable_is_initialized(n, v)) {
            std::cerr << "Rejecting access to uninitialized member " << n << "!\n";
            return NULL;
        }
        
        TypeSpec ts = tm[0].unprefix(partial_type);
        Value *allowed = make_cast_value(v, ts);
        
        return ts.lookup_inner(n, allowed);
    }
};


class MetaType: public Type {
public:
    typedef Value *(*TypeDefinitionFactory)();
    TypeDefinitionFactory factory;
    
    MetaType(std::string name, TypeDefinitionFactory f)
        :Type(name, 0) {
        factory = f;
        make_inner_scope(TypeSpec { any_type });
    }

    virtual Value *match(std::string name, Value *pivot) {
        if (name != this->name)
            return NULL;

        if (pivot)
            return NULL;

        return factory();
    }

    //virtual void allocate() {
    //    inner_scope->allocate();
    //}
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
    virtual Value *lookup_initializer(TypeMatch tm, std::string n, Scope *scope) {
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
