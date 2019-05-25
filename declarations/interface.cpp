#include "../plum.h"


InterfaceType::InterfaceType(std::string name, Metatypes param_metatypes)
    :Type(name, param_metatypes, interface_metatype) {
}

DataScope *InterfaceType::make_inner_scope() {
    DataScope *is = Type::make_inner_scope();

    is->be_abstract_scope();
    
    return is;
}

bool InterfaceType::complete_type() {
    for (auto &c : inner_scope->contents) {
        Associable *s = ptr_cast<Associable>(c.get());

        if (s) {
            s->set_parent(this);

            if (s->is_mainconv()) {
                throw INTERNAL_ERROR;
            }
            else if (s->is_baseconv()) {
                if (member_associables.size()) {
                    std::cerr << "Multiple base roles!\n";
                    return false;
                }
            }

            member_associables.push_back(s);

            dump_associable(s, 1);
        }

        Function *f = ptr_cast<Function>(c.get());

        if (f && f->type == FINALIZER_FUNCTION) {
            std::cerr << "Finalizer in Interface type!\n";
            return false;
        }

        if (f && f->type == INITIALIZER_FUNCTION) {
            std::cerr << "Initializer in Interface type!\n";
            return false;
        }

        if (f && f->type == LVALUE_FUNCTION)
            member_procedures.push_back(f);  // for transplanting only

        if (f && (f->type == GENERIC_FUNCTION || f->type == LVALUE_FUNCTION))
            member_functions.push_back(f);
    }

    //std::cerr << "Class " << name << " has " << member_allocables.size() << " member variables.\n";

    return true;
}

void InterfaceType::get_heritage(std::vector<Associable *> &assocs, std::vector<Function *> &funcs) {
    assocs = member_associables;
    funcs = member_functions;
}

Allocation InterfaceType::measure_identity(TypeMatch tm) {
    return { 0 };
}

void InterfaceType::create(TypeMatch tm, Storage s, Storage t, X64 *x64) {
    // Assume the target MEMORY is uninitialized
    
    switch (s.where * t.where) {
    case NOWHERE_MEMORY:  // TODO: hm?
        return;
    default:
        throw INTERNAL_ERROR;
    }
}

void InterfaceType::destroy(TypeMatch tm, Storage s, X64 *x64) {
    if (s.where == MEMORY) {
        return;
    }
    else
        throw INTERNAL_ERROR;
}

StorageWhere InterfaceType::where(TypeMatch tm, AsWhat as_what) {
    return (
        as_what == AS_VARIABLE ? MEMORY :
        as_what == AS_ARGUMENT ? MEMORY :  // for unalias hinting
        as_what == AS_LVALUE_ARGUMENT ? ALIAS :  // for unalias hinting
        throw INTERNAL_ERROR
    );
}

Value *InterfaceType::lookup_inner(TypeMatch tm, std::string n, Value *v, Scope *s) {
    std::cerr << "Interface " << name << " inner lookup " << n << ".\n";
    
    Value *value = Type::lookup_inner(tm, n, v, s);

    if (!value) {
        for (auto a : member_associables) {
            if (a->is_baseconv()) {
                TypeSpec ts = a->get_typespec(tm);
                value = ts.lookup_inner(n, v, s);
                break;
            }
        }
    }

    return value;
}




Implementation::Implementation(std::string name, PivotRequirement pr, TypeSpec ifts, InheritAs ia)
    :Associable(name, pr, ifts, ia, true) {
    std::cerr << "Creating implementation " << name << "\n";

    inherit();
}

Implementation::Implementation(std::string p, Associable *oi, TypeMatch explicit_tm)
    :Associable(p, oi, explicit_tm) {
    std::cerr << "Creating shadow implementation " << name << "\n";

    inherit();
}

bool Implementation::is_abstract() {
    return true;
}

Associable *Implementation::make_shadow(std::string prefix, TypeMatch explicit_tm) {
    return new Implementation(prefix, this, explicit_tm);
}

void Implementation::set_outer_scope(Scope *os) {
    Associable::set_outer_scope(os);
    
    DataScope *ds = ptr_cast<DataScope>(os);
    
    if (ds && ds->is_virtual_scope() && ds->is_abstract_scope()) {
        // Must add some dummy functions to allocate virtual indexes, even if
        // these methods will remain abstract
        
        for (unsigned i = 0; i < functions.size(); i++) {
            Function *f = functions[i]->clone_abstract(prefix);
            ds->add(f);
            
            if (!check_associated(f))
                throw INTERNAL_ERROR;
        }
    }
}

Value *Implementation::make_value(Value *orig, TypeMatch match) {
    // If the pivot is not a concrete type, but a Ptr to an interface, then this
    // is accessing an abstract role via an interface pointer
    TypeSpec ots = ::get_typespec(orig).rvalue();
    
    if (ots[0] == ptr_type && ots.unprefix(ptr_type).has_meta(interface_metatype)) {
        throw INTERNAL_ERROR;  // FIXME: this should be obsolete now
        //return make<RoleValue>(this, orig, match);
    }
    else
        return make<ImplementationConversionValue>(this, orig, match);
}

