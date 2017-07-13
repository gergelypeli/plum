
// Declarations

class Declaration {
public:
    Scope *outer;
    int offset;

    virtual Value *match(std::string, Value *) {
        return NULL;
    }
    
    virtual void allocate() {
    }
    
    virtual Label get_rollback_label() {
        std::cerr << "Can't roll back to this declaration!\n";
        throw INTERNAL_ERROR;
    }
};


class Scope: virtual public Declaration {
public:
    std::vector<std::unique_ptr<Declaration>> contents;
    bool is_ro;  // These flags should be subclass behavior instead
    bool is_downward;
    unsigned size;
    
    Scope(bool ro = false, bool dw = false)
        :Declaration() {
        is_ro = ro;
        is_downward = dw;
        size = 0;
    }
    
    virtual bool is_readonly() {
        return is_ro;
    }
    
    virtual void add(Declaration *decl) {
        contents.push_back(std::unique_ptr<Declaration>(decl));
        decl->outer = this;
    }
    
    virtual void remove(Declaration *decl) {
        if (contents.back().get() == decl) {
            contents.back().release();
            contents.pop_back();
            decl->outer = NULL;
        }
        else {
            std::cerr << "Not the last decl to remove!\n";
            throw INTERNAL_ERROR;
        }
    }
    
    virtual unsigned get_length() {
        return contents.size();
    }
    
    virtual Declaration *get_declaration(unsigned i) {
        return contents[i].get();
    }
    
    virtual Value *lookup(std::string name, Value *pivot) {
        for (auto &decl : contents) {
            Value *v = decl->match(name, pivot);
            
            if (v)
                return v;
        }

        return NULL;
    }
    
    virtual void allocate() {
        // TODO: this may not be correct for all kind of scopes
        for (auto &content : contents)
            content->allocate();
    }

    virtual int reserve(unsigned s) {
        unsigned old_size = size;
        size += round_up(s);
        
        if (is_downward) {
            return -size;
        }
        else {
            return old_size;
        }
    }
    
    virtual Storage get_storage() {
        if (outer)
            return outer->get_storage();
        else {
            std::cerr << "No scope storage!\n";
            throw INTERNAL_ERROR;
        }
    }
    
    virtual Declaration *get_rollback_declaration() {
        std::cerr << "This scope can't roll back!\n";
        throw INTERNAL_ERROR;
    }

    virtual void set_rollback_declaration(Declaration *) {
        std::cerr << "This scope can't roll back!\n";
        throw INTERNAL_ERROR;
    }
};


class FunctionHeadScope: public Scope {
public:
    FunctionHeadScope():Scope(true, true) {};
    
    virtual Storage get_storage() {
        return Storage(MEMORY, Address(RBP, offset));
    }
};


class FunctionBodyScope: public Scope {
public:
    Declaration *rollback_declaration;  // TODO: generalize to CodeScope
    Label rollback_label;
    
    FunctionBodyScope():Scope(false, true) {
        rollback_declaration = this;  // Roll back to the beginning of this scope
    };

    virtual Storage get_storage() {
        return Storage(MEMORY, Address(RBP, offset));
    }
    
    virtual void allocate() {
        Scope::allocate();
    
        rollback_label.allocate();
    }
    
    virtual Declaration *get_rollback_declaration() {
        return rollback_declaration;
    }

    virtual void set_rollback_declaration(Declaration *d) {
        rollback_declaration = d;
    }
    
    virtual Label get_rollback_label() {
        return rollback_label;
    }
};


class FunctionReturnScope: public Scope {
public:
    FunctionReturnScope():Scope(true, true) {};

    virtual Storage get_storage() {
        std::cerr << "How the hell did you access a return value variable?\n";
        throw INTERNAL_ERROR;
    }
};


class FunctionScope: public Scope {
public:
    FunctionReturnScope *return_scope;
    FunctionHeadScope *head_scope;
    FunctionBodyScope *body_scope;

    FunctionScope()
        :Scope() {
        return_scope = NULL;
        head_scope = NULL;
        body_scope = NULL;
    }
    
    FunctionReturnScope *add_return_scope() {
        return_scope = new FunctionReturnScope;
        add(return_scope);
        return return_scope;
    }
    
    FunctionHeadScope *add_head_scope() {
        head_scope = new FunctionHeadScope;
        add(head_scope);
        return head_scope;
    }
    
    FunctionBodyScope *add_body_scope() {
        body_scope = new FunctionBodyScope;
        add(body_scope);
        return body_scope;
    }
    
    virtual Value *lookup(std::string name, Value *pivot) {
        if (body_scope) {
            Value *v = head_scope->lookup(name, pivot);
            if (v)
                return v;
        }
        
        if (head_scope) {
            Value *v = return_scope->lookup(name, pivot);
            if (v)
                return v;
        }
            
        return NULL;
    }
    
