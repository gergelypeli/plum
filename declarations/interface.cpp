
static void dump_associable(Associable *a, int indent) {
    for (int i = 0; i < indent; i++)
        std::cerr << "  ";
        
    std::cerr << "'" << a->name << "' (" << (
        a->inherit_as == AS_BASE ? "BASE" :
        a->inherit_as == AS_MAIN ? "MAIN" :
        a->inherit_as == AS_ROLE ? "ROLE" :
        a->inherit_as == AS_AUTO ? "AUTO" :
        a->inherit_as == AS_ALIAS ? "ALIAS" :
        throw INTERNAL_ERROR
    ) << ") " << a->alloc_ts << "\n";
    
    for (auto &x : a->shadow_associables)
        dump_associable(x.get(), indent + 1);
}


class IdentityType: public Type, public Inheritable, public PartialInitializable {
public:
    std::vector<Allocable *> member_allocables;
    std::vector<std::string> member_names;
    
    std::vector<Function *> member_functions;
    std::vector<Associable *> member_associables;

    bool is_abstract;
    Function *finalizer_function;

    IdentityType(std::string n, Metatypes pmts, MetaType *mt, bool ia)
        :Type(n, pmts, mt) {
        is_abstract = ia;
        finalizer_function = NULL;
    }

    virtual TypeSpec make_pivot_type_hint() {
        return Type::make_pivot_type_hint().prefix(ptr_type);
    }

    virtual DataScope *make_inner_scope(TypeSpec pts) {
        DataScope *is = Type::make_inner_scope(pts);

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
            }

            if (f && (f->type == GENERIC_FUNCTION || f->type == ABSTRACT_FUNCTION))
                member_functions.push_back(f);
        }

        std::cerr << "Class " << name << " has " << member_allocables.size() << " member variables.\n";

        return true;
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

    virtual void override_virtual_entry(int vi, VirtualEntry *ve) {
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
        int method_vi = ptr_cast<Function>(ptr_cast<Type>(streamifiable_type)->get_inner_scope()->contents[0].get())->virtual_index;
        
        if (this == ptr_cast<IdentityType>(streamifiable_type)) {
            // Streamifiable role pivot, invoke method directly
            x64->op(MOVQ, R10, Address(RSP, ALIAS_SIZE));  // Ptr
            x64->op(MOVQ, R11, Address(R10, CLASS_VT_OFFSET));  // sable VT
            x64->op(CALL, Address(R11, method_vi * ADDRESS_SIZE));  // select method
            return;
        }
        
        // Try autoconv
        Associable *streamifiable_associable = NULL;
            
        for (auto a : member_associables) {
            streamifiable_associable = a->autoconv_streamifiable(tm);
                
            if (streamifiable_associable)
                break;
        }

        if (streamifiable_associable) {
            int role_vi = streamifiable_associable->virtual_index;

            x64->op(MOVQ, R10, Address(RSP, ALIAS_SIZE));  // Ptr
            x64->op(MOVQ, R11, Address(R10, CLASS_VT_OFFSET));  // VT
            x64->op(ADDQ, R10, Address(R11, role_vi * ADDRESS_SIZE));  // select role
            x64->op(MOVQ, Address(RSP, ALIAS_SIZE), R10);  // technically illegal
            x64->op(MOVQ, R11, Address(R10, CLASS_VT_OFFSET));  // sable VT
            x64->op(CALL, Address(R11, method_vi * ADDRESS_SIZE));  // select method
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
};


class InterfaceType: public IdentityType {
public:
    InterfaceType(std::string name, Metatypes param_metatypes)
        :IdentityType(name, param_metatypes, interface_metatype, true) {
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
    
    virtual Value *lookup_inner(TypeMatch tm, std::string n, Value *v, Scope *s) {
        std::cerr << "Interface " << name << " inner lookup " << n << ".\n";
        
        Value *value = IdentityType::lookup_inner(tm, n, v, s);

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


class Implementation: public Associable {
public:
    Implementation(std::string name, TypeSpec pts, TypeSpec ifts, InheritAs ia)
        :Associable(name, pts, ifts, ia) {
        std::cerr << "Creating " << (ia == AS_BASE ? "base " : ia == AS_AUTO ? "auto " : "") << "implementation " << name << "\n";

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

    virtual Associable *make_shadow(Associable *original) {
        return new Implementation(prefix, original, explicit_tm);
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

    virtual devector<VirtualEntry *> get_virtual_table_fragment() {
        return alloc_ts.get_virtual_table();  // FIXME: subst!
    }
    
    virtual void streamify(TypeMatch tm, X64 *x64) {
        // This allows complete built-in implementations of Streamifiable
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
        
        for (auto &sr : shadow_associables)
            sr->allocate();
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

