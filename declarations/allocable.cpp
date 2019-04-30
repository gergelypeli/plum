

class Allocable: public Identifier {
public:
    TypeSpec alloc_ts;
    Allocation offset;
    StorageWhere where;
    
    Allocable(std::string name, TypeSpec ats)
        :Identifier(name) {
        where = NOWHERE;
        alloc_ts = ats;
        
        if (alloc_ts == NO_TS)
            throw INTERNAL_ERROR;
    }

    virtual bool is_abstract() {
        return false;  // Interface implementations will override it. And others.
    }

    virtual TypeSpec get_typespec(TypeMatch tm) {
        return typesubst(alloc_ts, tm);
    }

    virtual Scope *get_allocation_scope() {
        return outer_scope;
    }
    
    virtual void allocate() {
        if (where != NOWHERE)
            throw INTERNAL_ERROR;
    }

    virtual Regs borrowing_requirements() {
        if (where == NOWHERE)
            throw INTERNAL_ERROR;
        else if (where == ALIAS) {
            // We can't be sure where the alias points to, so require both unclobbered
            return Regs::stackvars() | Regs::heapvars();
        }
        else if (where != MEMORY)
            throw INTERNAL_ERROR;
            
        ScopeType st = get_allocation_scope()->type;
            
        if (st == MODULE_SCOPE)
            return Regs::heapvars();
        else if (st == CODE_SCOPE || st == ARGUMENT_SCOPE) {
            // What flags are necessary to borrow a local variable?

            if (alloc_ts[0] == lvalue_type) {
                // For regular variables the stack must be unclobbered
                return Regs::stackvars();
            }
            else {
                // For constant (including T Ptr <Partial>) variables nothing is necessary
                return Regs();
            }
        }
        else
            throw INTERNAL_ERROR;
    }

    virtual int get_offset(TypeMatch tm) {
        // NOTE: will be overridden in aliased roles
        if (where == NOWHERE)
            throw INTERNAL_ERROR;
            
        return offset.concretize(tm);
    }

    virtual Storage get_storage(TypeMatch tm, Storage s) {
        int o = get_offset(tm);
        
        if (s.where == MEMORY) {
            if (where == MEMORY)
                return Storage(where, s.address + o);
            else if (where == ALIAS)
                return Storage(where, s.address + o, 0);
            else if (where == RETRO)
                return Storage(where, s.address + o);
            else
                throw INTERNAL_ERROR;
        }
        else if (s.where == ALIAS) {
            if (where == MEMORY)
                return Storage(ALIAS, s.address, s.value + o);
            else
                throw INTERNAL_ERROR;
        }
        else
            throw INTERNAL_ERROR;  // all variable containers must use MEMORY or ALIAS
    }

    virtual Storage get_local_storage() {
        // Without pivot as a function local variable
        return get_storage(TypeMatch(), get_allocation_scope()->get_local_storage());
    }
    
    virtual void destroy(TypeMatch tm, Storage s, X64 *x64) {
        throw INTERNAL_ERROR;
    }
};


class Variable: public Allocable {
public:
    AsWhat as_what;
    
    Variable(std::string name, TypeSpec vts)
        :Allocable(name, vts) {
        if (vts == NO_TS)
            throw INTERNAL_ERROR;
            
        as_what = AS_VARIABLE;
        
        //std::cerr << "Variable " << pts << " " << name << " is " << vts << ".\n";
    }

    virtual void set_outer_scope(Scope *os) {
        Allocable::set_outer_scope(os);
        
        if (os && os->type == ARGUMENT_SCOPE) {
            if (as_what != AS_VARIABLE)
                throw INTERNAL_ERROR;
                
            as_what = AS_ARGUMENT;
        }
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
            where == NOWHERE ? Allocation() :
            where == MEMORY ? alloc_ts.measure() :
            where == ALIAS ? Allocation(ALIAS_SIZE) :
            where == RETRO ? Allocation(ADDRESS_SIZE) :
            throw INTERNAL_ERROR
        );
        
        //if (a.count1 || a.count2 || a.count3)
        //    std::cerr << "Hohoho, allocating variable " << name << " with size " << a << ".\n";
        
        offset = get_allocation_scope()->reserve(a);
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
    
    SelfVariable(std::string name, TypeSpec vts)
        :Variable(name, vts) {
        self_info.reset(new SelfInfo);
    }
    
