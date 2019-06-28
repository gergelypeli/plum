#include "../plum.h"


Type::Type(std::string n, Metatypes pmts, MetaType *ut)
    :Identifier(n, NO_PIVOT) {
    if (pmts.size() > 3)
        throw INTERNAL_ERROR;  // current limitation
        
    prefix = n + QUALIFIER_NAME;
    param_metatypes = pmts;
    meta_type = ut;
}

unsigned Type::get_parameter_count() {
    return param_metatypes.size();
}

void Type::set_outer_scope(Scope *os) {
    // This slightly abuses the Scope structure, as the inner scope references directly
    // the outer scope, but that's fine here. But Type is not a proper Scope.
    
    Declaration::set_outer_scope(os);

    if (inner_scope)
        inner_scope->set_outer_scope(os);
}

TypeSpec Type::make_pivot_ts() {
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

DataScope *Type::make_inner_scope() {
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

DataScope *Type::get_inner_scope() {
    return inner_scope.get();
}

void Type::outer_scope_entered() {
    if (inner_scope)
        inner_scope->outer_scope_entered();
}

void Type::outer_scope_left() {
    if (inner_scope)
        inner_scope->outer_scope_left();
}

Value *Type::matched(Value *pivot, Scope *scope, TypeMatch &match) {
    return make<TypeValue>(this, meta_type, param_metatypes);
}

Value *Type::match(std::string name, Value *pivot, Scope *scope) {
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

void Type::allocate() {
    if (inner_scope)
        inner_scope->allocate();
}

StorageWhere Type::where(TypeMatch tm, AsWhat as_what) {
    std::cerr << "Nowhere type: " << name << "!\n";
    throw INTERNAL_ERROR;
}

Allocation Type::measure(TypeMatch tm) {
    std::cerr << "Unmeasurable type: " << name << "!\n";
    throw INTERNAL_ERROR;
}

Allocation Type::measure_identity(TypeMatch tm) {
    std::cerr << "Unmeasurableidentity type: " << name << "!\n";
    throw INTERNAL_ERROR;
}

Storage Type::optimal_value_storage(TypeMatch tm, Regs preferred) {
    return Storage(STACK);
}

void Type::store(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    // Assume MEMORY are initialized
    
    switch (s.where * t.where) {
    case NOWHERE_NOWHERE:
        return;

    case STACK_STACK:
        return;
        
    case MEMORY_NOWHERE:
        return;
    case MEMORY_ALISTACK:
        cx->op(LEA, R10, s.address);
        cx->op(PUSHQ, 0);
        cx->op(PUSHQ, R10);
        return;
        
    // ALIAS to ALISTACK is not implemented here, because that may push a stack relative
    // address onto the stack, which is normally disallowed, only the function call may
    // do it, because it knows how to do that safely.
        
    case ALISTACK_NOWHERE: {
        Label skip;  // TODO: this whole thing must be optimized
        cx->op(POPQ, R10);
        cx->op(POPQ, R10);
        cx->op(CMPQ, R10, 0);
        cx->op(JE, skip);
        cx->runtime->decref(R10);
        cx->code_label(skip);
    }
        return;
        
    case ALIAS_NOWHERE:
        return;

    default:
        std::cerr << "Unstorable type: " << name << " from " << s << " to " << t << "!\n";
        throw INTERNAL_ERROR;
    }
}

void Type::create(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    // Assume target is uninitialized
    
    std::cerr << "Uncreatable type: " << name << "!\n";
    throw INTERNAL_ERROR;
}

void Type::destroy(TypeMatch tm, Storage s, Cx *cx) {
    // Assume source is initialized
    
    std::cerr << "Undestroyable type: " << name << "!\n";
    throw INTERNAL_ERROR;
}

// Allowed to clobber EQUAL_CLOB
// Returns result in ZF (set iff equal)
void Type::equal(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    std::cerr << "Uncomparable type: " << name << "!\n";
    throw INTERNAL_ERROR;
}

// Allowed to clobber COMPARE_CLOB
// Returns result in R10B (-1/0/+1), and the flags (below&less/equal/above&greater)
void Type::compare(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    std::cerr << "Uncomparable type: " << name << "!\n";
    throw INTERNAL_ERROR;
}

// NOTE: allowed to clobber STREAMIFY_CLOB, because it is mostly called
// from interpolation, which is in Void context, so not much is lost. But
// nested streamifications must take care!
void Type::streamify(TypeMatch tm, Cx *cx) {
    Address alias_addr(RSP, 0);
    
    streamify_ascii("<unstreamifiable>", alias_addr, cx);
}

Value *Type::lookup_initializer(TypeMatch tm, std::string n, Scope *scope) {
    std::cerr << "No initializer " << name << " `" << n << "!\n";
    throw INTERNAL_ERROR;
}

Value *Type::lookup_matcher(TypeMatch tm, std::string n, Value *pivot, Scope *scope) {
    if (n == "{")
        return make<BulkEqualityMatcherValue>(pivot);

    std::cerr << "No matcher " << name << " ~" << n << "!\n";
    throw INTERNAL_ERROR;
}

Value *Type::lookup_inner(TypeMatch tm, std::string n, Value *v, Scope *s) {
    Scope *scope = get_inner_scope();
    
    if (scope) {
        //std::cerr << "Type inner lookup in " << name << ".\n";
        return scope->lookup(n, v, s);
    }
    
    return NULL;
}

devector<VirtualEntry *> Type::get_virtual_table(TypeMatch tm) {
    throw INTERNAL_ERROR;
}

Label Type::get_virtual_table_label(TypeMatch tm, Cx *cx) {
    throw INTERNAL_ERROR;
}

Label Type::get_interface_table_label(TypeMatch tm, Cx *cx) {
    throw INTERNAL_ERROR;
}

Label Type::get_finalizer_label(TypeMatch tm, Cx *cx) {
    throw INTERNAL_ERROR;
}

Value *Type::autoconv_scope(Scope *scope, TypeMatch tm, Type *target, Value *orig, TypeSpec &ifts) {
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

Value *Type::autoconv(TypeMatch tm, Type *target, Value *orig, TypeSpec &ifts) {
    Scope *inner_scope = get_inner_scope();
    if (!inner_scope)
        return NULL;

    Value *v = autoconv_scope(inner_scope, tm, target, orig, ifts);
    if (v)
        return v;

    return NULL;
}

void Type::init_vt(TypeMatch tm, Address addr, Cx *cx) {
    std::cerr << "Unvtinitable type: " << name << "!\n";
    throw INTERNAL_ERROR;
}

void Type::incref(TypeMatch tm, Register r, Cx *cx) {
    throw INTERNAL_ERROR;
}

void Type::decref(TypeMatch tm, Register r, Cx *cx) {
    throw INTERNAL_ERROR;
}

bool Type::complete_type() {
    return true;
}

void Type::debug_inner_scopes(TypeMatch tm, Cx *cx) {
    // For generating the Dwarf info for the contents
    
    if (inner_scope)
        inner_scope->debug(tm, cx);
}

void Type::type_info(TypeMatch tm, Cx *cx) {
    // For generating the type info for a concrete type
    std::cerr << "Untypeinfoable type: " << name << "!\n";
    throw INTERNAL_ERROR;
}


// Plain Type-s get their parameters in the form of pre-evaluated type names, and
// produce variables by declaring them to those types.
// MetaTypes get their parameters in the form of keyword arguments to be evaluated
// later, and produce types by declaring them to the resulting type.


MetaType::MetaType(std::string n, std::vector<MetaType *> sts, TypeDefinitionFactory f)
    :Type(n, Metatypes { }, metatype_hypertype) {
    // the metatype of metatype_hypertype is NULL, because it's not set when instantiated
    super_types = sts;
    factory = f;
}

TypeSpec MetaType::make_pivot_ts() {
    // Some metatypes contain useful operations
    if (super_types.size() == 1 && super_types[0] == value_metatype && value_metatype)
        return ANY_TS;
        
    throw INTERNAL_ERROR;  // to catch unusual cases
}

Value *MetaType::match(std::string name, Value *pivot, Scope *scope) {
    if (name != this->name)
        return NULL;
        
    if (pivot)
        return NULL;
        
    if (!factory)
        throw INTERNAL_ERROR;
        
    return factory();
}

void MetaType::store(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    if (s.where != NOWHERE || t.where != NOWHERE) {
        std::cerr << "Invalid metatype store from " << s << " to " << t << "!\n";
        throw INTERNAL_ERROR;
    }
}

bool MetaType::is_typedefinition(std::string n) {
    return name == n;
}

bool MetaType::has_super(MetaType *mt) {
    if (mt == this)
        return true;
        
    for (auto st : super_types)
        if (st->has_super(mt))
            return true;
            
    return false;
}


TupleMetaType::TupleMetaType(std::string n, std::vector<MetaType *> sts, TypeDefinitionFactory f)
    :MetaType(n, sts, f) {
}

TypeSpec TupleMetaType::make_pivot_ts() {
    // We have unpacking in the tuple metascope, but cannot describe the type of
    // an arbitrary long tuple, so hack this in.
    return NO_TS;
}

Value *TupleMetaType::lookup_initializer(TypeMatch tm, std::string n, Scope *s) {
    // Tuple types are not created by naming the tuple metatype (it has no name),
    // but by invoking the anonymous initializer.
    
    if (n == "{")
        return make<TupleTypeValue>();
        
    std::cerr << "No named initializer for creating tuple types!\n";
    return NULL;
}


AnyType::AnyType(std::string name, Metatypes param_metatypes, MetaType *mt)
    :Type(name, param_metatypes, mt) {
}


SameType::SameType(std::string name, Metatypes param_metatypes, MetaType *mt)
    :Type(name, param_metatypes, mt) {
}

StorageWhere SameType::where(TypeMatch tm, AsWhat as_what) {
    if (as_what == AS_VARIABLE)
        return MEMORY;  // TODO: all types must be MEMORY for this combination!
    else
        throw INTERNAL_ERROR;
}

Allocation SameType::measure(TypeMatch tm) {
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

void SameType::store(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    if (s.where != NOWHERE || t.where != NOWHERE) {
        std::cerr << "Invalid Same store from " << s << " to " << t << "!\n";
        throw INTERNAL_ERROR;
    }
}


AttributeType::AttributeType(std::string n, MetaType *arg_metatype)
    :Type(n, Metatypes { arg_metatype }, argument_metatype) {
}

StorageWhere AttributeType::where(TypeMatch tm, AsWhat as_what) {
    if (as_what == AS_ARGUMENT && this == lvalue_type)
        as_what = AS_LVALUE_ARGUMENT;

    return tm[1].where(as_what);
}

Storage AttributeType::optimal_value_storage(TypeMatch tm, Regs preferred) {
    return tm[1].optimal_value_storage(preferred);
}

Allocation AttributeType::measure(TypeMatch tm) {
    return tm[1].measure();
}

void AttributeType::store(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    tm[1].store(s, t, cx);
}

void AttributeType::create(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    tm[1].create(s, t, cx);
}

void AttributeType::destroy(TypeMatch tm, Storage s, Cx *cx) {
    tm[1].destroy(s, cx);
}

void AttributeType::equal(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    tm[1].equal(s, t, cx);
}

void AttributeType::compare(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    tm[1].compare(s, t, cx);
}

void AttributeType::streamify(TypeMatch tm, Cx *cx) {
    tm[1].streamify(cx);
}

Value *AttributeType::lookup_initializer(TypeMatch tm, std::string n, Scope *s) {
    return tm[1].lookup_initializer(n, s);
}

Value *AttributeType::lookup_matcher(TypeMatch tm, std::string n, Value *pivot, Scope *scope) {
    return tm[1].lookup_matcher(n, pivot, scope);
}

devector<VirtualEntry *> AttributeType::get_virtual_table(TypeMatch tm) {
    return tm[1].get_virtual_table();
}

Label AttributeType::get_virtual_table_label(TypeMatch tm, Cx *cx) {
    return tm[1].get_virtual_table_label(cx);
}

Value *AttributeType::lookup_inner(TypeMatch tm, std::string n, Value *v, Scope *s) {
    return tm[1].lookup_inner(n, v, s);
}

void AttributeType::type_info(TypeMatch tm, Cx *cx) {
    unsigned ts_index = cx->once->type_info(tm[1]);
    cx->dwarf->typedef_info(tm[0].symbolize(), ts_index);
}


DvalueType::DvalueType(std::string n)
    :Type(n, Metatypes { tuple_metatype }, argument_metatype) {
}

StorageWhere DvalueType::where(TypeMatch tm, AsWhat as_what) {
    // We use ALIAS storage even if it may point to another ALIAS in the future.
    // This is because Dvalue arguments always point to the stack, and needs fixing on
    // stack relocation, which is only done for ALIAS storage arguments.
    return (
        as_what == AS_ARGUMENT ? ALIAS :
        throw INTERNAL_ERROR
    );
}

Allocation DvalueType::measure(TypeMatch tm) {
    return ALIAS_SIZE;
}

void DvalueType::type_info(TypeMatch tm, Cx *cx) {
    cx->dwarf->unspecified_type_info(tm[0].symbolize());
}


CodeType::CodeType(std::string n)
    :Type(n, Metatypes { tuple_metatype }, argument_metatype) {
}

StorageWhere CodeType::where(TypeMatch tm, AsWhat as_what) {
    return (
        as_what == AS_ARGUMENT ? MEMORY :
        throw INTERNAL_ERROR
    );
}

Allocation CodeType::measure(TypeMatch tm) {
    return ADDRESS_SIZE;
}

void CodeType::store(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    switch (s.where * t.where) {
    case STACK_NOWHERE:
        cx->op(ADDQ, RSP, ADDRESS_SIZE);
        break;
    case STACK_STACK:
        break;
    default:
        throw INTERNAL_ERROR;
    }
}

void CodeType::type_info(TypeMatch tm, Cx *cx) {
    cx->dwarf->unspecified_type_info(tm[0].symbolize());
}




std::map<TupleType::Keywords, std::unique_ptr<TupleType>> TupleType::cache;


TupleType *TupleType::get(Keywords kws) {
    std::unique_ptr<TupleType> &t = cache[kws];
    
    if (!t) {
        t.reset(new TupleType(kws));
        t->make_inner_scope()->leave();
    }
        
    return t.get();
}


TupleType::TupleType(std::vector<std::string> kws)
    :Type("(" + std::to_string(kws.size()) + ")", Metatypes(kws.size(), argument_metatype), tuple_metatype) {
    keywords = kws;
}

TypeSpec TupleType::make_pivot_ts() {
    // We want to have an inner scope, but only to have a pointer to the metascope
    // than contains the unpacking operator.
    return NO_TS;
}

StorageWhere TupleType::where(TypeMatch tm, AsWhat as_what) {
    return (
        as_what == AS_VALUE ? STACK :
        as_what == AS_ARGUMENT ? MEMORY :
        throw INTERNAL_ERROR
    );
}

Allocation TupleType::measure(TypeMatch tm) {
    TSs tss;
    tm[0].unpack_tuple(tss);
    
    unsigned size = 0;
    
    for (auto &ts : tss) {
        StorageWhere param_where = ts.where(AS_ARGUMENT);
        size += ts.measure_where(param_where);
    }
        
    return Allocation(size);
}

void TupleType::store(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    if (t.where == STACK) {
        if (s.where == STACK)
            return;
        else if (this == tuple0_type)
            return;
        else if (this == tuple1_type) {
            StorageWhere param_where = tm[1].where(AS_ARGUMENT);
            tm[1].store(s, Storage(stacked(param_where)), cx);
            return;
        }
    }
    
    throw INTERNAL_ERROR;
}

void TupleType::create(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    switch (s.where * t.where) {
    case STACK_MEMORY: {
        // Used in :evaluate
        int size = measure(tm).bytes;
        int offset = 0;
        
        while (offset < size) {
            cx->op(POPQ, t.address + offset);
            offset += ADDRESS_SIZE;
        }
    }
        break;
    default:
        throw INTERNAL_ERROR;
    }
}

void TupleType::destroy(TypeMatch tm, Storage s, Cx *cx) {
    switch (s.where) {
    case MEMORY: {
        // Used in :evaluate
        TSs tss;
        tm[0].unpack_tuple(tss);
        int offset = 0;

        for (auto &ts : backward<std::vector<TypeSpec>>(tss)) {
            StorageWhere param_where = ts.where(AS_ARGUMENT);
            
            if (param_where == MEMORY)
                ts.destroy(Storage(param_where, s.address + offset), cx);
                
            offset += ts.measure_where(param_where);
        }
    }
        break;
    default:
        throw INTERNAL_ERROR;
    }
}



PartialType::PartialType(std::string name)
    :Type(name, Metatypes { type_metatype }, value_metatype) {
}

StorageWhere PartialType::where(TypeMatch tm, AsWhat as_what) {
    if (as_what == AS_ARGUMENT && tm[1].has_meta(record_metatype))
        as_what = AS_LVALUE_ARGUMENT;

    return tm[1].where(as_what);
}

Allocation PartialType::measure(TypeMatch tm) {
    return tm[1].measure();
}

void PartialType::store(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    tm[1].store(s, t, cx);
}

void PartialType::create(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    tm[1].create(s, t, cx);
}

Value *PartialType::lookup_inner(TypeMatch tm, std::string n, Value *v, Scope *s) {
    std::cerr << "Partial inner lookup " << n << ".\n";
    
    // Base initializers are looked up using "foo.", potentially "."
    bool dot = false;
    
    if (n[n.size() - 1] == '.') {
        // Chop trailing dot, but remember it
        dot = true;
        n = n.substr(0, n.size() - 1);
    }
    
    PartialInfo *pi = dynamic_cast<PartialVariableValue *>(v)->partial_info;
    TypeSpec cast_ts = tm[1];
    // Use lvalue pivot cast for records so the members will be lvalues, too
    if (cast_ts.has_meta(record_metatype))
        cast_ts = cast_ts.lvalue();

    Value *cast_value = make<CastValue>(v, cast_ts);
    Value *member = cast_value->lookup_inner(n, s);

    if (!member) {
        // Consider initializer delegation before giving up
        if (!dot) {
            std::cerr << "Partial not found, considering initializer delegation.\n";
        
            cast_value->ts = tm[1].prefix(initializable_type);
            member = cast_value->lookup_inner(n, s);
        
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
        
        TypeSpec member_ts = member->ts;
        //std::cerr << "Partial member " << n << " is " << member_ts << "\n";
        
        if (ptr_cast<RoleValue>(member)) {
            std::cerr << "Member role " << n << " is not yet initialized.\n";
            
            if (!dot) {
                // Accessed with the $ foo syntax, reject
                std::cerr << "Member role " << n << " is not yet initialized!\n";
                return NULL;
            }

            // Accessed with the $ foo. syntax, allow
            member->ts = member_ts.prefix(initializable_type);
            pi->be_initialized(n);
            return member;
        }
        else {
            if (dot) {
                std::cerr << "Member variable " << n << " has no base initializer!\n";
                return NULL;
            }
            
            std::cerr << "Member variable " << n << " is not yet initialized.\n";
            member->ts = member_ts.reprefix(lvalue_type, uninitialized_type);
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

void PartialType::type_info(TypeMatch tm, Cx *cx) {
    unsigned ts_index = cx->once->type_info(tm[1]);
    cx->dwarf->typedef_info(tm[0].symbolize(), ts_index);
}


UninitializedType::UninitializedType(std::string name)
    :Type(name, Metatypes { value_metatype }, argument_metatype) {
}

StorageWhere UninitializedType::where(TypeMatch tm, AsWhat as_what) {
    // This method is needed for probing if rvalue hinting is applicable
    if (as_what == AS_ARGUMENT)
        as_what = AS_LVALUE_ARGUMENT;
        
    return tm[1].where(as_what);
}


InitializableType::InitializableType(std::string name)
    :Type(name, Metatypes { type_metatype }, type_metatype) {
}

StorageWhere InitializableType::where(TypeMatch tm, AsWhat as_what) {
    if (as_what == AS_ARGUMENT && tm[1].has_meta(record_metatype))
        as_what = AS_LVALUE_ARGUMENT;
        
    return tm[1].where(as_what);
}

Allocation InitializableType::measure(TypeMatch tm) {
    return tm[1].measure();
}

void InitializableType::store(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    tm[1].store(s, t, cx);
}

void InitializableType::create(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    tm[1].create(s, t, cx);
}

Value *InitializableType::lookup_inner(TypeMatch tm, std::string n, Value *v, Scope *s) {
    return tm[1].lookup_inner(n, v, s);  // should look up role initializers
}


MultiType::MultiType(std::string name)
    :Type(name, {}, type_metatype) {
}

StorageWhere MultiType::where(TypeMatch tm, AsWhat as_what) {
    // The AS_ARGUMENT case is only used for unalias hinting, so we just
    // need not to crash here.
    
    return (
        as_what == AS_VALUE ? STACK :
        as_what == AS_ARGUMENT ? NOWHERE :
        throw INTERNAL_ERROR
    );
}


VoidType::VoidType(std::string name)
    :Type(name, {}, value_metatype) {
}

StorageWhere VoidType::where(TypeMatch tm, AsWhat as_what) {
    return NOWHERE;
}

void VoidType::store(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    if (s.where != NOWHERE || t.where != NOWHERE)
        throw INTERNAL_ERROR;
}

void VoidType::type_info(TypeMatch tm, Cx *cx) {
    cx->dwarf->unspecified_type_info(name);
}


UnitType::UnitType(std::string name)
    :Type(name, {}, value_metatype) {
}

StorageWhere UnitType::where(TypeMatch tm, AsWhat as_what) {
    return NOWHERE;
}

Allocation UnitType::measure(TypeMatch tm) {
    return Allocation();
}

void UnitType::store(TypeMatch tm, Storage s, Storage t, Cx *cx) {
}

void UnitType::create(TypeMatch tm, Storage s, Storage t, Cx *cx) {
}

void UnitType::destroy(TypeMatch tm, Storage s, Cx *cx) {
}

void UnitType::streamify(TypeMatch tm, Cx *cx) {
    Address alias_addr(RSP, 0);

    streamify_ascii("U", alias_addr, cx);
}

void UnitType::type_info(TypeMatch tm, Cx *cx) {
    cx->dwarf->unspecified_type_info(name);
}


ColonType::ColonType(std::string name)
    :UnitType(name) {
}

TypeSpec ColonType::make_pivot_ts() {
    return NO_TS;
}


WhateverType::WhateverType(std::string name)
    :Type(name, {}, argument_metatype) {
}

StorageWhere WhateverType::where(TypeMatch tm, AsWhat as_what) {
    return NOWHERE;
}

Allocation WhateverType::measure(TypeMatch tm) {
    throw INTERNAL_ERROR;
}

void WhateverType::store(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    // STACK storage is faked by Whatever controls
    if ((s.where != NOWHERE && s.where != STACK) || t.where != NOWHERE) {
        std::cerr << "Invalid Whatever store from " << s << " to " << t << "!\n";
        throw INTERNAL_ERROR;
    }
}

void WhateverType::destroy(TypeMatch tm, Storage s, Cx *cx) {
    throw INTERNAL_ERROR;
}

void WhateverType::type_info(TypeMatch tm, Cx *cx) {
    cx->dwarf->unspecified_type_info(name);
}


StringtemplateType::StringtemplateType(std::string name)
    :Type(name, {}, value_metatype) {
}


BareType::BareType(std::string name)
    :Type(name, {}, value_metatype) {
}
