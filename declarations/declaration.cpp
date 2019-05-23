

Declaration::Declaration() {
    outer_scope = NULL;
    need_finalization_label = false;
    is_finalized = false;
}

Declaration::~Declaration() {
}

void Declaration::set_outer_scope(Scope *os) {
    // Must first remove then add
    if (outer_scope && os)
        throw INTERNAL_ERROR;
        
    outer_scope = os;
    
    if (outer_scope) {
        // The outer scope was obviously entered if we're added to it
        outer_scope_entered();
    }
    else {
        // The outer scope was obviously entered if we're removed from it
        outer_scope_left();
    }
}

void Declaration::outer_scope_entered() {
    // Nothing to do here
}

void Declaration::outer_scope_left() {
    // Nothing to do here
}

Value *Declaration::match(std::string name, Value *pivot, Scope *scope) {
    return NULL;
}

bool Declaration::is_transient() {
    return false;
}

bool Declaration::may_omit_finalization() {
    return false;  // this is for optimization only
}

bool Declaration::is_typedefinition(std::string n) {
    return false;
}

void Declaration::allocate() {
}

void Declaration::finalize(X64 *x64) {
    if (is_finalized)
        throw INTERNAL_ERROR;
        
    if (need_finalization_label)
        x64->code_label(finalization_label);
        
    is_finalized = true;
}

void Declaration::jump_to_finalization(X64 *x64) {
    need_finalization_label = true;
    x64->op(JMP, finalization_label);
}

DataScope *Declaration::find_inner_scope(std::string name) {
    return NULL;
}

void Declaration::debug(TypeMatch tm, X64 *x64) {
}