    virtual Value *matched(Value *cpivot, Scope *scope, TypeMatch &match) {
        return make<SelfVariableValue>(this, cpivot, scope, match, self_info.get());
    }
};


class PartialVariable: public Variable {
public:
    std::unique_ptr<PartialInfo> partial_info;

    PartialVariable(std::string name, TypeSpec vts)
        :Variable(name, vts) {
        partial_info.reset(new PartialInfo);
    }

    virtual Value *matched(Value *cpivot, Scope *scope, TypeMatch &match) {
        return make<PartialVariableValue>(this, cpivot, scope, match, partial_info.get());
    }
};


class GlobalVariable: public Variable {
public:
    TypeSpec class_ts;  // TODO: allow overriding this
    Function *initializer_function;
    
    GlobalVariable(std::string n, TypeSpec mts, TypeSpec cts)
        :Variable(n, mts) {
        class_ts = cts;
        initializer_function = NULL;
    }

    virtual void set_initializer_function(Function *f) {
        initializer_function = f;
    }

    virtual TypeSpec get_pivot_ts() {
        return NO_TS;
    }

    virtual TypeSpec get_class_ts() {
        return class_ts;
    }

    virtual bool is_called(std::string n) {
        return n == name;
    }

    virtual bool is_abstract() {
        return true;  // No explicit initialization by its enclosing type
    }

    virtual Scope *get_allocation_scope() {
        return outer_scope->get_module_scope();
    }

    virtual void allocate() {
        Variable::allocate();
        
        // Allocation happens with ordered modules
        outer_scope->get_root_scope()->register_global_variable(this);
    }

    //virtual Storage get_local_storage() {  // TODO: shall we call it get_global_storage?
    //    Storage module_storage = outer_scope->get_module_scope()->get_global_storage();
    //    return get_storage(TypeMatch(), module_storage);
    //}

    // No initializers are accessible from the language, done by the runtime itself
    virtual Label compile_initializer(X64 *x64) {
        Label label;
        x64->code_label_local(label, name + "_initializer");  // FIXME: ambiguous name!

        Storage s = preinitialize_class(class_ts, x64);
        if (s.where != REGISTER)
            throw INTERNAL_ERROR;

        Storage t = get_local_storage();
        if (t.where != MEMORY)
            throw INTERNAL_ERROR;

        alloc_ts.store(s, Storage(STACK), x64);
        x64->op(CALL, function_get_label(initializer_function, x64));
        alloc_ts.create(Storage(STACK), t, x64);

        x64->op(RET);
        return label;
    }

    virtual Label compile_finalizer(X64 *x64) {
        Label label;
        x64->code_label_local(label, name + "_finalizer");  // FIXME: ambiguous name!

        Storage s = get_local_storage();
        
        alloc_ts.destroy(s, x64);

        x64->op(RET);
        
        return label;
    }
};


// This is a mutant global variable, used with Unit subtypes that won't be passed
// as pivot arguments, useful for grouping built-in functions in a namespace.
class GlobalNamespace: public GlobalVariable {
public:
    GlobalNamespace(std::string n, TypeSpec mts)
        :GlobalVariable(n, mts, NO_TS) {
    }

    virtual Regs borrowing_requirements() {
        return Regs();  // the scope type check would be useless here
    }

    virtual void allocate() {
        where = MEMORY;  // just to conform to others
    }

    virtual Storage get_local_storage() {
        return Storage();  // TODO: anything uses this?
    }

    virtual Label compile_initializer(X64 *x64) {
        Label label;
        x64->code_label_local(label, name + "_initializer");  // FIXME: ambiguous name!

        if (initializer_function)
            x64->op(CALL, function_get_label(initializer_function, x64));

        x64->op(RET);
        return label;
    }

    virtual Label compile_finalizer(X64 *x64) {
        Label label;
        x64->code_label_local(label, name + "_finalizer");  // FIXME: ambiguous name!

        x64->op(RET);
        
        return label;
    }
};


class RetroVariable: public Variable {
public:
    RetroVariable(std::string name, TypeSpec vts)
        :Variable(name, vts) {
        // These are actually arguments
        as_what = AS_ARGUMENT;
    }

    virtual void finalize(X64 *x64) {
        // These are not finalized
    }
};


class Evaluable: public Allocable {
public:
    std::vector<Variable *> dvalue_variables;
    
    Evaluable(std::string name, TypeSpec vts)
        :Allocable(name, vts) {
    }

