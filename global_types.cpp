
TypeSpec::TypeSpec() {
}


TypeSpec::TypeSpec(TypeSpecIter tsi) {
    unsigned counter = 1;
    
    while (counter--) {
        push_back(*tsi);
        counter += (*tsi)->parameter_count;
        tsi++;
    }
}


StorageWhere TypeSpec::where(bool is_arg) {
    TypeSpecIter this_tsi(begin());
    
    return (*this_tsi)->where(this_tsi, is_arg, false);  // initially assume not lvalue
}


Storage TypeSpec::boolval(Storage s, X64 *x64, bool probe) {
    TypeSpecIter this_tsi(begin());
    
    return (*this_tsi)->boolval(this_tsi, s, x64, probe);
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

    if (at(0)->parameter_count != 1) {
        std::cerr << "Can't unprefix Type with " << at(0)->parameter_count << " parameters: " << *this << "!\n";
        throw INTERNAL_ERROR;
    }
        
    TypeSpec ts;

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


TypeSpec TypeSpec::nonlvalue() {
    return at(0) == lvalue_type ? unprefix(lvalue_type) : *this;
}


TypeSpec TypeSpec::nonrvalue() {
    return at(0) != lvalue_type && at(0) != ovalue_type ? prefix(lvalue_type) : *this;
}


TypeSpec TypeSpec::varvalue() {
    return heap_type_cast(at(0)) ? prefix(reference_type) : *this;
}


unsigned TypeSpec::measure(StorageWhere where) {
    TypeSpecIter tsi(begin());
    return (*tsi)->measure(tsi, where);
}


std::vector<Function *> TypeSpec::get_virtual_table() {
    TypeSpecIter tsi(begin());
    return (*tsi)->get_virtual_table(tsi);
}


Label TypeSpec::get_virtual_table_label(X64 *x64) {
    TypeSpecIter tsi(begin());
    return (*tsi)->get_virtual_table_label(tsi, x64);
}


Label TypeSpec::get_finalizer_label(X64 *x64) {
    TypeSpecIter tsi(begin());
    return (*tsi)->get_finalizer_label(tsi, x64);
}


void TypeSpec::store(Storage s, Storage t, X64 *x64) {
    TypeSpecIter tsi(begin());
    return (*tsi)->store(tsi, s, t, x64);
}


void TypeSpec::create(Storage s, Storage t, X64 *x64) {
    TypeSpecIter tsi(begin());
    return (*tsi)->create(tsi, s, t, x64);
}


void TypeSpec::destroy(Storage s, X64 *x64) {
    TypeSpecIter tsi(begin());
    return (*tsi)->destroy(tsi, s, x64);
}


void TypeSpec::compare(Storage s, Storage t, X64 *x64, Label less, Label greater) {
    TypeSpecIter tsi(begin());
    (*tsi)->compare(tsi, s, t, x64, less, greater);
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


Value *TypeSpec::lookup_initializer(std::string name, Scope *scope) {
    TypeSpecIter tsi(begin());
    return (*tsi)->lookup_initializer(tsi, name, scope);
}


Value *TypeSpec::lookup_inner(std::string name, Value *pivot) {
    TypeSpecIter tsi(begin());
    return (*tsi)->lookup_inner(tsi, name, pivot);
}


DataScope *TypeSpec::get_inner_scope() {
    TypeSpecIter tsi(begin());
    return (*tsi)->get_inner_scope(tsi);
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
