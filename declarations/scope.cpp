

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

    virtual int reserve(unsigned size) {
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
    
    virtual bool intrude(Scope *intruder, Marker before, Declaration *escape) {
        // Insert a Scope taking all remaining declarations, except the first or the last one

        if (before.scope != this)
            throw INTERNAL_ERROR;

        Declaration *stop = before.last;
        Declaration *escaped_last = NULL;
        
        if (escape) {
            if (escape == contents.back().get()) {
                escaped_last = contents.back().release();
                contents.pop_back();
            }
            else if (escape == contents.front().get()) {
                stop = escape;
            }
            else
                throw INTERNAL_ERROR;
        }

        std::stack<Declaration *> victims;

        while (contents.size() && contents.back().get() != stop) {
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
        
            if (escaped_last)
                add(escaped_last);
                
            return true;
        }
        else {
            if (escaped_last)
                add(escaped_last);
                
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
    Scope *meta_scope;
    
    DataScope()
        :Scope() {
        pivot_ts = BOGUS_TS;
        meta_scope = NULL;
    }
    
    virtual bool is_pure() {
        return true;
    }
    
    virtual void set_meta_scope(Scope *ms) {
        meta_scope = ms;
    }
    
    virtual void set_pivot_type_hint(TypeSpec t) {
        pivot_ts = t;
    }

    virtual Value *lookup(std::string name, Value *pivot, TypeMatch &match) {
        if (name == "<datatype>" && !pivot) {
            match.push_back(VOID_TS);
            return make_type_value(pivot_type_hint().prefix(type_type));
        }
        else {
            Value *value = Scope::lookup(name, pivot, match);
            
            if (!value && meta_scope)
                value = meta_scope->lookup(name, pivot, match);
                
            return value;
        }
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
    int offset;
    bool may_be_aborted;
    Label epilogue_label;
    
    CodeScope()
        :Scope() {
        may_be_aborted = false;
    }

    virtual bool is_transient() {
        return true;
    }
    
    virtual void allocate() {
        offset = outer_scope->reserve(0);
        
        for (auto &d : contents)
            if (!d->is_transient())
                d->allocate();
                
        size = stack_size(size);
        unsigned min_size = size;
        unsigned max_size = size;

        for (auto &d : contents)
            if (d->is_transient()) {
                d->allocate();
                
                if (size > max_size)
                    max_size = size;
                    
                size = min_size;
            }
        
        //std::cerr << "CodeScope reserving " << min_size << "+" << (max_size - min_size) << " bytes.\n";
        outer_scope->reserve(max_size);
    }

    virtual int reserve(unsigned s) {
        unsigned ss = stack_size(s);  // Simple strategy
        size += ss;
        
        return offset - size;
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
    
    virtual void may_abort() {
        may_be_aborted = true;
    }

    virtual void jump_to_epilogue(X64 *x64) {
        x64->op(JMP, epilogue_label);
    }

    virtual void finalize_contents(X64 *x64) {
        for (int i = contents.size() - 1; i >= 0; i--)
            contents[i]->finalize(x64);
        
        // Unwinds before the first declaration will jump to this location
        if (may_be_aborted) {
            x64->code_label(epilogue_label);
            std::cerr << "Code scope may be aborted, checking for exceptions.\n";
            Label ok;
            x64->op(CMPQ, x64->exception_label, 0);
            x64->op(JE, ok);
        
            Marker m;  // TODO: store the marker instead
            m.scope = outer_scope;
            m.last = previous_declaration;
        
            x64->unwind->compile(m, x64);
            x64->code_label(ok);
        }
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
    //Label epilogue_label;

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
        size += s;
        return -size;
    }
    
    virtual void allocate() {
        head_scope->reserve(8 + 8);
        head_scope->allocate();

        self_scope->reserve(head_scope->size);
        self_scope->allocate();

        result_scope->reserve(self_scope->size);
        result_scope->allocate();

        //std::cerr << "Function head is " << head_scope->size - 16 << "bytes, self is " << self_scope->size - head_scope->size << " bytes, result is " << result_scope->size - self_scope->size << " bytes.\n";

        body_scope->allocate();
    }
    
    //virtual void finalize_scope(Storage s, X64 *x64) {
    //    body_scope->finalize_scope(s, x64);
    //}
    
    virtual unsigned get_frame_size() {
        return size;
    }
    
    //virtual Label get_epilogue_label() {
    //    return epilogue_label;
    //}

    virtual FunctionScope *get_function_scope() {
        return this;
    }
    
    virtual std::vector<Variable *> get_result_variables() {
        std::vector<Variable *> vars;
        
        for (auto &d : result_scope->contents)
            vars.push_back(variable_cast(d.get()));
            
        return vars;
    }
};
