
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
    
    virtual Value *get_implicit_value() {
        if (outer)
            return outer->get_implicit_value();
        else {
            std::cerr << "No implicit scope value!\n";
            throw INTERNAL_ERROR;
        }
    }
    
    virtual Declaration *get_rollback_declaration() {
        std::cerr << "This scope can't roll back!\n";
        throw INTERNAL_ERROR;
    }
};


class FunctionHeadScope: public Scope {
public:
    FunctionHeadScope():Scope(true, true) {};
    
    virtual Value *get_implicit_value() {
        return make_function_head_value(this);
    }
};


class FunctionBodyScope: public Scope {
public:
    Declaration *rollback_declaration;  // TODO: generalize to CodeScope
    Label rollback_label;
    
    FunctionBodyScope():Scope(false, true) {
        rollback_declaration = this;  // Roll back to the beginning of this scope
    };
    
    virtual Value *get_implicit_value() {
        return make_function_body_value(this);
    }
    
    virtual void allocate() {
        rollback_label.allocate();
    }
    
    virtual Declaration *get_rollback_declaration() {
        return rollback_declaration;
    }
    
    virtual Label get_rollback_label() {
        return rollback_label;
    }
};


class FunctionReturnScope: public Scope {
public:
    FunctionReturnScope():Scope(true, true) {};

