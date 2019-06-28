#include "../plum.h"


static void compile_virtual_table(const devector<VirtualEntry *> &vt, TypeMatch tm, Label label, std::string symbol, Cx *cx) {
    // Allow entries to compile some stuff if necessary
    for (int i = vt.low(); i < vt.high(); i++) {
        VirtualEntry *ve = vt.get(i);
        ve->compile(tm, cx);
    }

    std::cerr << "    Virtual table for " << symbol << " (" << vt.high() - vt.low() << ")\n";

    cx->data_align(8);

    for (int i = vt.low(); i < vt.high(); i++) {
        VirtualEntry *ve = vt.get(i);
        std::cerr << std::setw(8) << i << std::setw(0) << ": ";
        ve->out_virtual_entry(std::cerr, tm);
        std::cerr << "\n";
        
        Label l = vt.get(i)->get_virtual_entry_label(tm, cx);

        if (i == 0)
            cx->data_label_local(label, symbol + "__virtual_table");

        cx->data_reference(l);
    }
}


static void compile_autoconv_table(const std::vector<AutoconvEntry> &act, TypeMatch tm, Label label, std::string symbol, Cx *cx) {
    std::cerr << "    Autoconv table for " << symbol << " (" << act.size() << ")\n";

    cx->data_align(8);
    cx->data_label_local(label, symbol + "__autoconv_table");

    for (auto ace : act) {
        cx->data_reference(ace.role_ts.get_interface_table_label(cx));
        cx->data_qword(ace.role_offset);
    }

    // Sentry
    cx->data_qword(0);
}




IdentityType::IdentityType(std::string n, Metatypes pmts, MetaType *mt, bool ia)
    :Type(n, pmts, mt) {
    is_abstract = ia;
    finalizer_function = NULL;
}

TypeSpec IdentityType::make_pivot_ts() {
    return Type::make_pivot_ts().prefix(ptr_type);
}

DataScope *IdentityType::make_inner_scope() {
    DataScope *is = Type::make_inner_scope();

    is->be_virtual_scope();
    
    if (is_abstract)
        is->be_abstract_scope();
    
    return is;
}

bool IdentityType::complete_type() {
    for (auto &c : inner_scope->contents) {
        Allocable *a = ptr_cast<Allocable>(c.get());

        if (a && !a->is_abstract()) {
            member_allocables.push_back(a);
            member_names.push_back(a->name);
        }

        Associable *s = ptr_cast<Associable>(c.get());

        if (s) {
            s->set_parent(this);

            if (s->is_mainconv()) {
                if (member_associables.size()) {
                    std::cerr << "Multiple main roles!\n";
                    throw INTERNAL_ERROR;
                }
                
                if (is_abstract)
                    throw INTERNAL_ERROR;
            }
            else if (s->is_baseconv()) {
                if (member_associables.size()) {
                    std::cerr << "Multiple base roles!\n";
                    return false;
                }
            }

            member_associables.push_back(s);

            if (!is_abstract)
                s->check_full_implementation();

            dump_associable(s, 1);
        }

        Function *f = ptr_cast<Function>(c.get());

        if (f && f->type == FINALIZER_FUNCTION) {
            if (is_abstract) {
                std::cerr << "Finalizer in abstract type!\n";
                return false;
            }
            
            if (finalizer_function) {
                std::cerr << "Multiple finalizers!\n";
                return false;
            }

            finalizer_function = f;
        }

        if (f && f->type == INITIALIZER_FUNCTION) {
            if (is_abstract) {
                std::cerr << "Initializer in abstract type!\n";
                return false;
            }
        }

        if (f && (f->type == GENERIC_FUNCTION || f->type == LVALUE_FUNCTION))
            member_functions.push_back(f);
    }

    std::cerr << "Class " << name << " has " << member_allocables.size() << " member variables.\n";

    return true;
}

