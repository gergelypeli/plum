
enum ScopeType {
    ROOT_SCOPE, DATA_SCOPE, CODE_SCOPE, ARGUMENT_SCOPE, FUNCTION_SCOPE, MODULE_SCOPE, EXPORT_SCOPE, SINGLETON_SCOPE
};


class Scope: virtual public Declaration {
public:
    std::vector<std::unique_ptr<Declaration>> contents;
    Allocation size;
    bool is_allocated;  // for sanity checks
    bool is_left;
    ScopeType type;
    TypeSpec pivot_ts;
    
    Scope(ScopeType st)
        :Declaration() {
        type = st;
        size = Allocation { 0, 0, 0, 0 };
        is_allocated = false;
        is_left = true;
    }

    virtual void set_pivot_ts(TypeSpec t) {
        if (t == VOID_TS || t == NO_TS)
            throw INTERNAL_ERROR;
            
        pivot_ts = t;
    }

    virtual TypeSpec get_pivot_ts() {
        return pivot_ts;
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
        if (is_left) {
            std::cerr << "Scope already left!\n";
            throw INTERNAL_ERROR;
        }

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
    
    virtual void remove_internal(Declaration *d) {
        // FUCKME: no way to find elements in an unique_ptr vector
        //auto it = std::find(contents.begin(), contents.end(), d);
        
        for (unsigned i = 0; i < contents.size(); i++) {
            if (contents[i].get() == d) {
                contents[i].release();
                d->set_outer_scope(NULL);
                contents.erase(contents.begin() + i);
                return;
            }
        }
        
        throw INTERNAL_ERROR;
    }

    virtual void outer_scope_entered() {
        if (!is_left) {
            std::cerr << "Scope not left properly!\n";
            throw INTERNAL_ERROR;
        }
    }

    virtual void outer_scope_left() {
        if (!is_left) {
            std::cerr << "Scope not left properly!\n";
            throw INTERNAL_ERROR;
        }
    }
    
    virtual void enter() {
        is_left = false;
        
        for (unsigned i = 0; i < contents.size(); i++) {
            contents[i]->outer_scope_entered();
        }
    }
    
    virtual void leave() {
        is_left = true;
        
        for (int i = contents.size() - 1; i >= 0; i--) {
            contents[i]->outer_scope_left();
        }
    }
    
    unsigned get_size(TypeMatch tm) {
        allocate();
        //if (!is_allocated)
        //    throw INTERNAL_ERROR;
            
        return size.concretize(tm);
    }
    
    virtual Value *lookup(std::string name, Value *pivot, Scope *scope) {
        //std::cerr << "Scope lookup for " << name << " among " << contents.size() << " declarations.\n";

        for (int i = contents.size() - 1; i >= 0; i--) {
            Value *v = contents[i]->match(name, pivot, scope);
            
            if (v)
                return v;
        }

        return NULL;
    }

    virtual DataScope *get_data_scope() {
        return outer_scope->get_data_scope();
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

    virtual RootScope *get_root_scope() {
        return outer_scope->get_root_scope();
    }

    virtual void allocate() {
        // TODO: this may not be correct for all kind of scopes
        if (is_allocated)
            return;
        
        for (auto &content : contents)
            content->allocate();
            
        is_allocated = true;
    }

    virtual Allocation reserve(Allocation size) {
        throw INTERNAL_ERROR;
    }

    virtual std::string fully_qualify(std::string n) {
        throw INTERNAL_ERROR;
    }
    
    virtual bool is_typedefinition(std::string n) {
        for (int i = contents.size() - 1; i >= 0; i--) {
            if (contents[i]->is_typedefinition(n))
                return true;
        }
        
        return false;
    }
};


class NamedScope: public Scope {
public:
    std::string name;
    Scope *meta_scope;
    std::vector<Scope *> export_scopes;
    
    NamedScope(ScopeType st)
        :Scope(st) {
        meta_scope = NULL;
    }
    
    virtual void set_name(std::string n) {
        name = n;
    }
    
    virtual void set_meta_scope(Scope *ms) {
        meta_scope = ms;
    }
    
    virtual Value *lookup(std::string name, Value *pivot, Scope *scope) {
        for (int i = export_scopes.size() - 1; i >= 0; i--) {
            std::cerr << "Looking up in export scope #" << i << "\n";
            Value *v = export_scopes[i]->lookup(name, pivot, scope);
            
            if (v)
                return v;
        }
            
        Value *value = Scope::lookup(name, pivot, scope);
            
        if (!value && meta_scope)
            value = meta_scope->lookup(name, pivot, scope);
                
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

    virtual std::string fully_qualify(std::string n) {
        return outer_scope->fully_qualify(name + QUALIFIER_NAME + n);
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


class DataScope: public NamedScope {
public:
    devector<VirtualEntry *> virtual_table;
    bool am_virtual_scope;
    bool am_abstract_scope;
    
    DataScope(ScopeType st = DATA_SCOPE)
        :NamedScope(st) {
        am_virtual_scope = false;
        am_abstract_scope = false;
    }
    
    virtual void be_virtual_scope() {
        am_virtual_scope = true;
    }
    
    virtual bool is_virtual_scope() {
        return am_virtual_scope;
    }

    virtual void be_abstract_scope() {
        am_abstract_scope = true;
    }
    
    virtual bool is_abstract_scope() {
        return am_abstract_scope;
    }

    virtual DataScope *get_data_scope() {
        return this;
    }
    
    virtual void allocate() {
        if (is_allocated)
            return;
    
        // Do two passes to first compute the size
        for (auto &d : contents) {
            if (!ptr_cast<Function>(d.get()))
                d->allocate();
        }
            
        is_allocated = true;

        for (auto &d : contents) {
            if (ptr_cast<Function>(d.get()))
                d->allocate();
        }
    }

    virtual void virtual_initialize(devector<VirtualEntry *> vt) {
        if (!am_virtual_scope || !virtual_table.empty())
            throw INTERNAL_ERROR;
            
        virtual_table = vt;
    }

    virtual int virtual_reserve(VirtualEntry * ve) {
        if (!am_virtual_scope || virtual_table.empty())
            throw INTERNAL_ERROR;
        
        if (am_abstract_scope)
            return virtual_table.prepend(ve);
        else
            return virtual_table.append(ve);
    }

    virtual devector<VirtualEntry *> get_virtual_table() {
        if (!am_virtual_scope)
            throw INTERNAL_ERROR;

        return virtual_table;
    }
    
    virtual void set_virtual_entry(int i, VirtualEntry *entry) {
        if (!am_virtual_scope)
            throw INTERNAL_ERROR;

        //std::cerr << "DataScope setting virtual entry " << i << ".\n";
        virtual_table.set(i, entry);
    }
};


class RootScope: public NamedScope {
public:
    Label application_label;
    std::vector<GlobalVariable *> global_variables;
    
    RootScope()
        :NamedScope(ROOT_SCOPE) {
    }
    
    virtual RootScope *get_root_scope() {
        return this;
    }

    virtual void set_application_label(Label al) {
        application_label = al;
    }
    
    virtual Storage get_global_storage() {
        return Storage(MEMORY, Address(application_label, 0));
    }

    virtual std::string fully_qualify(std::string n) {
        return n;
    }
    
    virtual void register_global_variable(GlobalVariable *g) {
        global_variables.push_back(g);
    }
    
    virtual std::vector<GlobalVariable *> list_global_variables() {
        return global_variables;
    }
};


class ModuleScope: public NamedScope {
public:
    Allocation offset;
    
    ModuleScope(std::string mn, RootScope *rs)
        :NamedScope(MODULE_SCOPE) {
        set_name(mn);
        set_meta_scope(rs);
    }

    virtual void allocate() {
        if (is_allocated)
            return;
            
        NamedScope::allocate();
        
        offset = outer_scope->reserve(size);
    }

    virtual ModuleScope *get_module_scope() {
        return this;
    }

    virtual Storage get_global_storage() {
        allocate();
        //if (!is_allocated)
        //    throw INTERNAL_ERROR;
            
        return get_root_scope()->get_global_storage() + offset.concretize();
    }
};


class ExtensionScope: public DataScope {
public:
    DataScope *target_scope;
    
    ExtensionScope(DataScope *ts)
        :DataScope() {
        target_scope = ts;
    }

    virtual void outer_scope_entered() {
        DataScope::outer_scope_entered();
        
        target_scope->push_scope(this);
    }
    
    virtual void outer_scope_left() {
        DataScope::outer_scope_left();
        
        target_scope->pop_scope(this);
    }
};


class ExportScope: public Scope {
public:
    NamedScope *target_scope;
    
    ExportScope(NamedScope *ts)
        :Scope(EXPORT_SCOPE) {
        target_scope = ts;
    }

    virtual void outer_scope_entered() {
        Scope::outer_scope_entered();
        
        target_scope->push_scope(this);
    }
    
    virtual void outer_scope_left() {
        Scope::outer_scope_left();
        
        target_scope->pop_scope(this);
    }
};


class ImportScope: public ExportScope {
public:
    std::string prefix;
    ModuleScope *source_scope;
    std::set<std::string> identifiers;
    
    ImportScope(ModuleScope *ss, ModuleScope *ts)
        :ExportScope(ts) {
        source_scope = ss;
        prefix = source_scope->name + QUALIFIER_NAME;
    }
    
    virtual void add(std::string id) {
        identifiers.insert(id);
    }
    
    virtual Value *lookup(std::string name, Value *pivot, Scope *scope) {
        if (deprefix(name, prefix)) {
            std::cerr << "Looking up deprefixed identifier " << prefix << name << "\n";
            return source_scope->lookup(name, pivot, scope);
        }
            
        if (identifiers.count(name) > 0)
            return source_scope->lookup(name, pivot, scope);
            
        return NULL;
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
    
    virtual void be_taken() {
        is_taken = true;
    }

    virtual void leave() {
        if (!is_taken)
            throw INTERNAL_ERROR;

        Scope::leave();
    }
    
    virtual void allocate() {
        if (is_allocated)
            throw INTERNAL_ERROR;
            
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
    
    virtual TypeSpec get_pivot_ts() {
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
        is_taken = true;
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
        if (is_allocated)
            throw INTERNAL_ERROR;
            
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

    virtual TypeSpec get_pivot_ts() {
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
    
    ArgumentScope *add_result_scope() {
        result_scope = new ArgumentScope;
        add(result_scope);
        return result_scope;
    }
    
    ArgumentScope *add_self_scope() {
        self_scope = new ArgumentScope;
        add(self_scope);
        return self_scope;
    }

    ArgumentScope *add_head_scope() {
        head_scope = new ArgumentScope;
        add(head_scope);
        return head_scope;
    }
    
    CodeScope *add_body_scope() {
        body_scope = new CodeScope;
        add(body_scope);
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

    virtual TypeSpec get_pivot_ts() {
        // This is used when looking up explicit exception type names
        return NO_TS;
    }
    
    virtual Value *lookup(std::string name, Value *pivot, Scope *scope) {
        Value *v;
        
        v = head_scope ? head_scope->lookup(name, pivot, scope) : NULL;
        if (v)
            return v;
        
        v = self_scope ? self_scope->lookup(name, pivot, scope) : NULL;
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
        if (is_allocated)
            throw INTERNAL_ERROR;
            
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
