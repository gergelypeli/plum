

class Scope: virtual public Declaration {
public:
    std::vector<std::unique_ptr<Declaration>> contents;
    unsigned size;
    
    Scope()
        :Declaration() {
        size = 0;
    }
    
    virtual void add(Declaration *decl) {
        decl->added(mark());
        contents.push_back(std::unique_ptr<Declaration>(decl));
    }
    
    virtual void remove(Declaration *decl) {
        if (contents.back().get() == decl) {
            contents.back().release();
            contents.pop_back();
            
            Marker marker;
            marker.scope = NULL;
            marker.last = NULL;
            decl->added(marker);
        }
        else {
            std::cerr << "Not the last declaration to remove!\n";
            throw INTERNAL_ERROR;
        }
    }
    
    unsigned get_size() {
        return size;
    }
    
    virtual Marker mark() {
        Marker marker;
        marker.scope = this;
        marker.last = contents.size() ? contents.back().get() : NULL;
        return marker;
    }
    
    virtual Value *lookup(std::string name, Value *pivot, TypeMatch &match) {
        for (int i = contents.size() - 1; i >= 0; i--) {
            Value *v = contents[i]->match(name, pivot, match);
            
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

    virtual void expand(unsigned s) {
        // CodeScope-s in the declarations may call this, but just with 0 size
        //if (s > 0)
            throw INTERNAL_ERROR;
    }

    virtual bool is_pure() {
        throw INTERNAL_ERROR;
    }
    
    virtual TypeSpec variable_type_hint(TypeSpec t) {
        TypeSpec ts = t;
        TypeSpecIter tsi(ts.begin());
        
        if (*tsi == lvalue_type || *tsi == ovalue_type || *tsi == code_type)
            tsi++;
            
        if (heap_type_cast(*tsi))
            ts.insert(tsi, reference_type);
            
        return ts;
    }
    
    virtual TypeSpec pivot_type_hint() {
        throw INTERNAL_ERROR;
    }
    
    virtual void finalize_scope(Storage s, X64 *x64) {
        if (contents.size())
            contents.back().get()->finalize(SCOPE_FINALIZATION, s, x64);
    }
    
    virtual bool intrude(Scope *intruder, Marker before, bool escape_last) {
        // Insert a Scope taking all remaining declarations, maybe not the last one

        if (before.scope != this)
            throw INTERNAL_ERROR;

        Declaration *escaped = NULL;
        
        if (escape_last) {
            escaped = contents.back().release();
            contents.pop_back();
        }

        std::stack<Declaration *> victims;

        while (contents.size() && contents.back().get() != before.last) {
            Declaration *d = contents.back().release();
            contents.pop_back();
            victims.push(d);
        }

        if (victims.size() > 0) {
            add(intruder);

            while (victims.size()) {
                Declaration *d = victims.top();
                victims.pop();
                intruder->add(d);
            }
        
            if (escaped)
                add(escaped);
                
            return true;
        }
        else {
            if (escaped)
                add(escaped);
                
            return false;
        }
    }
};


Declaration *declaration_cast(Scope *scope) {
    return static_cast<Declaration *>(scope);
}


class DataScope: public Scope {
public:
    TypeSpec pivot_ts;
    
    DataScope()
        :Scope() {
        pivot_ts = BOGUS_TS;
    }
    
    virtual bool is_pure() {
        return true;
    }
    
    virtual void set_pivot_type_hint(TypeSpec t) {
        pivot_ts = t;
    }

    virtual Value *lookup(std::string name, Value *pivot, TypeMatch &match) {
        if (name == "<datatype>" && !pivot) {
            match.push_back(VOID_TS);
            return make_type_value(pivot_type_hint().prefix(type_type));
        }
        else
            return Scope::lookup(name, pivot, match);
    }
    
    virtual int reserve(unsigned s) {
        // Variables allocate nonzero bytes
        unsigned ss = stack_size(s);  // Simple strategy
        size += ss;
    
        //std::cerr << "DataScope is now " << size << " bytes.\n";
    
        return size - ss;
    }

    virtual TypeSpec variable_type_hint(TypeSpec ts) {
        if (ts[0] == lvalue_type || ts[0] == ovalue_type || ts[0] == code_type)
            throw TYPE_ERROR;
        
        return Scope::variable_type_hint(ts).lvalue();
    }
    
    virtual TypeSpec pivot_type_hint() {
        if (pivot_ts == BOGUS_TS)
            throw INTERNAL_ERROR;
            
        return pivot_ts;
    }
};


class CodeScope: public Scope {
public:
    unsigned expanded_size;
    
    CodeScope()
        :Scope() {
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
    
    virtual bool is_pure() {
        return false;
    }
    
    virtual TypeSpec variable_type_hint(TypeSpec ts) {
        if (ts[0] == lvalue_type || ts[0] == ovalue_type || ts[0] == code_type)
            throw TYPE_ERROR;
        
        return Scope::variable_type_hint(ts).lvalue();
    }
    
    virtual TypeSpec pivot_type_hint() {
        return VOID_TS;
    }
};




class ArgumentScope: public Scope {
public:
    ArgumentScope()
        :Scope() {
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

    virtual bool is_pure() {
        return true;
    }

    virtual TypeSpec variable_type_hint(TypeSpec ts) {
        return Scope::variable_type_hint(ts);
    }
    
    virtual TypeSpec pivot_type_hint() {
        return VOID_TS;
    }
};




class FunctionScope: public Scope {
public:
    ArgumentScope *result_scope;
    ArgumentScope *self_scope;
    ArgumentScope *head_scope;
    CodeScope *body_scope;
    Label epilogue_label;

    FunctionScope()
        :Scope() {
        result_scope = NULL;
        self_scope = NULL;
        head_scope = NULL;
        body_scope = NULL;
    }
    
    Scope *add_result_scope() {
        result_scope = new ArgumentScope;
        add(result_scope);
        return result_scope;
    }
    
    Scope *add_self_scope() {
        self_scope = new ArgumentScope;
        add(self_scope);
        return self_scope;
    }
    
    Scope *add_head_scope() {
        head_scope = new ArgumentScope;
        add(head_scope);
        return head_scope;
    }
    
    Scope *add_body_scope() {
        body_scope = new CodeScope;
        add(body_scope);
        
        // Let's pretend that body has nobody before it, so it won't accidentally
        // finalize the arguments
        Marker m;
        m.scope = this;
        m.last = NULL;
        body_scope->added(m);
        
        return body_scope;
    }
    
    virtual Value *lookup(std::string name, Value *pivot, TypeMatch &match) {
        if (body_scope) {
            Value *v = head_scope->lookup(name, pivot, match);
            if (v)
                return v;
        }
        
        if (head_scope) {
            Value *v = self_scope->lookup(name, pivot, match);
            if (v)
                return v;
        }

        if (self_scope) {
            Value *v = result_scope->lookup(name, pivot, match);
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
        size = stack_size(s);
    }
    
    virtual void allocate() {
        head_scope->reserve(8 + 8);
        head_scope->allocate();

        self_scope->reserve(head_scope->size);
        self_scope->allocate();

        result_scope->reserve(self_scope->size);
        result_scope->allocate();

        std::cerr << "Function head is " << head_scope->size - 16 << "bytes, self is " << self_scope->size - head_scope->size << " bytes, result is " << result_scope->size - self_scope->size << " bytes.\n";

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
        return size;
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
