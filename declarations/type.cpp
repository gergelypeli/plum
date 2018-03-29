
enum TypeType {
    GENERIC_TYPE, VALUE_TYPE, IDENTITY_TYPE, ATTRIBUTE_TYPE, META_TYPE, HYPER_TYPE
};

typedef std::vector<TypeType> TTs;

std::ostream &operator<<(std::ostream &os, const TypeType tt) {
    os << (
        tt == GENERIC_TYPE ? "GENERIC_TYPE" :
        tt == VALUE_TYPE ? "VALUE_TYPE" :
        tt == IDENTITY_TYPE ? "IDENTITY_TYPE" :
        tt == ATTRIBUTE_TYPE ? "ATTRIBUTE_TYPE" :
        tt == META_TYPE ? "META_TYPE" :
        tt == HYPER_TYPE ? "HYPER_TYPE" :
        throw INTERNAL_ERROR
    );
    
    return os;
}


class Type: public Declaration {
public:
    std::string name;
    TTs param_tts;
    DataScope *inner_scope;  // Will be owned by the outer scope
    TypeType type;
    Type *upper_type;
    // type and upper_type are somewhat overlapping in functionality for now
    
    Type(std::string n, TTs ptts, TypeType tt, Type *ut) {
        name = n;
        param_tts = ptts;
        type = tt;
        upper_type = ut;
        inner_scope = NULL;
        
        if (ut && ut->type != META_TYPE && ut->type != HYPER_TYPE)
            throw INTERNAL_ERROR;
    }
    
    virtual unsigned get_parameter_count() {
        return param_tts.size();
    }
    
    virtual void set_name(std::string n) {
        name = n;
    }
    
    virtual void set_outer_scope(Scope *os) {
        // This slightly abuses the Scope structure, as the inner scope references directly
        // the outer scope, but that's fine here. But Type is not a proper Scope.
        
        Declaration::set_outer_scope(os);

        if (inner_scope)
            inner_scope->set_outer_scope(os);
    }
    
