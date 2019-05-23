
Scope::Scope(ScopeType st)
    :Declaration() {
    type = st;
    size = Allocation { 0, 0, 0, 0 };
    is_allocated = false;
    is_left = true;
}

void Scope::set_pivot_ts(TypeSpec t) {
    if (t == VOID_TS || t == NO_TS)
        throw INTERNAL_ERROR;
        
    pivot_ts = t;
}

TypeSpec Scope::get_pivot_ts() {
    return pivot_ts;
}

void Scope::add(Declaration *decl) {
    if (!decl)
        throw INTERNAL_ERROR;
        
    if (is_left) {
        std::cerr << "Scope already left!\n";
        throw INTERNAL_ERROR;
    }
        
    decl->set_outer_scope(this);
    contents.push_back(std::unique_ptr<Declaration>(decl));
}

void Scope::remove(Declaration *decl) {
    if (is_left) {
        std::cerr << "Scope already left!\n";
        throw INTERNAL_ERROR;
    }

    if (contents.back().get() == decl) {
        contents.back().release();
        contents.pop_back();
        
        decl->set_outer_scope(NULL);
    }
    else {
        std::cerr << "Not the last declaration to remove!\n";
        throw INTERNAL_ERROR;
    }
}

void Scope::remove_internal(Declaration *d) {
    // FUCKME: no way to find elements in an unique_ptr vector
    //auto it = std::find(contents.begin(), contents.end(), d);
    
    for (unsigned i = 0; i < contents.size(); i++) {
        if (contents[i].get() == d) {
            contents[i].release();
            d->set_outer_scope(NULL);
            contents.erase(contents.begin() + i);
            return;
        }
    }
    
    throw INTERNAL_ERROR;
}

void Scope::outer_scope_entered() {
    if (!is_left) {
        std::cerr << "Scope not left properly!\n";
        throw INTERNAL_ERROR;
    }
}

void Scope::outer_scope_left() {
    if (!is_left) {
        std::cerr << "Scope not left properly!\n";
        throw INTERNAL_ERROR;
    }
}

void Scope::enter() {
    is_left = false;
    
    for (unsigned i = 0; i < contents.size(); i++) {
        contents[i]->outer_scope_entered();
    }
}

void Scope::leave() {
    is_left = true;
    
    for (int i = contents.size() - 1; i >= 0; i--) {
        contents[i]->outer_scope_left();
    }
}

Allocation Scope::get_size(TypeMatch tm) {
    allocate();
    //if (!is_allocated)
    //    throw INTERNAL_ERROR;
        
    return allocsubst(size, tm);
}

Value *Scope::lookup(std::string name, Value *pivot, Scope *scope) {
    //std::cerr << "Scope lookup for " << name << " among " << contents.size() << " declarations.\n";

    for (int i = contents.size() - 1; i >= 0; i--) {
        Value *v = contents[i]->match(name, pivot, scope);
        
        if (v)
            return v;
    }

    return NULL;
}

DataScope *Scope::get_data_scope() {
    return outer_scope->get_data_scope();
}

FunctionScope *Scope::get_function_scope() {
    return outer_scope->get_function_scope();
}

SwitchScope *Scope::get_switch_scope() {
    return outer_scope->get_switch_scope();
}

TryScope *Scope::get_try_scope() {
    return outer_scope->get_try_scope();
}

EvalScope *Scope::get_eval_scope() {
    return outer_scope->get_eval_scope();
}

RetroScope *Scope::get_retro_scope() {
    return outer_scope->get_retro_scope();
}

ModuleScope *Scope::get_module_scope() {
    return outer_scope->get_module_scope();
}

RootScope *Scope::get_root_scope() {
    return outer_scope->get_root_scope();
}

void Scope::be_unwindable(Unwound u) {
    throw INTERNAL_ERROR;  // only for CodeScope and friends
}

void Scope::allocate() {
    // TODO: this may not be correct for all kind of scopes
    if (is_allocated)
        return;
    
    for (auto &content : contents)
        content->allocate();
        
    is_allocated = true;
}

Storage Scope::get_local_storage() {
    throw INTERNAL_ERROR;
}

Allocation Scope::reserve(Allocation size) {
    throw INTERNAL_ERROR;
}

void Scope::rollback(Allocation checkpoint) {
    throw INTERNAL_ERROR;
}

