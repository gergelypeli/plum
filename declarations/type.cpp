
#define EQUAL_CLOB Regs()
#define COMPARE_CLOB Regs()
#define STREAMIFY_CLOB Regs::all()

typedef std::vector<MetaType *> Metatypes;

class Type: public Identifier {
public:
    std::string prefix;
    Metatypes param_metatypes;
    MetaType *meta_type;
    
    std::unique_ptr<DataScope> initializer_scope;
    std::unique_ptr<DataScope> inner_scope;  // Won't be visible from the outer scope
    std::unique_ptr<DataScope> lvalue_scope;
    
    Type(std::string n, Metatypes pmts, MetaType *ut)
        :Identifier(n) {
        if (pmts.size() > 3)
            throw INTERNAL_ERROR;  // current limitation
            
        prefix = n + QUALIFIER_NAME;
        param_metatypes = pmts;
        meta_type = ut;
    }
    
    virtual unsigned get_parameter_count() {
        return param_metatypes.size();
    }
    
    virtual void set_outer_scope(Scope *os) {
        // This slightly abuses the Scope structure, as the inner scope references directly
        // the outer scope, but that's fine here. But Type is not a proper Scope.
        
        Declaration::set_outer_scope(os);

        if (initializer_scope)
            initializer_scope->set_outer_scope(os);
            
        if (inner_scope)
            inner_scope->set_outer_scope(os);
            
        if (lvalue_scope)
            lvalue_scope->set_outer_scope(os);
    }

    virtual TypeSpec make_pivot_ts() {
        TypeSpec ts = { this };
        
        if (param_metatypes.size()) {
            if (!value_metatype || !identity_metatype || !any_type || !anyid_type)
                throw INTERNAL_ERROR;  // sanity check for initialization
        
            Type *any[] = { any_type, any2_type, any3_type };
            Type *anyid[] = { anyid_type, anyid2_type, anyid3_type };
        
            for (unsigned i = 0; i < param_metatypes.size(); i++) {
                MetaType *mt = param_metatypes[i];
            
                if (mt == value_metatype)
                    ts.push_back(any[i]);
                else if (mt == identity_metatype)
                    ts.push_back(anyid[i]);
                else
                    throw INTERNAL_ERROR;
            }
        }
        
        return ts;
    }

    virtual DataScope *make_inner_scope() {
        // This method must be called explicitly after the constructor, because it
        // may be customized by subclasses
        
        if (inner_scope)
            throw INTERNAL_ERROR;
        
        inner_scope.reset(new DataScope);
        inner_scope->set_name(name);
        
        TypeSpec pivot_ts = make_pivot_ts();
        if (pivot_ts != NO_TS)
            inner_scope->set_pivot_ts(pivot_ts);
        
        Scope *meta_scope = ptr_cast<Type>(meta_type)->get_inner_scope();
        if (meta_scope)
            inner_scope->set_meta_scope(meta_scope);

        if (outer_scope)
            inner_scope->set_outer_scope(outer_scope);

        inner_scope->enter();
        
        return inner_scope.get();
    }
    
    virtual DataScope *make_initializer_scope() {
        if (!inner_scope || initializer_scope)
            throw INTERNAL_ERROR;
        
        initializer_scope.reset(new DataScope);
        initializer_scope->set_name(name);

        TypeSpec pivot_ts = make_pivot_ts();
        if (pivot_ts != NO_TS)
            initializer_scope->set_pivot_ts(pivot_ts.prefix(initializable_type));

        if (outer_scope)
            initializer_scope->set_outer_scope(outer_scope);

        initializer_scope->enter();

        return initializer_scope.get();
    }
    
    virtual DataScope *make_lvalue_scope() {
        if (!inner_scope || lvalue_scope)
            throw INTERNAL_ERROR;
        
        lvalue_scope.reset(new DataScope);
        lvalue_scope->set_name(name);

        TypeSpec pivot_ts = make_pivot_ts();
        if (pivot_ts != NO_TS)
            lvalue_scope->set_pivot_ts(pivot_ts.prefix(lvalue_type));

        if (outer_scope)
            lvalue_scope->set_outer_scope(outer_scope);

        inner_scope->push_scope(lvalue_scope.get());

        lvalue_scope->enter();

        return lvalue_scope.get();
    }
    
    virtual void transplant_initializers(std::vector<Declaration *> inits) {
        initializer_scope->enter();
    
        for (auto d : inits) {
            inner_scope->remove_internal(d);
            initializer_scope->add(d);
        }

        initializer_scope->leave();
    }

    virtual void transplant_procedures(std::vector<Declaration *> procs) {
        lvalue_scope->enter();
    
        for (auto d : procs) {
            inner_scope->remove_internal(d);
            lvalue_scope->add(d);
        }

        lvalue_scope->leave();
    }

    virtual DataScope *get_initializer_scope() {
        return initializer_scope.get();
    }

    virtual DataScope *get_inner_scope() {
        return inner_scope.get();
    }