Associable *IdentityType::get_base_role() {
    return (member_associables.size() && member_associables[0]->is_baseconv() ? member_associables[0] : NULL);
}

Associable *IdentityType::get_main_role() {
    if (member_associables.empty())
        return NULL;
    
    Associable *role = member_associables[0];
    
    while (true) {
        if (role->is_mainconv())
            return role;
        else if (role->is_baseconv() && role->shadow_associables.size())
            role = role->shadow_associables[0].get();
        else
            return NULL;
    }
}

void IdentityType::get_heritage(std::vector<Associable *> &assocs, std::vector<Function *> &funcs) {
    assocs = member_associables;
    funcs = member_functions;
}

Allocation IdentityType::measure(TypeMatch tm) {
    std::cerr << "This is probably an error, shouldn't measure an identity type!\n";
    throw INTERNAL_ERROR;
}

Allocation IdentityType::measure_identity(TypeMatch tm) {
    return inner_scope->get_size(tm);
}

devector<VirtualEntry *> IdentityType::allocate_vt() {
    throw INTERNAL_ERROR;
}

void IdentityType::allocate() {
    // We must allocate this, because the header is shared among
    // the base, the main, and us.
    std::cerr << "Identity allocate " << get_fully_qualified_name() << "\n";
    Associable *base_role = get_base_role();
    Associable *main_role = get_main_role();
    
    Allocation size = (base_role ? base_role->alloc_ts.measure_identity() : CLASS_HEADER_SIZE);
    Allocation offset = inner_scope->reserve(size);

    if (offset.concretize() != 0)
        throw INTERNAL_ERROR;

    devector<VirtualEntry *> vt = allocate_vt();
    
    if (base_role) {
        // Even if we may have ripped the main from the base, the VT may still
        // contain entries for some part of it!
        devector<VirtualEntry *> base_vt = base_role->get_virtual_table_fragment();
        
        for (int i = 2; i < base_vt.high(); i++)
            vt.append(base_vt.get(i));
            
        // Clone inherited main role method implementations
        for (int i = -1; i >= base_vt.low(); i--)
            vt.prepend(base_vt.get(i));
    }
    
    if (main_role) {
        devector<VirtualEntry *> main_vt = main_role->get_virtual_table_fragment();
        
        // For extended main roles, add the extension part only
        for (int i = vt.low() - 1; i >= main_vt.low(); i--)
            vt.prepend(main_vt.get(i));
            
        if (main_vt.high() > 2)
            throw INTERNAL_ERROR;
    }

    inner_scope->virtual_initialize(vt);

    // Function overrides in virtual tables only happen here
    Type::allocate();
}

void IdentityType::override_virtual_entry(int vi, VirtualEntry *ve) {
    std::cerr << "Identity VT override " << get_fully_qualified_name() << " #" << vi << "\n";
    
    if (vi >= VT_HEADER_LOW_INDEX && vi < VT_HEADER_HIGH_INDEX)
        throw INTERNAL_ERROR;
        
    inner_scope->set_virtual_entry(vi, ve);
}

devector<VirtualEntry *> IdentityType::get_virtual_table(TypeMatch tm) {
    return inner_scope->get_virtual_table();
}

void IdentityType::incref(TypeMatch tm, Register r, Cx *cx) {
    Register q = (r == R10 ? R11 : R10);
    
    cx->op(MOVQ, q, Address(r, CLASS_VT_OFFSET));
    cx->op(MOVQ, q, Address(q, VT_FASTFORWARD_INDEX * ADDRESS_SIZE));
    cx->op(ADDQ, q, r);
    
    cx->runtime->incref(q);
}

void IdentityType::decref(TypeMatch tm, Register r, Cx *cx) {
    Register q = (r == R10 ? R11 : R10);
    
    cx->op(MOVQ, q, Address(r, CLASS_VT_OFFSET));
    cx->op(MOVQ, q, Address(q, VT_FASTFORWARD_INDEX * ADDRESS_SIZE));
    cx->op(ADDQ, q, r);
    
    cx->runtime->decref(q);
}