std::string Scope::fully_qualify(std::string n) {
    throw INTERNAL_ERROR;
}

bool Scope::is_typedefinition(std::string n) {
    for (int i = contents.size() - 1; i >= 0; i--) {
        if (contents[i]->is_typedefinition(n))
            return true;
    }
    
    return false;
}

void Scope::debug_contents(TypeMatch tm, X64 *x64) {
    for (auto &content : contents)
        content->debug(tm, x64);
}

void Scope::debug(TypeMatch tm, X64 *x64) {
    debug_contents(tm, x64);
}



NamedScope::NamedScope(ScopeType st)
    :Scope(st) {
    meta_scope = NULL;
}

void NamedScope::set_name(std::string n) {
    name = n;
}

void NamedScope::set_meta_scope(Scope *ms) {
    meta_scope = ms;
}

Value *NamedScope::lookup(std::string name, Value *pivot, Scope *scope) {
    for (int i = export_scopes.size() - 1; i >= 0; i--) {
        std::cerr << "Looking up in export scope #" << i << "\n";
        Value *v = export_scopes[i]->lookup(name, pivot, scope);
        
        if (v)
            return v;
    }
        
    Value *value = Scope::lookup(name, pivot, scope);
        
    if (!value && meta_scope)
        value = meta_scope->lookup(name, pivot, scope);
            
    return value;
}

Allocation NamedScope::reserve(Allocation s) {
    // Variables allocate nonzero bytes
    Allocation pos = size;
    
    size = size + s.stack_size();  // Simple strategy
    //std::cerr << "DataScope is now " << size << " bytes.\n";

    return pos;
}

std::string NamedScope::fully_qualify(std::string n) {
    return outer_scope->fully_qualify(name + QUALIFIER_NAME + n);
}

void NamedScope::push_scope(Scope *s) {
    std::cerr << "Pushed export scope to " << name << "\n";
    export_scopes.push_back(s);
}

void NamedScope::pop_scope(Scope *s) {
    if (export_scopes.back() != s)
        throw INTERNAL_ERROR;
        
    std::cerr << "Popped export scope from " << name << "\n";
    export_scopes.pop_back();
}



DataScope::DataScope(ScopeType st)
    :NamedScope(st) {
    am_virtual_scope = false;
    am_abstract_scope = false;
}

void DataScope::be_virtual_scope() {
    am_virtual_scope = true;
}

bool DataScope::is_virtual_scope() {
    return am_virtual_scope;
}

void DataScope::be_abstract_scope() {
    am_abstract_scope = true;
}

bool DataScope::is_abstract_scope() {
    return am_abstract_scope;
}

DataScope *DataScope::get_data_scope() {
    return this;
}

void DataScope::allocate() {
    if (is_allocated)
        return;

    // Do two passes to first compute the size
    for (auto &d : contents) {
        if (!ptr_cast<Function>(d.get()))
            d->allocate();
    }
        
    is_allocated = true;

    for (auto &d : contents) {
        if (ptr_cast<Function>(d.get()))
            d->allocate();
    }
}

void DataScope::virtual_initialize(devector<VirtualEntry *> vt) {
    if (!am_virtual_scope || !virtual_table.empty())
        throw INTERNAL_ERROR;
        
    virtual_table = vt;
}

int DataScope::virtual_reserve(VirtualEntry * ve) {
    if (!am_virtual_scope || virtual_table.empty())
        throw INTERNAL_ERROR;
    
    if (am_abstract_scope)
        return virtual_table.prepend(ve);
    else
        return virtual_table.append(ve);
}

devector<VirtualEntry *> DataScope::get_virtual_table() {
    if (!am_virtual_scope)
        throw INTERNAL_ERROR;

    return virtual_table;
}

void DataScope::set_virtual_entry(int i, VirtualEntry *entry) {
    if (!am_virtual_scope)
        throw INTERNAL_ERROR;

    //std::cerr << "DataScope setting virtual entry " << i << ".\n";
    virtual_table.set(i, entry);
}



RootScope::RootScope()
    :NamedScope(ROOT_SCOPE) {
}

RootScope *RootScope::get_root_scope() {
    return this;
}

void RootScope::set_application_label(Label al) {
    application_label = al;
}

