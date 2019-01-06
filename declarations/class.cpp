

static void compile_virtual_table(const devector<VirtualEntry *> &vt, TypeMatch tm, Label label, std::string symbol, X64 *x64) {
    x64->data_align(8);
    std::cerr << "    " << symbol << " (" << vt.high() - vt.low() << ")\n";

    for (int i = vt.low(); i < vt.high(); i++) {
        VirtualEntry *ve = vt.get(i);
        std::cerr << std::setw(8) << i << std::setw(0) << ": ";
        ve->out_virtual_entry(std::cerr, tm);
        std::cerr << "\n";
        
        Label l = vt.get(i)->get_virtual_entry_label(tm, x64);

        if (i == 0)
            x64->data_label_local(label, symbol);

        x64->data_reference(l);
    }
}


class ClassType: public InheritableType, public PartialInitializable {
public:
    std::vector<Allocable *> member_allocables;
    std::vector<std::string> member_names;
    std::vector<Role *> member_roles;  // including abstract ones
    std::vector<Function *> member_functions;
    Function *finalizer_function;
    RoleVirtualEntry *role_ve;
    FfwdVirtualEntry *fastforward_ve;
    Allocable *base_role;
    Allocable *main_role;

    ClassType(std::string name, Metatypes param_metatypes)
        :InheritableType(name, param_metatypes, class_metatype) {
        finalizer_function = NULL;
        role_ve = NULL;
        fastforward_ve = NULL;
        base_role = NULL;
        main_role = NULL;
    }

    virtual DataScope *make_inner_scope(TypeSpec pts) {
        DataScope *is = InheritableType::make_inner_scope(pts);

        is->be_virtual_scope();
        
        return is;
    }

    virtual bool complete_type() {
        for (auto &c : inner_scope->contents) {
            Allocable *a = ptr_cast<Allocable>(c.get());
            
            if (a && !a->is_abstract()) {
                member_allocables.push_back(a);
                member_names.push_back(a->name);
            }

            Role *r = ptr_cast<Role>(c.get());
            
            if (r) {
                member_roles.push_back(r);
                Associable *s = ptr_cast<Associable>(r);
                
                if (s->is_baseconv()) {
                    if (s->name == "main") {
                        if (main_role) {
                            std::cerr << "Multiple main roles!\n";
                            throw INTERNAL_ERROR;
                        }
                
                        main_role = a;
                    }
                    else if (s->name == "") {
                        if (base_role) {
                            std::cerr << "Multiple base roles!\n";
                            return false;
                        }
                
                        base_role = a;
                    }
                }
            }

            Function *f = ptr_cast<Function>(c.get());
            
            if (f && f->type == FINALIZER_FUNCTION) {
                if (finalizer_function) {
                    std::cerr << "Multiple finalizers!\n";
                    return false;
                }
                    
                finalizer_function = f;
            }
            
            if (f && f->type == GENERIC_FUNCTION)
                member_functions.push_back(f);
        }
        
        std::cerr << "Class " << name << " has " << member_allocables.size() << " member variables.\n";
        return true;
    }

    virtual void get_heritage(std::vector<Associable *> &assocs, std::vector<Function *> &funcs) {
        for (auto &r : member_roles)
            assocs.push_back(ptr_cast<Associable>(r));
            
        funcs = member_functions;
    }

    virtual void allocate() {
        // Let the base be allocated first, then it will skip itself

        role_ve = new RoleVirtualEntry(this, base_role);
        fastforward_ve = new FfwdVirtualEntry(Allocation(0));
        inner_scope->virtual_initialize(role_ve, fastforward_ve);

        if (!base_role) {
            Allocation vt_offset = inner_scope->reserve(Allocation(CLASS_HEADER_SIZE));  // VT pointer
            if (vt_offset.bytes != CLASS_VT_OFFSET)  // sanity check
                throw INTERNAL_ERROR;
        }
        
        /*
            // FIXME: must be handled like base_role
            if (base_implementation)
                base_implementation->allocate();
        */

        // Function overrides in virtual tables only happen here
        InheritableType::allocate();
    }

    virtual Allocation measure_identity(TypeMatch tm) {
        return inner_scope->get_size(tm);
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

            Value *value = inner_scope->lookup(name, preinit, scope);

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

    virtual std::vector<std::string> get_member_names() {
        return member_names;
    }

    virtual devector<VirtualEntry *> get_virtual_table(TypeMatch tm) {
        return inner_scope->get_virtual_table();
    }

    virtual Label get_virtual_table_label(TypeMatch tm, X64 *x64) {
        return x64->once->compile(compile_vt, tm[0]);
    }

    virtual Label get_finalizer_label(TypeMatch tm, X64 *x64) {
        return x64->once->compile(compile_finalizer, tm[0]);
    }
    
    static void compile_vt(Label label, TypeSpec ts, X64 *x64) {
        std::cerr << "Compiling virtual table for " << ts << "\n";
        
        devector<VirtualEntry *> vt = ts.get_virtual_table();
        TypeMatch tm = ts.match();
        std::string symbol = ts.symbolize() + "_virtual_table";
        
        ::compile_virtual_table(vt, tm, label, symbol, x64);

        ClassType *ct = ptr_cast<ClassType>(ts[0]);

        for (auto &r : ct->member_roles)
            role_compile_vt(r, tm, ts.symbolize(), x64);
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
        for (auto &r : member_roles)
            role_init_vt(r, tm, self_addr, x64);
    }

    virtual Value *autoconv(TypeMatch tm, Type *target, Value *orig, TypeSpec &ifts, bool assume_lvalue) {
        for (auto mr : member_roles) {
            Associable *r = ptr_cast<Associable>(mr);
            
            if (!r->is_autoconv())
                continue;
                
            Value *v = r->autoconv(tm, target, orig, ifts, assume_lvalue);
            
            if (v)
                return v;
        }
        
        return InheritableType::autoconv(tm, target, orig, ifts, assume_lvalue);
    }

    virtual Value *lookup_inner(TypeMatch tm, std::string n, Value *v, Scope *s) {
        std::cerr << "Class " << name << " inner lookup " << n << ".\n";
        bool dot = false;
        
        if (n[0] == '.') {
            dot = true;
            n = n.substr(1);
        }
        
        Value *value = InheritableType::lookup_inner(tm, n, v, s);
        
        if (!value && base_role) {
            TypeSpec ts = base_role->get_typespec(tm);
            value = ts.lookup_inner(n, v, s);
        }

        if (dot && value) {
            RoleValue *role_value = ptr_cast<RoleValue>(value);
                
            if (role_value) {
                role_value_be_static(role_value);
            }
            else {
                std::cerr << "Static cast can only be used on roles!\n";
                value = NULL;
            }
        }
                
        return value;
    }

    virtual Value *lookup_matcher(TypeMatch tm, std::string n, Value *v, Scope *s) {
        return make<ClassMatcherValue>(n, v);
    }
};


class Role: public Associable {
public:
    devector<VirtualEntry *> vt;
    Label vt_label;