void IdentityType::streamify(TypeMatch tm, Cx *cx) {
    // Try autoconv
    auto arg_regs = cx->abi_arg_regs();
    Associable *streamifiable_associable = NULL;
        
    for (auto a : member_associables) {
        streamifiable_associable = a->autoconv_streamifiable(tm);
            
        if (streamifiable_associable)
            break;
    }

    if (streamifiable_associable) {
        streamifiable_associable->streamify(tm, cx);
        return;
    }
    
    // We do this for identity types that don't implement Streamifiable
    Address value_addr(RSP, ALIAS_SIZE);
    Address alias_addr(RSP, 0);

    cx->op(MOVQ, arg_regs[0], value_addr);
    cx->op(MOVQ, arg_regs[1], alias_addr);

    cx->runtime->call_sysv(cx->runtime->sysv_streamify_pointer_label);
}

Value *IdentityType::lookup_matcher(TypeMatch tm, std::string n, Value *v, Scope *s) {
    if (n.size() > 0 && isupper(n[0]))
        return make<ClassMatcherValue>(n, v);
    
    return Type::lookup_matcher(tm, n, v, s);
}

Value *IdentityType::lookup_inner(TypeMatch tm, std::string n, Value *v, Scope *s) {
    std::cerr << "Identity " << name << " inner lookup " << n << ".\n";
    Value *value = Type::lookup_inner(tm, n, v, s);
    Associable *base_role = get_base_role();
    
    if (!value && base_role) {
        std::cerr << "Not found, looking up in base role.\n";
        TypeSpec ts = base_role->get_typespec(tm);
        value = ts.lookup_inner(n, v, s);
    }

    return value;
}




AbstractType::AbstractType(std::string name, Metatypes param_metatypes)
    :IdentityType(name, param_metatypes, abstract_metatype, true) {
}

devector<VirtualEntry *> AbstractType::allocate_vt() {
    devector<VirtualEntry *> vt;

    // Abstracts are not supposed to compile their own VT
    vt.append(NULL);
    vt.append(NULL);

    return vt;
}

Label AbstractType::get_interface_table_label(TypeMatch tm, Cx *cx) {
    return cx->once->compile(compile_ift, tm[0]);
}

void AbstractType::compile_ift(Label label, TypeSpec ts, Cx *cx) {
    std::cerr << "Compiling interface table for " << ts << "\n";
    std::string symbol = ts.symbolize("interface_table");

    // We only need this for dynamic type casts
    cx->data_align(8);
    cx->data_label_local(label, symbol);
    cx->data_qword(0);
}

void AbstractType::type_info(TypeMatch tm, Cx *cx) {
    cx->dwarf->begin_interface_type_info(tm[0].symbolize());
    
    debug_inner_scopes(tm, cx);
    
    cx->dwarf->end_info();
}



ClassType::ClassType(std::string name, Metatypes param_metatypes)
    :IdentityType(name, param_metatypes, class_metatype, false) {
    autoconv_ve = NULL;
    fastforward_ve = NULL;
}

std::vector<std::string> ClassType::get_partial_initializable_names() {
    return member_names;
}

void ClassType::destroy(TypeMatch tm, Storage s, Cx *cx) {
    if (s.where != MEMORY || s.address.base != RAX || s.address.index != NOREG)
        throw INTERNAL_ERROR;

    if (finalizer_function) {
        if (s.address.offset)
            cx->op(ADDQ, RAX, s.address.offset);
            
        cx->op(PUSHQ, RAX);
        cx->op(CALL, finalizer_function->get_label(cx));
        cx->op(POPQ, RAX);

        if (s.address.offset)
            cx->op(SUBQ, RAX, s.address.offset);
    }

    for (auto &var : member_allocables)  // FIXME: reverse!
        var->destroy(tm, s, cx);
}