Storage RootScope::get_local_storage() {
    return Storage(MEMORY, Address(application_label, 0));
}

std::string RootScope::fully_qualify(std::string n) {
    return n;
}

void RootScope::register_global_variable(GlobalVariable *g) {
    global_variables.push_back(g);
}

std::vector<GlobalVariable *> RootScope::list_global_variables() {
    return global_variables;
}



ModuleScope::ModuleScope(std::string mn, RootScope *rs)
    :NamedScope(MODULE_SCOPE) {
    set_name(mn);
    set_meta_scope(rs);
}

void ModuleScope::allocate() {
    if (is_allocated)
        return;
        
    NamedScope::allocate();
    
    offset = outer_scope->reserve(size);
}

ModuleScope *ModuleScope::get_module_scope() {
    return this;
}

Storage ModuleScope::get_local_storage() {
    allocate();
    //if (!is_allocated)
    //    throw INTERNAL_ERROR;
        
    return get_root_scope()->get_local_storage() + offset.concretize();
}

std::string ModuleScope::fully_qualify(std::string n) {
    // Modules are not yet added to the root scope during typization, so no outer_scope
    return name + QUALIFIER_NAME + n;
}



ExtensionScope::ExtensionScope(DataScope *ts)
    :DataScope() {
    target_scope = ts;
}

void ExtensionScope::outer_scope_entered() {
    DataScope::outer_scope_entered();
    
    target_scope->push_scope(this);
}

void ExtensionScope::outer_scope_left() {
    DataScope::outer_scope_left();
    
    target_scope->pop_scope(this);
}



ExportScope::ExportScope(NamedScope *ts)
    :Scope(EXPORT_SCOPE) {
    target_scope = ts;
}

void ExportScope::outer_scope_entered() {
    Scope::outer_scope_entered();
    
    target_scope->push_scope(this);
}

void ExportScope::outer_scope_left() {
    Scope::outer_scope_left();
    
    target_scope->pop_scope(this);
}



ImportScope::ImportScope(ModuleScope *ss, ModuleScope *ts)
    :ExportScope(ts) {
    source_scope = ss;
    prefix = source_scope->name + QUALIFIER_NAME;
}

void ImportScope::add(std::string id) {
    identifiers.insert(id);
}

Value *ImportScope::lookup(std::string name, Value *pivot, Scope *scope) {
    if (deprefix(name, prefix)) {
        std::cerr << "Looking up deprefixed identifier " << prefix << name << "\n";
        return source_scope->lookup(name, pivot, scope);
    }
        
    if (identifiers.count(name) > 0)
        return source_scope->lookup(name, pivot, scope);
        
    return NULL;
}




CodeScope::CodeScope()
    :Scope(CODE_SCOPE) {
    contents_finalized = false;
    is_taken = false;
    unwound = NOT_UNWOUND;
    low_pc = -1;
    high_pc = -1;
}

void CodeScope::be_taken() {
    is_taken = true;
}

bool CodeScope::is_transient() {
    return true;
}

bool CodeScope::may_omit_finalization() {
    return true;
}

void CodeScope::be_unwindable(Unwound u) {
    unwound = u;
    
    outer_scope->be_unwindable(u);
}

void CodeScope::leave() {
    if (!is_taken)
        throw INTERNAL_ERROR;

    Scope::leave();
}

Allocation CodeScope::reserve(Allocation s) {
    return outer_scope->reserve(s);
}

void CodeScope::rollback(Allocation checkpoint) {
    return outer_scope->rollback(checkpoint);
}

void CodeScope::allocate() {
    if (is_allocated)
        throw INTERNAL_ERROR;
        
    Allocation checkpoint = outer_scope->reserve(Allocation { 0, 0, 0, 0 });

    // Escaped variables from statements are sorted later than the statement scope
    // that declared them. Since statement scope may be finalized after such variables
    // are initialized, these allocation ranges must not overlap. So allocate all
    // code scopes above all the escaped variables.
    
    for (auto &d : contents)
        if (!d->is_transient())
            d->allocate();
            
    for (auto &d : contents)
        if (d->is_transient())
            d->allocate();
    
    outer_scope->rollback(checkpoint);

    //std::cerr << "Allocated " << this << " total " << min_size << " / " << max_size << " bytes.\n";
    is_allocated = true;
}