    virtual void allocate() {
        Scope::allocate();
        
        int offset = 8 + 8;  // From RBP upward, skipping the pushed RBP and RIP

        offset += head_scope->size;
        head_scope->offset = offset;

        offset += return_scope->size;
        return_scope->offset = offset;
        
        body_scope->offset = 0;
    }
};


class Identifier: public Declaration {
public:
    std::string name;
    TypeSpec pivot_ts;

    Identifier(std::string n, TypeSpec pts) {
        name = n;
        pivot_ts = pts;
    }
    
    virtual Value *matched(Value *) {
        std::cerr << "Unmatchable identifier!\n";
        throw INTERNAL_ERROR;
    }
    
    virtual Value *match(std::string n, Value *pivot) {
        if (n != name)
            return NULL;
            
        Value *cpivot = convertible(pivot_ts, pivot);
        
        if (cpivot || pivot_ts == VOID_TS)
            return matched(cpivot);
        else
            return NULL;
    }
};


class Variable: public Identifier {
public:
    TypeSpec var_ts;
    int offset;
    
    Variable(std::string name, TypeSpec pts, TypeSpec vts)
        :Identifier(name, pts) {
        var_ts = vts;
    }
    
    virtual Value *matched(Value *cpivot) {
        // cpivot may be NULL if this is a local variable
        return make_variable_value(this, cpivot);
    }
    
    virtual void allocate() {
        offset = outer->reserve(var_ts.measure());
    }
    
    virtual Storage get_storage(Storage s) {
        if (s.where != MEMORY)
            throw INTERNAL_ERROR;  // for now, variables are all in memory
            
        return Storage(MEMORY, s.address + offset);
    }
};


class Function: public Identifier {
public:
    std::vector<TypeSpec> arg_tss;
    std::vector<std::string> arg_names;
    TypeSpec ret_ts;

    Label x64_label;
    bool is_sysv;
    
    Function(std::string n, TypeSpec pts, std::vector<TypeSpec> ats, std::vector<std::string> ans, TypeSpec rts)
        :Identifier(n, pts) {
        arg_tss = ats;
        arg_names = ans;
        ret_ts = rts;
        
        is_sysv = false;
    }

    virtual Value *matched(Value *cpivot) {
        return make_function_value(this, cpivot);
    }

    virtual TypeSpec get_return_typespec() {
        return ret_ts;
    }
    
    virtual unsigned get_argument_count() {
        return arg_tss.size();
    }
    
    virtual TypeSpec get_argument_typespec(unsigned i) {
        //std::cerr << "Returning typespec for argument " << i << ".\n";
        return arg_tss[i];
    }

    virtual int get_argument_index(std::string keyword) {  // FIXME
        for (unsigned i = 0; i < arg_names.size(); i++)
            if (arg_names[i] == keyword)
                return i;
                
        return (unsigned)-1;
    }

    virtual void import(X64 *x64) {
        is_sysv = true;
        x64->code_label_import(x64_label, name);  // TODO: mangle import name!
    }

    virtual void allocate() {
        x64_label.allocate();
    }
};


class IntegerOperation: public Identifier {
public:
    NumericOperation operation;
    
    IntegerOperation(std::string n, TypeSpec t, NumericOperation o)
        :Identifier(n, t) {
        operation = o;
    }
    
    virtual Value *matched(Value *cpivot) {
        return make_integer_operation_value(operation, pivot_ts, cpivot);
    }
};


class BooleanOperation: public Identifier {
public:
    NumericOperation operation;
    
    BooleanOperation(std::string n, TypeSpec t, NumericOperation o)
        :Identifier(n, t) {
        operation = o;
    }
    
    virtual Value *matched(Value *cpivot) {
        return make_boolean_operation_value(operation, cpivot);
    }
};


class BooleanIf: public Identifier {
public:
    BooleanIf()
        :Identifier("if", BOOLEAN_TS) {
    }
    
    virtual Value *matched(Value *cpivot) {
        return make_boolean_if_value(cpivot);
    }
};


class Type: public Declaration {
public:
    std::string name;
    unsigned parameter_count;
    
    Type(std::string n, unsigned pc) {
        name = n;
        parameter_count = pc;
    }
    
    virtual unsigned get_parameter_count() {
        return parameter_count;
    }
    
    virtual int get_length() {
        return 1;
    }
    
    virtual std::string get_label() {
        return "";
    }
    
    virtual Value *match(std::string name, Value *pivot) {
        if (name == this->name && pivot == NULL && parameter_count == 0) {
            TypeSpec ts;
            ts.push_back(type_type);
            ts.push_back(this);
            
            return make_type_value(ts);
        }
        else
            return NULL;
    }
    
