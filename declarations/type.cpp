
#define EQUAL_CLOB Regs()
#define COMPARE_CLOB Regs()
#define STREAMIFY_CLOB Regs::all()

typedef std::vector<MetaType *> Metatypes;

class Type: public Declaration {
public:
    std::string name;
    std::string prefix;
    Metatypes param_metatypes;
    std::unique_ptr<DataScope> inner_scope;  // Won't be visible from the outer scope
    MetaType *meta_type;
    
    Type(std::string n, Metatypes pmts, MetaType *ut) {
        name = n;
        prefix = n + ".";
        param_metatypes = pmts;
        meta_type = ut;
        //inner_scope = NULL;
    }
    
    virtual unsigned get_parameter_count() {
        return param_metatypes.size();
    }
    
    virtual void set_name(std::string n) {
        name = n;
        prefix = n + ".";
        
        if (inner_scope)
            inner_scope->set_name(n);
    }
    
    virtual void set_outer_scope(Scope *os) {
        // This slightly abuses the Scope structure, as the inner scope references directly
        // the outer scope, but that's fine here. But Type is not a proper Scope.
        
        Declaration::set_outer_scope(os);

        if (inner_scope)
            inner_scope->set_outer_scope(os);
    }

    virtual DataScope *make_inner_scope() {
        return new DataScope;
    }
    
    virtual DataScope *make_inner_scope(TypeSpec pts) {
        if (inner_scope)
            throw INTERNAL_ERROR;
            
        inner_scope.reset(make_inner_scope());
        inner_scope->set_pivot_type_hint(pts);
        inner_scope->set_name(name);
        
        Scope *meta_scope = ptr_cast<Type>(meta_type)->get_inner_scope();
        
        if (meta_scope)
            inner_scope->set_meta_scope(meta_scope);
        
        if (outer_scope)
            inner_scope->set_outer_scope(outer_scope);
            
        inner_scope->enter();
            
        return inner_scope.get();
    }
    
    virtual DataScope *get_inner_scope() {
        return inner_scope.get();
    }

    virtual void outer_scope_entered() {
        if (inner_scope)
            inner_scope->outer_scope_entered();
    }

    virtual void outer_scope_left() {
        if (inner_scope)
            inner_scope->outer_scope_left();
    }

    virtual Value *matched(TypeSpec result_ts) {
        return make<TypeValue>(meta_type, result_ts);
    }
    
    virtual Value *match(std::string name, Value *pivot, Scope *scope) {
        //std::cerr << "Matching " << name << " to type " << this->name << "\n";
        
        if (name != this->name) {
            //std::cerr << "Rematching " << name << " to prefix " << this->prefix << "\n";
            
            if (deprefix(name, prefix)) {
                //std::cerr << "Entering explicit scope " << prefix << "\n";
                
                Scope *s = get_inner_scope();
                
                if (s)
                    return s->lookup(name, pivot, scope);
            }
                
            return NULL;
        }

        TSs tss;
        unsigned pc = get_parameter_count();
            
        if (pc == 0) {
            if (pivot)
                return NULL;
        }
        else if (pc == 1) {
            if (!pivot)
                return NULL;
            
            TypeSpec t = get_typespec(pivot);
            
            if (!t.is_meta()) {
                std::cerr << "Invalid type parameter type: " << t << "\n";
                return NULL;
            }
            
            tss.push_back(type_value_represented_ts(pivot));
        }
        else {
            TypeMatch match;
            
            if (!typematch(MULTITYPE_TS, pivot, match)) {
                std::cerr << "Type " << name << " needs type parameters!\n";
                return NULL;
            }
                
            if (!unpack_value(pivot, tss))
                throw INTERNAL_ERROR;

            if (tss.size() != pc) {
                std::cerr << "Type " << name << " needs " << pc << " parameters!\n";
                return NULL;
            }
        }

        TypeSpec result_ts = { this };
        
        for (unsigned i = 0; i < pc; i++) {
            TypeSpec &ts = tss[i];
            
            if (ts.is_meta()) {
                std::cerr << "Data type parameters must be data types!\n";
                return NULL;
            }

            if (!ts.has_meta(param_metatypes[i])) {
                std::cerr << "Type " << name << " parameter " << i + 1 << " is not a " << ptr_cast<Type>(param_metatypes[i])->name << " but " << ts << "!\n";
                return NULL;
            }
            
            result_ts.insert(result_ts.end(), ts.begin(), ts.end());
        }

        return matched(result_ts);
    }

    virtual void allocate() {
        if (inner_scope)
            inner_scope->allocate();
    }
    
