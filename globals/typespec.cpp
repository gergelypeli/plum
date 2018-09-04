
TypeSpec::TypeSpec() {
}


TypeSpec::TypeSpec(TypeSpecIter tsi) {
    unsigned counter = 1;
    
    while (counter--) {
        push_back(*tsi);
        counter += (*tsi)->get_parameter_count();
        tsi++;
    }
}


TypeSpec::TypeSpec(Type *t, TypeSpec &tm1, TypeSpec &tm2) {
    if (t->get_parameter_count() != 2) {
        std::cerr << "Not a 2-parameter type: " << t->name << "!\n";
        throw INTERNAL_ERROR;
    }
    
    push_back(t);
    insert(end(), tm1.begin(), tm1.end());
    insert(end(), tm2.begin(), tm2.end());
}


TypeSpec TypeSpec::prefix(Type *t) {
    if (t->get_parameter_count() != 1) {
        std::cerr << "Can't prefix " << t->name << " with " << t->get_parameter_count() << " parameters to " << *this << "!\n";
        throw INTERNAL_ERROR;
    }
    
    if (!has_meta(t->param_metatypes[0])) {
        std::cerr << "Can't prefix " << t->name << " requiring a " << t->param_metatypes[0]->name  << " parameter to " << *this << "!\n";
        throw INTERNAL_ERROR;
    }
    
    TypeSpec ts;
    ts.push_back(t);
    
    for (unsigned i = 0; i < size(); i++)
        ts.push_back(at(i));
        
    return ts;
}


TypeSpec TypeSpec::unprefix(Type *t) {
    if (t && at(0) != t) {
        std::cerr << "TypeSpec doesn't start with " << t->name << ": " << *this << "!\n";
        throw INTERNAL_ERROR;
    }

    if (at(0)->get_parameter_count() != 1) {
        std::cerr << "Can't unprefix Type with " << at(0)->get_parameter_count() << " parameters: " << *this << "!\n";
        throw INTERNAL_ERROR;
    }
        
    TypeSpec ts;

    for (unsigned i = 1; i < size(); i++)
        ts.push_back(at(i));
            
    return ts;
}


TypeSpec TypeSpec::reprefix(Type *s, Type *t) {
    if (at(0) != s) {
        std::cerr << "TypeSpec doesn't start with " << s->name << ": " << *this << "!\n";
        throw INTERNAL_ERROR;
    }

    if (s->get_parameter_count() != t->get_parameter_count()) {
        std::cerr << "Can't reprefix Type with " << s->get_parameter_count() << " parameters: " << *this << "!\n";
        throw INTERNAL_ERROR;
    }
        
    TypeSpec ts = { t };

    for (unsigned i = 1; i < size(); i++)
        ts.push_back(at(i));

    return ts;
}


TypeSpec TypeSpec::rvalue() {
    return at(0) == lvalue_type ? unprefix(lvalue_type) : at(0) == ovalue_type ? unprefix(ovalue_type) : *this;
}


TypeSpec TypeSpec::lvalue() {
    return at(0) == lvalue_type ? *this : at(0) == ovalue_type ? unprefix(ovalue_type).prefix(lvalue_type) : prefix(lvalue_type);
}


bool TypeSpec::has_meta(Type *mt) {
    if (size() == 0)
        return false;
        
    Type *ms = at(0)->upper_type;
    
    while (ms && ms != mt) {
        MetaType *meta = ptr_cast<MetaType>(ms);
        ms = (meta ? meta->super_type : NULL);
    }
        
    return ms == mt;
}


bool TypeSpec::is_meta() {
    return at(0)->upper_type && !at(0)->upper_type->upper_type;
}


bool TypeSpec::is_hyper() {
    return !at(0)->upper_type;
}


TypeMatch TypeSpec::match() {
    TypeMatch fake_match;
    fake_match[0] = *this;
    TypeSpecIter tsi(begin());
    tsi++;

    for (unsigned i = 0; i < at(0)->get_parameter_count(); i++) {
        fake_match[i + 1] = TypeSpec(tsi);
        tsi += fake_match[i + 1].size();
    }

    return fake_match;
}