    virtual Value *get_implicit_value() {
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


class Variable: public Declaration {
public:
    std::string name;
    TypeSpec pivot_ts;
    TypeSpec var_ts;
    int offset;
    
    Variable(std::string name, TypeSpec pts, TypeSpec vts) {
        this->name = name;
        this->pivot_ts = pts;
        this->var_ts = vts;
    }
    
    virtual Value *match(std::string name, Value *pivot) {
        TypeSpec pts = get_typespec(pivot);
        
        if (name == this->name && pts >> pivot_ts) {
            if (!pivot) {
                // Include an implicit pivot value for local variables
                pivot = outer->get_implicit_value();
            }
            
            Value *v = make_variable_value(this, pivot);
            return v;
        }
        else
            return NULL;
    }
    
    virtual void allocate() {
        offset = outer->reserve(measure(var_ts));
    }
};


class Function: public Declaration {
public:
    std::string name;
    TypeSpec pivot_ts;
    std::vector<TypeSpec> arg_tss;
    std::vector<std::string> arg_names;
    TypeSpec ret_ts;

    Label x64_label;
    bool is_sysv;
    
    Function(std::string n, TypeSpec pts, std::vector<TypeSpec> ats, std::vector<std::string> ans, TypeSpec rts) {
        name = n;
        pivot_ts = pts;
        arg_tss = ats;
        arg_names = ans;
        ret_ts = rts;
        
        is_sysv = false;
    }

    virtual Value *match(std::string name, Value *pivot) {
        TypeSpec pts = get_typespec(pivot);
        //std::cerr << "XXX Function.match " << name << " " << print_typespec(ts) << "\n";

        if (name == this->name && pts >> pivot_ts) {
            Value *v = make_function_value(this, pivot);
            return v;
        }
        else
            return NULL;
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


class Type: virtual public Declaration {
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
            if (!are_equal(this_tsi, that_tsi))
                return false;
        }
        
        return true;
    }

    virtual bool is_convertible(TypeSpecIter &this_tsi, TypeSpecIter &that_tsi) {
        return *that_tsi == boolean_type || is_equal(this_tsi, that_tsi);
    }
    
    virtual unsigned measure(TypeSpecIter &) {
        std::cerr << "Unmeasurable type!\n";
        throw INTERNAL_ERROR;
    }

    virtual void store(TypeSpecIter &, Storage, Storage, X64 *) {
        std::cerr << "Unstorable type!\n";
        throw INTERNAL_ERROR;
    }
};


class BasicType: public Type {
public:
    unsigned size;

    BasicType(std::string n, unsigned s)
        :Type(n, 0) {
        size = s;
    }
    
    virtual unsigned measure(TypeSpecIter &) {
        return size;
    }

    virtual void store(TypeSpecIter &, Storage s, Storage t, X64 *x64) {
        BinaryOp mov = (
            size == 1 ? MOVB :
            size == 2 ? MOVW :
            size == 4 ? MOVD :
            size == 8 ? MOVQ :
            throw INTERNAL_ERROR
        );
        
        switch (s.where) {
        case NOWHERE:
            switch (t.where) {
            case NOWHERE:
                return;
            default:
                throw INTERNAL_ERROR;
            }
        case STACK:
            switch (t.where) {
            case NOWHERE:
                x64->op(POPQ, RAX);
                return;
            case STACK:
                return;
            case REGISTER:
                x64->op(POPQ, RAX);
                return;
            case FRAME:
                if (size == 8)
                    x64->op(POPQ, Address(RBP, t.frame_offset));
                else {
                    x64->op(POPQ, RAX);
                    x64->op(mov, Address(RBP, t.frame_offset), RAX);
                }
                return;
            default:
                throw INTERNAL_ERROR;
            }
        case REGISTER:
            switch (t.where) {
            case NOWHERE:
                return;
            case STACK:
                x64->op(PUSHQ, RAX);
                return;
            case REGISTER:
                return;
            case FRAME:
                x64->op(mov, Address(RBP, t.frame_offset), RAX);
                return;
            default:
                throw INTERNAL_ERROR;
            }
        case FRAME:
            switch (t.where) {
            case NOWHERE:
                return;
            case STACK:
                if (size == 8)
                    x64->op(PUSHQ, Address(RBP, s.frame_offset));
                else {
                    x64->op(mov, RAX, Address(RBP, s.frame_offset));
                    x64->op(PUSHQ, RAX);
                }
                return;
            case REGISTER: 
                x64->op(mov, RAX, Address(RBP, s.frame_offset));
                return;
            case FRAME:
                x64->op(mov, RAX, Address(RBP, s.frame_offset));
                x64->op(mov, Address(RBP, t.frame_offset), RAX);
                return;
            default:
                throw INTERNAL_ERROR;
            }
        }
    }
};


class LvalueType: public Type {
public:
    LvalueType()
        :Type("<Lvalue>", 1) {
    }
    
    virtual bool is_convertible(TypeSpecIter &this_tsi, TypeSpecIter &that_tsi) {
        if (*this_tsi == *that_tsi) {
            // When an lvalue is required, only the same type suffices
            this_tsi++;
            that_tsi++;
            
            return are_equal(this_tsi, that_tsi);
        }
        else {
            // When an rvalue is required, subtypes are also fine
            this_tsi++;
            
            return are_convertible(this_tsi, that_tsi);
        }
    }
    
    virtual unsigned measure(TypeSpecIter &tsi) {
        tsi++;
        return ::measure(tsi);
    }

    virtual void store(TypeSpecIter &tsi, Storage s, Storage t, X64 *x64) {
        tsi++;
        return ::store(tsi, s, t, x64);
    }
};


bool are_equal(TypeSpecIter &this_tsi, TypeSpecIter &that_tsi) {
    return (*this_tsi)->is_equal(this_tsi, that_tsi);
}


bool are_convertible(TypeSpecIter &this_tsi, TypeSpecIter &that_tsi) {
    return (*this_tsi)->is_convertible(this_tsi, that_tsi);
}


bool operator>>(TypeSpec &this_ts, TypeSpec &that_ts) {
    TypeSpecIter this_tsi(this_ts.begin());
    TypeSpecIter that_tsi(that_ts.begin());
    
    return are_convertible(this_tsi, that_tsi);
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


unsigned measure(TypeSpecIter &tsi) {
    return (*tsi)->measure(tsi);
}


unsigned measure(TypeSpec &ts) {
    TypeSpecIter tsi(ts.begin());
    return measure(tsi);
}


void store(TypeSpecIter &tsi, Storage s, Storage t, X64 *x64) {
    (*tsi)->store(tsi, s, t, x64);
}


void store(TypeSpec &ts, Storage s, Storage t, X64 *x64) {
    TypeSpecIter tsi(ts.begin());
    return store(tsi, s, t, x64);
}

