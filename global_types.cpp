
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


TypeSpec TypeSpec::prefix(Type *t) {
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


TypeMatch TypeSpec::match() {
    return type_parameters_to_match(*this);
}


StorageWhere TypeSpec::where(bool is_arg, bool is_lvalue) {
    return at(0)->where(match(), is_arg, is_lvalue);
}


Storage TypeSpec::boolval(Storage s, X64 *x64, bool probe) {
    return at(0)->boolval(match(), s, x64, probe);
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


std::vector<Function *> TypeSpec::get_virtual_table() {
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


void TypeSpec::store(Storage s, Storage t, X64 *x64) {
    return at(0)->store(match(), s, t, x64);
}


void TypeSpec::create(Storage s, Storage t, X64 *x64) {
    return at(0)->create(match(), s, t, x64);
}


void TypeSpec::destroy(Storage s, X64 *x64) {
    return at(0)->destroy(match(), s, x64);
}


void TypeSpec::compare(Storage s, Storage t, X64 *x64, Label less, Label greater) {
    at(0)->compare(match(), s, t, x64, less, greater);
}


void TypeSpec::compare(Storage s, Storage t, X64 *x64, Register reg) {
    Label less, greater, end;
    compare(s, t, x64, less, greater);
        
    x64->op(MOVQ, reg, 0);
    x64->op(JMP, end);
        
    x64->code_label(less);
    x64->op(MOVQ, reg, -1);
    x64->op(JMP, end);
        
    x64->code_label(greater);
    x64->op(MOVQ, reg, 1);
        
    x64->code_label(end);
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


DataScope *TypeSpec::get_inner_scope() {
    return at(0)->get_inner_scope(match());
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


// Once

Label Once::compile(FunctionCompiler fc) {
    int before = function_compiler_labels.size();
    Label label = function_compiler_labels[fc];
    int after = function_compiler_labels.size();
    
    if (after != before) {
        function_compiler_todo.insert(fc);
        //std::cerr << "Will compile once " << (void *)fc << " as " << label.def_index << ".\n";
    }
    
    return label;
}


Label Once::compile(TypedFunctionCompiler tfc, TypeSpec ts) {
    int before = typed_function_compiler_labels.size();
    FunctionCompilerTuple t = make_pair(tfc, ts);
    Label label = typed_function_compiler_labels[t];
    int after = typed_function_compiler_labels.size();
    
    if (after != before) {
        typed_function_compiler_todo.insert(t);
        //std::cerr << "Will compile once " << (void *)tfc << " " << ts << " as " << label.def_index << ".\n";
    }
    
    return label;
}


void Once::for_all(X64 *x64) {
    // NOTE: once functions may ask to once compile other functions.
    
    while (typed_function_compiler_todo.size() || function_compiler_todo.size()) {
        while (typed_function_compiler_todo.size()) {
            FunctionCompilerTuple t = *typed_function_compiler_todo.begin();
            typed_function_compiler_todo.erase(typed_function_compiler_todo.begin());
        
            TypedFunctionCompiler tfc = t.first;
            TypeSpec ts = t.second;
            Label label = typed_function_compiler_labels[t];
    
            //std::cerr << "Now compiling " << (void *)tfc << " " << ts << " as " << label.def_index << ".\n";
            tfc(label, ts, x64);
        }

        while (function_compiler_todo.size()) {
            FunctionCompiler fc = *function_compiler_todo.begin();
            function_compiler_todo.erase(function_compiler_todo.begin());
        
            Label label = function_compiler_labels[fc];

            //std::cerr << "Now compiling " << (void *)fc << " as " << label.def_index << ".\n";
            fc(label, x64);
        }
    }
}


// Unwind

void Unwind::push(Value *v) {
    stack.push_back(v);
}


void Unwind::pop(Value *v) {
    if (v != stack.back())
        throw INTERNAL_ERROR;
        
    stack.pop_back();
}


void Unwind::initiate(Declaration *last, X64 *x64) {
    for (int i = stack.size() - 1; i >= 0; i--) {
        Scope *s = stack[i]->unwind(x64);
        
        if (s) {
            if (s != last->outer_scope)
                throw INTERNAL_ERROR;
                
            last->jump_to_finalization(x64);
            return;
        }
    }
    
    throw INTERNAL_ERROR;
}


// Allocation

Allocation::Allocation(int b, int c1, int c2, int c3) {
    bytes = b;
    count1 = c1;
    count2 = c2;
    count3 = c3;
}


int Allocation::concretize() {
    if (count1 || count2 || count3)
        throw INTERNAL_ERROR;
    else
        return bytes;
}


int Allocation::concretize(TypeMatch tm) {
    int concrete_size = bytes;
    
    if (count1)
        concrete_size += count1 * tm[1].measure_stack();
        
    if (count2)
        concrete_size += count2 * tm[2].measure_stack();
        
    if (count3)
        concrete_size += count3 * tm[3].measure_stack();
    
    //if (count1 || count2 || count3)
    //    std::cerr << "Hohoho, concretized " << *this << " with " << tm << " to " << concrete_size << " bytes.\n";
    
    return concrete_size;
}


std::ostream &operator<<(std::ostream &os, const Allocation &a) {
    if (a.count1 || a.count2 || a.count3)
        os << "A(" << a.bytes << "," << a.count1 << "," << a.count2 << "," << a.count3 << ")";
    else
        os << "A(" << a.bytes << ")";
        
    return os;
}
