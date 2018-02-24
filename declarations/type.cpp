
enum TypeType {
    GENERIC_TYPE, VALUE_TYPE, IDENTITY_TYPE
};


typedef std::vector<TypeType> TTs;


class Type: public Declaration {
public:
    std::string name;
    TTs param_tts;
    DataScope *inner_scope;  // Will be owned by the outer scope
    TypeType type;
    
    Type(std::string n, TTs ptts, TypeType tt) {
        name = n;
        param_tts = ptts;
        type = tt;
        inner_scope = NULL;
    }
    
    virtual unsigned get_parameter_count() {
        return param_tts.size();
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
        
        TSs tss;
        unsigned pc = get_parameter_count();
            
        if (pc == 0) {
            if (pivot)
                return NULL;
        }
        else if (pc == 1) {
            if (!pivot)
                return NULL;
                
            tss.push_back(get_typespec(pivot));
        }
        else {
            TypeMatch match;
            
            if (!typematch(MULTI_TYPE_TS, pivot, match))
                return NULL;
                
            if (!unpack_value(pivot, tss))
                throw INTERNAL_ERROR;

            if (tss.size() != pc)
                return NULL;
        }
        
        TypeSpec result_ts = { type_type, this };
        
        for (unsigned i = 0; i < pc; i++) {
            TypeSpec &ts = tss[i];
            
            if (ts[0] != type_type)
                return NULL;
            
            TypeType tt = ts[1]->type;
            TypeType &ptt = param_tts[i];
            
            bool ok = (
                ptt == GENERIC_TYPE ? (tt == GENERIC_TYPE || tt == VALUE_TYPE || tt == IDENTITY_TYPE) :
                ptt == VALUE_TYPE ? tt == VALUE_TYPE :
                ptt == IDENTITY_TYPE ? tt == IDENTITY_TYPE :
                false
            );
            
            if (!ok)
                return NULL;
                
            // TODO: this is becoming obsolete...
            if (ts[1] == lvalue_type || ts[1] == ovalue_type || ts[1] == code_type || ts[1] == multi_type) {
                std::cerr << "Invalid type parameter: " << ts << "!\n";
                return NULL;
            }
            
            result_ts.insert(result_ts.end(), ts.begin() + 1, ts.end());
        }

        return make_type_value(result_ts);
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

    virtual void streamify(TypeMatch tm, bool repr, X64 *x64) {
        // NOTE: streamify is allowed to clobber all registers, because it is mostly called
        // from interpolation, which is in Void context, so not much is lost. But
        // nested streamifications must take care!
        
        Label us_label = x64->data_heap_string(decode_utf8("<unstreamifiable>"));
        x64->op(LEARIP, RBX, us_label);
        x64->op(PUSHQ, RBX);
        x64->op(PUSHQ, Address(RSP, ADDRESS_SIZE));
        STRING_TS.streamify(false, x64);
        x64->op(ADDQ, RSP, 16);
    }

    virtual Value *lookup_initializer(TypeMatch tm, std::string n) {
        std::cerr << "Uninitializable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }

    virtual Value *lookup_matcher(TypeMatch tm, std::string n, Value *pivot) {
        std::cerr << "Unmatchable type: " << name << "!\n";
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
        if (tm[0][0] == *target) {
            ifts = tm[0];
            return orig;
        }

        return find_implementation(tm, target, orig, ifts);
    }
    
    virtual void complete_type() {
    }
};


class SpecialType: public Type {
public:
    SpecialType(std::string name, TTs param_tts, TypeType tt)
        :Type(name, param_tts, tt) {
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
    SameType(std::string name, TTs param_tts, TypeType tt)
        :Type(name, param_tts, tt) {
    }
    
    virtual StorageWhere where(TypeMatch tm, bool is_arg, bool is_lvalue) {
        if (!is_arg && is_lvalue)
            return MEMORY;  // TODO: all types must be MEMORY for this combination!
        else
            throw INTERNAL_ERROR;
    }

    virtual Allocation measure(TypeMatch tm) {
        if (this == same_type)
            return Allocation(0, 1, 0, 0);
        else if (this == same2_type)
            return Allocation(0, 0, 1, 0);
        else if (this == same3_type)
            return Allocation(0, 0, 0, 1);
        else
            throw INTERNAL_ERROR;
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
        :Type(n, TTs { VALUE_TYPE }, GENERIC_TYPE) {
    }

    virtual StorageWhere where(TypeMatch tm, bool is_arg, bool is_lvalue) {
        return tm[1].where(is_arg, is_lvalue || this == lvalue_type || this == dvalue_type);
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

    virtual void streamify(TypeMatch tm, bool repr, X64 *x64) {
        tm[1].streamify(repr, x64);
    }

    virtual Value *lookup_initializer(TypeMatch tm, std::string n) {
        return tm[1].lookup_initializer(n);
    }

    virtual Value *lookup_matcher(TypeMatch tm, std::string n, Value *pivot) {
        return tm[1].lookup_matcher(n, pivot);
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
        :Type(name, TTs { VALUE_TYPE }, VALUE_TYPE) {
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
        :Type(name, TTs {}, GENERIC_TYPE) {
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
    virtual Value *lookup_initializer(TypeMatch tm, std::string n) {
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
#include "option.cpp"