Value *ClassType::lookup_initializer(TypeMatch tm, std::string name, Scope *scope) {
    // NOTE: initializers must only appear in code scopes, and there all types
    // must be concrete, not having free parameters. Also, the automatic variable is
    // put in the local scope, so there will be no pivot for it to derive any
    // type parameters from. 
    
    if (name == "{") {
        std::cerr << "Classes can't be initialized anonymously!\n";
        return NULL;
    }
    else {
        // Named initializer
        Associable *main_role = get_main_role();
        
        if (!main_role) {
            std::cerr << "Can't instantiate class without main role: " << name << "!\n";
            return NULL;
        }
        
        TypeSpec rts = tm[0].prefix(ref_type);
        TypeSpec mts = main_role->alloc_ts.prefix(ref_type);
        
        Value *preinit = make<ClassPreinitializerValue>(rts);

        Value *value = inner_scope->lookup(name, preinit, scope);

        if (value) {
            return make<ClassPostinitializerValue>(mts, value);
        }
    }
    
    std::cerr << "Can't initialize class as " << name << "!\n";
    return NULL;
}

devector<VirtualEntry *> ClassType::allocate_vt() {
    autoconv_ve = new AutoconvVirtualEntry(this);
    fastforward_ve = new FfwdVirtualEntry(Allocation(0));

    devector<VirtualEntry *> vt;
    vt.append(autoconv_ve);
    vt.append(fastforward_ve);
    
    return vt;
}

std::vector<AutoconvEntry> ClassType::get_autoconv_table(TypeMatch tm) {
    std::vector<AutoconvEntry> act;

    Associable *role = get_main_role();

    while (role) {
        act.push_back({ role->get_typespec(tm), role->get_offset(tm) });
        role = (role->has_base_role() ? role->get_head_role() : NULL);
    }
    
    return act;
}

Label ClassType::get_autoconv_table_label(TypeMatch tm, Cx *cx) {
    return cx->once->compile(compile_act, tm[0]);
}

void ClassType::compile_act(Label label, TypeSpec ts, Cx *cx) {
    ClassType *ct = ptr_cast<ClassType>(ts[0]);
    TypeMatch tm = ts.match();
    std::string symbol = ts.symbolize();

    ::compile_autoconv_table(ct->get_autoconv_table(tm), tm, label, symbol, cx);
    
    for (auto &r : ct->member_associables)
        r->compile_act(tm, cx);
}

Label ClassType::get_virtual_table_label(TypeMatch tm, Cx *cx) {
    return cx->once->compile(compile_vt, tm[0]);
}

Label ClassType::get_finalizer_label(TypeMatch tm, Cx *cx) {
    return cx->once->compile(compile_finalizer, tm[0]);
}

void ClassType::compile_vt(Label label, TypeSpec ts, Cx *cx) {
    std::cerr << "Compiling virtual table for " << ts << "\n";

    // Includes main and base, but not regular roles
    devector<VirtualEntry *> vt = ts.get_virtual_table();
    TypeMatch tm = ts.match();
    std::string symbol = ts.symbolize();
    
    ::compile_virtual_table(vt, tm, label, symbol, cx);

    ClassType *ct = ptr_cast<ClassType>(ts[0]);

    for (auto &r : ct->member_associables)
        r->compile_vt(tm, cx);
}

void ClassType::compile_finalizer(Label label, TypeSpec ts, Cx *cx) {
    cx->code_label_local(label, ts[0]->name + "_finalizer");  // FIXME: ambiguous name!
    cx->prologue();

    cx->op(MOVQ, RAX, Address(RSP, ADDRESS_SIZE + RIP_SIZE));  // pointer arg
    ts.destroy(Storage(MEMORY, Address(RAX, 0)), cx);

    cx->epilogue();
}

