
NosyValueType::NosyValueType(std::string name)
    :PointerType(name) {
}

Allocation NosyValueType::measure(TypeMatch tm) {
    return Allocation(NOSYVALUE_SIZE);
}

void NosyValueType::store(TypeMatch tm, Storage s, Storage t, X64 *x64) {
    throw INTERNAL_ERROR;  // for safety, we'll handle everything manually
}

void NosyValueType::create(TypeMatch tm, Storage s, Storage t, X64 *x64) {
    if (s.where == STACK && t.where == MEMORY) {
        // Used when a Weak* container adds an elem. This is always an initialization
        // from a Ptr, so we keep the refcount, which will be decremented by
        // the container after setting up the FCB just in case.
        x64->op(POPQ, t.address + NOSYVALUE_RAW_OFFSET);
    }
    else if (s.where == MEMORY && t.where == MEMORY) {
        // Used when cloning an rbtree. We don't touch reference counters.
        x64->op(MOVQ, R10, s.address + NOSYVALUE_RAW_OFFSET);
        x64->op(MOVQ, t.address + NOSYVALUE_RAW_OFFSET, R10);
    }
    else
        throw INTERNAL_ERROR;
}

void NosyValueType::destroy(TypeMatch tm, Storage s, X64 *x64) {
    // Nothing to do
}




NosytreeType::NosytreeType(std::string name)
    :ContainerType(name, Metatypes { value_metatype }) {
}

void NosytreeType::type_info(TypeMatch tm, X64 *x64) {
    x64->dwarf->unspecified_type_info(name);
}




NosyrefType::NosyrefType(std::string name)
    :ContainerType(name, Metatypes { value_metatype }) {
}

void NosyrefType::type_info(TypeMatch tm, X64 *x64) {
    x64->dwarf->unspecified_type_info(name);
}




WeakrefType::WeakrefType(std::string n)
    :RecordType(n, Metatypes { identity_metatype }) {
}

Value *WeakrefType::lookup_initializer(TypeMatch tm, std::string n, Scope *s) {
    if (n == "to")
        return make<WeakrefToValue>(tm[1]);

    std::cerr << "No Weakref initializer " << n << "!\n";
    return NULL;
}

Value *WeakrefType::lookup_matcher(TypeMatch tm, std::string n, Value *p, Scope *s) {
    if (n == "dead")
        return make<WeakrefDeadMatcherValue>(p, tm);
    else if (n == "live")
        return make<WeakrefLiveMatcherValue>(p, tm);

    return RecordType::lookup_matcher(tm, n, p, s);
}
