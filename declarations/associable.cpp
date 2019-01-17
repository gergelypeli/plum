
class Inheritable {
public:
    virtual void get_heritage(Associable *&mr, Associable *&br, std::vector<Associable *> &assocs, std::vector<Function *> &funcs) {
        throw INTERNAL_ERROR;
    }
    
    virtual void override_virtual_entry(int vi, VirtualEntry *ve) {
        throw INTERNAL_ERROR;
    }
};


class Associable: public Allocable, public Inheritable {
public:
    std::string prefix;
    Inheritable *parent;
    InheritAs inherit_as;
    int virtual_index;  // of the entry that stores the data offset to the role
    Associable *original_associable;
    std::unique_ptr<Associable> shadow_base_associable, shadow_main_associable;
    std::vector<std::unique_ptr<Associable>> shadow_associables;
    std::vector<Function *> functions;
    std::set<std::string> associated_names;
    DataScope *associating_scope;
    FfwdVirtualEntry *fastforward_ve;
    Lself *associated_lself;
    TypeMatch explicit_tm;

    Associable(std::string n, TypeSpec pts, TypeSpec ts, InheritAs ia)
        :Allocable(n, pts, ts) {
        //std::cerr << "Creating " << (ia == AS_BASE ? "base " : ia == AS_AUTO ? "auto " : "") << "role " << name << ".\n";
        prefix = name + ".";
        parent = NULL;
        inherit_as = ia;
        virtual_index = 0;
        original_associable = NULL;
        //virtual_offset = -1;
        associating_scope = NULL;
        fastforward_ve = NULL;
        associated_lself = NULL;
        explicit_tm = alloc_ts.match();
    }

    Associable(std::string p, Associable *original, TypeMatch etm)
        :Allocable(p + original->name, NO_TS, typesubst(original->alloc_ts, etm)) {
        //std::cerr << "Creating shadow role " << name << ".\n";
        
        prefix = name + ".";
        parent = NULL;
        inherit_as = original->inherit_as;
        virtual_index = 0;
        original_associable = original;
        //virtual_offset = -1;
        associating_scope = NULL;
        fastforward_ve = NULL;
        associated_lself = NULL;
        explicit_tm = etm;
    }

    virtual void set_parent(Inheritable *p) {
        parent = p;
    }

    virtual Associable *shadow(Associable *original) {
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
        // overridden virtual function 'shadow'.
        
        Inheritable *i = original_associable;
        
        if (!i) {
            std::cerr << "Associable " << name << " inherits from type " << alloc_ts << "\n";
            i = ptr_cast<Inheritable>(alloc_ts[0]);
        }
        else {
            std::cerr << "Associable " << name << " inherits from original " << original_associable->get_fully_qualified_name() << "\n";
        }
            
        if (!i)
            throw INTERNAL_ERROR;

        Associable *main, *base;
        std::vector<Associable *> assocs;
        
        i->get_heritage(main, base, assocs, functions);
        
        if (main)
            shadow_main_associable.reset(shadow(main));
            
        if (base)
            shadow_base_associable.reset(shadow(base));
        
        for (auto &a : assocs)
            shadow_associables.push_back(std::unique_ptr<Associable>(shadow(a)));
    }
    
    virtual void get_heritage(Associable *&mr, Associable *&br, std::vector<Associable *> &assocs, std::vector<Function *> &funcs) {
        mr = shadow_main_associable.get();
        br = shadow_base_associable.get();

        for (auto &a : shadow_associables)
            assocs.push_back(a.get());

        funcs = functions;
    }

    virtual Associable *rip_main_role() {
        return shadow_main_associable.release();
    }

    virtual void set_associating_scope(DataScope *as) {
        associating_scope = as;

        if (shadow_main_associable)
            shadow_main_associable->set_associating_scope(as);

        if (shadow_base_associable)
            shadow_base_associable->set_associating_scope(as);
        
        for (auto &sr : shadow_associables) {
            sr->set_associating_scope(as);
        }
    }
    
    virtual void set_outer_scope(Scope *os) {
        if (original_associable)
            throw INTERNAL_ERROR;
            
        DataScope *ds = ptr_cast<DataScope>(os);
        if (!ds)
            throw INTERNAL_ERROR;
            
        //if (!ds->is_virtual_scope())
        //    throw INTERNAL_ERROR;

        Allocable::set_outer_scope(os);
        
        set_associating_scope(ds);
    }