Storage CodeScope::get_local_storage() {
    return outer_scope->get_local_storage();
}

TypeSpec CodeScope::get_pivot_ts() {
    return NO_TS;
}

bool CodeScope::may_omit_content_finalization() {
    for (auto &d : contents)
        if (!d->may_omit_finalization())
            return false;
    
    return true;
}

void CodeScope::initialize_contents(X64 *x64) {
    low_pc = x64->get_pc();
}

void CodeScope::finalize_contents(X64 *x64) {
    for (int i = contents.size() - 1; i >= 0; i--)
        contents[i]->finalize(x64);
        
    high_pc = x64->get_pc();
    contents_finalized = true;
}

void CodeScope::finalize_contents_and_unwind(X64 *x64) {
    if (unwound == NOT_UNWOUND) {
        finalize_contents(x64);
    }
    else if (may_omit_content_finalization()) {
        finalize_contents(x64);  // formal only
        
        x64->op(JMP, got_nothing_label);

        x64->code_label(got_exception_label);
        x64->code_label(got_yield_label);
        x64->unwind->initiate(this, x64);  // exception or yield

        x64->code_label(got_nothing_label);
    }
    else {
        x64->op(MOVQ, RDX, NO_EXCEPTION);
    
        finalize_contents(x64);
    
        x64->op(CMPQ, RDX, NO_EXCEPTION);
        x64->op(JE, got_nothing_label);
        
        x64->code_label(got_exception_label);
        x64->code_label(got_yield_label);
        x64->unwind->initiate(this, x64);  // exception or yield
        
        x64->code_label(got_nothing_label);
    }
}

void CodeScope::jump_to_content_finalization(Declaration *last, X64 *x64) {
    if (last->outer_scope != this)
        throw INTERNAL_ERROR;
        
    if (unwound == NOT_UNWOUND)
        x64->op(UD2);
    else if (may_omit_content_finalization()) {
        x64->op(JMP, got_exception_label);  // or yield, handled the same here
    }
    else {
        last->jump_to_finalization(x64);
    }
}

void CodeScope::finalize(X64 *x64) {
    if (!contents_finalized)
        throw INTERNAL_ERROR;
        
    Scope::finalize(x64);
}

void CodeScope::debug(TypeMatch tm, X64 *x64) {
    if (low_pc < 0 || high_pc < 0)
        throw INTERNAL_ERROR;

    // Don't spam the debug info with empty scopes
    if (contents.size()) {
        x64->dwarf->begin_lexical_block_info(low_pc, high_pc);
        debug_contents(tm, x64);
        x64->dwarf->end_info();
    }
}




RetroScope::RetroScope()
    :CodeScope() {
    // This scope will be compiled out of order, so let's defuse this sanity check
    contents_finalized = true;
}

RetroScope *RetroScope::get_retro_scope() {
    return this;
}

void RetroScope::allocate() {
    header_offset = outer_scope->reserve(Allocation { RIP_SIZE + ADDRESS_SIZE });
    
    CodeScope::allocate();
}

int RetroScope::get_frame_offset() {
    return header_offset.concretize();
}




SwitchScope::SwitchScope()
    :CodeScope() {
    switch_variable = NULL;
}

void SwitchScope::set_switch_variable(Variable *sv) {
    if (switch_variable || contents.size())
        throw INTERNAL_ERROR;
        
    add(ptr_cast<Declaration>(sv));
    switch_variable = sv;
}

Variable *SwitchScope::get_variable() {
    return switch_variable;
}

SwitchScope *SwitchScope::get_switch_scope() {
    return this;
}

void SwitchScope::debug(TypeMatch tm, X64 *x64) {
    if (low_pc < 0 || high_pc < 0)
        throw INTERNAL_ERROR;

    x64->dwarf->begin_catch_block_info(low_pc, high_pc);
    debug_contents(tm, x64);
    x64->dwarf->end_info();
}




TryScope::TryScope()
    :CodeScope() {
    exception_type = NULL;
    have_implicit_matcher = false;
}

TryScope *TryScope::get_try_scope() {
    return this;
}