    virtual DataScope *get_lvalue_scope() {
        return lvalue_scope.get();
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
        if (initializer_scope)
            initializer_scope->allocate();
            
        if (inner_scope)
            inner_scope->allocate();
            
        if (lvalue_scope)
            lvalue_scope->allocate();
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
        std::cerr << "Unmeasurableidentity type: " << name << "!\n";
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
            
        case ALISTACK_NOWHERE:
            x64->op(ADDQ, RSP, ALIAS_SIZE);
            return;
            
        case ALIAS_NOWHERE:
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
    virtual void streamify(TypeMatch tm, X64 *x64) {
        Address alias_addr(RSP, 0);
        
        streamify_ascii("<unstreamifiable>", alias_addr, x64);
    }

    virtual Value *lookup_initializer(TypeMatch tm, std::string n, Scope *scope) {
        std::cerr << "No initializer " << name << " `" << n << "!\n";
        throw INTERNAL_ERROR;
    }

    virtual Value *lookup_matcher(TypeMatch tm, std::string n, Value *pivot, Scope *scope) {
        if (n == "{")
            return make<BulkEqualityMatcherValue>(pivot);

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

    virtual Label get_interface_table_label(TypeMatch tm, X64 *x64) {
        throw INTERNAL_ERROR;
    }

    virtual Label get_finalizer_label(TypeMatch tm, X64 *x64) {
        throw INTERNAL_ERROR;
    }
    
    virtual Value *autoconv(TypeMatch tm, Type *target, Value *orig, TypeSpec &ifts) {
        Scope *inner_scope = get_inner_scope();
        if (!inner_scope)
            return NULL;

        for (auto &d : inner_scope->contents) {
            Associable *imp = ptr_cast<Associable>(d.get());
            
            if (imp) {
                if (!imp->is_autoconv())
                    continue;
                    
                Value *v = imp->autoconv(tm, target, orig, ifts);
                
                if (v)
                    return v;
            }
        }
        
        return NULL;    
    }

    virtual void init_vt(TypeMatch tm, Address addr, X64 *x64) {
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
};


class MetaType: public Type {
public:
    // Plain Type-s get their parameters in the form of pre-evaluated type names, and
    // produce variables by declaring them to those types.
    // MetaTypes get their parameters in the form of keyword arguments to be evaluated
    // later, and produce types by declaring them to the resulting type.

    std::vector<MetaType *> super_types;
    typedef Value *(*TypeDefinitionFactory)();
    TypeDefinitionFactory factory;
    
    MetaType(std::string n, std::vector<MetaType *> sts, TypeDefinitionFactory f)
        :Type(n, Metatypes { }, metatype_hypertype) {
        // the metatype of metatype_hypertype is NULL, because it's not set when instantiated
        super_types = sts;
        factory = f;
    }

    virtual TypeSpec make_pivot_ts() {
        // Some metatypes contain useful operations
        if (super_types.size() == 1 && super_types[0] == value_metatype && value_metatype)
            return ANY_TS;
            
        throw INTERNAL_ERROR;  // to catch unusual cases
    }

    virtual Value *match(std::string name, Value *pivot, Scope *scope) {
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

    virtual bool is_typedefinition(std::string n) {
        return name == n;
    }
    
    virtual bool has_super(MetaType *mt) {
        if (mt == this)
            return true;
            
        for (auto st : super_types)
            if (st->has_super(mt))
                return true;
                
        return false;
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

    virtual void streamify(TypeMatch tm, X64 *x64) {
        tm[1].streamify(x64);
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
        
        // Base initializers are looked up using "foo.", potentially "."
        bool dot = false;
        
        if (n[n.size() - 1] == '.') {
            // Chop trailing dot, but remember it
            dot = true;
            n = n.substr(0, n.size() - 1);
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
            if (!dot) {
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
            }
            
            std::cerr << "Partial member " << n << " not found!\n";
            return NULL;
        }
        
        if (pi->is_uninitialized(n)) {
            //std::cerr << "Partial member " << n << " is uninitialized.\n";
            
            TypeSpec member_ts = get_typespec(member);
            //std::cerr << "Partial member " << n << " is " << member_ts << "\n";
            
            if (ptr_cast<RoleValue>(member)) {
                std::cerr << "Member role " << n << " is not yet initialized.\n";
                
                if (!dot) {
                    // Accessed with the $ foo syntax, reject
                    std::cerr << "Member role " << n << " is not yet initialized!\n";
                    return NULL;
                }

                // Accessed with the $ foo. syntax, allow
                set_typespec(member, member_ts.prefix(initializable_type));
                pi->be_initialized(n);
                return member;
            }
            else {
                if (dot) {
                    std::cerr << "Member variable " << n << " has no base initializer!\n";
                    return NULL;
                }
                
                std::cerr << "Member variable " << n << " is not yet initialized.\n";
                set_typespec(member, member_ts.reprefix(lvalue_type, uninitialized_type));
                pi->be_initialized(n);
                return member;
            }
        }
        else if (pi->is_initialized(n)) {
            if (dot) {
                std::cerr << "Member " << n << " needs no base initializer!\n";
                return NULL;
            }
            
            std::cerr << "Partial member " << n << " is initialized.\n";
            return member;
        }
        else {
            if (!pi->is_complete()) {
                std::cerr << "Partial member " << n << " is not yet accessible!\n";
                return NULL;
            }
            
            std::cerr << "Partial member " << n << " is accessible.\n";
            return member;
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

/*
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
*/

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

    virtual StorageWhere where(TypeMatch tm, AsWhat as_what) {
        return NOWHERE;
    }

    virtual Allocation measure(TypeMatch tm) {
        return Allocation();
    }

    virtual void store(TypeMatch tm, Storage s, Storage t, X64 *x64) {
    }

    virtual void create(TypeMatch tm, Storage s, Storage t, X64 *x64) {
    }

    virtual void destroy(TypeMatch tm, Storage s, X64 *x64) {
    }

    virtual void streamify(TypeMatch tm, X64 *x64) {
        Address alias_addr(RSP, 0);

        streamify_ascii("U", alias_addr, x64);
    }
};


class ColonType: public UnitType {
public:
    ColonType(std::string name)
        :UnitType(name) {
    }
    
    virtual TypeSpec make_pivot_ts() {
        return NO_TS;
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

