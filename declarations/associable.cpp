
class Provision: public Identifier {
public:
    Associable *provider_associable;
    
    Provision(std::string n, Associable *pa)
        :Identifier(n) {
        provider_associable = pa;
    }
};


class Inheritable {
public:
    virtual void get_heritage(std::vector<Associable *> &assocs, std::vector<Function *> &funcs) {
        throw INTERNAL_ERROR;
    }
    
    virtual void override_virtual_entry(int vi, VirtualEntry *ve) {
        throw INTERNAL_ERROR;
    }
};


const std::string MAIN_ROLE_NAME = "@";
const std::string BASE_ROLE_NAME = "";
const std::string QUALIFIER_NAME = ".";


class Associable: public Allocable, public Inheritable {
public:
    std::string prefix;
    Inheritable *parent;
    InheritAs inherit_as;
    bool am_autoconv;
    
    int virtual_index;  // of the entry that stores the data offset to the role
    Associable *original_associable;
    Associable *provider_associable;
    std::vector<std::unique_ptr<Associable>> shadow_associables;
    std::vector<Function *> functions;
    std::set<std::string> associated_names;
    TypeMatch explicit_tm;

    Associable(std::string n, TypeSpec ts, InheritAs ia, bool ama)
        :Allocable(n, ts) {
        prefix = name + QUALIFIER_NAME;
        parent = NULL;
        inherit_as = ia;
        am_autoconv = ama;
        
        virtual_index = 0;
        original_associable = NULL;
        provider_associable = NULL;
        explicit_tm = alloc_ts.match();
    }

    Associable(std::string p, Associable *original, TypeMatch etm)
        :Allocable(mkname(p, original), typesubst(original->alloc_ts, etm)) {
        prefix = name + QUALIFIER_NAME;
        parent = NULL;
        inherit_as = original->inherit_as;
        am_autoconv = original->am_autoconv;
        virtual_index = 0;
        original_associable = original;
        provider_associable = original->provider_associable;
        explicit_tm = etm;
    }

    static std::string mkname(std::string prefix, Associable *original) {
        if (original->inherit_as == AS_BASE)
            return prefix.substr(0, prefix.size() - QUALIFIER_NAME.size());  // omit base suffix
        else if (original->inherit_as == AS_MAIN)
            return (prefix == BASE_ROLE_NAME + QUALIFIER_NAME ? original->name : prefix + original->name);
        else
            return prefix + original->name;
    }

    virtual void set_parent(Inheritable *p) {
        parent = p;
    }

    virtual Associable *make_shadow(std::string prefix, TypeMatch explicit_tm) {
        throw INTERNAL_ERROR;
    }

    virtual Value *make_value(Value *orig, TypeMatch tm) {
        throw INTERNAL_ERROR;
    }

    virtual devector<VirtualEntry *> get_virtual_table_fragment() {
        throw INTERNAL_ERROR;
    }

    virtual void inherit() {
        // Can't be called from this class constructor, because it needs an
        // overridden virtual function make_shadow.
        
        Inheritable *i = original_associable;
        
        if (!i) {
            std::cerr << "Associable " << name << " inherits from type " << alloc_ts << "\n";
            i = ptr_cast<Inheritable>(alloc_ts[0]);
        }
        else {
            std::cerr << "Associable " << name << " inherits from original " << original_associable->name << "\n";
        }
            
        if (!i)
            throw INTERNAL_ERROR;

        std::vector<Associable *> assocs;
        
        i->get_heritage(assocs, functions);
        
        for (auto &a : assocs) {
            Associable *s = a->make_shadow(prefix, explicit_tm);
            s->set_parent(this);
            shadow_associables.push_back(std::unique_ptr<Associable>(s));
        }
    }
    
    virtual void get_heritage(std::vector<Associable *> &assocs, std::vector<Function *> &funcs) {
        for (auto &a : shadow_associables)
            assocs.push_back(a.get());

        funcs = functions;
    }

    virtual void provision(Associable *pa) {
        if (!is_abstract() || provider_associable || !pa)
            throw INTERNAL_ERROR;
        
        provider_associable = pa;

        for (unsigned i = 0; i < shadow_associables.size(); i++)
            shadow_associables[i]->provision(pa->shadow_associables[i].get());
    }

    virtual int get_offset(TypeMatch tm) {
        if (provider_associable) {
            int o = provider_associable->get_offset(tm);
            //std::cerr << "XXX role offset " << name << " provided " << o << "\n";
            return o;
        }
        else if (is_requiring() || is_in_requiring())
            throw INTERNAL_ERROR;
        else
            return Allocable::get_offset(tm);
    }

    virtual Allocation subst_offset(TypeMatch tm) {
        if (provider_associable) {
            Allocation a = provider_associable->subst_offset(tm);
            return a;
        }
        else if (is_requiring() || is_in_requiring())
            throw INTERNAL_ERROR;
        else
            return allocsubst(offset, tm);
    }