    virtual DataScope *make_inner_scope(TypeSpec pts) {
        if (inner_scope)
            throw INTERNAL_ERROR;
            
        inner_scope = new DataScope;
        inner_scope->set_pivot_type_hint(pts);
        
        if (outer_scope)
            inner_scope->set_outer_scope(outer_scope);
            
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

        // Metatypes must override this method
        if (!upper_type || upper_type->type != META_TYPE)
            throw INTERNAL_ERROR;
        
        TypeSpec result_ts = { upper_type, this };
        
        for (unsigned i = 0; i < pc; i++) {
            TypeSpec &ts = tss[i];
            
            if (ts[0]->type != META_TYPE)
                return NULL;
            
            TypeType tt = ts[1]->type;
            TypeType &ptt = param_tts[i];
            
            if (ptt != GENERIC_TYPE && ptt != tt)
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

    virtual void allocate() {
        if (inner_scope)
            inner_scope->allocate();
    }
    
    virtual StorageWhere where(TypeMatch tm, AsWhat as_what, bool as_lvalue) {
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
        return lookup_partinitializer(tm, n, NULL);
    }

    virtual Value *lookup_partinitializer(TypeMatch tm, std::string n, Value *pivot) {
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
    
    virtual std::vector<VirtualEntry *> get_virtual_table(TypeMatch tm) {
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

    virtual void init_vt(TypeMatch tm, Address addr, int data_offset, Label vt_label, int virtual_offset, X64 *x64) {
        std::cerr << "Unvtinitable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }
    
    virtual bool complete_type() {
        return true;
    }
};


class AnyType: public Type {
public:
    AnyType(std::string name, TTs param_tts, TypeType tt)
        :Type(name, param_tts, tt, type_metatype) {
    }
};


class SameType: public Type {
public:
    SameType(std::string name, TTs param_tts, TypeType tt)
        :Type(name, param_tts, tt, type_metatype) {
    }
    
    virtual StorageWhere where(TypeMatch tm, AsWhat as_what, bool as_lvalue) {
        if (as_what == AS_VARIABLE)
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
        else if (this == sameid_type) {
            std::cerr << "Hmmm, maybe this shouldn't have been called!\n";
            throw INTERNAL_ERROR;
        }
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
        :Type(n, TTs { VALUE_TYPE }, ATTRIBUTE_TYPE, type_metatype) {
    }

    virtual StorageWhere where(TypeMatch tm, AsWhat as_what, bool as_lvalue) {
        return tm[1].where(as_what, as_lvalue || this == lvalue_type || this == dvalue_type);
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

    virtual std::vector<VirtualEntry *> get_virtual_table(TypeMatch tm) {
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
        :Type(name, TTs { GENERIC_TYPE }, VALUE_TYPE, type_metatype) {
    }

    virtual StorageWhere where(TypeMatch tm, AsWhat as_what, bool as_lvalue) {
        return tm[1].where(as_what, as_lvalue);
        //return tm[1][0]->type == IDENTITY_TYPE ? 
    }

    virtual Allocation measure(TypeMatch tm) {
        //return Allocation(ADDRESS_SIZE);
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
        PartialVariable *pv = partial_variable_get_pv(v);
        
        if (pv->is_uninitialized(n)) {
            pv->be_initialized(n);
            
            Value *member = tm[1].lookup_inner(n, make_cast_value(v, tm[1]));
            TypeSpec member_ts = get_typespec(member);
            
            if (member_ts[0] == lvalue_type) {
                std::cerr << "Member variable " << n << " is not yet initialized.\n";
                set_typespec(member, member_ts.reprefix(lvalue_type, uninitialized_type));
            }
            else if (member_ts[0] == weakref_type) {
                std::cerr << "Member role " << n << " is not yet initialized.\n";
                set_typespec(member, member_ts.prefix(initializable_type));
            }
            else
                throw INTERNAL_ERROR;
                
            return member;
        }
        else if (pv->is_initialized(n)) {
            return tm[1].lookup_inner(n, make_cast_value(v, tm[1]));
        }
        else
            return NULL;
    }
    
    virtual Value *lookup_partinitializer(TypeMatch tm, std::string n, Value *v) {
        std::cerr << "Partial partinitializer lookup " << n << ".\n";
        PartialVariable *pv = partial_variable_get_pv(v);

        if (pv->is_dirty()) {
            std::cerr << "Can't delegate initialization of a dirty partial variable!\n";
            return NULL;
        }

        Value *member = tm[1].lookup_inner(n, make_cast_value(v, tm[1].prefix(initializable_type)));
        if (!member)
            return NULL;
        
        pv->be_complete();
        
        return member;
    }
};


class UninitializedType: public Type {
public:
    UninitializedType(std::string name)
        :Type(name, TTs { GENERIC_TYPE }, GENERIC_TYPE, type_metatype) {
    }

    virtual Value *lookup_partinitializer(TypeMatch tm, std::string n, Value *pivot) {
        if (n == "create from")
            return make_create_value(pivot, tm);
        else
            return NULL;
    }
};


class InitializableType: public Type {
public:
    InitializableType(std::string name)
        :Type(name, TTs { GENERIC_TYPE }, GENERIC_TYPE, type_metatype) {
    }

    virtual StorageWhere where(TypeMatch tm, AsWhat as_what, bool as_lvalue) {
        return tm[1].where(as_what, as_lvalue);
    }

    virtual Allocation measure(TypeMatch tm) {
        return tm[1].measure();
    }

    virtual void store(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        tm[1].store(s, t, x64);
    }

    virtual void create(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        tm[1].create(s, t, x64);
    }

    virtual Value *lookup_partinitializer(TypeMatch tm, std::string n, Value *pivot) {
        return tm[1].lookup_partinitializer(n, pivot);
    }
};


class EqualitymatcherType: public Type {
public:
    // To be used only as type context in the :is control
    
    EqualitymatcherType(std::string name)
        :Type(name, TTs { VALUE_TYPE }, VALUE_TYPE, type_metatype) {
    }
    
    virtual Value *lookup_initializer(TypeMatch tm, std::string n) {
        // We've found an initializer where a match was expected
        
        if (n == "{}") {
            return make_bulk_equality_matcher_value();
        }
        else {
            Value *v = tm[1].lookup_initializer(n);
            
            if (!v)
                return NULL;
        
            // Any arguments for us are actually arguments for the initializer
            return make_initializer_equality_matcher_value(v);
        }
    }
};


class MultiType: public Type {
public:
    MultiType(std::string name)
        :Type(name, {}, GENERIC_TYPE, type_metatype) {
    }
    /*
    virtual void store(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        if (s.where != NOWHERE || t.where != NOWHERE) {
            std::cerr << "Invalid Multi store from " << s << " to " << t << "!\n";
            throw INTERNAL_ERROR;
        }
    }
    */
};


class VoidType: public Type {
public:
    VoidType(std::string name)
        :Type(name, {}, VALUE_TYPE, type_metatype) {
    }

    virtual Allocation measure(TypeMatch tm) {
        return Allocation();
    }

    virtual void store(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        if (s.where != NOWHERE || t.where != NOWHERE) {
            std::cerr << "Invalid Void store from " << s << " to " << t << "!\n";
            throw INTERNAL_ERROR;
        }
    }

    virtual void destroy(TypeMatch tm, Storage s, X64 *x64) {
    }
};


#include "interface.cpp"
#include "basic.cpp"
#include "reference.cpp"
#include "record.cpp"
#include "class.cpp"
#include "option.cpp"
