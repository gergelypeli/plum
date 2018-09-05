

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
        Storage fn_storage(MEMORY, Address(RBP, 0));
        return get_storage(TypeMatch(), fn_storage);
    }
    
    virtual void destroy(TypeMatch tm, Storage s, X64 *x64) {
        throw INTERNAL_ERROR;
    }
};


class Variable: public Allocable {
public:
    AsWhat as_what;
    
    Variable(std::string name, TypeSpec pts, TypeSpec vts)
        :Allocable(name, pts, vts) {
        if (vts == NO_TS)
            throw INTERNAL_ERROR;
            
        as_what = AS_VARIABLE;
        
        std::cerr << "Variable " << pts << " " << name << " is " << vts << ".\n";
    }

    virtual void set_outer_scope(Scope *os) {
        Allocable::set_outer_scope(os);
        
        if (os && os->type == ARGUMENT_SCOPE)
            as_what = (name == "$" ? AS_PIVOT_ARGUMENT : AS_ARGUMENT);
    }
    
    virtual TypeSpec get_typespec(TypeMatch tm) {
        TypeSpec ts = typesubst(alloc_ts, tm);
        
        if (as_what == AS_ARGUMENT && outer_scope == outer_scope->get_function_scope()->head_scope) {
            if (ts[0] == lvalue_type)
                return ts;
            else if (ts[0] == ovalue_type)
                return ts.reprefix(ovalue_type, lvalue_type);
            else if (ts[0] == dvalue_type || ts[0] == code_type)
                return ts;
            else
                return ts.lvalue();
        }
        
        return ts;
    }
    
    virtual Value *matched(Value *cpivot, TypeMatch &match) {
        // cpivot may be NULL if this is a local variable
        return make<VariableValue>(this, cpivot, match);
    }
    
    virtual void allocate() {
        Allocable::allocate();
            
        where = alloc_ts.where(as_what);
            
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
    
    virtual void equal(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        TypeSpec ts = typesubst(alloc_ts, tm);
        int o = offset.concretize(tm);
        ts.equal(s + o, t + o, x64);
    }

    virtual void compare(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        TypeSpec ts = typesubst(alloc_ts, tm);
        int o = offset.concretize(tm);
        ts.compare(s + o, t + o, x64);
    }
};


class PartialVariable: public Variable {
public:
    std::unique_ptr<PartialInfo> partial_info;

    PartialVariable(std::string name, TypeSpec pts, TypeSpec vts)
        :Variable(name, pts, vts) {
        partial_info.reset(new PartialInfo);
    }

    virtual Value *matched(Value *cpivot, TypeMatch &match) {
        return make<PartialVariableValue>(this, cpivot, match, partial_info.get());
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
            Variable *av = ptr_cast<Variable>(os->contents[i].get());
            
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
        return make<EvaluableValue>(this, cpivot, match);
    }
    
    virtual void allocate() {
        Allocable::allocate();
        
        where = ALIAS;
        offset = outer_scope->reserve(Allocation(ADDRESS_SIZE));
    }
};


class Role;
static Declaration *make_shadow_role(Role *orole, Role *prole);

class Role: public Allocable {
public:
    std::unique_ptr<RoleScope> inner_scope;
    Role *parent_role;
    
    Role(std::string name, TypeSpec pts, TypeSpec rts, DataScope *original_scope)
        :Allocable(name, pts, rts) {
        parent_role = NULL;
        
        inner_scope.reset(new RoleScope(this, original_scope));  // we won't look up from the inside
        inner_scope->set_name(name);
        inner_scope->be_virtual_scope();
        inner_scope->set_pivot_type_hint(pts);

        for (auto &d : original_scope->contents) {
            Role *r = ptr_cast<Role>(d.get());
            
            if (r)
                inner_scope->add(make_shadow_role(r, this));
        }
    }
    
    virtual bool complete_role() {
        inner_scope->leave();
        
        return true;
    }
    
    virtual Value *matched(Value *cpivot, TypeMatch &match) {
        return make<RoleValue>(this, cpivot, match);
    }

    virtual void set_outer_scope(Scope *os) {
        Allocable::set_outer_scope(os);
        
        // This is a bastardized version, as the outer scope will not be aware that the
        // inner scope is inside it. But it won't even be looked up directly, so that's OK.
        inner_scope->set_outer_scope(os);
    }

    virtual DataScope *find_inner_scope(std::string n) {
        if (name == n)
            return inner_scope.get();
        else
            return NULL;
    }

    virtual void allocate() {
        Allocable::allocate();
    
        where = MEMORY;
            
        Allocation a = alloc_ts.measure();
        a.bytes += ROLE_HEADER_SIZE;
        offset = outer_scope->reserve(a);
        offset.bytes += ROLE_HEADER_SIZE;
        
        inner_scope->virtual_reserve(alloc_ts.get_virtual_table());

        inner_scope->allocate();
    }
    