    virtual void outer_scope_left() {
        if (shadow_main_associable)
            shadow_main_associable->outer_scope_left();

        if (shadow_base_associable)
            shadow_base_associable->outer_scope_left();

        for (auto &si : shadow_associables)
            si->outer_scope_left();
    }
    
    virtual void check_full_implementation() {
        // TODO: this requires all inherited methods to be implemented
        for (auto f : functions) {
            // There can be NULL-s here for methods implemented by non-function builtins
            
            if (f && f->type == ABSTRACT_FUNCTION) {
                std::cerr << "Unimplemented function " << prefix + f->name << "!\n";
                throw TYPE_ERROR;
            }
        }

        if (shadow_main_associable)
            shadow_main_associable->check_full_implementation();

        if (shadow_base_associable)
            shadow_base_associable->check_full_implementation();

        for (auto &si : shadow_associables)
            si->check_full_implementation();
    }
    
    virtual Associable *lookup_associable(std::string n) {
        if (n == name)
            return this;
        else if (has_prefix(n, prefix)) {
            for (auto &sr : shadow_associables) {
                Associable *a = sr->lookup_associable(n);
                
                if (a)
                    return a;
            }
        }
        
        return NULL;
    }

    virtual bool check_associated(Declaration *d) {
        Identifier *id = ptr_cast<Identifier>(d);
        if (!id)
            throw INTERNAL_ERROR;
            
        std::string override_name = id->name;
        
        if (!deprefix(override_name, prefix))
            throw INTERNAL_ERROR;
        
        // TODO: collect the Function*-s into a vector, update it with the overrides, and
        // let the shadows copy it for themselves. Record all association names to prevent
        // duplicates. Finally check if we have any ABSTRACT_FUNCTION left (for now we don't
        // allow abstract classes).
        
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
        
            if (associated_lself)
                override->set_associated_lself(associated_lself);
        }

        // Unfortunately, this sets the entry to NULL for non-function overrides.
        functions[original_index] = override;
        
        return true;
    }

    virtual bool is_autoconv() {
        return inherit_as == AS_AUTO || inherit_as == AS_BASE;
    }

    virtual bool is_baseconv() {
        return inherit_as == AS_BASE;
    }

    virtual bool is_mainconv() {
        return inherit_as == AS_MAIN;
    }

    virtual Value *autoconv(TypeMatch tm, Type *target, Value *orig, TypeSpec &ifts, bool assume_lvalue) {
        if (associated_lself && !assume_lvalue)
            return NULL;

        ifts = typesubst(alloc_ts, tm);  // pivot match

        if (ifts[0] == target) {
            // Direct implementation
            std::cerr << "Found direct implementation " << name << ".\n";
            //return make<RoleValue>(this, orig, tm);
            return make_value(orig, tm);
        }
        else if (inherit_as == AS_BASE) {
            std::cerr << "Trying base role with " << ifts << "\n";
            
            if (shadow_base_associable) {
                Value *v = shadow_base_associable->autoconv(tm, target, orig, ifts, assume_lvalue);
                
                if (v)
                    return v;
            }
            
            for (auto &sr : shadow_associables) {
                if (sr->inherit_as == AS_AUTO) {
                    Value *v = sr->autoconv(tm, target, orig, ifts, assume_lvalue);
                
                    if (v)
                        return v;
                }
            }
        }
        
        return NULL;
    }

    virtual void set_associated_lself(Lself *l) {
        associated_lself = l;
    }

    virtual void relocate(Allocation explicit_offset) {
        // For Role
        throw INTERNAL_ERROR;
    }
    
    virtual void compile_vt(TypeMatch tm, std::string tname, X64 *x64) {
        // For Role
        throw INTERNAL_ERROR;
    }
    
    virtual void init_vt(TypeMatch tm, Address self_addr, X64 *x64) {
        throw INTERNAL_ERROR;
    }
    
    virtual std::string get_fully_qualified_name() {
        // TODO: this is currently used even when the outer scope-s are not all yet set up,
        // so fully_qualify fails.
        return associating_scope->name + "." + name;
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
            
        Allocation offset = associable->offset;
        
        Label label;
        x64->absolute_label(label, offset.concretize(tm));  // forcing an int into an unsigned64...
        return label;
    }

    virtual std::ostream &out_virtual_entry(std::ostream &os, TypeMatch tm) {
        return os << "DATA " << associable->name << " at " << associable->offset.concretize(tm);
    }
};