    virtual bool is_equal(TypeSpecIter &this_tsi, TypeSpecIter &that_tsi) {
        if (*this_tsi != *that_tsi)
            return false;
            
        this_tsi++;
        that_tsi++;
        
        for (unsigned p = 0; p < parameter_count; p++) {
            if (!(*this_tsi)->is_equal(this_tsi, that_tsi))
                return false;
        }
        
        return true;
    }

    virtual Value *convertible(TypeSpecIter &this_tsi, TypeSpecIter &that_tsi, Value *orig) {
        return (*this_tsi)->is_equal(this_tsi, that_tsi) ? orig : NULL;
    }

    virtual Storage convert(TypeSpecIter &, TypeSpecIter &, Storage, X64 *, Regs) {
        std::cerr << "Unconvertable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }
    
    virtual unsigned measure(TypeSpecIter &) {
        std::cerr << "Unmeasurable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }

    virtual void store(TypeSpecIter &, Storage, Storage, X64 *) {
        std::cerr << "Unstorable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }
    
    virtual void create(TypeSpecIter &, Storage, X64 *) {
        std::cerr << "Uncreatable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }

    virtual void destroy(TypeSpecIter &, Storage, X64 *) {
        std::cerr << "Undestroyable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }
};


class SpecialType: public Type {
public:
    SpecialType(std::string name, unsigned pc):Type(name, pc) {}
    
    virtual unsigned measure(TypeSpecIter &) {
        return 0;
    }

    virtual void store(TypeSpecIter &, Storage s, Storage t, X64 *) {
        if (s.where != NOWHERE || t.where != NOWHERE) {
            std::cerr << "Invalid special store from " << s << " to " << t << "!\n";
            throw INTERNAL_ERROR;
        }
    }
};


class BasicType: public Type {
public:
    unsigned size;
    int os;

    BasicType(std::string n, unsigned s)
        :Type(n, 0) {
        size = s;
        os = (s == 1 ? 0 : s == 2 ? 1 : s == 4 ? 2 : s == 8 ? 3 : throw INTERNAL_ERROR);        
    }
    
    virtual unsigned measure(TypeSpecIter &) {
        return size;
    }

    virtual void store(TypeSpecIter &, Storage s, Storage t, X64 *x64) {
        // We can't use any register, unless saved and restored
        BinaryOp mov = MOVQ % os;
        
        switch (s.where * t.where) {
        case NOWHERE_NOWHERE:
            return;
            
        case CONSTANT_NOWHERE:
            return;
        case CONSTANT_CONSTANT:
            return;
        case CONSTANT_REGISTER:
            x64->op(mov, t.reg, s.value);
            return;
        case CONSTANT_STACK:
            x64->op(PUSHQ, s.value);
            return;
        case CONSTANT_MEMORY:
            x64->op(mov, t.address, s.value);
            return;
            
        case FLAGS_NOWHERE:
            return;
        case FLAGS_FLAGS:
            return;
        case FLAGS_REGISTER:
            x64->op(s.bitset, t.reg);
            return;
        case FLAGS_STACK:
            x64->op(PUSHQ, 0);
            x64->op(s.bitset, Address(RSP, 0));
            return;
        case FLAGS_MEMORY:
            x64->op(s.bitset, t.address);
            return;

        case REGISTER_NOWHERE:
            return;
        case REGISTER_REGISTER:
            return;
        case REGISTER_STACK:
            x64->op(PUSHQ, s.reg);
            return;
        case REGISTER_MEMORY:
            x64->op(mov, t.address, s.reg);
            return;

        case STACK_NOWHERE:
            x64->op(ADDQ, RSP, 8);
            return;
        case STACK_REGISTER:
            x64->op(POPQ, t.reg);
            return;
        case STACK_STACK:
            return;
        case STACK_MEMORY:
            if (size == 8)
                x64->op(POPQ, t.address);
            else {
                x64->op(XCHGQ, RAX, Address(RSP, 0));
                x64->op(mov, t.address, RAX);
                x64->op(POPQ, RAX);
            }
            return;

        case MEMORY_NOWHERE:
            return;
        case MEMORY_REGISTER:
            x64->op(mov, t.reg, s.address);
            return;
        case MEMORY_STACK:
            if (size == 8)
                x64->op(PUSHQ, s.address);
            else {
                x64->op(PUSHQ, RAX);
                x64->op(mov, RAX, s.address);
                x64->op(XCHGQ, RAX, Address(RSP, 0));
            }
            return;
        case MEMORY_MEMORY:
            if (size == 8) {
                x64->op(PUSHQ, s.address);
                x64->op(POPQ, t.address);
            }
            else {
                x64->op(PUSHQ, RAX);
                x64->op(mov, RAX, s.address);
                x64->op(mov, t.address, RAX);
                x64->op(POPQ, RAX);
            }
            return;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual void create(TypeSpecIter &, Storage s, X64 *x64) {
        BinaryOp mov = (
            size == 1 ? MOVB :
            size == 2 ? MOVW :
            size == 4 ? MOVD :
            size == 8 ? MOVQ :
            throw INTERNAL_ERROR
        );

        if (s.where == MEMORY)
            x64->op(mov, s.address, 0);
        else
            throw INTERNAL_ERROR;
    }

    virtual void destroy(TypeSpecIter &, Storage, X64 *) {
        return;
    }

    virtual Value *convertible(TypeSpecIter &this_tsi, TypeSpecIter &that_tsi, Value *orig) {
        return (*this_tsi)->is_equal(this_tsi, that_tsi) ? orig : *that_tsi == boolean_type ? make_converted_value(BOOLEAN_TS, orig) : NULL;
    }

    virtual Storage convert(TypeSpecIter &, TypeSpecIter &, Storage s, X64 *x64, Regs) {
        switch (s.where) {
        case CONSTANT:
            return Storage(CONSTANT, s.value != 0);
        case REGISTER:
            x64->op(CMPQ % os, s.reg, 0);
            return Storage(FLAGS, SETNE);
        case STACK:
            x64->op(CMPQ % os, Address(RSP, 0), 0);
            x64->op(SETNE, Address(RSP, 0));
            return Storage(STACK);
        case MEMORY:
            x64->op(CMPQ % os, s.address, 0);
            return Storage(FLAGS, SETNE);
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class LvalueType: public Type {
public:
    LvalueType()
        :Type("<Lvalue>", 1) {
    }
    
    virtual Value *convertible(TypeSpecIter &this_tsi, TypeSpecIter &that_tsi, Value *orig) {
        if (*this_tsi == *that_tsi) {
            // When an lvalue is required, only the same type suffices
            this_tsi++;
            that_tsi++;
            
            return (*this_tsi)->is_equal(this_tsi, that_tsi) ? orig : NULL;
        }
        else {
            // When an rvalue is required, subtypes are also fine
            this_tsi++;
            
            return (*this_tsi)->convertible(this_tsi, that_tsi, orig);
        }
    }
    
    virtual unsigned measure(TypeSpecIter &tsi) {
        tsi++;
        return (*tsi)->measure(tsi);
    }

    virtual void store(TypeSpecIter &tsi, Storage s, Storage t, X64 *x64) {
        tsi++;
        return (*tsi)->store(tsi, s, t, x64);
    }

    virtual void create(TypeSpecIter &tsi, Storage s, X64 *x64) {
        tsi++;
        (*tsi)->create(tsi, s, x64);
    }

    virtual void destroy(TypeSpecIter &tsi, Storage s, X64 *x64) {
        tsi++;
        (*tsi)->destroy(tsi, s, x64);
    }
};


Value *TypeSpec::convertible(TypeSpec &other, Value *orig) {
    TypeSpecIter this_tsi(begin());
    TypeSpecIter that_tsi(other.begin());
    
    return (*this_tsi)->convertible(this_tsi, that_tsi, orig);
}


Storage TypeSpec::convert(TypeSpec &other, Storage s, X64 *x64, Regs regs) {
    TypeSpecIter this_tsi(begin());
    TypeSpecIter that_tsi(other.begin());
    
    return (*this_tsi)->convert(this_tsi, that_tsi, s, x64, regs);
}


TypeSpec TypeSpec::rvalue() {
    TypeSpec t;
    
    unsigned start = at(0) == lvalue_type ? 1 : 0;
    
    for (unsigned i = start; i < size(); i++)
        t.push_back(at(i));
            
    return t;
}


TypeSpec TypeSpec::lvalue() {
    TypeSpec t;
    
    if (at(0) != lvalue_type)
        t.push_back(lvalue_type);
        
    for (unsigned i = 0; i < size(); i++)
        t.push_back(at(i));
        
        
    return t;
}


std::ostream &operator<<(std::ostream &os, TypeSpec &ts) {
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


unsigned TypeSpec::measure() {
    TypeSpecIter tsi(begin());
    return (*tsi)->measure(tsi);
}


void TypeSpec::store(Storage s, Storage t, X64 *x64) {
    TypeSpecIter tsi(begin());
    return (*tsi)->store(tsi, s, t, x64);
}

void TypeSpec::create(Storage s, X64 *x64) {
    TypeSpecIter tsi(begin());
    return (*tsi)->create(tsi, s, x64);
}

void TypeSpec::destroy(Storage s, X64 *x64) {
    TypeSpecIter tsi(begin());
    return (*tsi)->destroy(tsi, s, x64);
}