    virtual int get_offset(TypeMatch tm) {
        if (where == NOWHERE)
            throw INTERNAL_ERROR;

        return offset.concretize(tm);
    }
    
    virtual int compute_offset(TypeMatch &tm) {
        // Outward just pass the initial tm for the class scope
        int offset = (parent_role ? parent_role->compute_offset(tm) : 0);
        
        // Inward compute the offset, and update the match for the inner roles
        offset += get_offset(tm);
        TypeSpec ts = typesubst(alloc_ts, tm);
        tm = ts.match();
        
        return offset;
    }

    virtual void compute_match(TypeMatch &tm) {
        // Outward just pass the initial tm for the class scope
        if (parent_role)
            parent_role->compute_match(tm);
        
        // Inward update the match for the inner roles
        TypeSpec ts = typesubst(alloc_ts, tm);
        tm = ts.match();
    }
    
    virtual void destroy(TypeMatch tm, Storage s, X64 *x64) {
        TypeSpec ts = typesubst(alloc_ts, tm);
        int o = offset.concretize(tm);
        ts.destroy(s + o, x64);
    }
    
    virtual void init_vt(TypeMatch tm, Address addr, int data_offset, Label vt_label, int virtual_offset, X64 *x64) {
        int role_data_offset = data_offset + get_offset(tm);
        int role_virtual_offset = virtual_offset + inner_scope->virtual_offset;
        TypeSpec role_ts = typesubst(alloc_ts, tm);
        
        role_ts.init_vt(addr, role_data_offset, vt_label, role_virtual_offset, x64);
    }
};


class BaseRole: public Role {
public:
    BaseRole(std::string name, TypeSpec pts, TypeSpec rts, DataScope *os)
        :Role(name, pts, rts, os) {
    }
    
    virtual void allocate() {
        // This will be called twice, must skip ourselves for the second time
        if (where != NOWHERE)
            return;
        
        // Overlay VT header
        Allocation overlay = outer_scope->reserve(Allocation(-ADDRESS_SIZE));
        if (overlay.concretize() != ADDRESS_SIZE)  // original offset
            throw INTERNAL_ERROR;
        
        Role::allocate();
    }
    
    virtual void init_vt(TypeMatch tm, Address addr, int data_offset, Label vt_label, int virtual_offset, X64 *x64) {
        // Don't overwrite the derived class' VT pointer!
    }
};


class ShadowRole: public Role {
public:
    Role *original_role;
    
    ShadowRole(Role *orole, Role *prole)
        :Role(orole->name, prole->pivot_ts, orole->alloc_ts, orole->inner_scope.get()) {
        // NOTE: the pivot is from the parent role, not the original one, as all overriding
        // methods have our implementing type for pivot
        original_role = orole;
        parent_role = prole;
    }

    virtual void allocate() {
        Allocable::allocate();
        
        where = original_role->where;
        offset = original_role->offset;
        inner_scope->virtual_offset = original_role->inner_scope->virtual_offset;
        
        if (where == NOWHERE)
            throw INTERNAL_ERROR;
            
        inner_scope->allocate();
    }
};


Declaration *make_shadow_role(Role *orole, Role *prole) {
    return new ShadowRole(orole, prole);
}


class SingletonVariable: public Variable {
public:
    //SingletonScope *singleton_scope;
    //Label application_label;
    
    SingletonVariable(std::string n, TypeSpec mts)
        :Variable(n, NO_TS, mts) {
        //singleton_scope = ss;
    }

    virtual void allocate() {
        // This is just an alias, we don't allocate actual data here
        where = MEMORY;
    }

    //virtual void set_application_label(Label al) {
    //    application_label = al;
    //}

    virtual Storage get_storage(TypeMatch tm, Storage s) {
        throw INTERNAL_ERROR;  // not contained in anything else
    }

    virtual Storage get_local_storage() {
        return outer_scope->get_singleton_scope()->get_global_storage();
        //int offset = singleton_scope->offset.concretize();
        //return Storage(MEMORY, Address(application_label, offset));
    }
};


class PartialSingletonVariable: public SingletonVariable {
public:
    std::unique_ptr<PartialInfo> partial_info;

    PartialSingletonVariable(std::string name, TypeSpec mts)
        :SingletonVariable(name, mts) {
        partial_info.reset(new PartialInfo);
    }

    virtual Value *matched(Value *cpivot, TypeMatch &match) {
        return make<PartialVariableValue>(this, cpivot, match, partial_info.get());
    }
};


// TODO: this is technically not a subclass of Allocable
// Commenting out until becomes usable again with strong ref counts
/*
class Borrow: public Declaration {
public:
    Allocation offset;
    
    virtual void allocate() {
        offset = outer_scope->reserve(Allocation(REFERENCE_SIZE));
    }
    
    virtual Address get_address() {
        return Address(RBP, offset.concretize());
    }
    
    virtual void finalize(X64 *x64) {
        //x64->log("Unborrowing.");
        x64->op(MOVQ, RBX, get_address());
        //x64->runtime->decweakref(RBX);
    }
};
*/
