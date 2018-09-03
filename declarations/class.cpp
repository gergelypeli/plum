
class ClassType: public HeapType, public VirtualEntry {
public:
    std::vector<Allocable *> member_allocables;  // FIXME: not necessary Variables
    std::vector<TypeSpec> member_tss;  // rvalues, for the initializer arguments
    std::vector<std::string> member_names;
    Function *finalizer_function;
    BaseRole *base_role;

    ClassType(std::string name, Metatypes param_metatypes)
        :HeapType(name, param_metatypes, class_metatype) {
        finalizer_function = NULL;
        base_role = NULL;
    }

    virtual bool complete_type() {
        for (auto &c : inner_scope->contents) {
            Allocable *v = ptr_cast<Allocable>(c.get());
            
            if (v) {
                member_allocables.push_back(v);
                member_tss.push_back(v->alloc_ts.rvalue());
                member_names.push_back(v->name);
            }
            
            Function *f = ptr_cast<Function>(c.get());
            
            if (f && f->type == FINALIZER_FUNCTION) {
                if (finalizer_function) {
                    std::cerr << "Multiple finalizers!\n";
                    return false;
                }
                    
                finalizer_function = f;
            }
            
            BaseRole *b = ptr_cast<BaseRole>(c.get());
            
            if (b) {
                if (base_role) {
                    std::cerr << "Multiple base roles!\n";
                    return false;
                }
                
                base_role = b;
            }
        }
        
        std::cerr << "Class " << name << " has " << member_allocables.size() << " member variables.\n";
        return true;
    }

    virtual void allocate() {
        // Let the base be allocated first, then it will skip itself
        
        if (base_role) {
            base_role->allocate();
            inner_scope->set_virtual_entry(0, this);
        }
        else {
            std::vector<VirtualEntry *> vt = { this };
            inner_scope->virtual_reserve(vt);
        }
            
        HeapType::allocate();
    }

