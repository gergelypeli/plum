

class InheritableType: public Type, public Inheritable {
public:
    InheritableType(std::string n, Metatypes pmts, MetaType *mt)
        :Type(n, pmts, mt) {
    }
    
    virtual Allocation measure(TypeMatch tm) {
        std::cerr << "This is probably an error, shouldn't measure an inheritable type!\n";
        throw INTERNAL_ERROR;
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
        // We do this for inheritable types that don't implement Streamifiable
        x64->op(MOVQ, RDI, Address(RSP, ALIAS_SIZE));
        x64->op(MOVQ, RSI, Address(RSP, 0));
    
        x64->runtime->call_sysv(x64->runtime->sysv_streamify_pointer_label);
    }
};


class InterfaceType: public InheritableType {
public:
    std::vector<Function *> member_functions;
    std::vector<Associable *> member_associables;
    
    InterfaceType(std::string name, Metatypes param_metatypes)
        :InheritableType(name, param_metatypes, interface_metatype) {
    }

    virtual bool complete_type() {
        for (auto &c : inner_scope->contents) {
            Function *f = ptr_cast<Function>(c.get());
            
            if (f) {
                member_functions.push_back(f);
                continue;
            }
            
            FunctionScope *fs = ptr_cast<FunctionScope>(c.get());
            
            if (fs)
                continue;

            Associable *imp = ptr_cast<Associable>(c.get());
            
            if (imp) {
                member_associables.push_back(imp);
                continue;
            }
                
            std::cerr << "Not a function or implementation in an interface!\n";
            throw INTERNAL_ERROR;
            return false;
        }
        
        //std::cerr << "Interface " << name << " has " << member_functions.size() << " member functions.\n";
        return true;
    }

    virtual DataScope *make_inner_scope(TypeSpec pts) {
        DataScope *is = Type::make_inner_scope(pts);

        is->be_virtual_scope();
        is->be_abstract_scope();
        
        return is;
    }
    
    virtual void get_heritage(Associable *&mr, Associable *&br, std::vector<Associable *> &assocs, std::vector<Function *> &funcs) {
        // TODO
        mr = NULL;
        br = NULL;
        
        assocs = member_associables;
        funcs = member_functions;
    }

    virtual Allocation measure_identity(TypeMatch tm) {
        return Allocation(CLASS_HEADER_SIZE);
    }