    virtual void set_outer_scope(Scope *os) {
        Allocable::set_outer_scope(os);
        
        // Collect the preceding Dvalue arguments for convenience
        std::vector<Variable *> dvs;
        
        for (unsigned i = os->contents.size() - 1; i != (unsigned)-1; i--) {
            Variable *av = ptr_cast<Variable>(os->contents[i].get());
            
            if (av && av->alloc_ts[0] == dvalue_type)
                dvs.push_back(av);
            else
                break;
        }
        
        while (dvs.size()) {
            dvalue_variables.push_back(dvs.back());
            dvs.pop_back();
        }
    }

    virtual std::vector<Variable *> get_dvalue_variables() {
        return dvalue_variables;
    }
    
    virtual Value *matched(Value *cpivot, Scope *scope, TypeMatch &match) {
        return make<EvaluableValue>(this, cpivot, match);
    }
    
    virtual void allocate() {
        Allocable::allocate();
        
        where = ALIAS;
        offset = outer_scope->reserve(Allocation(ALIAS_SIZE));
    }
};


// Extend the lifetime of Lvalue containers until the end of the innermost scope
// If created, it must be used, no runtime checks are made. This also means that
// if an operation raises an exception, this must be set before that, because the
// decref will happen anyway.
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
    virtual void compile(TypeMatch tm, X64 *x64) {
    }
    
    virtual Label get_virtual_entry_label(TypeMatch tm, X64 *x64) {
        throw INTERNAL_ERROR;
    }
    
    virtual std::ostream &out_virtual_entry(std::ostream &os, TypeMatch tm) {
        throw INTERNAL_ERROR;
    }
    
    virtual ~VirtualEntry() {
    }
};


class Autoconvertible {
public:
    virtual Label get_autoconv_table_label(TypeMatch tm, X64 *x64) {
        throw INTERNAL_ERROR;
    }

    virtual std::vector<AutoconvEntry> get_autoconv_table(TypeMatch tm) {
        throw INTERNAL_ERROR;
    }
};


class AutoconvVirtualEntry: public VirtualEntry {
public:
    Autoconvertible *autoconvertible;
    
    AutoconvVirtualEntry(Autoconvertible *a) {
        autoconvertible = a;
    }
    
    virtual Label get_virtual_entry_label(TypeMatch tm, X64 *x64) {
        return autoconvertible->get_autoconv_table_label(tm, x64);
    }

    virtual std::ostream &out_virtual_entry(std::ostream &os, TypeMatch tm) {
        // A bit overkill, just for debugging
        std::vector<AutoconvEntry> act = autoconvertible->get_autoconv_table(tm);
        
        os << "CONV to";
        
        if (act.size()) {
            for (auto ace : act)
                os << " " << ace.role_ts;
        }
        else
            os << " nothing";
            
        return os;
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
    virtual std::vector<std::string> get_partial_initializable_names() {
        throw INTERNAL_ERROR;
    }
};


class TemporaryAlias: public Declaration {
public:
    Allocation offset;

    TemporaryAlias()
        :Declaration() {
    }
    
    virtual void allocate() {
        offset = outer_scope->reserve(ALIAS_SIZE);
    }
    
    virtual Storage get_local_storage() {
        Storage ls = outer_scope->get_local_storage();
        
        if (ls.where != MEMORY)
            throw INTERNAL_ERROR;
            
        return Storage(ALIAS, ls.address + offset.concretize(), 0);
    }
    
    virtual Storage process(Storage s, X64 *x64) {
        Storage ts = get_local_storage();
        
        if (s.where == MEMORY) {
            if (s.address.base == RBP) {
                if (s.address.index != NOREG)
                    throw INTERNAL_ERROR;
                
                // Stack-local addresses are handled in compile time
                return s;
            }
            else if (s.address.base == RSP) {
                // RecordPreinitializer and FunctionCall can handle this, but they can
                // store RBP-relative values and fix later, while we can't.
                throw INTERNAL_ERROR;
            }
            else {
                // Dynamic addresses will be stored, and used as an ALIAS
                x64->op(LEA, R10, s.address);
                x64->op(MOVQ, ts.address, R10);
                
                return ts;
            }
        }
        else if (s.where == ALIAS) {
            if (s.address.base == RBP && s.address.index == NOREG) {
                // Stack-local addresses are handled in compile time
                return s;
            }
            else
                throw INTERNAL_ERROR;
        }
        else
            throw INTERNAL_ERROR;
    }
};