void ClassType::init_vt(TypeMatch tm, Address self_addr, Cx *cx) {
    Label vt_label = get_virtual_table_label(tm, cx);
    cx->op(LEA, R10, Address(vt_label, 0));
    cx->op(MOVQ, self_addr + CLASS_VT_OFFSET, R10);

    // Roles compute their offsets in terms of the implementor class type parameters
    for (auto &r : member_associables)
        r->init_vt(tm, self_addr, cx);
}

Value *ClassType::lookup_inner(TypeMatch tm, std::string n, Value *v, Scope *s) {
    if (v->ts[0] == initializable_type) {
        // Base role initialization, takes a pivot argument
        std::cerr << "Class " << name << " initializer lookup " << n << ".\n";
        
        return inner_scope->lookup(n, v, s);
    }
    
    return IdentityType::lookup_inner(tm, n, v, s);
}

void ClassType::type_info(TypeMatch tm, Cx *cx) {
    // This must be a concrete parametrization
    unsigned size = measure_identity(tm).concretize();
    
    cx->dwarf->begin_class_type_info(tm[0].symbolize(), size);
    
    debug_inner_scopes(tm, cx);
    
    cx->dwarf->end_info();
}



Role::Role(std::string n, PivotRequirement pr, TypeSpec ts, InheritAs ia, bool ama)
    :Associable(n, pr, ts, ia, ama) {
    std::cerr << "Creating " << (ia == AS_BASE ? "base " : ia == AS_MAIN ? "main " : "") << "role " << name << "\n";
    
    inherit();
}

Role::Role(std::string p, Associable *original, TypeMatch etm)
    :Associable(p, original, etm) {
    std::cerr << "Creating shadow role " << name << "\n";

    inherit();
}

bool Role::is_abstract() {
    return ptr_cast<AbstractType>(alloc_ts[0]) != NULL || is_requiring() || is_in_requiring();
}

Associable *Role::make_shadow(std::string prefix, TypeMatch explicit_tm) {
    return new Role(prefix, this, explicit_tm);
}

Value *Role::matched(Value *cpivot, Scope *scope, TypeMatch &match) {
    //std::cerr << "XXX Role matched " << name << " with " << typeidname(cpivot) << "\n";
    return make_value(cpivot, match);
}

Value *Role::make_value(Value *orig, TypeMatch tm) {
    return make<RoleValue>(this, orig, tm);
}

devector<VirtualEntry *> Role::get_virtual_table_fragment() {
    // Called on explicit roles only
    if (provider_associable)
        throw INTERNAL_ERROR;
    else if (inherit_as == AS_BASE || inherit_as == AS_MAIN)
        return alloc_ts.get_virtual_table();  // FIXME: subst!
    else
        return vt;
}

void Role::override_virtual_entry(int vi, VirtualEntry *ve) {
    if (provider_associable)
        throw INTERNAL_ERROR;
    if (inherit_as == AS_BASE || inherit_as == AS_MAIN) {
        // These roles don't store the VT
        parent->override_virtual_entry(vi, ve);
    }
    else
        vt.set(vi, ve);
}

bool Role::check_provisioning(std::string override_name, Associable *provider_associable) {
    if (!provider_associable)
        throw INTERNAL_ERROR;
        
    // Find the provisioned role
    Associable *requirer_associable = NULL;
    
    for (auto &sa : shadow_associables) {
        if (override_name == unqualify(sa->name)) {
            requirer_associable = sa.get();
            break;
        }
    }
    
    if (!requirer_associable) {
        std::cerr << "No role " << override_name << " to provision!\n";
        return false;
    }
    
    // Make sure the provider provides the required role
    // Don't alter provider_associable, because base/main roles will have a fixed
    // offset of 0, and would be useless to find the real offset.
    Associable *a = provider_associable;
    
    while (true) {
        //std::cerr << "XXX " << a->alloc_ts << " <=> " << requirer_associable->alloc_ts << "\n";

        if (a->alloc_ts == requirer_associable->alloc_ts)
            break;
        
        if (a->has_base_role() || a->has_main_role())
            a = a->get_head_role();
        else {
            std::cerr << "Provider role is not " << requirer_associable->alloc_ts << "!\n";
            return false;
        }
    }

    requirer_associable->provision(provider_associable);
    return true;
}

