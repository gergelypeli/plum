

static void compile_virtual_table(const devector<VirtualEntry *> &vt, TypeMatch tm, Label label, std::string symbol, X64 *x64) {
    std::cerr << "    Virtual table for " << symbol << " (" << vt.high() - vt.low() << ")\n";

    x64->data_align(8);

    for (int i = vt.low(); i < vt.high(); i++) {
        VirtualEntry *ve = vt.get(i);
        std::cerr << std::setw(8) << i << std::setw(0) << ": ";
        ve->out_virtual_entry(std::cerr, tm);
        std::cerr << "\n";
        
        Label l = vt.get(i)->get_virtual_entry_label(tm, x64);

        if (i == 0)
            x64->data_label_local(label, symbol + "_virtual_table");

        x64->data_reference(l);
    }
}


static void compile_autoconv_table(const std::vector<AutoconvEntry> &act, TypeMatch tm, Label label, std::string symbol, X64 *x64) {
    std::cerr << "    Autoconv table for " << symbol << " (" << act.size() << ")\n";

    x64->data_align(8);
    x64->data_label_local(label, symbol + "_autoconv_table");

    for (auto ace : act) {
        x64->data_reference(ace.role_ts.get_interface_table_label(x64));
        x64->data_qword(ace.role_offset);
    }

    // Sentry
    x64->data_qword(0);
}


class IdentityType: public Type, public Inheritable, public PartialInitializable {
public:
    std::vector<Allocable *> member_allocables;
    std::vector<std::string> member_names;
    
    std::vector<Function *> member_functions;
    std::vector<Associable *> member_associables;
    std::vector<Declaration *> member_initializers;
    std::vector<Declaration *> member_procedures;

    bool is_abstract;
    Function *finalizer_function;

    IdentityType(std::string n, Metatypes pmts, MetaType *mt, bool ia)
        :Type(n, pmts, mt) {
        is_abstract = ia;
        finalizer_function = NULL;
    }

    virtual TypeSpec make_pivot_ts() {
        return Type::make_pivot_ts().prefix(ptr_type);
    }

    virtual DataScope *make_inner_scope() {
        DataScope *is = Type::make_inner_scope();

        is->be_virtual_scope();
        
        if (is_abstract)
            is->be_abstract_scope();
        
        return is;
    }

    virtual bool complete_type() {
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

                member_initializers.push_back(f);
            }

            if (f && f->type == LVALUE_FUNCTION)
                member_procedures.push_back(f);  // for transplanting only

