
class Scope: virtual public Declaration {
public:
    std::vector<std::unique_ptr<Declaration>> contents;
    bool scope_finalization;
    
    Scope()
        :Declaration() {
        scope_finalization = false;
    }
    
    virtual void add(Declaration *decl) {
        decl->added(this, mark());
        contents.push_back(std::unique_ptr<Declaration>(decl));
    }
    
    virtual void remove(Declaration *decl) {
        if (contents.back().get() == decl) {
            contents.back().release();
            contents.pop_back();
            decl->added(NULL, NULL);
        }
        else {
            std::cerr << "Not the last declaration to remove!\n";
            throw INTERNAL_ERROR;
        }
    }
    
    virtual Declaration *mark() {
        return contents.size() ? contents.back().get() : this;
    }
    
    virtual Value *lookup(std::string name, Value *pivot) {
        // FIXME: backwards, please!
        for (auto &decl : contents) {
            Value *v = decl->match(name, pivot);
            
            if (v) {
                //std::cerr << "XXX: " << name << " " << get_typespec(pivot) << " => " << get_typespec(v) << "\n";
                return v;
            }
        }

        return NULL;
    }
    
    virtual FunctionScope *get_function_scope() {
        return outer_scope->get_function_scope();
    }
    
    virtual void allocate() {
        // TODO: this may not be correct for all kind of scopes
        for (auto &content : contents)
            content->allocate();
    }

    virtual int reserve(unsigned) {
        throw INTERNAL_ERROR;
    }

    virtual void expand(unsigned) {
        throw INTERNAL_ERROR;
    }
    
    virtual void finalize(FinalizationType ft, Storage s, X64 *x64) {
        if (scope_finalization)
            scope_finalization = false;
        else
            Declaration::finalize(ft, s, x64);
    }
    
    virtual void finalize_scope(Storage s, X64 *x64) {
        if (contents.size()) {
            scope_finalization = true;
            contents.back().get()->finalize(SCOPE_FINALIZATION, s, x64);
        }
    }
};




class CodeScope: virtual public Scope {
public:
    unsigned size;
    unsigned expanded_size;
    
    CodeScope()
        :Scope() {
        size = 0;
        expanded_size = 0;
    }
    
    virtual void allocate() {
        Scope::allocate();
        outer_scope->expand(expanded_size);
    }

    virtual int reserve(unsigned s) {
        if (s) {
            // Variables allocate nonzero bytes
            unsigned ss = stack_size(s);  // Simple strategy
            size += ss;
        
            if (size > expanded_size)
                expanded_size = size;
        
            return outer_scope->reserve(0) - size;
        }
        else {
            // Inner CodeScope-s just probe
            return outer_scope->reserve(0) - stack_size(size);
        }
    }
    
    virtual void expand(unsigned s) {
        unsigned es = stack_size(size) + s;
        
        if (es > expanded_size)
            expanded_size = es;
    }
};




class ArgumentScope: virtual public Scope {
public:
    unsigned size;
    
    ArgumentScope()
        :Scope() {
        size = 0;
    }

    virtual void add(Declaration *decl) {
        if (!variable_cast(decl))
            throw INTERNAL_ERROR;
            
        Scope::add(decl);
    }
    
    virtual void allocate() {
        // Backward, because the last arguments align to [RBP+16]
        for (int i = contents.size() - 1; i >= 0; i--)
            contents[i]->allocate();
    }

    virtual int reserve(unsigned s) {
        // Each argument must be rounded up separately
        unsigned ss = stack_size(s);
        //std::cerr << "Reserving " << ss << " bytes for an argument.\n";
        unsigned offset = size;
        size += ss;
        //std::cerr << "Now size is " << size << " bytes.\n";
        return offset;
    }
};




class FunctionScope: public Scope {
public:
    ArgumentScope *result_scope;
    ArgumentScope *head_scope;
    CodeScope *body_scope;
    unsigned frame_size;
    Label epilogue_label;

    FunctionScope()
        :Scope() {
        result_scope = NULL;
        head_scope = NULL;
        body_scope = NULL;
        
        frame_size = 0;
    }
    
    Scope *add_result_scope() {
        result_scope = new ArgumentScope;
        add(result_scope);
        return result_scope;
    }
    
    Scope *add_head_scope() {
        head_scope = new ArgumentScope;
        add(head_scope);
        return head_scope;
    }
    
    Scope *add_body_scope() {
        body_scope = new CodeScope;
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
            Value *v = result_scope->lookup(name, pivot);
            if (v)
                return v;
        }

        return NULL;
    }
    
    virtual int reserve(unsigned s) {
        if (s)
            throw INTERNAL_ERROR;
        
        return 0;
    }
    
    virtual void expand(unsigned s) {
        frame_size = stack_size(s);
    }
    
    virtual void allocate() {
        head_scope->reserve(8 + 8);
        head_scope->allocate();

        result_scope->reserve(head_scope->size);
        result_scope->allocate();

        //std::cerr << "Function head is " << head_scope->size - 16 << " bytes, result is " << result_scope->size - head_scope->size << " bytes.\n";

        body_scope->allocate();
    }
    
    virtual void finalize(FinalizationType ft, Storage s, X64 *x64) {
        // If we got here, someone is returning or throwing something
        x64->op(JMP, epilogue_label);
    }
    
    virtual void finalize_scope(Storage s, X64 *x64) {
        body_scope->finalize_scope(s, x64);
    }
    
    virtual unsigned get_frame_size() {
        return frame_size;
    }
    
    virtual Label get_epilogue_label() {
        return epilogue_label;
    }

    virtual FunctionScope *get_function_scope() {
        return this;
    }
    
    virtual Variable *get_result_variable() {
        switch (result_scope->contents.size()) {
        case 0:
            return NULL;
        case 1:
            return variable_cast(result_scope->contents.back().get());
        default:
            throw INTERNAL_ERROR;
        }
    }
};