void Role::allocate() {
    // Only called for explicitly declared roles
    std::cerr << " Role allocate " << get_fully_qualified_name() << "\n";
    
    if (original_associable) {
        throw INTERNAL_ERROR;
    }
        
    Associable::allocate();
    where = MEMORY;
    
    // Data will be allocated in the class scope
    if (inherit_as == AS_BASE || inherit_as == AS_MAIN) {
        // Since base and main roles share the header with the class, they can't
        // reserve the data themselves, but the class does it for them.
    }
    else {
        DataScope *associating_scope = ptr_cast<DataScope>(outer_scope);
        
        if (provider_associable) {
            // No need to set offset, the aliased will be used
        }
        else {
            Allocation size = alloc_ts.measure_identity();
            offset = associating_scope->reserve(size);
        
            vt = alloc_ts.get_virtual_table();
            VirtualEntry *autoconv_ve = new AutoconvVirtualEntry(this);
            vt.set(VT_AUTOCONV_INDEX, autoconv_ve);
            VirtualEntry *fastforward_ve = new FfwdVirtualEntry(offset);
            vt.set(VT_FASTFORWARD_INDEX, fastforward_ve);
        }
        
        // Add VT entry for our data offset into the class VT
        VirtualEntry *ve = new DataVirtualEntry(this);
        virtual_index = associating_scope->virtual_reserve(ve);
    }

    for (auto &sr : shadow_associables)
        sr->relocate(offset);
}

void Role::relocate(Allocation explicit_offset) {
    // Only called for shadow roles, or explicit main roles
    std::cerr << "  Role relocate " << get_fully_qualified_name() << "\n";

    if (!original_associable) {
        if (inherit_as != AS_MAIN)
            throw INTERNAL_ERROR;
    }

    Associable::allocate();
    where = MEMORY;
    
    // Virtual table will be allocated separately
    if (inherit_as == AS_BASE || inherit_as == AS_MAIN)
        ; // Base and main already included in the parent role
    else {
        // Shadow roles never allocate data, as the explicit role already did that
        DataScope *associating_scope = ptr_cast<DataScope>(outer_scope);
        
        if (provider_associable) {
            // No need to set offset, the provided will be used
            
            // Patch provider virtual table
            // The patching method doesn't know the patching role's offset from the
            // patched role's offset, so cannot compile in the defference to adjust.
            // Instead we must load the offset into R11 in a method trampoline,
            // and the method will use the runtime value.
            devector<VirtualEntry *> ovt = original_associable->get_virtual_table_fragment();
            
            for (unsigned i = 0; i < functions.size(); i++) {
                //if (functions[i]->associated == original_associable) {
                    // This function was patched in the original role
                    // FIXME: maybe it can be patched in a derived class as well...
                Associable *a = functions[i]->associated;
                
                // FIXME: let's hope this is a sensible check...
                if (a && (a->is_requiring() || a->is_in_requiring())) {
                    // This function was patched
                    // Find the role that corresponds to the class of the patching method.
                    // Step up as many roles as necessary to step back in originals.
                    Associable *up = this;
                    Associable *orig = this;
                    
                    while (orig != a) {
                        orig = orig->original_associable;
                        up = ptr_cast<Associable>(up->parent);
                    }
                    
                    int patching_offset = up->get_offset(explicit_tm);
                    int provider_offset = provider_associable->get_offset(explicit_tm);
                    int diff = provider_offset - patching_offset;
                    VirtualEntry *ve = new PatchMethodVirtualEntry(functions[i], diff);
                    
                    int vi = functions[i]->virtual_index;
                    std::cerr << "Patching VT " << provider_associable->get_fully_qualified_name() << " #" << vi << " from " << get_fully_qualified_name() << "\n";
                    provider_associable->override_virtual_entry(vi, ve);
                }
            }
        }
        else {
            if (original_associable->is_abstract())
                offset = associating_scope->reserve(alloc_ts.measure_identity());
            else
                offset = explicit_offset + original_associable->subst_offset(explicit_tm);
        
            vt = original_associable->get_virtual_table_fragment();
            VirtualEntry *autoconv_ve = new AutoconvVirtualEntry(this);
            vt.set(VT_AUTOCONV_INDEX, autoconv_ve);
            VirtualEntry *fastforward_ve = new FfwdVirtualEntry(offset);
            vt.set(VT_FASTFORWARD_INDEX, fastforward_ve);
        }
        
        virtual_index = original_associable->virtual_index;
        VirtualEntry *ve = new DataVirtualEntry(this);
        parent->override_virtual_entry(virtual_index, ve);
    }

    for (auto &sr : shadow_associables)
        sr->relocate(explicit_offset);
}

