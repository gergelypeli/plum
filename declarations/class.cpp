

class ClassType: public InheritableType, public PartialInitializable {
public:
    std::vector<Allocable *> member_allocables;
    std::vector<std::string> member_names;
    std::vector<Role *> member_roles;  // including abstract ones
    std::vector<Function *> member_functions;
    Function *finalizer_function;
    Allocable *base_role;
    VtVirtualEntry *basevt_ve;
    FfwdVirtualEntry *fastforward_ve;
    Implementation *base_implementation;

    ClassType(std::string name, Metatypes param_metatypes)
        :InheritableType(name, param_metatypes, class_metatype) {
        finalizer_function = NULL;
        base_role = NULL;
        basevt_ve = NULL;
        fastforward_ve = NULL;
        base_implementation = NULL;
    }

    virtual bool complete_type() {
        for (auto &c : inner_scope->contents) {
            Allocable *v = ptr_cast<Allocable>(c.get());
            
            if (v && !v->is_abstract()) {
                member_allocables.push_back(v);
                member_names.push_back(v->name);
            }

            Role *r = ptr_cast<Role>(c.get());
            
            if (r) {
                member_roles.push_back(r);
                
                if (ptr_cast<Associable>(r)->is_baseconv()) {
                    if (base_role) {
                        std::cerr << "Multiple base roles!\n";
                        return false;
                    }
                
                    if (base_implementation) {
                        std::cerr << "Sorry!\n";
                        return false;
                    }
                
                    base_role = ptr_cast<Allocable>(r);
                }
            }

            Implementation *i = ptr_cast<Implementation>(c.get());
            
            if (i) {
                // FIXME: shouldn't happen anymore
                throw INTERNAL_ERROR;
                
                if (ptr_cast<Associable>(i)->is_baseconv()) {
                    if (base_implementation)
                        throw INTERNAL_ERROR;

                    if (base_role) {
                        std::cerr << "Sorry!\n";
                        return false;
                    }
                
                    base_implementation = i;
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
        // TODO: now we can only handle either a base role or a base implementation,
        // because VT-s cannot yet grow in both directions.

        basevt_ve = new VtVirtualEntry(base_role);
        fastforward_ve = new FfwdVirtualEntry(Allocation(0));
        
        if (base_role) {
            base_role->allocate();
            inner_scope->set_virtual_entry(VT_BASEVT_INDEX, basevt_ve);
            inner_scope->set_virtual_entry(VT_FASTFORWARD_INDEX, fastforward_ve);
        }
        else {
            inner_scope->virtual_initialize(basevt_ve, fastforward_ve);
            
            // FIXME: must be handled like base_role
            if (base_implementation)
                base_implementation->allocate();
        }
        
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

    virtual DataScope *make_inner_scope(TypeSpec pts) {
        DataScope *is = InheritableType::make_inner_scope(pts);

        is->be_virtual_scope();
        
        Allocation vt_offset = is->reserve(Allocation(CLASS_HEADER_SIZE));  // VT pointer
        if (vt_offset.bytes != CLASS_VT_OFFSET)  // sanity check
            throw INTERNAL_ERROR;
        
        return is;
    }

    virtual std::vector<std::string> get_member_names() {
        return member_names;
    }

    virtual devector<VirtualEntry *> get_virtual_table(TypeMatch tm) {
        return inner_scope->get_virtual_table();
    }

    virtual Label get_virtual_table_label(TypeMatch tm, X64 *x64) {
        return x64->once->compile(compile_virtual_table, tm[0]);
    }

    virtual Label get_finalizer_label(TypeMatch tm, X64 *x64) {
        return x64->once->compile(compile_finalizer, tm[0]);
    }
    
    static void compile_virtual_table(Label label, TypeSpec ts, X64 *x64) {
        devector<VirtualEntry *> vt = ts.get_virtual_table();
        std::cerr << "XXX " << ts << " VT has " << vt.high() - vt.low() << " entries.\n";
        TypeMatch tm = ts.match();

        x64->data_align(8);

        for (int i = vt.low(); i < vt.high(); i++) {
            Label l = vt.get(i)->get_virtual_entry_label(tm, x64);
            //std::cerr << "Virtual entry of " << ts[0]->name << " is " << l.def_index << ".\n";

            if (i == 0)
                x64->data_label_local(label, ts.symbolize() + "_virtual_table");

            x64->data_reference(l);
        }
    }
    
    static void compile_finalizer(Label label, TypeSpec ts, X64 *x64) {
        x64->code_label_local(label, ts[0]->name + "_finalizer");  // FIXME: ambiguous name!

        x64->op(MOVQ, RAX, Address(RSP, ADDRESS_SIZE));  // pointer arg
        ts.destroy(Storage(MEMORY, Address(RAX, 0)), x64);

        x64->op(RET);
    }

    virtual void init_vt(TypeMatch tm, Address self_addr, Label vt_label, X64 *x64) {
        x64->op(LEA, R10, Address(vt_label, 0));
        x64->op(MOVQ, self_addr + CLASS_VT_OFFSET, R10);

        // Roles compute their offsets in terms of the implementor class type parameters
        for (auto &r : member_roles)
            role_init_vt(r, tm, self_addr, vt_label, x64);
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
    Role(std::string n, TypeSpec pts, TypeSpec ts, InheritAs ia)
        :Associable(n, pts, ts, ia) {
        std::cerr << "Creating " << (ia == AS_BASE ? "base " : ia == AS_AUTO ? "auto " : "") << "role " << name << ".\n";
        
        inherit();
        //for (auto &r : ct->member_roles) {
        //    shadow_roles.push_back(std::make_unique<Role>(prefix, r, explicit_tm));
        //}
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

    virtual Scope *get_target_inner_scope() {
        ClassType *ct = ptr_cast<ClassType>(alloc_ts[0]);
        return ct->inner_scope.get();
    }

    virtual void allocate() {
        // Only called for explicitly declared roles
        
        if (original_associable)
            throw INTERNAL_ERROR;
            
        if (virtual_offset != -1) {
            if (inherit_as == AS_BASE)
                return;
            else
                throw INTERNAL_ERROR;
        }
        
        Allocable::allocate();
        where = MEMORY;
        
        Allocation size = alloc_ts.measure_identity();
        offset = associating_scope->reserve(size);
            
        devector<VirtualEntry *> vt = alloc_ts.get_virtual_table();
        virtual_offset = associating_scope->virtual_reserve(vt);
        std::cerr << "Reserved new virtual index " << virtual_offset << " for role " << name << ".\n";

        fastforward_ve = new FfwdVirtualEntry(offset);
        associating_scope->set_virtual_entry(virtual_offset + VT_FASTFORWARD_INDEX, fastforward_ve);

        for (auto &sr : shadow_associables)
            sr->relocate(offset, virtual_offset);
    }
    
    virtual void relocate(Allocation explicit_offset, int explicit_virtual_offset) {
        // Only called for shadow roles

        if (!original_associable)
            throw INTERNAL_ERROR;

        if (virtual_offset != -1)
            throw INTERNAL_ERROR;
        
        // Offset within the current class, in terms of its type parameters
        offset = explicit_offset + allocsubst(original_associable->offset, explicit_tm);
            
        virtual_offset = explicit_virtual_offset + original_associable->virtual_offset;
        
        fastforward_ve = new FfwdVirtualEntry(offset);
        associating_scope->set_virtual_entry(virtual_offset + VT_FASTFORWARD_INDEX, fastforward_ve);
        
        for (auto &sr : shadow_associables)
            sr->relocate(explicit_offset, explicit_virtual_offset);
    }

    virtual void destroy(TypeMatch tm, Storage s, X64 *x64) {
        if (original_associable)
            throw INTERNAL_ERROR;
            
        TypeSpec ts = typesubst(alloc_ts, tm);
        int o = offset.concretize(tm);
        ts.destroy(s + o, x64);
    }
    
    virtual void init_vt(TypeMatch tm, Address self_addr, Label vt_label, X64 *x64) {
        // Base roles have a VT pointer overlapping the main class VT, don't overwrite
        if (inherit_as != AS_BASE) {
            x64->op(LEA, R10, Address(vt_label, virtual_offset * ADDRESS_SIZE));
            x64->op(MOVQ, self_addr + offset.concretize(tm) + CLASS_VT_OFFSET, R10);
        }

        for (auto &sr : shadow_associables)
            ptr_cast<Role>(sr.get())->init_vt(tm, self_addr, vt_label, x64);
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