    virtual void create(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        // Assume the target MEMORY is uninitialized
        
        switch (s.where * t.where) {
        case NOWHERE_MEMORY:  // TODO: hm?
            return;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual void destroy(TypeMatch tm, Storage s, X64 *x64) {
        if (s.where == MEMORY) {
            return;
        }
        else
            throw INTERNAL_ERROR;
    }

    virtual StorageWhere where(TypeMatch tm, AsWhat as_what) {
        return (
            as_what == AS_VARIABLE ? MEMORY :
            throw INTERNAL_ERROR
        );
    }

    virtual devector<VirtualEntry *> get_virtual_table(TypeMatch tm) {
        return inner_scope->get_virtual_table();
    }

    virtual Label get_virtual_table_label(TypeMatch tm, X64 *x64) {
        return x64->once->compile(compile_vt, tm[0]);
    }

    static void compile_vt(Label label, TypeSpec ts, X64 *x64) {
        std::cerr << "Compiling virtual table for " << ts << "\n";
        std::string symbol = ts.symbolize() + "_virtual_table";

        // We only need this for dynamic type casts
        x64->data_align(8);
        x64->data_label_local(label, symbol);
        x64->data_qword(0);
    }
    
    virtual void allocate() {
        // FIXME: handle base, like class does

        // These stay zeros for now
        VirtualEntry *rolevt_ve = new RoleVirtualEntry(this, NULL);
        VirtualEntry *fastforward_ve = new FfwdVirtualEntry(Allocation(0));
        
        devector<VirtualEntry *> vt;
        vt.append(rolevt_ve);
        vt.append(fastforward_ve);
                
        inner_scope->virtual_initialize(vt);

        Type::allocate();
    }
    
    virtual Value *lookup_matcher(TypeMatch tm, std::string n, Value *v, Scope *s) {
        return make<ClassMatcherValue>(n, v);
    }
};


class Implementation: public Associable {
public:
    Implementation(std::string name, TypeSpec pts, TypeSpec ifts, InheritAs ia)
        :Associable(name, pts, ifts, ia) {
        inherit();
        /*
        TypeMatch explicit_tm = interface_ts.match();

        for (auto &imp : ift->member_implementations)
            shadow_implementations.push_back(std::make_unique<Implementation>(prefix, imp, explicit_tm));
        
        for (auto &f : ift->member_functions)
            missing_function_names.insert(prefix + f->name);
        */
    }
    
    Implementation(std::string p, Associable *oi, TypeMatch explicit_tm)
        :Associable(p, oi, explicit_tm) {
        inherit();
    }

    virtual bool is_abstract() {
        return true;
    }

    virtual Associable *shadow(Associable *original) {
        return new Implementation(prefix, original, explicit_tm);
    }

    virtual void set_name(std::string n) {
        throw INTERNAL_ERROR;  // too late!
    }

    virtual TypeSpec get_interface_ts(TypeMatch match) {
        return typesubst(alloc_ts, match);
    }

    virtual Value *make_value(Value *orig, TypeMatch match) {
        // If the pivot is not a concrete type, but a Ptr to an interface, then this
        // is accessing an abstract role via an interface pointer
        TypeSpec ots = ::get_typespec(orig).rvalue();
        
        if (ots[0] == ptr_type && ots.unprefix(ptr_type).has_meta(interface_metatype))
            return make<RoleValue>(this, orig, match);
        else
            return make<ImplementationConversionValue>(this, orig, match);
    }

    virtual devector<VirtualEntry *> get_virtual_table_fragment() {
        return alloc_ts.get_virtual_table();  // FIXME: subst!
    }

    // TODO: can this be merged with the above one?
    virtual Implementation *autoconv_streamifiable_implementation(TypeMatch match) {
        if (associated_lself)
            return NULL;

        TypeSpec ifts = get_interface_ts(match);

        if (ifts[0] == streamifiable_type) {
            return this;
        }
        else if (inherit_as == AS_BASE) {
            for (auto &sa : shadow_associables) {
                if (sa->inherit_as == AS_ROLE)
                    continue;
                
                Implementation *si = ptr_cast<Implementation>(sa.get());
                Implementation *i = si->autoconv_streamifiable_implementation(match);
                
                if (i)
                    return i;
            }
        }
            
        return NULL;
    }

    virtual void streamify(TypeMatch tm, X64 *x64) {
        if (alloc_ts[0] != streamifiable_type)
            throw INTERNAL_ERROR;
            
        for (auto &d : associating_scope->contents) {
            Function *f = ptr_cast<Function>(d.get());
            
            if (f && f->associated == this) {
                x64->op(CALL, f->get_label(x64));
                return;
            }
        }
        
        throw INTERNAL_ERROR;
    }

    virtual Value *matched(Value *pivot, Scope *scope, TypeMatch &match) {
        return make_value(pivot, match);
    }
    
    virtual void allocate() {
        Associable::allocate();
        where = MEMORY;
        
        // Be role-like in interfaces
        if (associating_scope->is_virtual_scope()) {
            VirtualEntry *ve = new DataVirtualEntry(this);
            virtual_index = associating_scope->virtual_reserve(ve);
        }
    }
};


class AltStreamifiableImplementation: public Implementation {
public:
    AltStreamifiableImplementation(std::string name, TypeSpec pts)
        :Implementation(name, pts, STREAMIFIABLE_TS, AS_ROLE) {
    }
    
    virtual void check_full_implementation() {
        // We pretend to implement the streamify function, because we implement
        // streamification as a built-in feature.
    }
    
    virtual void streamify(TypeMatch tm, X64 *x64) {
        tm[0].streamify(true, x64);
    }
};


class Lself: public Associable {
public:
    std::vector<Implementation *> outer_implementations;

    Lself(std::string name, TypeSpec pts)
        :Associable(name, pts, NO_TS, AS_ROLE) {
    }

    virtual void set_outer_scope(Scope *os) {
        DataScope *ds = ptr_cast<DataScope>(os);
        if (!ds)
            throw INTERNAL_ERROR;
        
        associating_scope = ds;
    }

    virtual void set_name(std::string n) {
        throw INTERNAL_ERROR;  // too late!
    }
    /*
    virtual Associable *lookup_associable(std::string n) {
        if (n == name)
            return this;
        else if (has_prefix(n, prefix)) {
            for (auto &oi : outer_implementations) {
                Associable *a = oi->lookup_associable(n);
                
                if (a)
                    return a;
            }
        }
        
        return NULL;
    }
    */
    virtual bool check_associated(Declaration *decl) {
        Function *f = ptr_cast<Function>(decl);
        if (f) {
            f->set_associated_lself(this);
            return true;
        }

        Implementation *i = ptr_cast<Implementation>(decl);
        if (i) {
            i->set_associated_lself(this);
            return true;
        }
        
        std::cerr << "This declaration can't be associated with Lself " << name << "!\n";
        return false;
    }
};