bool TryScope::set_exception_type(TreenumerationType *et, bool is_implicit_matcher) {
    if (!exception_type) {
        exception_type = et;
        have_implicit_matcher = is_implicit_matcher;
        return true;
    }
    
    if (exception_type != et) {
        std::cerr << "Different exception types raised: " << treenumeration_get_name(exception_type) << " and " << treenumeration_get_name(et) << "!\n";
        return false;
    }
    
    if (have_implicit_matcher != is_implicit_matcher) {
        std::cerr << "Mixed implicit and explicit matchers!\n";
        return false;
    }

    return true;
}

TreenumerationType *TryScope::get_exception_type() {
    return exception_type;
}

bool TryScope::has_implicit_matcher() {
    return have_implicit_matcher;
}

void TryScope::finalize_contents_and_unwind(X64 *x64) {
    if (unwound == NOT_UNWOUND) {
        finalize_contents(x64);
    }
    else if (may_omit_content_finalization()) {
        finalize_contents(x64);  // formal only
        
        x64->op(JMP, got_nothing_label);

        x64->code_label(got_yield_label);
        x64->unwind->initiate(this, x64);  // exception or yield
    }
    else {
        x64->op(MOVQ, RDX, NO_EXCEPTION);
    
        finalize_contents(x64);
    
        x64->op(CMPQ, RDX, NO_EXCEPTION);
        x64->op(JE, got_nothing_label);
        x64->op(JG, got_exception_label);

        x64->code_label(got_yield_label);
        x64->unwind->initiate(this, x64);
    }
}

void TryScope::jump_to_content_finalization(Declaration *last, X64 *x64) {
    if (last->outer_scope != this)
        throw INTERNAL_ERROR;
        
    if (unwound == NOT_UNWOUND)
        x64->op(UD2);
    else if (may_omit_content_finalization()) {
        x64->op(CMPQ, RDX, NO_EXCEPTION);
        x64->op(JG, got_exception_label);
        x64->op(JL, got_yield_label);
        x64->op(UD2);
    }
    else {
        last->jump_to_finalization(x64);
    }
}

void TryScope::debug(TypeMatch tm, X64 *x64) {
    if (low_pc < 0 || high_pc < 0)
        throw INTERNAL_ERROR;

    x64->dwarf->begin_try_block_info(low_pc, high_pc);
    debug_contents(tm, x64);
    x64->dwarf->end_info();
}




TransparentTryScope::TransparentTryScope()
    :TryScope() {
    is_taken = true;
    contents_finalized = true;
    unwound = BOTH_UNWOUND;  // unfortunately RaisingDummy-s avoid us, don't optimize!
}

void TransparentTryScope::add(Declaration *d) {
    outer_scope->add(d);
}

void TransparentTryScope::jump_to_content_finalization(Declaration *last, X64 *x64) {
    // Must tweak this check, as this scope has no content
    if (last->outer_scope != outer_scope)
        throw INTERNAL_ERROR;
    
    // Even though the 'last' raising dummy fell out to the outer scope with all
    // the other declarations, the finalization must finalize this scope. Since
    // we're empty, we have no finalization label to jump to, so check and jump
    // to the handling code directly.

    if (unwound == NOT_UNWOUND)
        x64->op(UD2);
    else if (may_omit_content_finalization()) {
        x64->op(CMPQ, RDX, NO_EXCEPTION);
        x64->op(JG, got_exception_label);
        x64->op(JL, got_yield_label);
        x64->op(UD2);
    }
    else
        throw INTERNAL_ERROR;
}




EvalScope::EvalScope()
    :CodeScope() {
}

int EvalScope::get_yield_value() {
    EvalScope *prev = outer_scope->get_eval_scope();
    
    return (prev ? prev->get_yield_value() - 1 : RETURN_EXCEPTION - 1);
}

EvalScope *EvalScope::get_eval_scope() {
    return this;
}

void EvalScope::finalize_contents_and_unwind(X64 *x64) {
    if (unwound == NOT_UNWOUND) {
        finalize_contents(x64);
    }
    else if (may_omit_content_finalization()) {
        finalize_contents(x64);  // formal only
        
        x64->op(JMP, got_nothing_label);

        x64->code_label(got_exception_label);
        x64->code_label(got_yield_label);
        x64->unwind->initiate(this, x64);  // exception or yield

        x64->code_label(got_nothing_label);
    }
    else {
        x64->op(MOVQ, RDX, NO_EXCEPTION);
    
        finalize_contents(x64);

        x64->op(CMPQ, RDX, NO_EXCEPTION);
        x64->op(JE, got_nothing_label);
        x64->op(CMPQ, RDX, get_yield_value());
        x64->op(JE, got_nothing_label);

        x64->code_label(got_exception_label);
        x64->code_label(got_yield_label);
        x64->unwind->initiate(this, x64);
    
        x64->code_label(got_nothing_label);
    }
}

