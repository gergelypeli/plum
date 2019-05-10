
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
            if (!value_metatype || !identity_metatype || !any_type || !anyid_type || !anytuple_type)
                throw INTERNAL_ERROR;  // sanity check for initialization
        
            Type *any[] = { any_type, any2_type, any3_type };
            Type *anyid[] = { anyid_type, anyid2_type, anyid3_type };
            Type *anytuple[] = { anytuple_type, anytuple2_type, anytuple3_type };
        
            for (unsigned i = 0; i < param_metatypes.size(); i++) {
                MetaType *mt = param_metatypes[i];
            
                if (mt == value_metatype)
                    ts.push_back(any[i]);
                else if (mt == identity_metatype)
                    ts.push_back(anyid[i]);
                else if (mt == tuple_metatype)
                    ts.push_back(anytuple[i]);
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

    virtual Value *matched(Value *pivot, Scope *scope, TypeMatch &match) {
        return make<TypeValue>(this, meta_type, param_metatypes);
    }

    virtual TypeSpec get_pivot_ts() {
        return NO_TS;
    }

    virtual Value *match(std::string name, Value *pivot, Scope *scope) {
        //std::cerr << "Matching " << name << " to type " << this->name << "\n";

        Value *v = Identifier::match(name, pivot, scope);
        if (v)
            return v;
        
        if (deprefix(name, prefix)) {
            //std::cerr << "Entering explicit scope " << prefix << "\n";
                
            Scope *s = get_inner_scope();
            
            if (s)
                return s->lookup(name, pivot, scope);
        }
                
        return NULL;
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

    virtual Storage optimal_value_storage(TypeMatch tm, Regs preferred) {
        return Storage(STACK);
    }

    virtual void store(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        // Assume MEMORY are initialized
        
        switch (s.where * t.where) {
        case NOWHERE_NOWHERE:
            return;
    
        case STACK_STACK:
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
    
    virtual Value *autoconv_scope(Scope *scope, TypeMatch tm, Type *target, Value *orig, TypeSpec &ifts) {
        for (auto &d : scope->contents) {
            Associable *imp = ptr_cast<Associable>(d.get());
            
            if (imp) {
                if (!imp->is_autoconv()) {
                    std::cerr << "Not considering role " << imp->name << "\n";
                    continue;
                }
                
                std::cerr << "Considering role " << imp->name << "\n";
                Value *v = imp->autoconv(tm, target, orig, ifts);
                
                if (v)
                    return v;
                    
                std::cerr << "Nope.\n";
            }
        }
        
        return NULL;
    }
    
    virtual Value *autoconv(TypeMatch tm, Type *target, Value *orig, TypeSpec &ifts) {
        Scope *inner_scope = get_inner_scope();
        if (!inner_scope)
            return NULL;

        Value *v = autoconv_scope(inner_scope, tm, target, orig, ifts);
        if (v)
            return v;

        // FIXME: this must only be checked for lvalue originals            
        Scope *lvalue_scope = get_lvalue_scope();
        if (!lvalue_scope)
            return NULL;

        v = autoconv_scope(lvalue_scope, tm, target, orig, ifts);
        if (v)
            return v;
        
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

    virtual void debug_inner_scopes(TypeMatch tm, X64 *x64) {
        // For generating the Dwarf info for the contents
        
        if (initializer_scope)
            initializer_scope->debug(tm, x64);
            
        if (inner_scope)
            inner_scope->debug(tm, x64);
            
        if (lvalue_scope)
            lvalue_scope->debug(tm, x64);
    }
    
    virtual void type_info(TypeMatch tm, X64 *x64) {
        // For generating the type info for a concrete type
        std::cerr << "Untypeinfoable type: " << name << "!\n";
        throw INTERNAL_ERROR;
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


class TupleMetaType: public MetaType {
public:
    TupleMetaType(std::string n, std::vector<MetaType *> sts, TypeDefinitionFactory f)
        :MetaType(n, sts, f) {
    }

    virtual TypeSpec make_pivot_ts() {
        // We have unpacking in the tuple metascope, but cannot describe the type of
        // an arbitrary long tuple, so hack this in.
        return NO_TS;
    }
    
    virtual Value *lookup_initializer(TypeMatch tm, std::string n, Scope *s) {
        // Tuple types are not created by naming the tuple metatype (it has no name),
        // but by invoking the anonymous initializer.
        
        if (n == "{")
            return make<TupleTypeValue>();
            
        std::cerr << "No named initializer for creating tuple types!\n";
        return NULL;
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
        :Type(n, Metatypes { arg_metatype }, argument_metatype) {
    }

    virtual StorageWhere where(TypeMatch tm, AsWhat as_what) {
        if (as_what == AS_ARGUMENT && this == lvalue_type)
            as_what = AS_LVALUE_ARGUMENT;

        return tm[1].where(as_what);
    }

    virtual Storage optimal_value_storage(TypeMatch tm, Regs preferred) {
        return tm[1].optimal_value_storage(preferred);
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

    virtual void type_info(TypeMatch tm, X64 *x64) {
        unsigned ts_index = x64->once->type_info(tm[1]);
        x64->dwarf->typedef_info(tm[0].symbolize(), ts_index);
    }
};


class DvalueType: public Type {
public:
    DvalueType(std::string n)
        :Type(n, Metatypes { tuple_metatype }, argument_metatype) {
    }

    virtual StorageWhere where(TypeMatch tm, AsWhat as_what) {
        // We use ALIAS storage even if it may point to another ALIAS in the future.
        // This is because Dvalue arguments always point to the stack, and needs fixing on
        // stack relocation, which is only done for ALIAS storage arguments.
        return (
            as_what == AS_ARGUMENT ? ALIAS :
            throw INTERNAL_ERROR
        );
    }

    virtual Allocation measure(TypeMatch tm) {
        return ALIAS_SIZE;
    }

    virtual void type_info(TypeMatch tm, X64 *x64) {
        x64->dwarf->unspecified_type_info(tm[0].symbolize());
    }
};


class CodeType: public Type {
public:
    CodeType(std::string n)
        :Type(n, Metatypes { tuple_metatype }, argument_metatype) {
    }

    virtual StorageWhere where(TypeMatch tm, AsWhat as_what) {
        return (
            as_what == AS_ARGUMENT ? MEMORY :
            throw INTERNAL_ERROR
        );
    }

    virtual Allocation measure(TypeMatch tm) {
        return ADDRESS_SIZE;
    }

    virtual void store(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        switch (s.where * t.where) {
        case STACK_NOWHERE:
            x64->op(ADDQ, RSP, ADDRESS_SIZE);
            break;
        case STACK_STACK:
            break;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual void type_info(TypeMatch tm, X64 *x64) {
        x64->dwarf->unspecified_type_info(tm[0].symbolize());
    }
};


class TupleType: public Type {
public:
    typedef std::vector<std::string> Keywords;
    
    static std::map<Keywords, std::unique_ptr<TupleType>> cache;
    
    static TupleType *get(Keywords kws) {
        std::unique_ptr<TupleType> &t = cache[kws];
        
        if (!t) {
            t.reset(new TupleType(kws));
            t->make_inner_scope()->leave();
        }
            
        return t.get();
    }
    
    Keywords keywords;

    TupleType(std::vector<std::string> kws)
        :Type("(" + std::to_string(kws.size()) + ")", Metatypes(kws.size(), argument_metatype), tuple_metatype) {
        keywords = kws;
    }

    virtual TypeSpec make_pivot_ts() {
        // We want to have an inner scope, but only to have a pointer to the metascope
        // than contains the unpacking operator.
        return NO_TS;
    }

    virtual StorageWhere where(TypeMatch tm, AsWhat as_what) {
        return (
            as_what == AS_VALUE ? STACK :
            as_what == AS_ARGUMENT ? MEMORY :
            throw INTERNAL_ERROR
        );
    }

    virtual Allocation measure(TypeMatch tm) {
        TSs tss;
        tm[0].unpack_tuple(tss);
        
        unsigned size = 0;
        
        for (auto &ts : tss)
            size += ts.measure_stack();
            
        return Allocation(size);
    }

    virtual void store(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        if (t.where == STACK) {
            if (s.where == STACK)
                return;
            else if (this == tuple0_type)
                return;
            else if (this == tuple1_type) {
                tm[1].store(s, t, x64);
                return;
            }
        }
        
        throw INTERNAL_ERROR;
    }
    
    virtual void create(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        switch (s.where * t.where) {
        case STACK_MEMORY: {
            // Used in :evaluate
            int size = measure(tm).bytes;
            int offset = 0;
            
            while (offset < size) {
                x64->op(POPQ, t.address + offset);
                offset += ADDRESS_SIZE;
            }
        }
            break;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual void destroy(TypeMatch tm, Storage s, X64 *x64) {
        switch (s.where) {
        case MEMORY: {
            // Used in :evaluate
            TSs tss;
            tm[0].unpack_tuple(tss);
            int offset = 0;

            for (auto &ts : backward<std::vector<TypeSpec>>(tss)) {
                ts.destroy(Storage(MEMORY, s.address + offset), x64);
                offset += ts.measure_stack();
            }
        }
            break;
        default:
            throw INTERNAL_ERROR;
        }
    }
};

std::map<TupleType::Keywords, std::unique_ptr<TupleType>> TupleType::cache;


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

    virtual void type_info(TypeMatch tm, X64 *x64) {
        unsigned ts_index = x64->once->type_info(tm[1]);
        x64->dwarf->typedef_info(tm[0].symbolize(), ts_index);
    }
};


class UninitializedType: public Type {
public:
    UninitializedType(std::string name)
        :Type(name, Metatypes { value_metatype }, argument_metatype) {
    }

    virtual StorageWhere where(TypeMatch tm, AsWhat as_what) {
        // This method is needed for probing if rvalue hinting is applicable
        if (as_what == AS_ARGUMENT)
            as_what = AS_LVALUE_ARGUMENT;
            
        return tm[1].where(as_what);
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


class MultiType: public Type {
public:
    MultiType(std::string name)
        :Type(name, {}, type_metatype) {
    }

    virtual StorageWhere where(TypeMatch tm, AsWhat as_what) {
        // The AS_ARGUMENT case is only used for unalias hinting, so we just
        // need not to crash here.
        
        return (
            as_what == AS_VALUE ? STACK :
            as_what == AS_ARGUMENT ? NOWHERE :
            throw INTERNAL_ERROR
        );
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

    virtual void type_info(TypeMatch tm, X64 *x64) {
        x64->dwarf->unspecified_type_info(name);
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

    virtual void type_info(TypeMatch tm, X64 *x64) {
        x64->dwarf->unspecified_type_info(name);
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


// For expressions that don't return
class WhateverType: public Type {
public:
    WhateverType(std::string name)
        :Type(name, {}, argument_metatype) {
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

    virtual void type_info(TypeMatch tm, X64 *x64) {
        x64->dwarf->unspecified_type_info(name);
    }
};


class StringtemplateType: public Type {
public:
    StringtemplateType(std::string name)
        :Type(name, {}, value_metatype) {
    }
};


// A dummy type, only to be used as Bare Uninitialized for bare declarations,
// whose type will be derived from the right hand side of their initialization.
class BareType: public Type {
public:
    BareType(std::string name)
        :Type(name, {}, value_metatype) {
    }
};