void Implementation::streamify(TypeMatch tm, X64 *x64) {
    // This allows complete built-in implementations of Streamifiable
    if (alloc_ts[0] != streamifiable_type)
        throw INTERNAL_ERROR;

    std::cerr << "XXX streamify " << get_fully_qualified_name() << "\n";
    
    if (functions.size() != 1)
        throw INTERNAL_ERROR;
        
    Function *sf = functions[0];
    
    if (sf->virtual_index == 0) {
        // Implementation in a value type
        x64->op(CALL, sf->get_label(x64));
    }
    else {
        // Implementation in an identity type
        x64->op(MOVQ, R10, Address(RSP, ALIAS_SIZE));  // Ptr
        x64->op(MOVQ, R11, Address(R10, CLASS_VT_OFFSET));  // VT
        x64->op(CALL, Address(R11, sf->virtual_index * ADDRESS_SIZE));  // select method
    }
}

Value *Implementation::matched(Value *pivot, Scope *scope, TypeMatch &match) {
    return make_value(pivot, match);
}

void Implementation::allocate() {
    std::cerr << " Implementation allocate " << get_fully_qualified_name() << "\n";

    // Implementations cannot be grabbed by their identifier, so they never have an offset
    // within the implementor type, so leave the offset as zero.
    where = MEMORY;
}

void Implementation::relocate(Allocation explicit_offset) {
    std::cerr << " Implementation relocate " << get_fully_qualified_name() << "\n";
    where = MEMORY;
}

void Implementation::override_virtual_entry(int vi, VirtualEntry *ve) {
    parent->override_virtual_entry(vi, ve);
}

void Implementation::compile_vt(TypeMatch tm, X64 *x64) {
}

void Implementation::init_vt(TypeMatch tm, Address self_addr, X64 *x64) {
}

void Implementation::compile_act(TypeMatch tm, X64 *x64) {
}




RawStreamifiableImplementation::RawStreamifiableImplementation(std::string name, Once::FunctionCompiler cs)
    :Implementation(name, RVALUE_PIVOT, STREAMIFIABLE_TS, AS_ROLE) {
    compile_streamification = cs;
}

void RawStreamifiableImplementation::check_full_implementation() {
    // We pretend to implement the streamify function, because we implement
    // streamification as a built-in feature.
}

void RawStreamifiableImplementation::streamify(TypeMatch tm, X64 *x64) {
    Label st_label = x64->once->compile(compile_streamification);
    x64->op(CALL, st_label);
}




StringRawStreamifiableImplementation::StringRawStreamifiableImplementation(std::string name)
    :RawStreamifiableImplementation(name, compile_raw_streamification) {
}

void StringRawStreamifiableImplementation::compile_raw_streamification(Label label, X64 *x64) {
    // RAX - target array, RCX - size, R10 - source array, R11 - alias
    Address value_addr(RSP, RIP_SIZE + ALIAS_SIZE);
    Address alias_addr(RSP, RIP_SIZE);
    
    x64->code_label_local(label, "String__raw_streamification");
    
    x64->op(MOVQ, R10, value_addr);  // reference to the string
    
    x64->op(MOVQ, R10, Address(R10, LINEARRAY_LENGTH_OFFSET));

    stream_preappend2(alias_addr, x64);
    
    x64->op(MOVQ, R10, value_addr);

    x64->op(LEA, RDI, Address(RAX, LINEARRAY_ELEMS_OFFSET));
    x64->op(ADDQ, RDI, Address(RAX, LINEARRAY_LENGTH_OFFSET));
    x64->op(ADDQ, RDI, Address(RAX, LINEARRAY_LENGTH_OFFSET));  // Yes, added twice (CHARACTER_SIZE)

    x64->op(LEA, RSI, Address(R10, LINEARRAY_ELEMS_OFFSET));
    x64->op(MOVQ, RCX, Address(R10, LINEARRAY_LENGTH_OFFSET));
    x64->op(ADDQ, Address(RAX, LINEARRAY_LENGTH_OFFSET), RCX);
    x64->op(SHLQ, RCX, 1);
    
    x64->op(REPMOVSB);
    
    x64->op(RET);
}




CharacterRawStreamifiableImplementation::CharacterRawStreamifiableImplementation(std::string name)
    :RawStreamifiableImplementation(name, compile_raw_streamification) {
}

void CharacterRawStreamifiableImplementation::compile_raw_streamification(Label label, X64 *x64) {
    Address value_addr(RSP, RIP_SIZE + ALIAS_SIZE);
    Address alias_addr(RSP, RIP_SIZE);

    x64->code_label_local(label, "Character__raw_streamification");

    x64->op(MOVQ, R10, 1);
    stream_preappend2(alias_addr, x64);

    x64->op(MOVQ, RCX, Address(RAX, LINEARRAY_LENGTH_OFFSET));
    x64->op(MOVW, R10W, value_addr);  // the character
    x64->op(MOVW, Address(RAX, RCX, Address::SCALE_2, LINEARRAY_ELEMS_OFFSET), R10W);  // stream end
    x64->op(ADDQ, Address(RAX, LINEARRAY_LENGTH_OFFSET), 1);

    x64->op(RET);
}
