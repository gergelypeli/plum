

class Allocable: public Identifier {
public:
    TypeSpec alloc_ts;
    Allocation offset;
    StorageWhere where;
    
    Allocable(std::string name, TypeSpec pts, TypeSpec ats)
        :Identifier(name, pts) {
        where = NOWHERE;
        alloc_ts = ats;
    }
    
    virtual Value *matched(Value *cpivot, TypeMatch &match) {
        throw INTERNAL_ERROR;
    }
    
    virtual void allocate() {
        if (where != NOWHERE)
            throw INTERNAL_ERROR;
    }

    virtual Storage get_storage(TypeMatch tm, Storage s) {
        if (where == NOWHERE)
            throw INTERNAL_ERROR;

        if (s.where != MEMORY)
            throw INTERNAL_ERROR;  // all variable containers must use MEMORY
        
        return Storage(where, s.address + offset.concretize(tm));
    }

    virtual Storage get_local_storage() {
        // Without pivot as a function local variable
        return get_storage(TypeMatch(), Storage(MEMORY, Address(RBP, 0)));
    }
    
    virtual void destroy(TypeMatch tm, Storage s, X64 *x64) {
        throw INTERNAL_ERROR;
    }
};


class Variable: public Allocable {
public:
    bool is_argument;
    
    Variable(std::string name, TypeSpec pts, TypeSpec vts)
        :Allocable(name, pts, vts) {
        if (vts == NO_TS)
            throw INTERNAL_ERROR;
            
        is_argument = false;
    }

    virtual void set_outer_scope(Scope *os) {
        Allocable::set_outer_scope(os);
        
        is_argument = (os && os->type == ARGUMENT_SCOPE);
    }
    
    virtual Value *matched(Value *cpivot, TypeMatch &match) {
        // cpivot may be NULL if this is a local variable
        return make_variable_value(this, cpivot, match);
    }
    
    virtual void allocate() {
        Allocable::allocate();
            
        where = alloc_ts.where(is_argument ? AS_ARGUMENT : AS_VARIABLE);
            
        Allocation a = (
            where == MEMORY ? alloc_ts.measure() :
            where == ALIAS ? Allocation(ALIAS_SIZE) :
            throw INTERNAL_ERROR
        );
        
        //if (a.count1 || a.count2 || a.count3)
        //    std::cerr << "Hohoho, allocating variable " << name << " with size " << a << ".\n";
        
        offset = outer_scope->reserve(a);
        //std::cerr << "Allocated variable " << name << " to " << offset << ".\n";
    }
    
    virtual void finalize(X64 *x64) {
        // This method is only called on local variables, and it's an overload
        Identifier::finalize(x64);  // Place label
        //std::cerr << "Finalizing variable " << name << ".\n";
        
        alloc_ts.destroy(get_local_storage(), x64);
    }
    
    virtual void create(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        TypeSpec ts = typesubst(alloc_ts, tm);
        int o = offset.concretize(tm);
        ts.create(s.where == NOWHERE ? s : s + o, t + o, x64);
    }

    virtual void store(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        TypeSpec ts = typesubst(alloc_ts, tm);
        int o = offset.concretize(tm);
        ts.store(s + o, t + o, x64);
    }

    virtual void destroy(TypeMatch tm, Storage s, X64 *x64) {
        TypeSpec ts = typesubst(alloc_ts, tm);
        int o = offset.concretize(tm);
        ts.destroy(s + o, x64);
    }
    
    virtual Storage boolval(TypeMatch tm, Storage s, X64 *x64, bool probe) {
        TypeSpec ts = typesubst(alloc_ts, tm);
        int o = offset.concretize(tm);
        return ts.boolval(s + o, x64, probe);
    }

    virtual void compare(TypeMatch tm, Storage s, Storage t, X64 *x64, Label less, Label greater) {
        TypeSpec ts = typesubst(alloc_ts, tm);
        int o = offset.concretize(tm);
        ts.compare(s + o, t + o, x64, less, greater);
    }
};


class PartialVariable: public Variable {
public:
    std::set<std::string> uninitialized_member_names;
    std::set<std::string> initialized_member_names;
    
    PartialVariable(std::string name, TypeSpec pts, TypeSpec vts)
        :Variable(name, pts, vts) {
    }

    virtual Value *matched(Value *cpivot, TypeMatch &match) {
        return make_partial_variable_value(this, cpivot, match);
    }

    virtual void set_member_names(std::vector<std::string> mn) {
        uninitialized_member_names.insert(mn.begin(), mn.end());
    }
    
    virtual void be_initialized(std::string name) {
        initialized_member_names.insert(name);
        uninitialized_member_names.erase(name);
    }
    
    virtual bool is_initialized(std::string name) {
        return initialized_member_names.count(name) == 1;
    }

    virtual bool is_uninitialized(std::string name) {
        return uninitialized_member_names.count(name) == 1;
    }
    
    virtual bool is_complete() {
        return uninitialized_member_names.size() == 0;
    }

    virtual void be_complete() {
        initialized_member_names.insert(uninitialized_member_names.begin(), uninitialized_member_names.end());
        uninitialized_member_names.clear();
    }

    virtual bool is_dirty() {
        return initialized_member_names.size() != 0;
    }
};


class RetroVariable: public Variable {
public:
    RetroVariable(std::string name, TypeSpec pts, TypeSpec vts)
        :Variable(name, pts, vts) {
    }

    virtual void finalize(X64 *x64) {
        // These are not finalized
    }
};