void Role::destroy(TypeMatch tm, Storage s, Cx *cx) {
    if (original_associable)
        throw INTERNAL_ERROR;
        
    TypeSpec ts = typesubst(alloc_ts, tm);
    int o = allocsubst(offset, tm).concretize();
    ts.destroy(s + o, cx);
}

void Role::compile_vt(TypeMatch tm, Cx *cx) {
    if (provider_associable)
        ;  // vt_label is unused
    else if (inherit_as == AS_BASE || inherit_as == AS_MAIN) {
        ;
    }
    else {
        std::string symbol = tm[0].symbolize() + QUALIFIER_NAME + name;
        ::compile_virtual_table(vt, tm, vt_label, symbol, cx);
    }

    for (auto &sr : shadow_associables)
        sr->compile_vt(tm, cx);
}

void Role::init_vt(TypeMatch tm, Address self_addr, Cx *cx) {
    if (provider_associable) {
        ;  // Aliases are initialized when the aliased role is initialized
    }
    else if (inherit_as == AS_BASE || inherit_as == AS_MAIN) {
        ;  // Base roles have a VT pointer overlapping the main class VT, don't overwrite
    }
    else {
        cx->op(LEA, R10, Address(vt_label, 0));
        cx->op(MOVQ, self_addr + allocsubst(offset, tm).concretize() + CLASS_VT_OFFSET, R10);
    }

    for (auto &sr : shadow_associables)
        sr->init_vt(tm, self_addr, cx);
}

std::vector<AutoconvEntry> Role::get_autoconv_table(TypeMatch tm) {
    if (provider_associable)
        throw INTERNAL_ERROR;
        
    std::vector<AutoconvEntry> act;

    Associable *role = (has_main_role() ? get_head_role() : NULL);

    while (role) {
        act.push_back({ role->get_typespec(tm), role->get_offset(tm) });
        role = (role->has_base_role() ? role->get_head_role() : NULL);
    }
    
    return act;
}

Label Role::get_autoconv_table_label(TypeMatch tm, Cx *cx) {
    if (provider_associable)
        throw INTERNAL_ERROR;
    else if (inherit_as == AS_BASE || inherit_as == AS_MAIN) {
        throw INTERNAL_ERROR;
    }
    else
        return act_label;
}

void Role::compile_act(TypeMatch tm, Cx *cx) {
    if (provider_associable) {
        // act_label is unused
    }
    else if (inherit_as == AS_BASE || inherit_as == AS_MAIN) {
        // Compiled in the containing role
    }
    else {
        std::string symbol = tm[0].symbolize() + QUALIFIER_NAME + name;
        ::compile_autoconv_table(get_autoconv_table(tm), tm, act_label, symbol, cx);
    }
    
    for (auto &r : shadow_associables)
        r->compile_act(tm, cx);
}