    virtual bool has_base_role() {
        return (shadow_associables.size() && shadow_associables[0]->is_baseconv());
    }

    virtual bool has_main_role() {
        return (shadow_associables.size() && shadow_associables[0]->is_mainconv());
    }

    virtual Associable *get_head_role() {
        if (shadow_associables.empty())
            throw INTERNAL_ERROR;

        return shadow_associables[0].get();
    }
    
    virtual void set_head_role(Associable *a) {
        if (shadow_associables.empty())
            throw INTERNAL_ERROR;
            
        shadow_associables[0].reset(a);
        a->set_parent(this);
    }

    virtual void insert_head_role(Associable *a) {
        shadow_associables.insert(shadow_associables.begin(), std::unique_ptr<Associable>(a));
    }

    virtual void set_outer_scope(Scope *os) {
        Allocable::set_outer_scope(os);

        for (auto &sr : shadow_associables)
            sr->set_outer_scope(os);
    }

    virtual void outer_scope_left() {
        for (auto &si : shadow_associables)
            si->outer_scope_left();
    }

    virtual void check_full_implementation() {
        // TODO: this requires all inherited methods to be implemented
        for (auto f : functions) {
            // There can be NULL-s here for methods implemented by non-function builtins
            // Only the initial abstract declarations mean that a function was left
            // unimplemented. An associated abstract function is only used internally
            // in Implementations inside Abstracts, and those are associated, so are
            // not part of the Abstract itself.
            
            if (f && f->is_abstract() && !f->associated && !provider_associable) {
                std::cerr << "Unimplemented function " << prefix + f->name << "!\n";
                throw TYPE_ERROR;
            }
        }

        if (is_requiring() && !provider_associable) {
            if (!original_associable) {
                // Allow the explicit required role go without provisioned, obviously.
            }
            else if (ptr_cast<AbstractType>(alloc_ts[0]) != NULL) {
                // Inherited required abstract role, but all of its methods are
                // implemented. Allow it.
            }
            else {
                // Required class, but unprovided, make it an error.
                std::cerr << "Unprovided required role " << name << "!\n";
                throw TYPE_ERROR;
            }
        }

        for (auto &si : shadow_associables)
            si->check_full_implementation();
    }
    
    virtual Associable *lookup_associable(std::string n) {
        //std::cerr << "XXX " << n << " in " << name << "\n";

        if (n == name)
            return this;
        else if (has_prefix(n, prefix)) {
            for (auto &sr : shadow_associables) {
                Associable *a = sr->lookup_associable(n);
                
                if (a)
                    return a;
            }
        }
        else if (n == MAIN_ROLE_NAME || has_prefix(n, MAIN_ROLE_NAME + QUALIFIER_NAME)) {
            // TODO: this is a temporary hack
            if (has_base_role() || has_main_role()) {
                Associable *a = get_head_role()->lookup_associable(n);
                
                if (a)
                    return a;
            }
        }
        
        return NULL;
    }

    virtual bool check_provisioning(std::string override_name, Associable *provider_associable) {
        // Only Role should do this
        throw INTERNAL_ERROR;
    }
    
    virtual bool check_associated(Declaration *d) {
        Identifier *id = ptr_cast<Identifier>(d);
        if (!id)
            throw INTERNAL_ERROR;
            
        std::string override_name = id->name;
        
        if (!deprefix(override_name, prefix))
            throw INTERNAL_ERROR;

        if (override_name.find(QUALIFIER_NAME) != std::string::npos)
            throw INTERNAL_ERROR;  // this role must be the exact role for d

        Provision *provision = ptr_cast<Provision>(d);
        
        if (provision) {
            return check_provisioning(override_name, provision->provider_associable);
        }
        
        // TODO: collect the Function*-s into a vector, update it with the overrides, and
        // let the shadows copy it for themselves. Record all association names to prevent
        // duplicates.
        
        if (associated_names.count(override_name)) {
            std::cerr << "Multiple associations for " << id->name << "\n";
            return false;
        }
        
        associated_names.insert(override_name);
        
        // The associated thing may be an implementation, or a built-in operation, too,
        // we can't check those.
        Function *override = ptr_cast<Function>(d);
        Function *original = NULL;
        int original_index = -1;
        
        for (unsigned i = 0; i < functions.size(); i++) {
            // This can be prefixed, if it was defined as an override
            std::string oname = functions[i]->name;
            
            if (desuffix(oname, override_name)) {
                // Check for proper equality of the unqualified name
                int s = oname.size();
                
                if (s == 0 || oname[s - 1] == '.') {
                    original = functions[i];
                    original_index = i;
                    break;
                }
            }
        }

        if (!original) {
            // Non-function overrides may have a non-function original
            if (!override)
                return true;
                
            std::cerr << "No function to override: " << id->name << "!\n";
            throw INTERNAL_ERROR;
            return false;
        }

        if (override) {
            // assume parameterless outermost class, derive role parameters
            TypeMatch role_tm = alloc_ts.match();

            // this automatically sets original_function
            if (!override->does_implement(TypeMatch(), original, role_tm))
                return false;
        
            override->set_associated(this);
        }

        // Unfortunately, this sets the entry to NULL for non-function overrides.
        functions[original_index] = override;
        
        return true;
    }