class Evaluable: public Allocable {
public:
    std::vector<Variable *> arg_variables;
    
    Evaluable(std::string name, TypeSpec pts, TypeSpec vts)
        :Allocable(name, pts, vts) {
    }

    virtual void set_outer_scope(Scope *os) {
        Allocable::set_outer_scope(os);
        
        std::vector<Variable *> avs;
        
        for (unsigned i = os->contents.size() - 1; i != (unsigned)-1; i--) {
            Variable *av = variable_cast(os->contents[i].get());
            
            if (av && av->alloc_ts[0] == dvalue_type)
                avs.push_back(av);
            else
                break;
        }
        
        while (avs.size()) {
            arg_variables.push_back(avs.back());
            avs.pop_back();
        }
    }
    
    virtual Value *matched(Value *cpivot, TypeMatch &match) {
        return make_evaluable_value(this, cpivot, match);
    }
    
    virtual void allocate() {
        Allocable::allocate();
        
        where = ALIAS;
        offset = outer_scope->reserve(Allocation(ADDRESS_SIZE));
    }
};


class Role: public Allocable {
public:
    int virtual_index;
    DataScope *inner_scope;
    
    Role(std::string name, TypeSpec pts, TypeSpec rts)
        :Allocable(name, pts, rts) {
        virtual_index = -1;
        inner_scope = new DataScope();  // we won't look up from the inside
        inner_scope->set_pivot_type_hint(pts);
    }
    
    virtual Value *matched(Value *cpivot, TypeMatch &match) {
        return make_role_value(this, cpivot, match);
    }

    virtual void set_outer_scope(Scope *os) {
        Allocable::set_outer_scope(os);
        
        // This is a bastardized version, as the outer scope will not be aware that the
        // inner scope is inside it. But it won't even be looked up directly, so that's OK.
        inner_scope->set_outer_scope(os);
    }

    virtual DataScope *find_inner_scope(std::string n) {
        if (name == n)
            return inner_scope;
        else
            return NULL;
    }

    virtual bool complete_role() {
        TypeMatch iftm = type_parameters_to_match(alloc_ts);
        TypeMatch empty_match;
        Scope *base_inner_scope = alloc_ts.get_inner_scope();
        
        for (auto &d : inner_scope->contents) {
            Function *f = function_cast(d.get());
            
            if (f) {
                bool found = false;
                
                for (auto &e : base_inner_scope->contents) {
                    Function *iff = function_cast(e.get());
                    
                    if (iff) {
                        if (f->does_implement(empty_match, iff, iftm)) {
                            found = true;
                            break;
                        }
                    }
                }
                
                if (!found) {
                    std::cerr << "Invalid function override " << f->name << "!\n";
                    return false;
                }
            }
        }
        
        return true;
    }

    virtual void allocate() {
        Allocable::allocate();
    
        where = MEMORY;
            
        Allocation a = alloc_ts.measure();
        a.bytes += ROLE_HEADER_SIZE;
        offset = outer_scope->reserve(a);
        offset.bytes += ROLE_HEADER_SIZE;
        
        DataScope *ds = data_scope_cast(outer_scope);
        virtual_index = ds->virtual_reserve(alloc_ts.get_virtual_table());
        
        for (auto &d : inner_scope->contents) {
            Function *f = function_cast(d.get());
            
            if (f) {
                int vi = f->set_self_adjustment(offset);
                ds->set_virtual_entry(virtual_index + vi, f);
            }
        }
        
        inner_scope->allocate();
    }
    
    virtual int get_offset(TypeMatch tm) {
        if (where == NOWHERE)
            throw INTERNAL_ERROR;

        return offset.concretize(tm);
    }
    /*
    virtual void create(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        // Only NOWHERE_MEMORY
        TypeSpec ts = typesubst(role_ts, tm);
        int o = offset.concretize(tm);
        
        x64->op(MOVQ, t.address + o + ROLE_WEAKREF_OFFSET, 0);
        
        ts.create(s, t + o, x64);
    }
    */
    virtual void destroy(TypeMatch tm, Storage s, X64 *x64) {
        TypeSpec ts = typesubst(alloc_ts, tm);
        int o = offset.concretize(tm);
        ts.destroy(s + o, x64);
    }
    
    virtual void init_vt(TypeMatch tm, Address addr, int data_offset, Label vt_label, int virtual_offset, X64 *x64) {
        int role_data_offset = data_offset + get_offset(tm);
        int role_virtual_offset = virtual_offset + virtual_index;
        TypeSpec role_ts = typesubst(alloc_ts, tm);
        
        role_ts.init_vt(addr, role_data_offset, vt_label, role_virtual_offset, x64);
    }        
};


class BaseRole: public Role {
public:
    BaseRole(std::string name, TypeSpec pts, TypeSpec rts)
        :Role(name, pts, rts) {
    }
    
    virtual void allocate() {
        // This will be called twice, must skip ourselves for the second time
        if (where != NOWHERE)
            return;
        
        // Overlay weakrefcount and VT headers
        Allocation overlay = outer_scope->reserve(Allocation(-ADDRESS_SIZE * 2));
        if (overlay.concretize() != ADDRESS_SIZE)  // original offset
            throw INTERNAL_ERROR;
        
        Role::allocate();
    }
    
    virtual void init_vt(TypeMatch tm, Address addr, int data_offset, Label vt_label, int virtual_offset, X64 *x64) {
        // Don't overwrite the derived class' VT pointer!
    }
};