    Role(std::string n, TypeSpec pts, TypeSpec ts, InheritAs ia)
        :Associable(n, pts, ts, ia) {
        std::cerr << "Creating " << (ia == AS_BASE ? "base " : ia == AS_AUTO ? "auto " : "") << "role " << name << ".\n";
        
        inherit();
    }

    Role(std::string p, Associable *original, TypeMatch etm)
        :Associable(p, original, etm) {
        std::cerr << "Creating shadow role " << name << ".\n";

        inherit();
    }

    virtual bool is_abstract() {
        return ptr_cast<InterfaceType>(alloc_ts[0]) != NULL;
    }

    virtual Associable *shadow(Associable *original) {
        return new Role(prefix, original, explicit_tm);
    }

    virtual Value *matched(Value *cpivot, Scope *scope, TypeMatch &match) {
        //std::cerr << "XXX Role matched " << name << " with " << typeidname(cpivot) << "\n";
        return make<RoleValue>(this, cpivot, match);
    }

    virtual Value *make_value(Value *orig, TypeMatch tm) {
        return make<RoleValue>(this, orig, tm);
    }

    virtual devector<VirtualEntry *> get_virtual_table() {
        return vt;
    }

    virtual void override_virtual_entry(int vi, VirtualEntry *ve) {
        vt.set(vi, ve);  // FIXME: not good for cloned VT entries!
    }

    virtual Scope *get_target_inner_scope() {
        ClassType *ct = ptr_cast<ClassType>(alloc_ts[0]);
        return ct->inner_scope.get();
    }

    virtual void allocate() {
        // Only called for explicitly declared roles
        
        if (original_associable)
            throw INTERNAL_ERROR;
            
        Allocable::allocate();
        where = MEMORY;
        
        // Data will be allocated in the class scope
        Allocation size = alloc_ts.measure_identity();
        offset = associating_scope->reserve(size);
        
        if (inherit_as == AS_BASE && name != "main") {
            // Sanity check
            if (offset.concretize() != 0)
                throw INTERNAL_ERROR;
        }
            
        // Virtual table will be allocated separately, except for bases
        vt = alloc_ts.get_virtual_table();
        
        if (inherit_as == AS_BASE) {
            // Clone base VT, it won't be compiled separately
            for (int i = 2; i < vt.high(); i++)
                associating_scope->virtual_reserve(vt.get(i));
        }
        else {
            fastforward_ve = new FfwdVirtualEntry(offset);
            vt.set(VT_FASTFORWARD_INDEX, fastforward_ve);
        }

        for (auto &sr : shadow_associables)
            sr->relocate(offset);
    }
    
    virtual void relocate(Allocation explicit_offset) {
        // Only called for shadow roles

        if (!original_associable)
            throw INTERNAL_ERROR;

        where = MEMORY;
        
        // Offset within the current class, in terms of its type parameters
        offset = explicit_offset + allocsubst(original_associable->offset, explicit_tm);
            
        //virtual_offset = explicit_virtual_offset + original_associable->virtual_offset;
        
        // Virtual table will be allocated separately
        vt = original_associable->get_virtual_table();
        
        fastforward_ve = new FfwdVirtualEntry(offset);
        vt.set(VT_FASTFORWARD_INDEX, fastforward_ve);
        
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

    virtual void compile_vt(TypeMatch tm, std::string tname, X64 *x64) {
        if (!original_associable && inherit_as == AS_BASE) {
            std::cerr << "Skipping explicit base VT compilation.\n";
        }
        else {
            std::string symbol = tname + "." + name + "_virtual_table";
            ::compile_virtual_table(vt, tm, vt_label, symbol, x64);
        }

        for (auto &sr : shadow_associables)
            sr->compile_vt(tm, tname, x64);
    }
    
    virtual void init_vt(TypeMatch tm, Address self_addr, X64 *x64) {
        // Base roles have a VT pointer overlapping the main class VT, don't overwrite
        if (!original_associable && inherit_as == AS_BASE) {
            std::cerr << "Skipping explicit base VT initialization.\n";
        }
        else {
            x64->op(LEA, R10, Address(vt_label, 0));
            x64->op(MOVQ, self_addr + offset.concretize(tm) + CLASS_VT_OFFSET, R10);
        }

        for (auto &sr : shadow_associables)
            ptr_cast<Role>(sr.get())->init_vt(tm, self_addr, x64);
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