    virtual bool is_requiring() {
        return inherit_as == AS_REQUIRE;
    }

    virtual bool is_in_requiring() {
        Associable *a = ptr_cast<Associable>(parent);
        
        return (a ? a->is_requiring() || a->is_in_requiring() : false);
    }

    virtual bool is_autoconv() {
        return am_autoconv;
    }

    virtual bool is_baseconv() {
        return inherit_as == AS_BASE;
    }

    virtual bool is_mainconv() {
        return inherit_as == AS_MAIN;
    }

    virtual Value *autoconv(TypeMatch tm, Type *target, Value *orig, TypeSpec &ifts) {
        ifts = typesubst(alloc_ts, tm);  // pivot match
        std::cerr << "Checking autoconv from " << ifts << " to " << ptr_cast<Identifier>(target)->name << " " << this << "\n";

        if (ifts[0] == target) {
            // Direct implementation
            std::cerr << "Found direct implementation " << name << ".\n";
            return make_value(orig, tm);
        }
        else if (inherit_as == AS_BASE) {
            std::cerr << "Trying base role with " << ifts << "\n";
            
            for (auto &sr : shadow_associables) {
                if (sr->is_autoconv()) {
                    Value *v = sr->autoconv(tm, target, orig, ifts);
                
                    if (v)
                        return v;
                }
            }
        }
        
        return NULL;
    }

    //virtual void set_associated_lself(Lself *l) {
    //    associated_lself = l;
    //}

    virtual Associable *autoconv_streamifiable(TypeMatch match) {
        //if (associated_lself)
        //    return NULL;

        TypeSpec ifts = get_typespec(match);

        if (ifts[0] == ptr_cast<Type>(streamifiable_type)) {
            return this;
        }
        else if (inherit_as == AS_BASE || inherit_as == AS_MAIN) {
            for (auto &sa : shadow_associables) {
                if (!sa->is_autoconv())
                    continue;
                
                Associable *i = sa->autoconv_streamifiable(match);
                
                if (i)
                    return i;
            }
        }
            
        return NULL;
    }

    virtual void streamify(TypeMatch tm, X64 *x64) {
        // Allow roles implement Streamifiable flexibly
        throw INTERNAL_ERROR;
    }

    virtual void relocate(Allocation explicit_offset) {
        // For Role
        throw INTERNAL_ERROR;
    }
    
    virtual void compile_vt(TypeMatch tm, X64 *x64) {
        // For Role
        throw INTERNAL_ERROR;
    }
    
    virtual void init_vt(TypeMatch tm, Address self_addr, X64 *x64) {
        throw INTERNAL_ERROR;
    }

    virtual void compile_act(TypeMatch tm, X64 *x64) {
        // For Role
        throw INTERNAL_ERROR;
    }
};


class DataVirtualEntry: public VirtualEntry {
public:
    Associable *associable;
    
    DataVirtualEntry(Associable *a) {
        associable = a;
    }
    
    virtual Label get_virtual_entry_label(TypeMatch tm, X64 *x64) {
        // Can't create entry that points to an abstract role
        if (associable->where == NOWHERE)
            throw INTERNAL_ERROR;
            
        int offset = associable->get_offset(tm);
        
        Label label;
        x64->absolute_label(label, offset);  // forcing an int into an unsigned64...
        return label;
    }

    virtual std::ostream &out_virtual_entry(std::ostream &os, TypeMatch tm) {
        return os << "DATA " << associable->name << " at " << associable->get_offset(tm);
    }
};


static void dump_associable(Associable *a, int indent) {
    // FIXME: method!
    
    for (int i = 0; i < indent; i++)
        std::cerr << "  ";
        
    std::cerr << "'" << a->name << "' (" << (
        a->is_autoconv() ? "auto " : ""
    ) << (
        a->inherit_as == AS_BASE ? "BASE" :
        a->inherit_as == AS_MAIN ? "MAIN" :
        a->inherit_as == AS_ROLE ? "ROLE" :
        a->inherit_as == AS_REQUIRE ? "REQUIRE" :
        throw INTERNAL_ERROR
    ) << ") " << a->alloc_ts << "\n";
    
    for (auto &x : a->shadow_associables)
        dump_associable(x.get(), indent + 1);
}

