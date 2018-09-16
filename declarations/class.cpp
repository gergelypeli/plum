
class ClassType: public HeapType, public VirtualEntry {
public:
    std::vector<Allocable *> member_allocables;  // FIXME: not necessary Variables
    std::vector<TypeSpec> member_tss;  // rvalues, for the initializer arguments
    std::vector<std::string> member_names;
    std::vector<Role *> member_roles;
    Function *finalizer_function;
    Role *base_role;
    AbsoluteVirtualEntry *fastforward_ve;

    ClassType(std::string name, Metatypes param_metatypes)
        :HeapType(name, param_metatypes, class_metatype) {
        finalizer_function = NULL;
        base_role = NULL;
        fastforward_ve = new AbsoluteVirtualEntry;
    }

    virtual bool complete_type() {
        for (auto &c : inner_scope->contents) {
            Allocable *v = ptr_cast<Allocable>(c.get());
            
            if (v) {
                member_allocables.push_back(v);
                member_tss.push_back(v->alloc_ts.rvalue());
                member_names.push_back(v->name);
            }

            Role *r = ptr_cast<Role>(c.get());
            
            if (r) {
                member_roles.push_back(r);
                
                if (role_is_base(r)) {
                    if (base_role) {
                        std::cerr << "Multiple base roles!\n";
                        return false;
                    }
                
                    base_role = r;
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
        
        if (base_role) {
            ptr_cast<Allocable>(base_role)->allocate();
            inner_scope->set_virtual_entry(VT_BASEVT_INDEX, this);
            inner_scope->set_virtual_entry(VT_FASTFORWARD_INDEX, fastforward_ve);
        }
        else {
            std::vector<VirtualEntry *> vt = { this, fastforward_ve };
            int virtual_index = inner_scope->virtual_reserve(vt);
            
            if (virtual_index != VT_BASEVT_INDEX)
                throw INTERNAL_ERROR;
                
            if (virtual_index + 1 != VT_FASTFORWARD_INDEX)
                throw INTERNAL_ERROR;
        }
        
        HeapType::allocate();
    }

    virtual Label get_virtual_entry_label(TypeMatch tm, X64 *x64) {
        // This must point to our base type's virtual table, or NULL
        
        if (base_role)
            return typesubst(ptr_cast<Allocable>(base_role)->alloc_ts, tm).get_virtual_table_label(x64);
        else {
            //std::cerr << "Class " << tm << " has no base.\n";
            return x64->runtime->zero_label;
        }
    }

    virtual Allocation measure(TypeMatch tm) {
        return inner_scope->get_size(tm);
    }

    virtual void store(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        Type::store(tm, s, t, x64);
    }

    virtual void create(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        throw INTERNAL_ERROR;
    }
    
    virtual void destroy(TypeMatch tm, Storage s, X64 *x64) {
        if (s.where == MEMORY) {
            if (finalizer_function) {
                if (s.address.base != RAX || s.address.index != NOREG)
                    throw INTERNAL_ERROR;  // Hoho
                    
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

            // Allow the destroyed members release potential backreferences first
            //Label no_weakrefs;
            //x64->op(CMPQ, Address(RAX, ROLE_WEAKREFCOUNT_OFFSET), 0);
            //x64->op(JE, no_weakrefs);
            //x64->runtime->die("Weakly referenced role finalized!");
            //x64->code_label(no_weakrefs);
        }
        else
            throw INTERNAL_ERROR;
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
        
        //is->set_meta_scope(class_metatype->get_inner_scope());

        Allocation vt_offset = is->reserve(Allocation(CLASS_HEADER_SIZE));  // VT pointer
        if (vt_offset.bytes != CLASS_VT_OFFSET)  // sanity check
            throw INTERNAL_ERROR;
        
        return is;
    }
    /*
    virtual std::vector<TypeSpec> get_member_tss(TypeMatch &match) {
        std::vector<TypeSpec> tss;
        for (auto &ts : member_tss)
            tss.push_back(typesubst(ts, match));
        return tss;
    }
    */
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

        ts.destroy(Storage(MEMORY, Address(RAX, 0)), x64);

        x64->op(RET);
    }

    virtual void init_vt(TypeMatch tm, Address self_addr, Label vt_label, X64 *x64) {
        x64->op(LEA, RBX, Address(vt_label, 0));
        x64->op(MOVQ, self_addr + CLASS_VT_OFFSET, RBX);

        // Roles compute their offsets in terms of the implementor class type parameters
        for (auto &r : member_roles)
            role_init_vt(r, tm, self_addr, vt_label, x64);
    }

    virtual Value *autoconv(TypeMatch tm, TypeSpecIter target, Value *orig, TypeSpec &ifts) {
        if (tm[0][0] == *target) {
            ifts = tm[0];
            
            TypeSpec ts = get_typespec(orig).rvalue();
            Type *t = ts[0];
            
            if (t == ref_type || t == ptr_type)
                ts = ts.unprefix(t);
            else
                throw INTERNAL_ERROR;
                
            if (ts[0] == this)
                return orig;
            else {
                ts = tm[0].prefix(t);
                std::cerr << "Autoconverting a " << get_typespec(orig) << " to " << ts << ".\n";
                return make<CastValue>(orig, ts);
            }
        }
        
        if (base_role) {
            TypeSpec ts = typesubst(ptr_cast<Allocable>(base_role)->alloc_ts, tm);
            Value *v = ts.autoconv(target, orig, ifts);
            
            if (v)
                return v;
        }
        
        return HeapType::autoconv(tm, target, orig, ifts);
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
            TypeSpec ts = typesubst(ptr_cast<Allocable>(base_role)->alloc_ts, tm);
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


class Role: public Allocable {
public:
    std::string prefix;
    bool is_base;
    Role *original_role;
    int virtual_offset;
    std::vector<std::unique_ptr<Role>> shadow_roles;
    DataScope *virtual_scope;  // All shadow Role-s will point to the class scope
    AbsoluteVirtualEntry *fastforward_ve;
    
    Role(std::string n, TypeSpec pts, TypeSpec ts, bool ib)
        :Allocable(n, pts, ts) {
        std::cerr << "Creating " << (ib ? "base " : "") << "role " << name << ".\n";
        
        prefix = name + ".";
        is_base = ib;
        original_role = NULL;
        virtual_offset = -1;
        virtual_scope = NULL;
        fastforward_ve = new AbsoluteVirtualEntry;

        ClassType *ct = ptr_cast<ClassType>(alloc_ts[0]);
        if (!ct)
            throw INTERNAL_ERROR;

        for (auto &r : ct->member_roles) {
            shadow_roles.push_back(std::make_unique<Role>(prefix, r));
        }
    }

    Role(std::string p, Role *role)
        :Allocable(p + role->name, NO_TS, role->alloc_ts) {
        std::cerr << "Creating shadow role " << name << ".\n";
        
        prefix = name + ".";
        is_base = false;
        original_role = role;
        virtual_offset = -1;
        fastforward_ve = new AbsoluteVirtualEntry;

        for (auto &sr : role->shadow_roles) {
            shadow_roles.push_back(std::make_unique<Role>(prefix, sr.get()));
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
        std::cerr << "XXX Role matched " << name << " with " << typeidname(cpivot) << "\n";
        return make<RoleValue>(this, cpivot, match);
    }

    virtual Role *lookup_role(std::string name) {
        std::string n = name;
        
        if (deprefix(n, prefix)) {
            if (n.find('.') != std::string::npos) {
                for (auto &sr : shadow_roles) {
                    Role *r = sr->lookup_role(name);
                    if (r)
                        return r;
                }
                
                return NULL;
            }
            else
                return this;
        }
        else
            return NULL;
    }

    virtual bool check_override(Function *override) {
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
        
        return true;
    }

    virtual void allocate() {
        // Only called for explicitly declared roles
        
        if (original_role)
            throw INTERNAL_ERROR;
            
        if (virtual_offset != -1) {
            if (is_base)
                return;
            else
                throw INTERNAL_ERROR;
        }
        
        where = MEMORY;
        
        Allocation size = alloc_ts.measure();
        offset = virtual_scope->reserve(size);
            
        std::vector<VirtualEntry *> vt = alloc_ts.get_virtual_table();
        virtual_offset = virtual_scope->virtual_reserve(vt);

        fastforward_ve->set(-offset.concretize());  // well, this is not parametrized yet
        virtual_scope->set_virtual_entry(virtual_offset + VT_FASTFORWARD_INDEX, fastforward_ve);

        // Just in case we'll have parametrized classes
        // Let the shadow roles express their offsets in terms of the implementor type
        // parameters. The offsets they can grab from their original scopes are expressed
        // in terms of our parameters, so calculate and pass the transformation matrix
        // so thay can calculate the required size polynoms.
        TypeMatch tm = alloc_ts.match();
        Allocation size1 = (tm[1] != NO_TS ? tm[1].measure() : Allocation());
        Allocation size2 = (tm[2] != NO_TS ? tm[2].measure() : Allocation());
        Allocation size3 = (tm[3] != NO_TS ? tm[3].measure() : Allocation());

        // Every concretization step rounds the concrete size up
        size1.bytes = stack_size(size1.bytes);
        size2.bytes = stack_size(size2.bytes);
        size3.bytes = stack_size(size3.bytes);

        for (auto &sr : shadow_roles)
            sr->relocate(size1, size2, size3, offset, virtual_offset);
    }
    
    virtual void relocate(Allocation size1, Allocation size2, Allocation size3, Allocation explicit_offset, int explicit_virtual_offset) {
        // Only called for shadow roles

        if (!original_role)
            throw INTERNAL_ERROR;

        if (virtual_offset != -1)
            throw INTERNAL_ERROR;
        
        // Offset within the explicit role, in terms of the role type parameters
        Allocation o = original_role->offset;
        
        // Offset within the current class, in terms of its type parameters
        offset = Allocation(
            o.bytes + o.count1 * size1.bytes  + o.count2 * size2.bytes  + o.count3 * size3.bytes  + explicit_offset.bytes,
            0 +       o.count1 * size1.count1 + o.count2 * size2.count1 + o.count3 * size3.count1 + explicit_offset.count1,
            0 +       o.count1 * size1.count2 + o.count2 * size2.count2 + o.count3 * size3.count2 + explicit_offset.count2,
            0 +       o.count1 * size1.count3 + o.count2 * size2.count3 + o.count3 * size3.count3 + explicit_offset.count3
        );
            
        virtual_offset = original_role->virtual_offset + explicit_virtual_offset;
        
        fastforward_ve->set(-offset.concretize());  // well, this is not parametrized yet
        virtual_scope->set_virtual_entry(virtual_offset + VT_FASTFORWARD_INDEX, fastforward_ve);
        
        for (auto &sr : shadow_roles)
            sr->relocate(size1, size2, size3, explicit_offset, explicit_virtual_offset);
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
        if (!is_base) {
            x64->op(LEA, RBX, Address(vt_label, virtual_offset * ADDRESS_SIZE));
            x64->op(MOVQ, self_addr + offset.concretize(tm) + CLASS_VT_OFFSET, RBX);
        }

        for (auto &sr : shadow_roles)
            sr->init_vt(tm, self_addr, vt_label, x64);
    }
};


class StackType: public ClassType {
public:
    StackType(std::string name)
        :ClassType(name, Metatypes { value_metatype }) {
    }
    
    virtual Value *lookup_initializer(TypeMatch tm, std::string name, Scope *scope) {
        TypeSpec ats = tm[0].unprefix(stack_type).prefix(array_type);
        Value *array_initializer = ats.lookup_initializer(name, scope);
        
        if (!array_initializer) {
            std::cerr << "No Stack initializer called " << name << "!\n";
            return NULL;
        }

        TypeSpec rts = tm[0].prefix(ref_type);
        
        Value *pivot = make<ClassPreinitializerValue>(rts);
        
        return make<ClassWrapperInitializerValue>(pivot, array_initializer);
    }
};


class QueueType: public ClassType {
public:
    QueueType(std::string name)
        :ClassType(name, Metatypes { value_metatype }) {
    }
    
    virtual Value *lookup_initializer(TypeMatch tm, std::string name, Scope *scope) {
        TypeSpec cts = tm[0].unprefix(queue_type).prefix(circularray_type);
        Value *carray_initializer = cts.lookup_initializer(name, scope);
        
        if (!carray_initializer) {
            std::cerr << "No Queue initializer called " << name << "!\n";
            return NULL;
        }

        TypeSpec rts = tm[0].prefix(ref_type);
        
        Value *pivot = make<ClassPreinitializerValue>(rts);
        
        return make<ClassWrapperInitializerValue>(pivot, carray_initializer);
    }
};


class SetType: public ClassType {
public:
    SetType(std::string name)
        :ClassType(name, Metatypes { value_metatype }) {
    }
    
    virtual Value *lookup_initializer(TypeMatch tm, std::string name, Scope *scope) {
        TypeSpec tts = tm[0].unprefix(set_type).prefix(rbtree_type);
        Value *tree_initializer = tts.lookup_initializer(name, scope);
        
        if (!tree_initializer) {
            std::cerr << "No Set initializer called " << name << "!\n";
            return NULL;
        }

        TypeSpec rts = tm[0].prefix(ref_type);
        
        Value *pivot = make<ClassPreinitializerValue>(rts);
        
        return make<ClassWrapperInitializerValue>(pivot, tree_initializer);
    }
};


class MapType: public ClassType {
public:
    MapType(std::string name, Metatypes param_metatypes)
        :ClassType(name, param_metatypes) {
    }
    
    virtual Value *lookup_map_initializer(TypeSpec real_ts, TypeSpec key_ts, TypeSpec value_ts, std::string name, Scope *scope) {
        TypeSpec tree_ts = TypeSpec(item_type, key_ts, value_ts).prefix(rbtree_type);
        Value *tree_initializer = tree_ts.lookup_initializer(name, scope);
        
        if (!tree_initializer) {
            std::cerr << "No " << this->name << " initializer called " << name << "!\n";
            return NULL;
        }

        // This may be something non-Map, if subclasses call this.
        TypeSpec rts = real_ts.prefix(ref_type);
        
        Value *pivot = make<ClassPreinitializerValue>(rts);
        
        return make<ClassWrapperInitializerValue>(pivot, tree_initializer);
    }
    
    virtual Value *lookup_initializer(TypeMatch tm, std::string name, Scope *scope) {
        return lookup_map_initializer(tm[0], tm[1], tm[2], name, scope);
    }
};


class WeakValueMapType: public MapType {
public:
    WeakValueMapType(std::string name, Metatypes param_metatypes)
        :MapType(name, param_metatypes) {
    }
    
    virtual Value *lookup_initializer(TypeMatch tm, std::string name, Scope *scope) {
        return MapType::lookup_map_initializer(tm[0], tm[1], tm[2].prefix(nosyvalue_type), name, scope);
    }
};


class WeakIndexMapType: public MapType {
public:
    WeakIndexMapType(std::string name, Metatypes param_metatypes)
        :MapType(name, param_metatypes) {
    }
    
    virtual Value *lookup_initializer(TypeMatch tm, std::string name, Scope *scope) {
        return MapType::lookup_map_initializer(tm[0], tm[1].prefix(nosyvalue_type), tm[2], name, scope);
    }
};


class WeakSetType: public MapType {
public:
    WeakSetType(std::string name, Metatypes param_metatypes)
        :MapType(name, param_metatypes) {
    }
    
    virtual Value *lookup_initializer(TypeMatch tm, std::string name, Scope *scope) {
        return MapType::lookup_map_initializer(tm[0], tm[1].prefix(nosyvalue_type), UNIT_TS, name, scope);
    }
};
