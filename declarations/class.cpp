
class VtVirtualEntry: public VirtualEntry {
public:
    Allocable *allocable;
    
    VtVirtualEntry(Allocable *a) {
        allocable = a;
    }
    
    virtual Label get_virtual_entry_label(TypeMatch tm, X64 *x64) {
        if (allocable)
            return allocable->get_typespec(tm).get_virtual_table_label(x64);
        else
            return x64->runtime->zero_label;
    }
};


class FfwdVirtualEntry: public VirtualEntry {
public:
    Allocation offset;
    
    FfwdVirtualEntry(Allocation o) {
        offset = o;
    }
    
    virtual Label get_virtual_entry_label(TypeMatch tm, X64 *x64) {
        Label label;
        x64->absolute_label(label, -offset.concretize(tm));  // forcing an int into an unsigned64...
        return label;
    }
};


class ClassType: public HeapType {
public:
    std::vector<Allocable *> member_allocables;
    std::vector<std::string> member_names;
    std::vector<Role *> member_roles;
    Function *finalizer_function;
    Allocable *base_role;
    VtVirtualEntry *basevt_ve;
    FfwdVirtualEntry *fastforward_ve;

    ClassType(std::string name, Metatypes param_metatypes)
        :HeapType(name, param_metatypes, class_metatype) {
        finalizer_function = NULL;
        base_role = NULL;
        basevt_ve = NULL;
        fastforward_ve = NULL;
    }