StorageWhere TypeSpec::where(AsWhat as_what) {
    return at(0)->where(match(), as_what);
}


Allocation TypeSpec::measure() {
    return at(0)->measure(match());
}


int TypeSpec::measure_raw() {
    return measure().concretize();
}


int TypeSpec::measure_elem() {
    return elem_size(measure_raw());
}


int TypeSpec::measure_stack() {
    return stack_size(measure_raw());
}


int TypeSpec::measure_where(StorageWhere where) {
    switch (where) {
    case NOWHERE:
        return 0;
    case MEMORY:
        return measure_raw();
    case STACK:
        return measure_stack();
    case ALIAS:
        return ALIAS_SIZE;
    case ALISTACK:
        return ALIAS_SIZE;
    default:
        throw INTERNAL_ERROR;
    }
}


std::vector<VirtualEntry *> TypeSpec::get_virtual_table() {
    return at(0)->get_virtual_table(match());
}


Label TypeSpec::get_virtual_table_label(X64 *x64) {
    return at(0)->get_virtual_table_label(match(), x64);
}


Label TypeSpec::get_finalizer_label(X64 *x64) {
    return at(0)->get_finalizer_label(match(), x64);
}


Value *TypeSpec::autoconv(TypeSpecIter target, Value *orig, TypeSpec &ifts) {
    return at(0)->autoconv(match(), target, orig, ifts);
}


Storage TypeSpec::store(Storage s, Storage t, X64 *x64) {
    at(0)->store(match(), s, t, x64);
    return t;
}


Storage TypeSpec::create(Storage s, Storage t, X64 *x64) {
    at(0)->create(match(), s, t, x64);
    return t;
}


void TypeSpec::destroy(Storage s, X64 *x64) {
    at(0)->destroy(match(), s, x64);
}


void TypeSpec::equal(Storage s, Storage t, X64 *x64) {
    at(0)->equal(match(), s, t, x64);
}


void TypeSpec::compare(Storage s, Storage t, X64 *x64) {
    at(0)->compare(match(), s, t, x64);
}


void TypeSpec::streamify(bool repr, X64 *x64) {
    at(0)->streamify(match(), repr, x64);
}


Value *TypeSpec::lookup_initializer(std::string name) {
    return at(0)->lookup_initializer(match(), name);
}


Value *TypeSpec::lookup_matcher(std::string name, Value *pivot) {
    return at(0)->lookup_matcher(match(), name, pivot);
}


Value *TypeSpec::lookup_inner(std::string name, Value *pivot) {
    return at(0)->lookup_inner(match(), name, pivot);
}


//DataScope *TypeSpec::get_inner_scope() {
//    return at(0)->get_inner_scope(match());
//}


void TypeSpec::init_vt(Address addr, int data_offset, Label vt_label, int virtual_offset, X64 *x64) {
    at(0)->init_vt(match(), addr, data_offset, vt_label, virtual_offset, x64);
}


std::ostream &operator<<(std::ostream &os, const TypeSpec &ts) {
    os << "[";
    
    bool start = true;
    
    for (auto type : ts) {
        if (start)
            start = false;
        else
            os << ",";
            
        os << type->name;
    }
    
    os << "]";
    
    return os;
}


std::ostream &operator<<(std::ostream &os, const TSs &tss) {
    os << "{";
    
    bool start = true;
    
    for (auto ts : tss) {
        if (start)
            start = false;
        else
            os << ",";
            
        os << ts;
    }
    
    os << "}";
    
    return os;
}


std::ostream &operator<<(std::ostream &os, const TypeMatch &tm) {
    os << "{";
    
    bool start = true;
    
    for (auto ts : tm) {
        if (start)
            start = false;
        else
            os << ",";
            
        os << ts;
    }
    
    os << "}";
    
    return os;
}


