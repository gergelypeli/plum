
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

