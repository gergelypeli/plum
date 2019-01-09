

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

    virtual bool is_abstract() {
        return false;  // Interface implementations will override it
    }

    virtual TypeSpec get_typespec(TypeMatch tm) {
        return typesubst(alloc_ts, tm);
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
    
    //virtual devector<VirtualEntry *> get_virtual_table() {
    //    throw INTERNAL_ERROR;
    //}

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
            as_what = AS_ARGUMENT;  // (name == "$" ? AS_PIVOT_ARGUMENT : AS_ARGUMENT);
    }
    
    virtual TypeSpec get_typespec(TypeMatch tm) {
        TypeSpec ts = typesubst(alloc_ts, tm);
        
        if (ts[0] == ovalue_type)
            ts = ts.unprefix(ovalue_type);
        
        return ts;
    }
    
    virtual Value *matched(Value *cpivot, Scope *scope, TypeMatch &match) {
        // cpivot may be NULL if this is a local variable
        return make<VariableValue>(this, cpivot, scope, match);
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
        Allocable::finalize(x64);  // Place label
        //x64->runtime->log(std::string("Finalizing local variable ") + name);
        
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
        ts.store(s.where == NOWHERE ? s : s + o, t + o, x64);
    }

    virtual void destroy(TypeMatch tm, Storage s, X64 *x64) {
        //x64->runtime->log(std::string("Destroying variable ") + name);
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


class SelfVariable: public Variable {
public:
    std::unique_ptr<SelfInfo> self_info;
    
    SelfVariable(std::string name, TypeSpec pts, TypeSpec vts)
        :Variable(name, pts, vts) {
        self_info.reset(new SelfInfo);
    }
    
    virtual Value *matched(Value *cpivot, Scope *scope, TypeMatch &match) {
        return make<SelfVariableValue>(this, cpivot, scope, match, self_info.get());
    }
};


class PartialVariable: public Variable {
public:
    std::unique_ptr<PartialInfo> partial_info;

    PartialVariable(std::string name, TypeSpec pts, TypeSpec vts)
        :Variable(name, pts, vts) {
        partial_info.reset(new PartialInfo);
    }

    virtual Value *matched(Value *cpivot, Scope *scope, TypeMatch &match) {
        return make<PartialVariableValue>(this, cpivot, scope, match, partial_info.get());
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
    
    virtual Value *matched(Value *cpivot, Scope *scope, TypeMatch &match) {
        return make<EvaluableValue>(this, cpivot, match);
    }
    
    virtual void allocate() {
        Allocable::allocate();
        
        where = ALIAS;
        offset = outer_scope->reserve(Allocation(ADDRESS_SIZE));
    }
};


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

    virtual Value *matched(Value *cpivot, Scope *scope, TypeMatch &match) {
        return make<PartialVariableValue>(this, cpivot, scope, match, partial_info.get());
    }
};


// Extend the lifetime of Lvalue containers until the end of the innermost scope
class Unborrow: public Declaration {
public:
    TypeSpec heap_ts;
    bool is_used;
    Allocation offset;
    
    Unborrow(TypeSpec hts)
        :Declaration() {
        heap_ts = hts;
        is_used = false;
    }
    
    virtual void allocate() {
        offset = outer_scope->reserve(Allocation(REFERENCE_SIZE));
    }

    virtual Address get_address() {
        is_used = true;
        return Address(RBP, offset.concretize());
    }
    
    virtual void finalize(X64 *x64) {
        if (!is_used)
            return;
            
        //x64->runtime->log("Unborrowing.");
        x64->op(MOVQ, R10, get_address());
        heap_ts.decref(R10, x64);
    }
};


class VirtualEntry {
public:
    virtual Label get_virtual_entry_label(TypeMatch tm, X64 *x64) {
        throw INTERNAL_ERROR;
    }
    
    virtual std::ostream &out_virtual_entry(std::ostream &os, TypeMatch tm) {
        throw INTERNAL_ERROR;
    }
};


class RoleVirtualEntry: public VirtualEntry {
public:
    Identifier *type;
    Allocable *allocable;
    
    RoleVirtualEntry(Identifier *t, Allocable *a) {
        type = t;
        allocable = a;
    }
    
    virtual Label get_virtual_entry_label(TypeMatch tm, X64 *x64) {
        if (allocable)
            return allocable->get_typespec(tm).get_virtual_table_label(x64);
        else
            return x64->runtime->zero_label;
    }

    virtual std::ostream &out_virtual_entry(std::ostream &os, TypeMatch tm) {
        if (allocable)
            return os << "ROLE " << type->name << " (" << allocable->get_typespec(tm) << ")";
        else
            return os << "ROLE " << type->name;
    }
};


class FfwdVirtualEntry: public VirtualEntry {
public:
    Allocation offset;
    
    FfwdVirtualEntry(Allocation o) {
        offset = o;
    }
    
    virtual Label get_virtual_entry_label(TypeMatch tm, X64 *x64) {
        Label label;
        x64->absolute_label(label, -offset.concretize(tm));  // forcing an int into an unsigned64...
        return label;
    }

    virtual std::ostream &out_virtual_entry(std::ostream &os, TypeMatch tm) {
        return os << "FFWD " << -offset.concretize(tm);
    }
};


class PartialInitializable {
public:
    virtual std::vector<std::string> get_member_names() {
        throw INTERNAL_ERROR;
    }
};