    virtual Label get_virtual_entry_label(TypeMatch tm, X64 *x64) {
        // This must point to our base type's virtual table, or NULL
        
        if (base_role)
            return typesubst(base_role->alloc_ts, tm).get_virtual_table_label(x64);
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
            Label no_weakrefs;
            x64->op(CMPQ, Address(RAX, ROLE_WEAKREFCOUNT_OFFSET), 0);
            x64->op(JE, no_weakrefs);
            x64->runtime->die("Weakly referenced role finalized!");
            x64->code_label(no_weakrefs);
        }
        else
            throw INTERNAL_ERROR;
    }

    virtual Value *lookup_partinitializer(TypeMatch tm, std::string name, Value *pivot) {
        //TypeSpec ts(tsi);

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
            
            Value *preinit = pivot ? pivot : make<ClassPreinitializerValue>(rts);

            Value *value = inner_scope->lookup(name, preinit);

            if (value) {
                if (is_initializer_function_call(value))
                    return pivot ? value : make<ClassPostinitializerValue>(value);
                        
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

    virtual std::vector<TypeSpec> get_member_tss(TypeMatch &match) {
        std::vector<TypeSpec> tss;
        for (auto &ts : member_tss)
            tss.push_back(typesubst(ts, match));
        return tss;
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

        ts.destroy(Storage(MEMORY, Address(RAX, 0)), x64);

        x64->op(RET);
    }

    virtual void init_vt(TypeMatch tm, Address addr, int data_offset, Label vt_label, int virtual_offset, X64 *x64) {
        x64->op(LEA, RBX, Address(vt_label, virtual_offset * ADDRESS_SIZE));
        x64->op(MOVQ, addr + data_offset + CLASS_VT_OFFSET, RBX);

        for (auto &var : member_allocables) {
            Role *r = ptr_cast<Role>(var);
            
            if (r)
                r->init_vt(tm, addr, data_offset, vt_label, virtual_offset, x64);
        }
    }

    virtual Value *autoconv(TypeMatch tm, TypeSpecIter target, Value *orig, TypeSpec &ifts) {
        if (tm[0][0] == *target) {
            ifts = tm[0];
            
            TypeSpec ts = get_typespec(orig).rvalue();
            Type *t = ts[0];
            
            if (t == ref_type || t == weakref_type)
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
            TypeSpec ts = typesubst(base_role->alloc_ts, tm);
            Value *v = ts.autoconv(target, orig, ifts);
            
            if (v)
                return v;
        }
        
        return HeapType::autoconv(tm, target, orig, ifts);
    }

    virtual Value *lookup_inner(TypeMatch tm, std::string n, Value *v) {
        std::cerr << "Class inner lookup " << n << ".\n";
        
        Value *value = HeapType::lookup_inner(tm, n, v);
        
        if (!value && base_role) {
            TypeSpec ts = typesubst(base_role->alloc_ts, tm);
            value = ts.lookup_inner(n, v);
        }
        
        return value;
    }

    virtual Value *lookup_matcher(TypeMatch tm, std::string n, Value *v) {
        return make<ClassMatcherValue>(n, v);
    }
};


class StackType: public ClassType {
public:
    StackType(std::string name)
        :ClassType(name, Metatypes { value_metatype }) {
    }
    
    virtual Value *lookup_partinitializer(TypeMatch tm, std::string name, Value *pivot) {
        TypeSpec ats = tm[0].unprefix(stack_type).prefix(array_type);
        Value *array_initializer = ats.lookup_initializer(name);
        
        if (!array_initializer) {
            std::cerr << "No Stack initializer called " << name << "!\n";
            return NULL;
        }

        TypeSpec rts = tm[0].prefix(ref_type);
        
        if (!pivot)
            pivot = make<ClassPreinitializerValue>(rts);
        
        return make<ClassWrapperInitializerValue>(pivot, array_initializer);
    }
};


class QueueType: public ClassType {
public:
    QueueType(std::string name)
        :ClassType(name, Metatypes { value_metatype }) {
    }
    
    virtual Value *lookup_partinitializer(TypeMatch tm, std::string name, Value *pivot) {
        TypeSpec cts = tm[0].unprefix(queue_type).prefix(circularray_type);
        Value *carray_initializer = cts.lookup_initializer(name);
        
        if (!carray_initializer) {
            std::cerr << "No Queue initializer called " << name << "!\n";
            return NULL;
        }

        TypeSpec rts = tm[0].prefix(ref_type);
        
        if (!pivot)
            pivot = make<ClassPreinitializerValue>(rts);
        
        return make<ClassWrapperInitializerValue>(pivot, carray_initializer);
    }
};


class SetType: public ClassType {
public:
    SetType(std::string name)
        :ClassType(name, Metatypes { value_metatype }) {
    }
    
    virtual Value *lookup_partinitializer(TypeMatch tm, std::string name, Value *pivot) {
        TypeSpec tts = tm[0].unprefix(set_type).prefix(rbtree_type);
        Value *tree_initializer = tts.lookup_initializer(name);
        
        if (!tree_initializer) {
            std::cerr << "No Set initializer called " << name << "!\n";
            return NULL;
        }

        TypeSpec rts = tm[0].prefix(ref_type);
        
        if (!pivot)
            pivot = make<ClassPreinitializerValue>(rts);
        
        return make<ClassWrapperInitializerValue>(pivot, tree_initializer);
    }
};


class MapType: public ClassType {
public:
    MapType(std::string name, Metatypes param_metatypes)
        :ClassType(name, param_metatypes) {
    }
    
    virtual Value *lookup_map_partinitializer(TypeSpec real_ts, TypeSpec key_ts, TypeSpec value_ts, std::string name, Value *pivot) {
        TypeSpec tree_ts = TypeSpec(item_type, key_ts, value_ts).prefix(rbtree_type);
        Value *tree_initializer = tree_ts.lookup_initializer(name);
        
        if (!tree_initializer) {
            std::cerr << "No " << this->name << " initializer called " << name << "!\n";
            return NULL;
        }

        // This may be something non-Map, if subclasses call this.
        TypeSpec rts = real_ts.prefix(ref_type);
        
        if (!pivot)
            pivot = make<ClassPreinitializerValue>(rts);
        
        return make<ClassWrapperInitializerValue>(pivot, tree_initializer);
    }
    
    virtual Value *lookup_partinitializer(TypeMatch tm, std::string name, Value *pivot) {
        return lookup_map_partinitializer(tm[0], tm[1], tm[2], name, pivot);
    }
};


class WeakValueMapType: public MapType {
public:
    WeakValueMapType(std::string name, Metatypes param_metatypes)
        :MapType(name, param_metatypes) {
    }
    
    virtual Value *lookup_partinitializer(TypeMatch tm, std::string name, Value *pivot) {
        return MapType::lookup_map_partinitializer(tm[0], tm[1], tm[2].prefix(weakanchor_type), name, pivot);
    }
};


class WeakIndexMapType: public MapType {
public:
    WeakIndexMapType(std::string name, Metatypes param_metatypes)
        :MapType(name, param_metatypes) {
    }
    
    virtual Value *lookup_partinitializer(TypeMatch tm, std::string name, Value *pivot) {
        return MapType::lookup_map_partinitializer(tm[0], tm[1].prefix(weakanchor_type), tm[2], name, pivot);
    }
};


class WeakSetType: public MapType {
public:
    WeakSetType(std::string name, Metatypes param_metatypes)
        :MapType(name, param_metatypes) {
    }
    
    virtual Value *lookup_partinitializer(TypeMatch tm, std::string name, Value *pivot) {
        return MapType::lookup_map_partinitializer(tm[0], tm[1].prefix(weakanchor_type), UNIT_TS, name, pivot);
    }
};
