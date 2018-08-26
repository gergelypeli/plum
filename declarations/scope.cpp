
enum ScopeType {
    ROOT_SCOPE, DATA_SCOPE, CODE_SCOPE, ARGUMENT_SCOPE, FUNCTION_SCOPE, MODULE_SCOPE, EXPORT_SCOPE
};


class Scope: virtual public Declaration {
public:
    std::vector<std::unique_ptr<Declaration>> contents;
    Allocation size;
    bool is_allocated;  // for sanity checks
    bool is_left;
    ScopeType type;
    
    Scope(ScopeType st)
        :Declaration() {
        type = st;
        size = Allocation { 0, 0, 0, 0 };
        is_allocated = false;
        is_left = false;
    }
    
    virtual void add(Declaration *decl) {
        if (!decl)
            throw INTERNAL_ERROR;
            
        if (is_left) {
            std::cerr << "Scope already left!\n";
            throw INTERNAL_ERROR;
        }
            
        decl->set_outer_scope(this);
        contents.push_back(std::unique_ptr<Declaration>(decl));
    }
    
    virtual void remove(Declaration *decl) {
        if (contents.back().get() == decl) {
            contents.back().release();
            contents.pop_back();
            
            decl->set_outer_scope(NULL);
        }
        else {
            std::cerr << "Not the last declaration to remove!\n";
            throw INTERNAL_ERROR;
        }
    }

    virtual void outer_scope_left() {
        if (!is_left) {
            std::cerr << "Scope not left properly!\n";
            throw INTERNAL_ERROR;
        }
    }
    
    virtual void leave() {
        is_left = true;
        
        for (int i = contents.size() - 1; i >= 0; i--) {
            contents[i]->outer_scope_left();
        }
    }
    
    unsigned get_size(TypeMatch tm) {
        if (!is_allocated)
            throw INTERNAL_ERROR;
            
        return size.concretize(tm);
    }
    
