
class SingletonType: public Type {
public:
    std::vector<Allocable *> member_allocables;
    //std::vector<TypeSpec> member_tss;  // rvalues, for the initializer arguments
    std::vector<std::string> member_names;
    Function *initializer_function;
    Function *finalizer_function;
    //BaseRole *base_role;

    SingletonType(std::string name)
        :Type(name, {}, singleton_metatype) {
        initializer_function = NULL;
        finalizer_function = NULL;
        //base_role = NULL;
    }

    virtual Value *match(std::string name, Value *pivot) {
        if (name != this->name)
            return NULL;

        if (pivot)
            return NULL;
            
        TypeSpec result_ts = { this };
        
        return make<TypeValue>(this, result_ts);
    }

    virtual bool complete_type() {
        for (auto &c : inner_scope->contents) {
            Allocable *v = ptr_cast<Allocable>(c.get());
            
            if (v) {
                member_allocables.push_back(v);
                //member_tss.push_back(v->alloc_ts.rvalue());
                member_names.push_back(v->name);
            }
            
            Function *f = ptr_cast<Function>(c.get());
            
            if (f && f->type == INITIALIZER_FUNCTION) {
                if (initializer_function) {
                    std::cerr << "Multiple singleton initializers!\n";
                    return false;
                }
                    
                initializer_function = f;
            }

            if (f && f->type == FINALIZER_FUNCTION) {
                if (finalizer_function) {
                    std::cerr << "Multiple singleton finalizers!\n";
                    return false;
                }
                    
                finalizer_function = f;
            }
            
            //BaseRole *b = ptr_cast<BaseRole>(c.get());
            
            //if (b) {
            //    if (base_role) {
            //        std::cerr << "Multiple base roles!\n";
            //        return false;
            //    }
                
            //    base_role = b;
            //}
        }
        
        std::cerr << "Singleton " << name << " has " << member_allocables.size() << " member variables.\n";
        return true;
    }

    virtual void allocate() {
        // Let the base be allocated first, then it will skip itself
        
        //if (base_role) {
        //    base_role->allocate();
        //    inner_scope->set_virtual_entry(0, this);
        //}
        //else {
        //    std::vector<VirtualEntry *> vt = { this };
        //    inner_scope->virtual_reserve(vt);
        //}
            
        Type::allocate();
    }
    /*
    virtual Label get_virtual_entry_label(TypeMatch tm, X64 *x64) {
        // This must point to our base type's virtual table, or NULL
        
        if (base_role)
            return typesubst(base_role->alloc_ts, tm).get_virtual_table_label(x64);
        else {
            //std::cerr << "Class " << tm << " has no base.\n";
            return x64->runtime->zero_label;
        }
    }
    */

    virtual Allocation measure(TypeMatch tm) {
        return inner_scope->get_size(tm);
    }

    virtual StorageWhere where(TypeMatch tm, AsWhat as_what) {
        return NOWHERE;
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
        }
        else
            throw INTERNAL_ERROR;
    }

    // No initializers are accessible from the language, done by the runtime itself
    
    virtual DataScope *make_inner_scope(TypeSpec pts) {
        // TODO: this is copied from Type, because we need a special scope type
        inner_scope = new SingletonScope;
        inner_scope->set_pivot_type_hint(pts);
        
        if (outer_scope)
            inner_scope->set_outer_scope(outer_scope);

        inner_scope->set_meta_scope(singleton_metatype->get_inner_scope(TypeMatch()));

        //Allocation vt_offset = is->reserve(Allocation(CLASS_HEADER_SIZE));  // VT pointer
        //if (vt_offset.bytes != CLASS_VT_OFFSET)  // sanity check
        //    throw INTERNAL_ERROR;
        
        return inner_scope;
    }

    //virtual std::vector<TypeSpec> get_member_tss(TypeMatch &match) {
    //    std::vector<TypeSpec> tss;
    //    for (auto &ts : member_tss)
    //        tss.push_back(typesubst(ts, match));
    //    return tss;
    //}
    
    virtual std::vector<std::string> get_member_names() {
        return member_names;
    }

    //virtual std::vector<VirtualEntry *> get_virtual_table(TypeMatch tm) {
    //    return inner_scope->get_virtual_table();
    //}

    //virtual Label get_virtual_table_label(TypeMatch tm, X64 *x64) {
    //    return x64->once->compile(compile_virtual_table, tm[0]);
    //}

    virtual Function *get_initializer_function() {
        return initializer_function;
    }

    virtual Function *get_finalizer_function() {
        return finalizer_function;
    }

    virtual Label get_finalizer_label(TypeMatch tm, X64 *x64) {
        return x64->once->compile(compile_finalizer, tm[0]);
    }
    
    static void compile_finalizer(Label label, TypeSpec ts, X64 *x64) {
        x64->code_label_local(label, ts[0]->name + "_finalizer");  // FIXME: ambiguous name!

        ts.destroy(Storage(MEMORY, Address(RAX, 0)), x64);

        x64->op(RET);
    }
};


class ModuleType: public Type {
public:
    ModuleScope *module_scope;
    std::vector<std::string> member_names;
    Function *initializer_function;
    std::vector<SingletonType *> singleton_types;

    ModuleType(std::string name, ModuleScope *ms)
        :Type(name, {}, module_metatype) {
        module_scope = ms;
        initializer_function = NULL;
    }
    
    virtual void store(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        if (s.where != NOWHERE || t.where != NOWHERE) {
            std::cerr << "Invalid module store from " << s << " to " << t << "!\n";
            throw INTERNAL_ERROR;
        }
    }

    virtual Value *lookup_inner(TypeMatch tm, std::string n, Value *v) {
        return module_scope->lookup(n, v);
    }
        
    virtual bool complete_type() {
        for (auto &c : module_scope->contents) {
            Allocable *v = ptr_cast<Allocable>(c.get());
            
            if (v) {
                //member_allocables.push_back(v);
                //member_tss.push_back(v->alloc_ts.rvalue());
                member_names.push_back(v->name);
            }

            Function *f = ptr_cast<Function>(c.get());
            
            if (f && f->type == INITIALIZER_FUNCTION) {
                if (initializer_function) {
                    std::cerr << "Multiple module initializers!\n";
                    return false;
                }
                    
                initializer_function = f;
            }
            
            SingletonType *s = ptr_cast<SingletonType>(c.get());
            
            if (s)
                singleton_types.push_back(s);
        }
        
        std::cerr << "Module " << name << " has " << member_names.size() << " member variables.\n";
        return true;
    }

    virtual std::vector<std::string> get_member_names() {
        return member_names;
    }
    
    virtual void collect_initializer_labels(std::vector<Label> &labels, X64 *x64) {
        for (SingletonType *s : singleton_types) {
            Function *f = s->get_initializer_function();
            
            if (f)
                labels.push_back(f->get_label(x64));
        }
    }

    virtual void collect_finalizer_labels(std::vector<Label> &labels, X64 *x64) {
        for (SingletonType *s : singleton_types) {
            Function *f = s->get_finalizer_function();
            
            if (f)
                labels.push_back(f->get_label(x64));
        }
    }
};