    virtual StorageWhere where(TypeMatch tm, AsWhat as_what) {
        std::cerr << "Nowhere type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }

    virtual Allocation measure(TypeMatch tm) {
        std::cerr << "Unmeasurable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }

    virtual Allocation measure_identity(TypeMatch tm) {
        std::cerr << "Unmeasurableidenity type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }

    virtual void store(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        // Assume MEMORY are initialized
        
        switch (s.where * t.where) {
        case NOWHERE_NOWHERE:
            return;
            
        case MEMORY_NOWHERE:
            return;
        case MEMORY_ALISTACK:
            x64->op(LEA, R10, s.address);
            x64->op(PUSHQ, R10);
            return;
        case MEMORY_ALIAS:
            x64->op(LEA, R10, s.address);
            x64->op(MOVQ, t.address, R10);
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
            x64->op(MOVQ, R10, s.address);
            x64->op(MOVQ, t.address, R10);
            return;
            
        default:
            std::cerr << "Unstorable type: " << name << " from " << s << " to " << t << "!\n";
            throw INTERNAL_ERROR;
        }
    }
    
    virtual void create(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        // Assume target is uninitialized
        
        std::cerr << "Uncreatable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }

    virtual void destroy(TypeMatch tm, Storage s, X64 *x64) {
        // Assume source is initialized
        
        std::cerr << "Undestroyable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }

    // Allowed to clobber EQUAL_CLOB
    // Returns result in ZF (set iff equal)
    virtual void equal(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        std::cerr << "Uncomparable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }

    // Allowed to clobber COMPARE_CLOB
    // Returns result in R10B (-1/0/+1), and the flags (below&less/equal/above&greater)
    virtual void compare(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        std::cerr << "Uncomparable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }

    // NOTE: allowed to clobber STREAMIFY_CLOB, because it is mostly called
    // from interpolation, which is in Void context, so not much is lost. But
    // nested streamifications must take care!
    virtual void streamify(TypeMatch tm, bool alt, X64 *x64) {
        Label us_label = x64->runtime->data_heap_string(decode_utf8("<unstreamifiable>"));
        
        x64->op(LEA, R10, Address(us_label, 0));
        x64->op(PUSHQ, R10);
        x64->op(PUSHQ, Address(RSP, ADDRESS_SIZE));
        STRING_TS.streamify(true, x64);
        x64->op(ADDQ, RSP, 2 * ADDRESS_SIZE);
    }

    virtual void borrow(TypeMatch tm, Register reg, Unborrow *unborrow, X64 *x64) {
        std::cerr << "Unborrowable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }

    virtual Value *lookup_initializer(TypeMatch tm, std::string n, Scope *scope) {
        std::cerr << "No initializer " << name << " `" << n << "!\n";
        throw INTERNAL_ERROR;
    }

    virtual Value *lookup_matcher(TypeMatch tm, std::string n, Value *pivot, Scope *scope) {
        std::cerr << "No matcher " << name << " ~" << n << "!\n";
        throw INTERNAL_ERROR;
    }

    virtual Value *lookup_inner(TypeMatch tm, std::string n, Value *v, Scope *s) {
        Scope *scope = get_inner_scope();
        
        if (scope) {
            //std::cerr << "Type inner lookup in " << name << ".\n";
            return scope->lookup(n, v, s);
        }
        
        return NULL;
    }

    virtual devector<VirtualEntry *> get_virtual_table(TypeMatch tm) {
        throw INTERNAL_ERROR;
    }

    virtual Label get_virtual_table_label(TypeMatch tm, X64 *x64) {
        throw INTERNAL_ERROR;
    }

    virtual Label get_finalizer_label(TypeMatch tm, X64 *x64) {
        throw INTERNAL_ERROR;
    }
    
    virtual Value *autoconv(TypeMatch tm, Type *target, Value *orig, TypeSpec &ifts, bool assume_lvalue) {
        Scope *inner_scope = get_inner_scope();
        if (!inner_scope)
            return NULL;

        for (auto &d : inner_scope->contents) {
            Associable *imp = ptr_cast<Associable>(d.get());
            
            if (imp) {
                if (!imp->is_autoconv())
                    continue;
                    
                Value *v = imp->autoconv(tm, target, orig, ifts, assume_lvalue);
                
                if (v)
                    return v;
            }
        }
        
        return NULL;    
    }

    virtual void init_vt(TypeMatch tm, Address addr, Label vt_label, X64 *x64) {
        std::cerr << "Unvtinitable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }

    virtual void incref(TypeMatch tm, Register r, X64 *x64) {
        throw INTERNAL_ERROR;
    }

    virtual void decref(TypeMatch tm, Register r, X64 *x64) {
        throw INTERNAL_ERROR;
    }
    
    virtual bool complete_type() {
        return true;
    }
    
    virtual std::string get_fully_qualified_name() {
        return outer_scope->fully_qualify(name);
    }
};


class AnyType: public Type {
public:
    AnyType(std::string name, Metatypes param_metatypes, MetaType *mt)
        :Type(name, param_metatypes, mt) {
    }
};


class SameType: public Type {
public:
    SameType(std::string name, Metatypes param_metatypes, MetaType *mt)
        :Type(name, param_metatypes, mt) {
    }
    
    virtual StorageWhere where(TypeMatch tm, AsWhat as_what) {
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
    AttributeType(std::string n, MetaType *arg_metatype = value_metatype)
        :Type(n, Metatypes { arg_metatype }, attribute_metatype) {
    }

    virtual StorageWhere where(TypeMatch tm, AsWhat as_what) {
        if (as_what == AS_ARGUMENT && (this == lvalue_type || this == dvalue_type))
            as_what = AS_LVALUE_ARGUMENT;
            
        return tm[1].where(as_what);
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

    virtual void destroy(TypeMatch tm, Storage s, X64 *x64) {
        tm[1].destroy(s, x64);
    }

    virtual void equal(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        tm[1].equal(s, t, x64);
    }

    virtual void compare(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        tm[1].compare(s, t, x64);
    }

    virtual void streamify(TypeMatch tm, bool alt, X64 *x64) {
        tm[1].streamify(alt, x64);
    }

    virtual Value *lookup_initializer(TypeMatch tm, std::string n, Scope *s) {
        return tm[1].lookup_initializer(n, s);
    }

    virtual Value *lookup_matcher(TypeMatch tm, std::string n, Value *pivot, Scope *scope) {
        return tm[1].lookup_matcher(n, pivot, scope);
    }

    virtual devector<VirtualEntry *> get_virtual_table(TypeMatch tm) {
        return tm[1].get_virtual_table();
    }

    virtual Label get_virtual_table_label(TypeMatch tm, X64 *x64) {
        return tm[1].get_virtual_table_label(x64);
    }

    virtual Value *lookup_inner(TypeMatch tm, std::string n, Value *v, Scope *s) {
        return tm[1].lookup_inner(n, v, s);
    }
};


class PartialType: public Type {
public:
    PartialType(std::string name)
        :Type(name, Metatypes { type_metatype }, value_metatype) {
    }

    virtual StorageWhere where(TypeMatch tm, AsWhat as_what) {
        if (as_what == AS_ARGUMENT && tm[1].has_meta(record_metatype))
            as_what = AS_LVALUE_ARGUMENT;

        return tm[1].where(as_what);
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

    virtual Value *lookup_inner(TypeMatch tm, std::string n, Value *v, Scope *s) {
        std::cerr << "Partial inner lookup " << n << ".\n";
        bool dot = false;
        
        if (n[0] == '.') {
            dot = true;
            n = n.substr(1);
        }
        
        PartialInfo *pi = partial_variable_get_info(v);
        TypeSpec cast_ts = tm[1];
        // Use lvalue pivot cast for records so the members will be lvalues, too
        if (cast_ts.has_meta(record_metatype))
            cast_ts = cast_ts.lvalue();

        Value *cast_value = make<CastValue>(v, cast_ts);
        Value *member = value_lookup_inner(cast_value, n, s);

        if (!member) {
            // Consider initializer delegation before giving up
            std::cerr << "Partial not found, considering initializer delegation.\n";
            
            set_typespec(cast_value, tm[1].prefix(initializable_type));
            member = value_lookup_inner(cast_value, n, s);
            
            if (member) {
                if (pi->is_dirty()) {
                    std::cerr << "Can't delegate initialization of a dirty partial variable!\n";
                    return NULL;
                }
                
                pi->be_complete();
                return member;
            }
            
            std::cerr << "Partial member " << n << " not found!\n";
            return NULL;
        }
        
        if (pi->is_uninitialized(n)) {
            std::cerr << "Partial member " << n << " is uninitialized.\n";
            
            TypeSpec member_ts = get_typespec(member);
            //std::cerr << "Partial member " << n << " is " << member_ts << "\n";
            
            if (ptr_cast<RoleValue>(member)) {
                std::cerr << "Member role " << n << " is not yet initialized.\n";
                
                if (dot) {
                    // Accessed with the $.foo syntax, allow
                    set_typespec(member, member_ts.prefix(initializable_type));
                    pi->be_initialized(n);
                    return member;
                }
                else {
                    // Accessed with the $ foo syntax, reject
                    std::cerr << "Member role " << n << " is not yet initialized!\n";
                    return NULL;
                }
            }
            else {
                std::cerr << "Member variable " << n << " is not yet initialized.\n";
                set_typespec(member, member_ts.reprefix(lvalue_type, uninitialized_type));
                pi->be_initialized(n);
                return member;
            }
        }
        else if (pi->is_initialized(n)) {
            std::cerr << "Partial member " << n << " is initialized.\n";
            return member;
        }
        else {
            if (!pi->is_complete()) {
                std::cerr << "Partial member " << n << " is not yet accessible!\n";
                return NULL;
            }
            else {
                std::cerr << "Partial member " << n << " is accessible.\n";
                return member;
            }
        }
    }
};


class UninitializedType: public Type {
public:
    UninitializedType(std::string name)
        :Type(name, Metatypes { value_metatype }, type_metatype) {
    }
};


class InitializableType: public Type {
public:
    InitializableType(std::string name)
        :Type(name, Metatypes { type_metatype }, type_metatype) {
    }

    virtual StorageWhere where(TypeMatch tm, AsWhat as_what) {
        if (as_what == AS_ARGUMENT && tm[1].has_meta(record_metatype))
            as_what = AS_LVALUE_ARGUMENT;
            
        return tm[1].where(as_what);
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

    virtual Value *lookup_inner(TypeMatch tm, std::string n, Value *v, Scope *s) {
        return tm[1].lookup_inner(n, v, s);  // should look up role initializers
    }
};


class EqualitymatcherType: public Type {
public:
    // To be used only as type context in the :is control
    
    EqualitymatcherType(std::string name)
        :Type(name, Metatypes { value_metatype }, value_metatype) {
    }
    
    virtual Value *lookup_initializer(TypeMatch tm, std::string n, Scope *s) {
        // We've found an initializer where a match was expected
        
        if (n == "{}") {
            return make<BulkEqualityMatcherValue>();
        }
        else {
            Value *v = tm[1].lookup_initializer(n, s);
            
            if (!v)
                return NULL;
        
            // Any arguments for us are actually arguments for the initializer
            return make<InitializerEqualityMatcherValue>(v);
        }
    }
};


class MultiType: public Type {
public:
    MultiType(std::string name)
        :Type(name, {}, type_metatype) {
    }
};


class VoidType: public Type {
public:
    VoidType(std::string name)
        :Type(name, {}, value_metatype) {
    }

    virtual StorageWhere where(TypeMatch tm, AsWhat as_what) {
        return NOWHERE;
    }

    virtual void store(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        if (s.where != NOWHERE || t.where != NOWHERE)
            throw INTERNAL_ERROR;
    }
};


class UnitType: public Type {
public:
    UnitType(std::string name)
        :Type(name, {}, value_metatype) {
    }

    virtual Allocation measure(TypeMatch tm) {
        return Allocation();
    }

    virtual void store(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        // Moving 0 bits is easy
    }

    virtual void destroy(TypeMatch tm, Storage s, X64 *x64) {
    }

    virtual void streamify(TypeMatch tm, bool alt, X64 *x64) {
        Label us_label = x64->runtime->data_heap_string(decode_utf8("U"));
        
        x64->op(LEA, R10, Address(us_label, 0));
        x64->op(PUSHQ, R10);
        x64->op(PUSHQ, Address(RSP, ADDRESS_SIZE));
        STRING_TS.streamify(true, x64);
        x64->op(ADDQ, RSP, 2 * ADDRESS_SIZE);
    }
};


// Mostly for expressions that don't return, but Whatever Uninitialized is also
// used for bare declarations.
class WhateverType: public Type {
public:
    WhateverType(std::string name)
        :Type(name, {}, value_metatype) {
    }

    virtual StorageWhere where(TypeMatch tm, AsWhat as_what) {
        return NOWHERE;
    }

    virtual Allocation measure(TypeMatch tm) {
        throw INTERNAL_ERROR;
    }

    virtual void store(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        // STACK storage is faked by Whatever controls
        if ((s.where != NOWHERE && s.where != STACK) || t.where != NOWHERE) {
            std::cerr << "Invalid Whatever store from " << s << " to " << t << "!\n";
            throw INTERNAL_ERROR;
        }
    }

    virtual void destroy(TypeMatch tm, Storage s, X64 *x64) {
        throw INTERNAL_ERROR;
    }
};


class StringtemplateType: public Type {
public:
    StringtemplateType(std::string name)
        :Type(name, {}, value_metatype) {
    }
};