void EvalScope::jump_to_content_finalization(Declaration *last, X64 *x64) {
    if (last->outer_scope != this)
        throw INTERNAL_ERROR;
        
    if (unwound == NOT_UNWOUND)
        x64->op(UD2);
    else if (may_omit_content_finalization()) {
        x64->op(CMPQ, RDX, get_yield_value());  // RDX can't be NO_EXCEPTION here
        x64->op(JE, got_nothing_label);
        x64->op(JMP, got_yield_label);
    }
    else {
        last->jump_to_finalization(x64);
    }
}




ArgumentScope::ArgumentScope()
    :Scope(ARGUMENT_SCOPE) {
}

void ArgumentScope::allocate() {
    if (is_allocated)
        throw INTERNAL_ERROR;
        
    // Backward, because the last arguments align to [RBP+16]
    for (int i = contents.size() - 1; i >= 0; i--)
        contents[i]->allocate();
        
    is_allocated = true;
}

Allocation ArgumentScope::reserve(Allocation s) {
    // Upwards, because we allocate backwards
    Allocation offset = size;

    size = size + s.stack_size();  // Each argument must be rounded up separately
    
    //std::cerr << "Now size is " << size << " bytes.\n";
    return offset;
}

Storage ArgumentScope::get_local_storage() {
    return outer_scope->get_local_storage();
}

TypeSpec ArgumentScope::get_pivot_ts() {
    return NO_TS;
}





RetroArgumentScope::RetroArgumentScope(TypeSpec tts)
    :ArgumentScope() {
    tuple_ts = tts;
}

Value *RetroArgumentScope::match(std::string name, Value *pivot, Scope *scope) {
    // Allow the lookup of retro variables
    return lookup(name, pivot, scope);
}

void RetroArgumentScope::allocate() {
    Allocation total_size = tuple_ts.measure();
    offset = outer_scope->reserve(total_size);
    
    ArgumentScope::allocate();
    
    // Fewer named retro variables are allowed
    if (size.bytes > total_size.bytes)  // FIXME: proper comparison!
        throw INTERNAL_ERROR;
}

Storage RetroArgumentScope::get_local_storage() {
    Storage s = ArgumentScope::get_local_storage();
    
    if (s.where != MEMORY)
        throw INTERNAL_ERROR;
        
    return Storage(MEMORY, s.address + offset.concretize());
}




FunctionScope::FunctionScope()
    :Scope(FUNCTION_SCOPE) {
    result_scope = NULL;
    self_scope = NULL;
    head_scope = NULL;
    body_scope = NULL;
    exception_type = NULL;
    frame_base_offset = 0;
}

ArgumentScope *FunctionScope::add_result_scope() {
    result_scope = new ArgumentScope;
    add(result_scope);
    return result_scope;
}

ArgumentScope *FunctionScope::add_self_scope() {
    self_scope = new ArgumentScope;
    add(self_scope);
    return self_scope;
}

ArgumentScope *FunctionScope::add_head_scope() {
    head_scope = new ArgumentScope;
    add(head_scope);
    return head_scope;
}

CodeScope *FunctionScope::add_body_scope() {
    body_scope = new CodeScope;
    add(body_scope);
    return body_scope;
}

void FunctionScope::adjust_frame_base_offset(int fbo) {
    // NOTE: the frame base offset is 0 for compiling the plain function body.
    // But when compiling the retro scopes in it, RBP will point to the retro
    // frame, which is an artificial stack frame at a fixed offset from the enclosing
    // function frame. So accessing anything RBP-relative within the retro scopes
    // must take this offset into consideration.
    frame_base_offset += fbo;
}

void FunctionScope::set_exception_type(TreenumerationType *et) {
    exception_type = et;
}

TreenumerationType *FunctionScope::get_exception_type() {
    return exception_type;
}