            if (f && (f->type == GENERIC_FUNCTION || f->type == LVALUE_FUNCTION))
                member_functions.push_back(f);
        }

        std::cerr << "Class " << name << " has " << member_allocables.size() << " member variables.\n";

        return true;
    }

    virtual Associable *get_base_role() {
        return (member_associables.size() && member_associables[0]->is_baseconv() ? member_associables[0] : NULL);
    }
    
    virtual Associable *get_main_role() {
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

    virtual void get_heritage(std::vector<Associable *> &assocs, std::vector<Function *> &funcs) {
        assocs = member_associables;
        funcs = member_functions;
    }
    
    virtual Allocation measure(TypeMatch tm) {
        std::cerr << "This is probably an error, shouldn't measure an identity type!\n";
        throw INTERNAL_ERROR;
    }

    virtual Allocation measure_identity(TypeMatch tm) {
        return inner_scope->get_size(tm);
    }

    virtual std::vector<std::string> get_member_names() {
        return member_names;
    }

    virtual devector<VirtualEntry *> allocate_vt() {
        throw INTERNAL_ERROR;
    }

    virtual void allocate() {
        // We must allocate this, because the header is shared among
        // the base, the main, and us.
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

    virtual void override_virtual_entry(int vi, VirtualEntry *ve) {
        std::cerr << "Identity VT override " << get_fully_qualified_name() << " #" << vi << "\n";
        
        if (vi >= VT_HEADER_LOW_INDEX && vi < VT_HEADER_HIGH_INDEX)
            throw INTERNAL_ERROR;
            
        inner_scope->set_virtual_entry(vi, ve);
    }

    virtual devector<VirtualEntry *> get_virtual_table(TypeMatch tm) {
        return inner_scope->get_virtual_table();
    }

    virtual void incref(TypeMatch tm, Register r, X64 *x64) {
        Register q = (r == R10 ? R11 : R10);
        
        x64->op(MOVQ, q, Address(r, CLASS_VT_OFFSET));
        x64->op(MOVQ, q, Address(q, VT_FASTFORWARD_INDEX * ADDRESS_SIZE));
        x64->op(ADDQ, q, r);
        
        x64->runtime->incref(q);
    }

    virtual void decref(TypeMatch tm, Register r, X64 *x64) {
        Register q = (r == R10 ? R11 : R10);
        
        x64->op(MOVQ, q, Address(r, CLASS_VT_OFFSET));
        x64->op(MOVQ, q, Address(q, VT_FASTFORWARD_INDEX * ADDRESS_SIZE));
        x64->op(ADDQ, q, r);
        
        x64->runtime->decref(q);
    }

    virtual void streamify(TypeMatch tm, bool alt, X64 *x64) {
        // NOTE: unlike with Implementation, we cannot just delegate the job to an
        // Associable, because we may have a direct Streamifiable Ptr pivot.
        
        // The pivot is on the stack as rvalue, and the stream as lvalue.
        //int method_vi = ptr_cast<Function>(ptr_cast<Type>(streamifiable_type)->get_inner_scope()->contents[0].get())->virtual_index;
        /*
        if (this == ptr_cast<AbstractType>(streamifiable_type)) {
            // Streamifiable role pivot, invoke method directly
            x64->op(MOVQ, R10, Address(RSP, ALIAS_SIZE));  // Ptr
            x64->op(MOVQ, R11, Address(R10, CLASS_VT_OFFSET));  // sable VT
            x64->op(CALL, Address(R11, method_vi * ADDRESS_SIZE));  // select method
            return;
        }
        */
        // Try autoconv
        Associable *streamifiable_associable = NULL;
            
        for (auto a : member_associables) {
            streamifiable_associable = a->autoconv_streamifiable(tm);
                
            if (streamifiable_associable)
                break;
        }

        if (streamifiable_associable) {
            streamifiable_associable->streamify(tm, x64);
            /*
            int role_vi = streamifiable_associable->virtual_index;

            x64->op(MOVQ, R10, Address(RSP, ALIAS_SIZE));  // Ptr
            x64->op(MOVQ, R11, Address(R10, CLASS_VT_OFFSET));  // VT
            x64->op(ADDQ, R10, Address(R11, role_vi * ADDRESS_SIZE));  // select role
            x64->op(MOVQ, Address(RSP, ALIAS_SIZE), R10);  // technically illegal
            x64->op(MOVQ, R11, Address(R10, CLASS_VT_OFFSET));  // sable VT
            x64->op(CALL, Address(R11, method_vi * ADDRESS_SIZE));  // select method
            */
            return;
        }
        
        // We do this for identity types that don't implement Streamifiable
        x64->op(MOVQ, RDI, Address(RSP, ALIAS_SIZE));
        x64->op(MOVQ, RSI, Address(RSP, 0));
    
        x64->runtime->call_sysv(x64->runtime->sysv_streamify_pointer_label);
    }

    virtual Value *lookup_matcher(TypeMatch tm, std::string n, Value *v, Scope *s) {
        return make<ClassMatcherValue>(n, v);
    }

    virtual Value *lookup_inner(TypeMatch tm, std::string n, Value *v, Scope *s) {
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
};


class AbstractType: public IdentityType {
public:
    AbstractType(std::string name, Metatypes param_metatypes)
        :IdentityType(name, param_metatypes, abstract_metatype, true) {
    }

    virtual devector<VirtualEntry *> allocate_vt() {
        devector<VirtualEntry *> vt;

        // Abstracts are not supposed to compile their own VT
        vt.append(NULL);
        vt.append(NULL);

        return vt;
    }

    virtual Label get_interface_table_label(TypeMatch tm, X64 *x64) {
        return x64->once->compile(compile_ift, tm[0]);
    }

    static void compile_ift(Label label, TypeSpec ts, X64 *x64) {
        std::cerr << "Compiling interface table for " << ts << "\n";
        std::string symbol = ts.symbolize() + "_interface_table";

        // We only need this for dynamic type casts
        x64->data_align(8);
        x64->data_label_local(label, symbol);
        x64->data_qword(0);
    }
};


class ClassType: public IdentityType, public Autoconvertible {
public:
    AutoconvVirtualEntry *autoconv_ve;
    FfwdVirtualEntry *fastforward_ve;

    ClassType(std::string name, Metatypes param_metatypes)
        :IdentityType(name, param_metatypes, class_metatype, false) {
        autoconv_ve = NULL;
        fastforward_ve = NULL;
    }

    virtual bool complete_type() {
        if (!IdentityType::complete_type())
            return false;
            
        if (initializer_scope)
            transplant_initializers(member_initializers);

        if (lvalue_scope)
            transplant_procedures(member_procedures);
            
        return true;
    }

    virtual void destroy(TypeMatch tm, Storage s, X64 *x64) {
        if (s.where != MEMORY || s.address.base != RAX || s.address.index != NOREG)
            throw INTERNAL_ERROR;

        if (finalizer_function) {
            if (s.address.offset)
                x64->op(ADDQ, RAX, s.address.offset);
                
            x64->op(PUSHQ, RAX);
            x64->op(CALL, finalizer_function->get_label(x64));
            x64->op(POPQ, RAX);

            if (s.address.offset)
                x64->op(SUBQ, RAX, s.address.offset);
        }
    
        for (auto &var : member_allocables)  // FIXME: reverse!
            var->destroy(tm, s, x64);
    }

    virtual Value *lookup_initializer(TypeMatch tm, std::string name, Scope *scope) {
        // NOTE: initializers must only appear in code scopes, and there all types
        // must be concrete, not having free parameters. Also, the automatic variable is
        // put in the local scope, so there will be no pivot for it to derive any
        // type parameters from. 
        
        if (name == "{}") {
            std::cerr << "Classes can't be initialized anonymously!\n";
            return NULL;
        }
        else {
            // Named initializer
            TypeSpec rts = tm[0].prefix(ref_type);
            
            Value *preinit = make<ClassPreinitializerValue>(rts);

            //Value *value = inner_scope->lookup(name, preinit, scope);
            Value *value = initializer_scope->lookup(name, preinit, scope);

            if (value) {
                if (is_initializer_function_call(value))
                    return make<ClassPostinitializerValue>(value);
                        
                std::cerr << "Can't initialize class with non-initializer " << name << "!\n";
                return NULL;
            }
        }
        
        std::cerr << "Can't initialize class as " << name << "!\n";
        return NULL;
    }

    virtual devector<VirtualEntry *> allocate_vt() {
        autoconv_ve = new AutoconvVirtualEntry(this);
        fastforward_ve = new FfwdVirtualEntry(Allocation(0));

        devector<VirtualEntry *> vt;
        vt.append(autoconv_ve);
        vt.append(fastforward_ve);
        
        return vt;
    }

    virtual std::vector<AutoconvEntry> get_autoconv_table(TypeMatch tm) {
        std::vector<AutoconvEntry> act;

        Associable *role = get_main_role();

        while (role) {
            act.push_back({ role->get_typespec(tm), role->offset.concretize(tm) });
            role = (role->has_base_role() ? role->get_head_role() : NULL);
        }
        
        return act;
    }

    virtual Label get_autoconv_table_label(TypeMatch tm, X64 *x64) {
        return x64->once->compile(compile_act, tm[0]);
    }
    
    static void compile_act(Label label, TypeSpec ts, X64 *x64) {
        ClassType *ct = ptr_cast<ClassType>(ts[0]);
        TypeMatch tm = ts.match();
        std::string symbol = ts.symbolize();

        ::compile_autoconv_table(ct->get_autoconv_table(tm), tm, label, symbol, x64);
        
        for (auto &r : ct->member_associables)
            r->compile_act(tm, x64);
    }

    virtual Label get_virtual_table_label(TypeMatch tm, X64 *x64) {
        return x64->once->compile(compile_vt, tm[0]);
    }

    virtual Label get_finalizer_label(TypeMatch tm, X64 *x64) {
        return x64->once->compile(compile_finalizer, tm[0]);
    }

    static void compile_vt(Label label, TypeSpec ts, X64 *x64) {
        std::cerr << "Compiling virtual table for " << ts << "\n";

        // Includes main and base, but not regular roles
        devector<VirtualEntry *> vt = ts.get_virtual_table();
        TypeMatch tm = ts.match();
        std::string symbol = ts.symbolize();
        
        ::compile_virtual_table(vt, tm, label, symbol, x64);

        ClassType *ct = ptr_cast<ClassType>(ts[0]);

        for (auto &r : ct->member_associables)
            r->compile_vt(tm, x64);
    }

    static void compile_finalizer(Label label, TypeSpec ts, X64 *x64) {
        x64->code_label_local(label, ts[0]->name + "_finalizer");  // FIXME: ambiguous name!

        x64->op(MOVQ, RAX, Address(RSP, ADDRESS_SIZE));  // pointer arg
        ts.destroy(Storage(MEMORY, Address(RAX, 0)), x64);

        x64->op(RET);
    }

    virtual void init_vt(TypeMatch tm, Address self_addr, X64 *x64) {
        Label vt_label = get_virtual_table_label(tm, x64);
        x64->op(LEA, R10, Address(vt_label, 0));
        x64->op(MOVQ, self_addr + CLASS_VT_OFFSET, R10);

        // Roles compute their offsets in terms of the implementor class type parameters
        for (auto &r : member_associables)
            r->init_vt(tm, self_addr, x64);
    }

    virtual Value *lookup_inner(TypeMatch tm, std::string n, Value *v, Scope *s) {
        if (get_typespec(v)[0] == initializable_type) {
            // Base role initialization, takes a pivot argument
            std::cerr << "Class " << name << " initializer lookup " << n << ".\n";
            
            return initializer_scope->lookup(n, v, s);
        }
        
        return IdentityType::lookup_inner(tm, n, v, s);
    }
};


class Role: public Associable, public Autoconvertible {
public:
    devector<VirtualEntry *> vt;
    Label vt_label;
    Label act_label;

    Role(std::string n, TypeSpec ts, InheritAs ia, bool ama)
        :Associable(n, ts, ia, ama) {
        std::cerr << "Creating " << (ia == AS_BASE ? "base " : ia == AS_MAIN ? "main " : "") << "role " << name << "\n";
        
        inherit();
    }

    Role(std::string p, Associable *original, TypeMatch etm)
        :Associable(p, original, etm) {
        std::cerr << "Creating shadow role " << name << "\n";

        inherit();
    }

    virtual bool is_abstract() {
        return ptr_cast<AbstractType>(alloc_ts[0]) != NULL || is_requiring() || is_in_requiring();
    }

    virtual Associable *make_shadow(std::string prefix, TypeMatch explicit_tm) {
        return new Role(prefix, this, explicit_tm);
    }

    virtual Value *matched(Value *cpivot, Scope *scope, TypeMatch &match) {
        //std::cerr << "XXX Role matched " << name << " with " << typeidname(cpivot) << "\n";
        return make_value(cpivot, match);
    }

    virtual Value *make_value(Value *orig, TypeMatch tm) {
        return make<RoleValue>(this, orig, tm);
    }

    virtual devector<VirtualEntry *> get_virtual_table_fragment() {
        // Called on explicit roles only
        if (provider_associable)
            throw INTERNAL_ERROR;
        else if (inherit_as == AS_BASE || inherit_as == AS_MAIN)
            return alloc_ts.get_virtual_table();  // FIXME: subst!
        else
            return vt;
    }

    virtual void override_virtual_entry(int vi, VirtualEntry *ve) {
        if (provider_associable)
            throw INTERNAL_ERROR;
        if (inherit_as == AS_BASE || inherit_as == AS_MAIN) {
            // These roles don't store the VT
            parent->override_virtual_entry(vi, ve);
        }
        else
            vt.set(vi, ve);
    }

    virtual bool check_provisioning(std::string override_name, Associable *provider_associable) {
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

    virtual void allocate() {
        // Only called for explicitly declared roles
        
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
    
    virtual void relocate(Allocation explicit_offset) {
        // Only called for shadow roles, or explicit main roles

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
            // Offset within the current class, in terms of its type parameters
            DataScope *associating_scope = ptr_cast<DataScope>(outer_scope);
            
            if (provider_associable) {
                // No need to set offset, the aliased will be used
                
                // Patch aliased virtual table
                devector<VirtualEntry *> ovt = original_associable->get_virtual_table_fragment();
                
                for (unsigned i = 0; i < functions.size(); i++) {
                    if (functions[i]->associated == original_associable) {
                        // This function was patched in the original role
                        int vi = functions[i]->virtual_index;
                        Associable *pa = provider_associable;
                        std::cerr << "Patching VT " << pa->get_fully_qualified_name() << " #" << vi << " from " << get_fully_qualified_name() << "\n";
                        pa->override_virtual_entry(vi, ovt.get(vi));
                    }
                }
            }
            else {
                if (original_associable->is_abstract())
                    offset = associating_scope->reserve(alloc_ts.measure_identity());
                else
                    offset = explicit_offset + allocsubst(original_associable->offset, explicit_tm);
            
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

    virtual void destroy(TypeMatch tm, Storage s, X64 *x64) {
        if (original_associable)
            throw INTERNAL_ERROR;
            
        TypeSpec ts = typesubst(alloc_ts, tm);
        int o = offset.concretize(tm);
        ts.destroy(s + o, x64);
    }

    virtual void compile_vt(TypeMatch tm, X64 *x64) {
        if (provider_associable)
            ;  // vt_label is unused
        else if (inherit_as == AS_BASE || inherit_as == AS_MAIN) {
            ;
        }
        else {
            std::string symbol = tm[0].symbolize() + QUALIFIER_NAME + name;
            ::compile_virtual_table(vt, tm, vt_label, symbol, x64);
        }

        for (auto &sr : shadow_associables)
            sr->compile_vt(tm, x64);
    }
    
    virtual void init_vt(TypeMatch tm, Address self_addr, X64 *x64) {
        if (provider_associable) {
            ;  // Aliases are initialized when the aliased role is initialized
        }
        else if (inherit_as == AS_BASE || inherit_as == AS_MAIN) {
            ;  // Base roles have a VT pointer overlapping the main class VT, don't overwrite
        }
        else {
            x64->op(LEA, R10, Address(vt_label, 0));
            x64->op(MOVQ, self_addr + offset.concretize(tm) + CLASS_VT_OFFSET, R10);
        }

        for (auto &sr : shadow_associables)
            sr->init_vt(tm, self_addr, x64);
    }

    virtual std::vector<AutoconvEntry> get_autoconv_table(TypeMatch tm) {
        if (provider_associable)
            throw INTERNAL_ERROR;
            
        std::vector<AutoconvEntry> act;

        Associable *role = (has_main_role() ? get_head_role() : NULL);

        while (role) {
            act.push_back({ role->get_typespec(tm), role->offset.concretize(tm) });
            role = (role->has_base_role() ? role->get_head_role() : NULL);
        }
        
        return act;
    }
    
    virtual Label get_autoconv_table_label(TypeMatch tm, X64 *x64) {
        if (provider_associable)
            throw INTERNAL_ERROR;
        else if (inherit_as == AS_BASE || inherit_as == AS_MAIN) {
            throw INTERNAL_ERROR;
        }
        else
            return act_label;
    }

    void compile_act(TypeMatch tm, X64 *x64) {
        if (provider_associable) {
            // act_label is unused
        }
        else if (inherit_as == AS_BASE || inherit_as == AS_MAIN) {
            // Compiled in the containing role
        }
        else {
            std::string symbol = tm[0].symbolize() + QUALIFIER_NAME + name;
            ::compile_autoconv_table(get_autoconv_table(tm), tm, act_label, symbol, x64);
        }
        
        for (auto &r : shadow_associables)
            r->compile_act(tm, x64);
    }
};


class WrappedClassType: public ClassType {
public:
    TypeSpec wrapped_ts;
    
    WrappedClassType(std::string name, Metatypes param_metatypes, TypeSpec wts)
        :ClassType(name, param_metatypes) {
        if (wts == NO_TS)
            throw INTERNAL_ERROR;  // this can be a global initialization issue
            
        wrapped_ts = wts;
    }
    
    virtual Value *lookup_initializer(TypeMatch tm, std::string name, Scope *scope) {
        TypeSpec member_ts = typesubst(wrapped_ts, tm);
        Value *member_initializer = member_ts.lookup_initializer(name, scope);
        
        if (!member_initializer) {
            std::cerr << "No " << this->name << " member initializer called " << name << "!\n";
            return NULL;
        }

        TypeSpec self_ts = tm[0].prefix(ref_type);
        Value *pivot = make<ClassPreinitializerValue>(self_ts);
        
        return make<ClassWrapperInitializerValue>(pivot, member_initializer);
    }

    virtual void streamify(TypeMatch tm, bool alt, X64 *x64) {
        if (alt) {
            TypeSpec member_ts = typesubst(wrapped_ts, tm);
        
            x64->op(MOVQ, RAX, Address(RSP, ADDRESS_SIZE));
            x64->op(MOVQ, RBX, Address(RSP, 0));
        
            member_ts.store(Storage(MEMORY, Address(RAX, CLASS_MEMBERS_OFFSET)), Storage(STACK), x64);
            x64->op(PUSHQ, RBX);
        
            member_ts.streamify(true, x64);  // clobbers all
        
            x64->op(POPQ, RBX);
            member_ts.store(Storage(STACK), Storage(), x64);
        }
        else
            ClassType::streamify(tm, alt, x64);
    }
};

