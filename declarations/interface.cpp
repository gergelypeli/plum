
class InterfaceType: public Type, public Inheritable {
public:
    std::vector<Function *> member_functions;
    std::vector<Declaration *> member_procedures;
    std::vector<Associable *> member_associables;

    InterfaceType(std::string name, Metatypes param_metatypes)
        :Type(name, param_metatypes, interface_metatype) {
    }

    virtual bool complete_type() {
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

    virtual void get_heritage(std::vector<Associable *> &assocs, std::vector<Function *> &funcs) {
        assocs = member_associables;
        funcs = member_functions;
    }

    virtual Allocation measure_identity(TypeMatch tm) {
        return { 0 };
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
    /*
    virtual void allocate() {
        // This will only be used for role implementations, as the inner scope size only
        // contributes to measure_identity.
        Allocation size = CLASS_HEADER_SIZE;
        Allocation offset = inner_scope->reserve(size);

        if (offset.concretize() != 0)
            throw INTERNAL_ERROR;

        // This virtual table will only be used for cloning to other tables,
        // not compiled into the executables.

        devector<VirtualEntry *> vt;
        vt.append(NULL);
        vt.append(NULL);
        inner_scope->virtual_initialize(vt);

        Type::allocate();
    }
    */
    virtual Value *lookup_inner(TypeMatch tm, std::string n, Value *v, Scope *s) {
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
    /*
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
    */
};


class Implementation: public Associable {
public:
    int virtual_offset;  // If implemented in an identity type

    Implementation(std::string name, TypeSpec ifts, InheritAs ia)
        :Associable(name, ifts, ia) {
        std::cerr << "Creating " << (ia == AS_BASE ? "base " : ia == AS_AUTO ? "auto " : "") << "implementation " << name << "\n";
        virtual_offset = 0;

        inherit();
    }
    
    Implementation(std::string p, Associable *oi, TypeMatch explicit_tm)
        :Associable(p, oi, explicit_tm) {
        std::cerr << "Creating shadow implementation " << name << "\n";

        inherit();
    }

    virtual bool is_abstract() {
        return true;
    }

    virtual Associable *make_shadow(std::string prefix, TypeMatch explicit_tm) {
        return new Implementation(prefix, this, explicit_tm);
    }

    virtual void set_name(std::string n) {
        throw INTERNAL_ERROR;  // too late!
    }

    //virtual TypeSpec get_interface_ts(TypeMatch match) {
    //    return typesubst(alloc_ts, match);
    //}

    virtual Value *make_value(Value *orig, TypeMatch match) {
        // If the pivot is not a concrete type, but a Ptr to an interface, then this
        // is accessing an abstract role via an interface pointer
        TypeSpec ots = ::get_typespec(orig).rvalue();
        
        if (ots[0] == ptr_type && ots.unprefix(ptr_type).has_meta(interface_metatype))
            return make<RoleValue>(this, orig, match);
        else
            return make<ImplementationConversionValue>(this, orig, match);
    }

    //virtual devector<VirtualEntry *> get_virtual_table_fragment() {
        //return devector<VirtualEntry *>();
        //return alloc_ts.get_virtual_table();  // FIXME: subst!
    //}
    
    virtual void streamify(TypeMatch tm, X64 *x64) {
        // This allows complete built-in implementations of Streamifiable
        if (alloc_ts[0] != streamifiable_type)
            throw INTERNAL_ERROR;

        std::cerr << "XXX streamify " << get_fully_qualified_name() << "\n";
            
        for (auto &d : outer_scope->contents) {
            Function *f = ptr_cast<Function>(d.get());
            
            if (f && f->associated == this) {
                x64->op(CALL, f->get_label(x64));
                return;
            }
        }
        
        if (virtual_offset) {
            // Implementation in an identity type
            
            x64->op(MOVQ, R10, Address(RSP, ALIAS_SIZE));  // Ptr
            x64->op(MOVQ, R11, Address(R10, CLASS_VT_OFFSET));  // sable VT
            x64->op(CALL, Address(R11, virtual_offset * ADDRESS_SIZE));  // select method
            return;
        }
        
        throw INTERNAL_ERROR;
    }
    
    virtual Value *matched(Value *pivot, Scope *scope, TypeMatch &match) {
        return make_value(pivot, match);
    }
    
    virtual void allocate() {
        Associable::allocate();
        where = MEMORY;
        DataScope *associating_scope = ptr_cast<DataScope>(outer_scope);
        
        // Allocate VT entries in abstracts
        if (associating_scope->is_virtual_scope()) {
            for (unsigned i = 0; i < functions.size(); i++) {
                int vi = associating_scope->virtual_reserve(NULL);
                if (!vi)
                    throw INTERNAL_ERROR;
            
                if (!virtual_offset)
                    virtual_offset = vi;
            }
            
            //VirtualEntry *ve = new DataVirtualEntry(this);
            //virtual_index = associating_scope->virtual_reserve(ve);
        }
        
        for (auto &sr : shadow_associables)
            sr->allocate();
    }

    virtual void relocate(Allocation explicit_offset) {
        // If implemented in an identity type
        
        if (!original_associable)
            throw INTERNAL_ERROR;
            
        virtual_offset = ptr_cast<Implementation>(original_associable)->virtual_offset;
    }

    virtual void override_virtual_entry(int vi, VirtualEntry *ve) {
        parent->override_virtual_entry(vi + virtual_offset, ve);
    }
    
    virtual void compile_vt(TypeMatch tm, X64 *x64) {
    }
    
    virtual void init_vt(TypeMatch tm, Address self_addr, X64 *x64) {
    }

    virtual void compile_act(TypeMatch tm, X64 *x64) {
    }
};


class AltStreamifiableImplementation: public Implementation {
public:
    AltStreamifiableImplementation(std::string name)
        :Implementation(name, STREAMIFIABLE_TS, AS_ROLE) {
    }
    
    virtual void check_full_implementation() {
        // We pretend to implement the streamify function, because we implement
        // streamification as a built-in feature.
    }
    
    virtual void streamify(TypeMatch tm, X64 *x64) {
        tm[0].streamify(true, x64);
    }
};