void FunctionScope::make_forwarded_exception_storage() {
    forwarded_exception_storage.where = MEMORY;
}

Storage FunctionScope::get_forwarded_exception_storage() {
    if (forwarded_exception_storage.where == MEMORY)
        return forwarded_exception_storage + frame_base_offset;
    else
        return Storage();
}

void FunctionScope::make_result_alias_storage() {
    result_alias_storage.where = MEMORY;
}

Storage FunctionScope::get_result_alias_storage() {
    if (result_alias_storage.where == MEMORY)
        return result_alias_storage + frame_base_offset;
    else
        return Storage();
}

void FunctionScope::make_associated_offset_storage() {
    associated_offset_storage.where = MEMORY;
}

Storage FunctionScope::get_associated_offset_storage() {
    if (associated_offset_storage.where == MEMORY)
        return associated_offset_storage + frame_base_offset;
    else
        return Storage();
}

TypeSpec FunctionScope::get_pivot_ts() {
    // This is used when looking up explicit exception type names
    return NO_TS;
}

Value *FunctionScope::lookup(std::string name, Value *pivot, Scope *scope) {
    Value *v;
    
    v = head_scope ? head_scope->lookup(name, pivot, scope) : NULL;
    if (v)
        return v;
    
    v = self_scope ? self_scope->lookup(name, pivot, scope) : NULL;
    if (v)
        return v;

    return NULL;
}

Allocation FunctionScope::reserve(Allocation s) {
    current_size = current_size + s.stack_size();
    
    size.bytes = std::max(size.bytes, current_size.bytes);
    size.count1 = std::max(size.count1, current_size.count1);
    size.count2 = std::max(size.count2, current_size.count2);
    size.count3 = std::max(size.count3, current_size.count3);
    
    return current_size * (-1);
}

void FunctionScope::rollback(Allocation checkpoint) {
    current_size = checkpoint * (-1);
}

void FunctionScope::allocate() {
    if (is_allocated)
        throw INTERNAL_ERROR;
        
    // reserve return address and saved RBP
    head_scope->reserve(Allocation { ADDRESS_SIZE + ADDRESS_SIZE, 0, 0, 0 });
    head_scope->allocate();

    self_scope->reserve(head_scope->size);
    self_scope->allocate();

    // results are now dynamically located from offset 0
    //result_scope->reserve(self_scope->size);
    result_scope->allocate();

    //std::cerr << "Function head is " << head_scope->size - 16 << "bytes, self is " << self_scope->size - head_scope->size << " bytes, result is " << result_scope->size - self_scope->size << " bytes.\n";

    // FIXME: this is technically not an ALIAS, just an address
    if (result_alias_storage.where == MEMORY) {
        Allocation a = reserve(Allocation { ADDRESS_SIZE, 0, 0, 0 });
        result_alias_storage.address = Address(RBP, a.concretize());
    }
    
    if (forwarded_exception_storage.where == MEMORY) {
        Allocation a = reserve(Allocation { INTEGER_SIZE, 0, 0, 0 });
        forwarded_exception_storage.address = Address(RBP, a.concretize());
    }

    if (associated_offset_storage.where == MEMORY) {
        Allocation a = reserve(Allocation { ADDRESS_SIZE, 0, 0, 0 });
        associated_offset_storage.address = Address(RBP, a.concretize());
    }
    
    body_scope->allocate();
    
    is_allocated = true;
}

Storage FunctionScope::get_local_storage() {
    return Storage(MEMORY, Address(RBP, frame_base_offset));
}

unsigned FunctionScope::get_frame_size() {
    return size.concretize();
}

FunctionScope *FunctionScope::get_function_scope() {
    return this;
}

SwitchScope *FunctionScope::get_switch_scope() {
    return NULL;
}

TryScope *FunctionScope::get_try_scope() {
    return NULL;
}

EvalScope *FunctionScope::get_eval_scope() {
    return NULL;
}

RetroScope *FunctionScope::get_retro_scope() {
    return NULL;
}

void FunctionScope::be_unwindable(Unwound u) {
    // Finish the notification here
}

std::vector<Variable *> FunctionScope::get_result_variables() {
    std::vector<Variable *> vars;
    
    for (auto &d : result_scope->contents)
        vars.push_back(ptr_cast<Variable>(d.get()));
        
    return vars;
}