    virtual bool complete_type() {
        for (auto &c : inner_scope->contents) {
            Allocable *v = ptr_cast<Allocable>(c.get());
            
            if (v) {
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
                
                    base_role = ptr_cast<Allocable>(r);
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
        }
        
        std::cerr << "Class " << name << " has " << member_allocables.size() << " member variables.\n";
        return true;
    }

    virtual void allocate() {
        // Let the base be allocated first, then it will skip itself

        basevt_ve = new VtVirtualEntry(base_role);
        fastforward_ve = new FfwdVirtualEntry(Allocation(0));
        
        if (base_role) {
            base_role->allocate();
            inner_scope->set_virtual_entry(VT_BASEVT_INDEX, basevt_ve);
            inner_scope->set_virtual_entry(VT_FASTFORWARD_INDEX, fastforward_ve);
        }
        else {
            std::vector<VirtualEntry *> vt = { basevt_ve, fastforward_ve };
            int virtual_index = inner_scope->virtual_reserve(vt);
            
            if (virtual_index != VT_BASEVT_INDEX)
                throw INTERNAL_ERROR;
                
            if (virtual_index + 1 != VT_FASTFORWARD_INDEX)
                throw INTERNAL_ERROR;
        }
        
        HeapType::allocate();
    }

    virtual Allocation measure(TypeMatch tm) {
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
        DataScope *is = HeapType::make_inner_scope(pts);

        is->be_virtual_scope();
        
        Allocation vt_offset = is->reserve(Allocation(CLASS_HEADER_SIZE));  // VT pointer
        if (vt_offset.bytes != CLASS_VT_OFFSET)  // sanity check
            throw INTERNAL_ERROR;
        
        return is;
    }

    virtual std::vector<std::string> get_member_names() {
        return member_names;
    }

    virtual std::vector<VirtualEntry *> get_virtual_table(TypeMatch tm) {
        return inner_scope->get_virtual_table();
    }

    virtual Label get_virtual_table_label(TypeMatch tm, X64 *x64) {
        return x64->once->compile(compile_virtual_table, tm[0]);
    }

    virtual Label get_finalizer_label(TypeMatch tm, X64 *x64) {
        return x64->once->compile(compile_finalizer, tm[0]);
    }
    
    static void compile_virtual_table(Label label, TypeSpec ts, X64 *x64) {
        std::vector<VirtualEntry *> vt = ts.get_virtual_table();
        TypeMatch tm = ts.match();

        x64->data_align(8);
        
        std::stringstream ss;
        ss << ts[0]->name << "_virtual_table";
        x64->data_label_local(label, ss.str());

        for (auto entry : vt) {
            Label l = entry->get_virtual_entry_label(tm, x64);
            //std::cerr << "Virtual entry of " << ts[0]->name << " is " << l.def_index << ".\n";
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
        
        return HeapType::autoconv(tm, target, orig, ifts, assume_lvalue);
    }

    virtual Value *lookup_inner(TypeMatch tm, std::string n, Value *v, Scope *s) {
        std::cerr << "Class " << name << " inner lookup " << n << ".\n";
        bool dot = false;
        
        if (n[0] == '.') {
            dot = true;
            n = n.substr(1);
        }
        
        Value *value = HeapType::lookup_inner(tm, n, v, s);
        
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
    
    virtual void incref(TypeMatch tm, Register r, X64 *x64) {
        x64->op(PUSHQ, r);
        x64->op(MOVQ, r, Address(r, CLASS_VT_OFFSET));
        x64->op(MOVQ, r, Address(r, VT_FASTFORWARD_INDEX * ADDRESS_SIZE));
        x64->op(ADDQ, r, Address(RSP, 0));
        x64->runtime->incref(r);
        x64->op(POPQ, r);
    }

    virtual void decref(TypeMatch tm, Register r, X64 *x64) {
        x64->op(PUSHQ, r);
        x64->op(MOVQ, r, Address(r, CLASS_VT_OFFSET));
        x64->op(MOVQ, r, Address(r, VT_FASTFORWARD_INDEX * ADDRESS_SIZE));
        x64->op(ADDQ, r, Address(RSP, 0));
        x64->runtime->decref(r);
        x64->op(POPQ, r);
    }
};


class Role: public Allocable, public Associable {
public:
    std::string prefix;
    InheritAs inherit_as;
    Role *original_role;
    int virtual_offset;
    std::vector<std::unique_ptr<Role>> shadow_roles;
    DataScope *virtual_scope;  // All shadow Role-s will point to the class scope
    FfwdVirtualEntry *fastforward_ve;
    Lself *associated_lself;
    TypeMatch explicit_tm;
    
    Role(std::string n, TypeSpec pts, TypeSpec ts, InheritAs ia)
        :Allocable(n, pts, ts) {
        std::cerr << "Creating " << (ia == AS_BASE ? "base " : ia == AS_AUTO ? "auto " : "") << "role " << name << ".\n";
        
        prefix = name + ".";
        inherit_as = ia;
        original_role = NULL;
        virtual_offset = -1;
        virtual_scope = NULL;
        fastforward_ve = NULL;
        associated_lself = NULL;
        explicit_tm = alloc_ts.match();

        ClassType *ct = ptr_cast<ClassType>(alloc_ts[0]);
        if (!ct)
            throw INTERNAL_ERROR;

        for (auto &r : ct->member_roles) {
            shadow_roles.push_back(std::make_unique<Role>(prefix, r, explicit_tm));
        }
    }

    Role(std::string p, Role *role, TypeMatch etm)
        :Allocable(p + role->name, NO_TS, typesubst(role->alloc_ts, etm)) {
        std::cerr << "Creating shadow role " << name << ".\n";
        
        prefix = name + ".";
        inherit_as = role->inherit_as;
        original_role = role;
        virtual_offset = -1;
        virtual_scope = NULL;
        fastforward_ve = NULL;
        associated_lself = NULL;
        explicit_tm = etm;

        for (auto &sr : role->shadow_roles) {
            shadow_roles.push_back(std::make_unique<Role>(prefix, sr.get(), etm));
        }
    }

    virtual void set_virtual_scope(DataScope *vs) {
        virtual_scope = vs;
        
        for (auto &sr : shadow_roles) {
            sr->set_virtual_scope(vs);
        }
    }
    
    virtual void set_outer_scope(Scope *os) {
        if (original_role)
            throw INTERNAL_ERROR;
            
        DataScope *ds = ptr_cast<DataScope>(os);
        if (!ds)
            throw INTERNAL_ERROR;
            
        if (!ds->is_virtual_scope())
            throw INTERNAL_ERROR;
            
        set_virtual_scope(ds);
    }
    
    virtual Value *matched(Value *cpivot, Scope *scope, TypeMatch &match) {
        //std::cerr << "XXX Role matched " << name << " with " << typeidname(cpivot) << "\n";
        return make<RoleValue>(this, cpivot, match);
    }

    virtual Associable *lookup_associable(std::string n) {
        if (n == name)
            return this;
        else if (has_prefix(n, prefix)) {
            for (auto &sr : shadow_roles) {
                Associable *a = sr->lookup_associable(n);
                
                if (a)
                    return a;
            }
        }
        
        return NULL;
    }

    virtual bool check_associated(Declaration *d) {
        Function *override = ptr_cast<Function>(d);
        if (!override) {
            std::cerr << "This declaration can't be associated with role " << name << "!\n";
            return false;
        }
        
        Declaration *decl = NULL;
        ClassType *ct = ptr_cast<ClassType>(alloc_ts[0]);
        std::string override_name = override->name;
        
        if (!deprefix(override_name, prefix))
            throw INTERNAL_ERROR;
        
        for (auto &d : ct->inner_scope->contents) {
            if (d->is_called(override_name)) {
                decl = d.get();
                break;
            }
        }
        
        if (!decl) {
            std::cerr << "No function to override!\n";
            return NULL;
        }
        
        Function *original = ptr_cast<Function>(decl);
        
        if (!original) {
            std::cerr << "Not a function to override!\n";
            return false;
        }

        TypeMatch role_tm;  // assume parameterless outermost class, derive role parameters
        //containing_role->compute_match(role_tm);

        // this automatically sets original_function
        if (!override->does_implement(prefix, TypeMatch(), original, role_tm))
            return false;
        
        override->set_associated_role(this);
        
        return true;
    }

    virtual bool is_autoconv() {
        return inherit_as != AS_ROLE;
    }

    virtual bool is_baseconv() {
        return inherit_as == AS_BASE;
    }

    virtual Value *autoconv(TypeMatch tm, Type *target, Value *orig, TypeSpec &ifts, bool assume_lvalue) {
        if (associated_lself && !assume_lvalue)
            return NULL;

        ifts = typesubst(alloc_ts, tm);  // pivot match

        if (ifts[0] == target) {
            // Direct implementation
            //std::cerr << "Found direct implementation.\n";
            return make<RoleValue>(this, orig, tm);
        }
        else if (inherit_as == AS_BASE) {
            //std::cerr << "Trying indirect implementation with " << ifts << "\n";
            for (auto &sr : shadow_roles) {
                if (sr->inherit_as == AS_ROLE)
                    continue;
                    
                Value *v = sr->autoconv(tm, target, orig, ifts, assume_lvalue);
                
                if (v)
                    return v;
            }
        }
        
        return NULL;
    }

    virtual void allocate() {
        // Only called for explicitly declared roles
        
        if (original_role)
            throw INTERNAL_ERROR;
            
        if (virtual_offset != -1) {
            if (inherit_as == AS_BASE)
                return;
            else
                throw INTERNAL_ERROR;
        }
        
        Allocable::allocate();
        where = MEMORY;
        
        Allocation size = alloc_ts.measure();
        offset = virtual_scope->reserve(size);
            
        std::vector<VirtualEntry *> vt = alloc_ts.get_virtual_table();
        virtual_offset = virtual_scope->virtual_reserve(vt);

        fastforward_ve = new FfwdVirtualEntry(offset);
        virtual_scope->set_virtual_entry(virtual_offset + VT_FASTFORWARD_INDEX, fastforward_ve);

        for (auto &sr : shadow_roles)
            sr->relocate(offset, virtual_offset);
    }
    
    virtual void relocate(Allocation explicit_offset, int explicit_virtual_offset) {
        // Only called for shadow roles

        if (!original_role)
            throw INTERNAL_ERROR;

        if (virtual_offset != -1)
            throw INTERNAL_ERROR;
        
        // Offset within the current class, in terms of its type parameters
        offset = explicit_offset + allocsubst(original_role->offset, explicit_tm);
            
        virtual_offset = explicit_virtual_offset + original_role->virtual_offset;
        
        fastforward_ve = new FfwdVirtualEntry(offset);
        virtual_scope->set_virtual_entry(virtual_offset + VT_FASTFORWARD_INDEX, fastforward_ve);
        
        for (auto &sr : shadow_roles)
            sr->relocate(explicit_offset, explicit_virtual_offset);
    }

    virtual void destroy(TypeMatch tm, Storage s, X64 *x64) {
        if (original_role)
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

        for (auto &sr : shadow_roles)
            sr->init_vt(tm, self_addr, vt_label, x64);
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

/*
class ClassWrapperAltStreamifiableImplementation: public AltStreamifiableImplementation {
public:
    TypeSpec wrapped_ts;
    
    ClassWrapperAltStreamifiableImplementation(std::string name, TypeSpec pts, TypeSpec wts)
        :AltStreamifiableImplementation(name, pts) {
        wrapped_ts = wts;
    }
    
    virtual void streamify(TypeMatch tm, X64 *x64) {
        TypeSpec member_ts = typesubst(wrapped_ts, tm);
        
        x64->op(MOVQ, RAX, Address(RSP, ADDRESS_SIZE));
        x64->op(MOVQ, RBX, Address(RSP, 0));
        
        member_ts.store(Storage(MEMORY, Address(RAX, CLASS_MEMBERS_OFFSET)), Storage(STACK), x64);
        x64->op(PUSHQ, RBX);
        
        member_ts.streamify(true, x64);  // clobbers all
        
        x64->op(POPQ, RBX);
        member_ts.store(Storage(STACK), Storage(), x64);
    }
};
*/
/*
class StackType: public WrappedClassType {
public:
    StackType(std::string name)
        :WrappedClassType(name, { value_metatype }, SAME_ARRAY_REF_LVALUE_TS) {
    }
};


class QueueType: public WrappedClassType {
public:
    QueueType(std::string name)
        :WrappedClassType(name, { value_metatype }, SAME_CIRCULARRAY_REF_LVALUE_TS) {
    }
};
*/

class SetType: public WrappedClassType {
public:
    SetType(std::string name)
        :WrappedClassType(name, { value_metatype }, SAME_RBTREE_REF_LVALUE_TS) {
    }
};


class MapType: public WrappedClassType {
public:
    MapType(std::string name)
        :WrappedClassType(name, { value_metatype, value_metatype }, SAME_SAME2_ITEM_RBTREE_REF_LVALUE_TS) {
    }
};


class WeakValueMapType: public WrappedClassType {
public:
    WeakValueMapType(std::string name)
        :WrappedClassType(name, { value_metatype, identity_metatype }, SAME_SAMEID2_NOSYVALUE_ITEM_RBTREE_REF_LVALUE_TS) {
    }
};


class WeakIndexMapType: public WrappedClassType {
public:
    WeakIndexMapType(std::string name)
        :WrappedClassType(name, { identity_metatype, value_metatype }, SAMEID_NOSYVALUE_SAME2_ITEM_RBTREE_REF_LVALUE_TS) {
    }
};


class WeakSetType: public WrappedClassType {
public:
    WeakSetType(std::string name)
        :WrappedClassType(name, { identity_metatype }, SAMEID_NOSYVALUE_UNIT_ITEM_RBTREE_REF_LVALUE_TS) {
    }
};
