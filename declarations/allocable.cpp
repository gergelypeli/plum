
Allocable::Allocable(std::string name, PivotRequirement pr, TypeSpec ats)
    :Identifier(name, pr) {
    where = NOWHERE;
    alloc_ts = ats;
    
    if (alloc_ts == NO_TS)
        throw INTERNAL_ERROR;
}

bool Allocable::is_abstract() {
    return false;  // Interface implementations will override it. And others.
}

TypeSpec Allocable::get_typespec(TypeMatch tm) {
    return typesubst(alloc_ts, tm);
}

Scope *Allocable::get_allocation_scope() {
    return outer_scope;
}

void Allocable::allocate() {
    if (where != NOWHERE)
        throw INTERNAL_ERROR;
}

Regs Allocable::borrowing_requirements() {
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

int Allocable::get_offset(TypeMatch tm) {
    // NOTE: will be overridden in aliased roles
    if (where == NOWHERE)
        throw INTERNAL_ERROR;
        
    return allocsubst(offset, tm).concretize();
}

Storage Allocable::get_storage(TypeMatch tm, Storage s) {
    int o = get_offset(tm);
    
    if (s.where == MEMORY) {
        if (where == MEMORY)
            return Storage(where, s.address + o);
        else if (where == ALIAS)
            return Storage(where, s.address + o, 0);
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

Storage Allocable::get_local_storage() {
    // Without pivot as a function local variable
    return get_storage(TypeMatch(), get_allocation_scope()->get_local_storage());
}

void Allocable::destroy(TypeMatch tm, Storage s, X64 *x64) {
    throw INTERNAL_ERROR;
}



Variable::Variable(std::string name, TypeSpec vts)
    :Allocable(name, VARIABLE_PIVOT, vts) {
    if (vts == NO_TS)
        throw INTERNAL_ERROR;
        
    as_what = AS_VARIABLE;
    
    //std::cerr << "Variable " << pts << " " << name << " is " << vts << ".\n";
}

void Variable::set_outer_scope(Scope *os) {
    Allocable::set_outer_scope(os);
    
    if (os && os->type == ARGUMENT_SCOPE) {
        if (as_what != AS_VARIABLE)
            throw INTERNAL_ERROR;
            
        as_what = AS_ARGUMENT;
    }
}

TypeSpec Variable::get_typespec(TypeMatch tm) {
    TypeSpec ts = typesubst(alloc_ts, tm);
    
    if (ts[0] == ovalue_type)
        ts = ts.unprefix(ovalue_type);
    
    return ts;
}

Value *Variable::matched(Value *cpivot, Scope *scope, TypeMatch &match) {
    // cpivot may be NULL if this is a local variable
    return make<VariableValue>(this, cpivot, scope, match);
}

void Variable::allocate() {
    Allocable::allocate();
    
    where = alloc_ts.where(as_what);
        
    Allocation a = (
        where == NOWHERE ? Allocation() :
        where == MEMORY ? alloc_ts.measure() :
        where == ALIAS ? Allocation(ALIAS_SIZE) :
        throw INTERNAL_ERROR
    );
    
    //if (a.count1 || a.count2 || a.count3)
    //    std::cerr << "Hohoho, allocating variable " << name << " with size " << a << ".\n";
    
    offset = get_allocation_scope()->reserve(a);
    //std::cerr << "Allocated variable " << name << " to " << offset << ".\n";
}

void Variable::finalize(X64 *x64) {
    // This method is only called on local variables, and it's an overload
    Allocable::finalize(x64);  // Place label
    //x64->runtime->log(std::string("Finalizing local variable ") + name);
    
    alloc_ts.destroy(get_local_storage(), x64);
}

void Variable::create(TypeMatch tm, Storage s, Storage t, X64 *x64) {
    TypeSpec ts = typesubst(alloc_ts, tm);
    int o = allocsubst(offset, tm).concretize();
    ts.create(s.where == NOWHERE ? s : s + o, t + o, x64);
}

void Variable::store(TypeMatch tm, Storage s, Storage t, X64 *x64) {
    TypeSpec ts = typesubst(alloc_ts, tm);
    int o = allocsubst(offset, tm).concretize();
    ts.store(s.where == NOWHERE ? s : s + o, t + o, x64);
}

void Variable::destroy(TypeMatch tm, Storage s, X64 *x64) {
    //x64->runtime->log(std::string("Destroying variable ") + name);
    TypeSpec ts = typesubst(alloc_ts, tm);
    int o = allocsubst(offset, tm).concretize();
    ts.destroy(s + o, x64);
}

void Variable::equal(TypeMatch tm, Storage s, Storage t, X64 *x64) {
    TypeSpec ts = typesubst(alloc_ts, tm);
    int o = allocsubst(offset, tm).concretize();
    ts.equal(s + o, t + o, x64);
}

void Variable::compare(TypeMatch tm, Storage s, Storage t, X64 *x64) {
    TypeSpec ts = typesubst(alloc_ts, tm);
    int o = allocsubst(offset, tm).concretize();
    ts.compare(s + o, t + o, x64);
}

void Variable::debug(TypeMatch tm, X64 *x64) {
    if (where != MEMORY && where != ALIAS)
        throw INTERNAL_ERROR;
        
    TypeSpec ts = typesubst(alloc_ts, tm);
    bool as_alias = (where == ALIAS);
    unsigned ts_index = x64->once->type_info(ts, as_alias);

    bool is_artificial = (name[0] == '$' || name[0] == '<');
    
    if (outer_scope->type == CODE_SCOPE) {
        Storage s = get_local_storage();
        if (s.address.base != RBP)
            throw INTERNAL_ERROR;
    
        x64->dwarf->local_variable_info(name, s.address.offset, ts_index, is_artificial);
    }
    else if (outer_scope->type == ARGUMENT_SCOPE) {
        Storage s = get_local_storage();
        if (s.address.base != RBP)
            throw INTERNAL_ERROR;
    
        // gdb treats $ as a history expansion token
        std::string display_name = (name == "$" ? "SELF" : name);
        x64->dwarf->formal_parameter_info(display_name, s.address.offset, ts_index, is_artificial);
    }
    else if (outer_scope->type == DATA_SCOPE) {
        int o = allocsubst(offset, tm).concretize();
        x64->dwarf->member_info(name, o, ts_index, is_artificial);
    }
}



SelfVariable::SelfVariable(std::string name, TypeSpec vts)
    :Variable(name, vts) {
    self_info.reset(new SelfInfo);
}

Value *SelfVariable::matched(Value *cpivot, Scope *scope, TypeMatch &match) {
    return make<SelfVariableValue>(this, cpivot, scope, match, self_info.get());
}



PartialVariable::PartialVariable(std::string name, TypeSpec vts)
    :Variable(name, vts) {
    partial_info.reset(new PartialInfo);
}

Value *PartialVariable::matched(Value *cpivot, Scope *scope, TypeMatch &match) {
    return make<PartialVariableValue>(this, cpivot, scope, match, partial_info.get());
}




GlobalVariable::GlobalVariable(std::string n, TypeSpec mts, TypeSpec cts)
    :Variable(n, mts) {
    class_ts = cts;
    initializer_function = NULL;
    pivot_requirement = NO_PIVOT;
}

void GlobalVariable::set_initializer_function(Function *f) {
    initializer_function = f;
}

TypeSpec GlobalVariable::get_class_ts() {
    return class_ts;
}

bool GlobalVariable::is_called(std::string n) {
    return n == name;
}

bool GlobalVariable::is_abstract() {
    return true;  // No explicit initialization by its enclosing type
}

Scope *GlobalVariable::get_allocation_scope() {
    return outer_scope->get_module_scope();
}

void GlobalVariable::allocate() {
    Variable::allocate();
    
    // Allocation happens with ordered modules
    outer_scope->get_root_scope()->register_global_variable(this);
}

// No initializers are accessible from the language, done by the runtime itself
Label GlobalVariable::compile_initializer(X64 *x64) {
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

Label GlobalVariable::compile_finalizer(X64 *x64) {
    Label label;
    x64->code_label_local(label, name + "_finalizer");  // FIXME: ambiguous name!

    Storage s = get_local_storage();
    
    alloc_ts.destroy(s, x64);

    x64->op(RET);
    
    return label;
}

void GlobalVariable::debug(TypeMatch tm, X64 *x64) {
    Variable::debug(tm, x64);
    
    // We must also trigger the debug info generation of the actual class type,
    // because it is not our declared type. Without this not even the Application
    // class would get debug infos.
    
    TypeSpec ts = typesubst(class_ts, tm);
    x64->once->type_info(ts);
}


GlobalNamespace::GlobalNamespace(std::string n, TypeSpec mts)
    :GlobalVariable(n, mts, NO_TS) {
}

Regs GlobalNamespace::borrowing_requirements() {
    return Regs();  // the scope type check would be useless here
}

void GlobalNamespace::allocate() {
    where = MEMORY;  // just to conform to others
}

Storage GlobalNamespace::get_local_storage() {
    return Storage();  // TODO: anything uses this?
}

Label GlobalNamespace::compile_initializer(X64 *x64) {
    Label label;
    x64->code_label_local(label, name + "_initializer");  // FIXME: ambiguous name!

    if (initializer_function)
        x64->op(CALL, function_get_label(initializer_function, x64));

    x64->op(RET);
    return label;
}

Label GlobalNamespace::compile_finalizer(X64 *x64) {
    Label label;
    x64->code_label_local(label, name + "_finalizer");  // FIXME: ambiguous name!

    x64->op(RET);
    
    return label;
}

void GlobalNamespace::debug(TypeMatch tm, X64 *x64) {
}




RetroVariable::RetroVariable(std::string name, TypeSpec vts)
    :Variable(name, vts) {
    // These are actually arguments
    as_what = AS_ARGUMENT;
}

void RetroVariable::finalize(X64 *x64) {
    // These are not finalized by their scope, because only lived during the
    // invocation of the retro code.
}

void RetroVariable::allocate() {
    Variable::allocate();
    
    // The thing that is passed as a Dvalue argument must always have MEMORY storage,
    // because its address needs to be passed as ALIAS.
    if (where != MEMORY)
        throw INTERNAL_ERROR;
}



Evaluable::Evaluable(std::string name, TypeSpec vts)
    :Allocable(name, NO_PIVOT, vts) {
}

void Evaluable::set_outer_scope(Scope *os) {
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

std::vector<Variable *> Evaluable::get_dvalue_variables() {
    return dvalue_variables;
}

Value *Evaluable::matched(Value *cpivot, Scope *scope, TypeMatch &match) {
    return make<EvaluableValue>(this);
}

void Evaluable::allocate() {
    Allocable::allocate();
    
    where = MEMORY;
    offset = outer_scope->reserve(Allocation(ADDRESS_SIZE));
}

void Evaluable::debug(TypeMatch tm, X64 *x64) {
    Storage s = get_local_storage();
    TypeSpec ts = typesubst(alloc_ts, tm);
    unsigned ts_index = x64->once->type_info(ts);
    x64->dwarf->formal_parameter_info(name, s.address.offset, ts_index, false);
}