    virtual Value *lookup(std::string name, Value *pivot) {
        //std::cerr << "Scope lookup among " << contents.size() << " declarations.\n";
        
        for (int i = contents.size() - 1; i >= 0; i--) {
            Value *v = contents[i]->match(name, pivot);
            
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

    virtual SwitchScope *get_switch_scope() {
        return outer_scope->get_switch_scope();
    }

    virtual TryScope *get_try_scope() {
        return outer_scope->get_try_scope();
    }
    
    virtual EvalScope *get_eval_scope() {
        return outer_scope->get_eval_scope();
    }

    virtual ModuleScope *get_module_scope() {
        return outer_scope->get_module_scope();
    }

    virtual SingletonScope *get_singleton_scope() {
        return outer_scope->get_singleton_scope();
    }

    virtual void allocate() {
        // TODO: this may not be correct for all kind of scopes
        for (auto &content : contents)
            content->allocate();
            
        is_allocated = true;
    }

    virtual Allocation reserve(Allocation size) {
        throw INTERNAL_ERROR;
    }

    virtual TypeSpec pivot_type_hint() {
        throw INTERNAL_ERROR;
    }
    
    virtual std::string fully_qualify(std::string n) {
        throw INTERNAL_ERROR;
    }
};


class ModuleScope: public Scope {
public:
    std::string module_name;
    Allocation offset;
    
    ModuleScope(std::string mn)
        :Scope(MODULE_SCOPE) {
        module_name = mn;
    }

    virtual Allocation reserve(Allocation s) {
        Allocation pos = size;
        
        size.bytes += stack_size(s.bytes);  // Simple strategy
        size.count1 += s.count1;
        size.count2 += s.count2;
        size.count3 += s.count3;
        //std::cerr << "ModuleScope is now " << size << " bytes.\n";
    
        return pos;
    }

    virtual void allocate() {
        Scope::allocate();
        
        offset = outer_scope->reserve(size);
    }

    virtual ModuleScope *get_module_scope() {
        return this;
    }

    virtual std::string fully_qualify(std::string n) {
        return module_name + "." + n;
    }
};


class DataScope: public Scope {
public:
    std::string name;
    TypeSpec pivot_ts;
    Scope *meta_scope;
    std::vector<VirtualEntry *> virtual_table;
    bool am_virtual_scope;
    std::vector<Scope *> export_scopes;
    
    DataScope()
        :Scope(DATA_SCOPE) {
        pivot_ts = NO_TS;
        meta_scope = NULL;
        am_virtual_scope = false;
    }
    
    virtual void set_name(std::string n) {
        name = n;
    }
    
    virtual void be_virtual_scope() {
        am_virtual_scope = true;
    }
    
    virtual bool is_virtual_scope() {
        return am_virtual_scope;
    }
    
    virtual void set_meta_scope(Scope *ms) {
        meta_scope = ms;
    }
    
    virtual void set_pivot_type_hint(TypeSpec t) {
        if (t == VOID_TS || t == NO_TS)
            throw INTERNAL_ERROR;
            
        pivot_ts = t;
    }

    virtual Value *lookup(std::string name, Value *pivot) {
        for (int i = export_scopes.size() - 1; i >= 0; i--) {
            std::cerr << "Looking up in export scope #" << i << "\n";
            Value *v = export_scopes[i]->lookup(name, pivot);
            
            if (v)
                return v;
        }
            
        Value *value = Scope::lookup(name, pivot);
            
        if (!value && meta_scope)
            value = meta_scope->lookup(name, pivot);
                
        return value;
    }
    
    virtual Allocation reserve(Allocation s) {
        // Variables allocate nonzero bytes
        Allocation pos = size;
        
        size.bytes += stack_size(s.bytes);  // Simple strategy
        size.count1 += s.count1;
        size.count2 += s.count2;
        size.count3 += s.count3;
        //std::cerr << "DataScope is now " << size << " bytes.\n";
    
        return pos;
    }

    virtual TypeSpec pivot_type_hint() {
        //if (pivot_ts == NO_TS)
        //    throw INTERNAL_ERROR;
            
        return pivot_ts;
    }

    virtual int virtual_reserve(std::vector<VirtualEntry *> vt) {
        int virtual_index = virtual_table.size();
        virtual_table.insert(virtual_table.end(), vt.begin(), vt.end());
        return virtual_index;
    }
    
    virtual std::vector<VirtualEntry *> get_virtual_table() {
        return virtual_table;
    }
    
    virtual void set_virtual_entry(int i, VirtualEntry *entry) {
        //std::cerr << "DataScope setting virtual entry " << i << ".\n";
        virtual_table[i] = entry;
    }

    virtual std::string fully_qualify(std::string n) {
        if (name == "")
            throw INTERNAL_ERROR;
            
        return outer_scope->fully_qualify(name + "." + n);
    }
    
    virtual void push_scope(Scope *s) {
        std::cerr << "Pushed export scope to " << name << "\n";
        export_scopes.push_back(s);
    }
    
    virtual void pop_scope(Scope *s) {
        if (export_scopes.back() != s)
            throw INTERNAL_ERROR;
            
        std::cerr << "Popped export scope from " << name << "\n";
        export_scopes.pop_back();
    }
};


class RoleScope: public DataScope {
public:
    Role *role;
    DataScope *original_scope;
    int virtual_offset;

    RoleScope(Role *r, DataScope *os)
        :DataScope() {
        role = r;
        original_scope = os;
        virtual_offset = -1;
    }

    virtual Role *get_role() {
        return role;
    }
    
    virtual int virtual_reserve(std::vector<VirtualEntry *> vt) {
        if (virtual_offset != -1)
            throw INTERNAL_ERROR;
            
        virtual_offset = ptr_cast<DataScope>(outer_scope)->virtual_reserve(vt);
        
        return virtual_offset;
    }
    
    virtual void set_virtual_entry(int i, VirtualEntry *entry) {
        if (virtual_offset == -1)
            throw INTERNAL_ERROR;
            
        ptr_cast<DataScope>(outer_scope)->set_virtual_entry(virtual_offset + i, entry);
    }
    
    virtual Declaration *get_original_declaration(std::string n) {
        for (auto &d : original_scope->contents) {
            if (d->is_called(n))
                return d.get();
        }
        
        RoleScope *rs = ptr_cast<RoleScope>(original_scope);
        
        if (rs)
            return rs->get_original_declaration(n);
            
        return NULL;
    }
};


class SingletonScope: public DataScope {
public:
    Allocation offset;
    
    SingletonScope()
        :DataScope() {
    }

    virtual void allocate() {
        DataScope::allocate();
        
        offset = outer_scope->reserve(size);
    }

    virtual SingletonScope *get_singleton_scope() {
        return this;
    }
};


class RootScope: public DataScope {
public:
    RootScope()
        :DataScope() {
    }
    
    virtual ModuleScope *get_module_scope() {
        return NULL;
    }
};


class ExportScope: public Scope {
public:
    DataScope *target_scope;
    
    ExportScope(DataScope *ts)
        :Scope(EXPORT_SCOPE) {
        target_scope = ts;
        target_scope->push_scope(this);
    }
    
    virtual void outer_scope_left() {
        Scope::outer_scope_left();
        
        target_scope->pop_scope(this);
    }
};


class CodeScope: public Scope {
public:
    Allocation offset;
    bool contents_finalized;  // for sanity check
    bool is_taken;  // too
    
    CodeScope()
        :Scope(CODE_SCOPE) {
        contents_finalized = false;
        is_taken = false;
    }

    virtual bool is_transient() {
        return true;
    }
    
    virtual void taken() {
        is_taken = true;
    }
    
    virtual void allocate() {
        offset = outer_scope->reserve(Allocation { 0, 0, 0, 0 });
        
        //std::cerr << "Allocating " << this << " persistent contents.\n";
        for (auto &d : contents)
            if (!d->is_transient())
                d->allocate();
                
        size.bytes = stack_size(size.bytes);
        Allocation min_size = size;
        Allocation max_size = size;

        //std::cerr << "Allocating " << this << " transient contents.\n";
        for (auto &d : contents)
            if (d->is_transient()) {
                d->allocate();
                
                max_size.bytes = std::max(max_size.bytes, size.bytes);
                max_size.count1 = std::max(max_size.count1, size.count1);
                max_size.count2 = std::max(max_size.count2, size.count2);
                max_size.count3 = std::max(max_size.count3, size.count3);
                
                size = min_size;
            }
        
        //std::cerr << "Allocated " << this << " total " << min_size << " / " << max_size << " bytes.\n";
        outer_scope->reserve(max_size);
        is_allocated = true;
    }

    virtual Allocation reserve(Allocation s) {
        size.bytes += stack_size(s.bytes);  // Simple strategy
        size.count1 += s.count1;
        size.count2 += s.count2;
        size.count3 += s.count3;
        
        return Allocation {
            offset.bytes - size.bytes,
            offset.count1 - size.count1,
            offset.count2 - size.count2,
            offset.count3 - size.count3
        };
    }
    
    virtual TypeSpec pivot_type_hint() {
        return NO_TS;
    }

    virtual void finalize_contents(X64 *x64) {
        for (int i = contents.size() - 1; i >= 0; i--)
            contents[i]->finalize(x64);
            
        contents_finalized = true;
    }
    
    virtual void finalize(X64 *x64) {
        if (!contents_finalized)
            throw INTERNAL_ERROR;
            
        Scope::finalize(x64);
    }
};




class SwitchScope: public CodeScope {
public:
    SwitchScope *get_switch_scope() {
        return this;
    }
    
    const char *get_variable_name() {
        return "<switched>";
    }
    
    Variable *get_variable() {
        return ptr_cast<Variable>(contents[0].get());
    }
};




class TryScope: public CodeScope {
public:
    TreenumerationType *exception_type;

    TryScope()
        :CodeScope() {
        exception_type = NULL;
    }

    TryScope *get_try_scope() {
        return this;
    }
    
    bool set_exception_type(TreenumerationType *et) {
        if (exception_type && exception_type != et)
            return false;
            
        exception_type = et;
        return true;
    }
    
    TreenumerationType *get_exception_type() {
        return exception_type;
    }
};




class TransparentTryScope: public TryScope {
public:
    TransparentTryScope()
        :TryScope() {
        contents_finalized = true;
    }

    virtual void add(Declaration *d) {
        outer_scope->add(d);
    }
};




class EvalScope: public CodeScope {
public:
    YieldableValue *yieldable_value;

    EvalScope(YieldableValue *yv)
        :CodeScope() {
        yieldable_value = yv;
    }

    YieldableValue *get_yieldable_value() {
        return yieldable_value;
    }

    EvalScope *get_eval_scope() {
        return this;
    }
};




class ArgumentScope: public Scope {
public:
    ArgumentScope()
        :Scope(ARGUMENT_SCOPE) {
    }

    virtual void allocate() {
        // Backward, because the last arguments align to [RBP+16]
        for (int i = contents.size() - 1; i >= 0; i--)
            contents[i]->allocate();
            
        is_allocated = true;
    }

    virtual Allocation reserve(Allocation s) {
        // Upwards, because we allocate backwards
        Allocation offset = size;
        size.bytes += stack_size(s.bytes);  // Each argument must be rounded up separately
        size.count1 += s.count1;
        size.count2 += s.count2;
        size.count3 += s.count3;
        
        //std::cerr << "Now size is " << size << " bytes.\n";
        return offset;
    }

    virtual TypeSpec pivot_type_hint() {
        return NO_TS;
    }
};




class FunctionScope: public Scope {
public:
    ArgumentScope *result_scope;
    ArgumentScope *self_scope;
    ArgumentScope *head_scope;
    CodeScope *body_scope;
    TreenumerationType *exception_type;
    Storage forwarded_exception_storage;
    Storage result_alias_storage;

    FunctionScope()
        :Scope(FUNCTION_SCOPE) {
        result_scope = NULL;
        self_scope = NULL;
        head_scope = NULL;
        body_scope = NULL;
        exception_type = NULL;
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
        bool hack = is_left;  // See FunctionDefinitionValue
        is_left = false;
        add(body_scope);
        is_left = hack;
        return body_scope;
    }
    
    void set_exception_type(TreenumerationType *et) {
        exception_type = et;
    }
    
    TreenumerationType *get_exception_type() {
        return exception_type;
    }

    void make_forwarded_exception_storage() {
        forwarded_exception_storage.where = MEMORY;
    }
    
    Storage get_forwarded_exception_storage() {
        return forwarded_exception_storage;
    }

    void make_result_alias_storage() {
        result_alias_storage.where = MEMORY;
    }
    
    Storage get_result_alias_storage() {
        return result_alias_storage;
    }

    virtual TypeSpec pivot_type_hint() {
        // This is used when looking up explicit exception type names
        return NO_TS;
    }
    
    virtual Value *lookup(std::string name, Value *pivot) {
        Value *v;
        
        v = head_scope ? head_scope->lookup(name, pivot) : NULL;
        if (v)
            return v;
        
        v = self_scope ? self_scope->lookup(name, pivot) : NULL;
        if (v)
            return v;

        return NULL;
    }
    
    virtual Allocation reserve(Allocation s) {
        size.bytes += stack_size(s.bytes);
        size.count1 += s.count1;
        size.count2 += s.count2;
        size.count3 += s.count3;
        
        return Allocation { -size.bytes, -size.count1, -size.count2, -size.count3 };
    }
    
    virtual void allocate() {
        // reserve return address and saved RBP
        head_scope->reserve(Allocation { ADDRESS_SIZE + ADDRESS_SIZE, 0, 0, 0 });
        head_scope->allocate();

        self_scope->reserve(head_scope->size);
        self_scope->allocate();

        // results are now dynamically located from offset 0
        //result_scope->reserve(self_scope->size);
        result_scope->allocate();

        //std::cerr << "Function head is " << head_scope->size - 16 << "bytes, self is " << self_scope->size - head_scope->size << " bytes, result is " << result_scope->size - self_scope->size << " bytes.\n";

        if (result_alias_storage.where == MEMORY) {
            Allocation a = body_scope->reserve(Allocation { ALIAS_SIZE, 0, 0, 0 });
            result_alias_storage.address = Address(RBP, a.concretize());
        }
        
        if (forwarded_exception_storage.where == MEMORY) {
            Allocation a = body_scope->reserve(Allocation { INTEGER_SIZE, 0, 0, 0 });
            forwarded_exception_storage.address = Address(RBP, a.concretize());
        }
        
        body_scope->allocate();
        
        is_allocated = true;
    }
    
    virtual unsigned get_frame_size() {
        return size.concretize();
    }
    
    virtual FunctionScope *get_function_scope() {
        return this;
    }

    virtual SwitchScope *get_switch_scope() {
        return NULL;
    }

    virtual TryScope *get_try_scope() {
        return NULL;
    }

    virtual EvalScope *get_eval_scope() {
        return NULL;
    }
    
    virtual std::vector<Variable *> get_result_variables() {
        std::vector<Variable *> vars;
        
        for (auto &d : result_scope->contents)
            vars.push_back(ptr_cast<Variable>(d.get()));
            
        return vars;
    }
};
