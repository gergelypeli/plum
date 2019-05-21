
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
    
    Allocation get_size(TypeMatch tm) {
        allocate();
        //if (!is_allocated)
        //    throw INTERNAL_ERROR;
            
        return allocsubst(size, tm);
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

    virtual RetroScope *get_retro_scope() {
        return outer_scope->get_retro_scope();
    }

    virtual ModuleScope *get_module_scope() {
        return outer_scope->get_module_scope();
    }

    virtual RootScope *get_root_scope() {
        return outer_scope->get_root_scope();
    }

    virtual void be_unwindable() {
        throw INTERNAL_ERROR;  // only for CodeScope and friends
    }

    virtual void allocate() {
        // TODO: this may not be correct for all kind of scopes
        if (is_allocated)
            return;
        
        for (auto &content : contents)
            content->allocate();
            
        is_allocated = true;
    }

    virtual Storage get_local_storage() {
        throw INTERNAL_ERROR;
    }

    virtual Allocation reserve(Allocation size) {
        throw INTERNAL_ERROR;
    }

    virtual void rollback(Allocation checkpoint) {
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

    virtual void debug_contents(TypeMatch tm, X64 *x64) {
        for (auto &content : contents)
            content->debug(tm, x64);
    }

    virtual void debug(TypeMatch tm, X64 *x64) {
        debug_contents(tm, x64);
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
        
        size = size + s.stack_size();  // Simple strategy
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
    
    virtual Storage get_local_storage() {
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

    virtual Storage get_local_storage() {
        allocate();
        //if (!is_allocated)
        //    throw INTERNAL_ERROR;
            
        return get_root_scope()->get_local_storage() + offset.concretize();
    }

    virtual std::string fully_qualify(std::string n) {
        // Modules are not yet added to the root scope during typization, so no outer_scope
        return name + QUALIFIER_NAME + n;
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


class RaisingDummy: public Declaration {
public:
    RaisingDummy()
        :Declaration() {
    }

    virtual void set_outer_scope(Scope *os) {
        Declaration::set_outer_scope(os);
        
        outer_scope->be_unwindable();
    }
};


class CodeScope: public Scope {
public:
    Allocation offset;
    bool contents_finalized;  // for sanity check
    bool is_taken;  // too
    bool am_unwindable;  // for optimizing out checks
    int low_pc;
    int high_pc;
    
    CodeScope()
        :Scope(CODE_SCOPE) {
        contents_finalized = false;
        is_taken = false;
        am_unwindable = false;
        low_pc = -1;
        high_pc = -1;
    }

    virtual void be_taken() {
        is_taken = true;
    }

    virtual bool is_transient() {
        return true;
    }

    virtual void be_unwindable() {
        am_unwindable = true;
        
        outer_scope->be_unwindable();
    }
    
    virtual bool is_unwindable() {
        return am_unwindable;
    }

    virtual void leave() {
        if (!is_taken)
            throw INTERNAL_ERROR;

        Scope::leave();
    }

    virtual Allocation reserve(Allocation s) {
        return outer_scope->reserve(s);
    }

    virtual void rollback(Allocation checkpoint) {
        return outer_scope->rollback(checkpoint);
    }

    virtual void allocate() {
        if (is_allocated)
            throw INTERNAL_ERROR;
            
        Allocation checkpoint = outer_scope->reserve(Allocation { 0, 0, 0, 0 });

        // Escaped variables from statements are sorted later than the statement scope
        // that declared them. Since statement scope may be finalized after such variables
        // are initialized, these allocation ranges must not overlap. So allocate all
        // code scopes above all the escaped variables.
        
        for (auto &d : contents)
            if (!d->is_transient())
                d->allocate();
                
        for (auto &d : contents)
            if (d->is_transient())
                d->allocate();
        
        outer_scope->rollback(checkpoint);

        //std::cerr << "Allocated " << this << " total " << min_size << " / " << max_size << " bytes.\n";
        is_allocated = true;
    }

    
    virtual Storage get_local_storage() {
        return outer_scope->get_local_storage();
    }
    
    virtual TypeSpec get_pivot_ts() {
        return NO_TS;
    }

    virtual void initialize_contents(X64 *x64) {
        low_pc = x64->get_pc();
    }

    virtual bool has_finalizable_contents() {
        for (auto &d : contents)
            if (!d->is_transient())
                return true;
        
        return false;
    }
    
    virtual void finalize_contents(X64 *x64) {
        for (int i = contents.size() - 1; i >= 0; i--)
            contents[i]->finalize(x64);
            
        high_pc = x64->get_pc();
        contents_finalized = true;
    }
    
    virtual void finalize(X64 *x64) {
        if (!contents_finalized)
            throw INTERNAL_ERROR;
            
        Scope::finalize(x64);
    }

    virtual void reraise(X64 *x64) {
        x64->unwind->initiate(this, x64);
    }

    virtual void debug(TypeMatch tm, X64 *x64) {
        if (low_pc < 0 || high_pc < 0)
            throw INTERNAL_ERROR;

        // Don't spam the debug info with empty scopes
        if (contents.size()) {
            x64->dwarf->begin_lexical_block_info(low_pc, high_pc);
            debug_contents(tm, x64);
            x64->dwarf->end_info();
        }
    }
};




class RetroScope: public CodeScope {
public:
    Allocation header_offset;
    
    RetroScope()
        :CodeScope() {
        // This scope will be compiled out of order, so let's defuse this sanity check
        contents_finalized = true;
    }

    virtual RetroScope *get_retro_scope() {
        return this;
    }

    virtual void allocate() {
        header_offset = outer_scope->reserve(Allocation { RIP_SIZE + ADDRESS_SIZE });
        
        CodeScope::allocate();
    }
    
    int get_frame_offset() {
        return header_offset.concretize();
    }
};




class SwitchScope: public CodeScope {
public:
    Variable *switch_variable;

    SwitchScope()
        :CodeScope() {
        switch_variable = NULL;
    }

    void set_switch_variable(Variable *sv) {
        if (switch_variable || contents.size())
            throw INTERNAL_ERROR;
            
        add(ptr_cast<Declaration>(sv));
        switch_variable = sv;
    }

    Variable *get_variable() {
        return switch_variable;
    }

    SwitchScope *get_switch_scope() {
        return this;
    }

    virtual void debug(TypeMatch tm, X64 *x64) {
        if (low_pc < 0 || high_pc < 0)
            throw INTERNAL_ERROR;

        x64->dwarf->begin_catch_block_info(low_pc, high_pc);
        debug_contents(tm, x64);
        x64->dwarf->end_info();
    }
};




class TryScope: public CodeScope {
public:
    TreenumerationType *exception_type;
    bool have_implicit_matcher;

    TryScope()
        :CodeScope() {
        exception_type = NULL;
        have_implicit_matcher = false;
    }

    TryScope *get_try_scope() {
        return this;
    }
    
    bool set_exception_type(TreenumerationType *et, bool is_implicit_matcher) {
        if (!exception_type) {
            exception_type = et;
            have_implicit_matcher = is_implicit_matcher;
            return true;
        }
        
        if (exception_type != et) {
            std::cerr << "Different exception types raised: " << treenumeration_get_name(exception_type) << " and " << treenumeration_get_name(et) << "!\n";
            return false;
        }
        
        if (have_implicit_matcher != is_implicit_matcher) {
            std::cerr << "Mixed implicit and explicit matchers!\n";
            return false;
        }

        return true;
    }
    
    TreenumerationType *get_exception_type() {
        return exception_type;
    }
    
    bool has_implicit_matcher() {
        return have_implicit_matcher;
    }

    virtual void debug(TypeMatch tm, X64 *x64) {
        if (low_pc < 0 || high_pc < 0)
            throw INTERNAL_ERROR;

        x64->dwarf->begin_try_block_info(low_pc, high_pc);
        debug_contents(tm, x64);
        x64->dwarf->end_info();
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

        size = size + s.stack_size();  // Each argument must be rounded up separately
        
        //std::cerr << "Now size is " << size << " bytes.\n";
        return offset;
    }

    virtual Storage get_local_storage() {
        return outer_scope->get_local_storage();
    }

    virtual TypeSpec get_pivot_ts() {
        return NO_TS;
    }
};




class RetroArgumentScope: public ArgumentScope {
public:
    TypeSpec tuple_ts;
    Allocation offset;
    
    RetroArgumentScope(TypeSpec tts)
        :ArgumentScope() {
        tuple_ts = tts;
    }
    
    virtual Value *match(std::string name, Value *pivot, Scope *scope) {
        // Allow the lookup of retro variables
        return lookup(name, pivot, scope);
    }

    virtual void allocate() {
        Allocation total_size = tuple_ts.measure();
        offset = outer_scope->reserve(total_size);
        
        ArgumentScope::allocate();
        
        // Fewer named retro variables are allowed
        if (size.bytes > total_size.bytes)  // FIXME: proper comparison!
            throw INTERNAL_ERROR;
    }
    
    virtual Storage get_local_storage() {
        Storage s = ArgumentScope::get_local_storage();
        
        if (s.where != MEMORY)
            throw INTERNAL_ERROR;
            
        return Storage(MEMORY, s.address + offset.concretize());
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
    Storage associated_offset_storage;
    Allocation current_size;
    int frame_base_offset;  

    FunctionScope()
        :Scope(FUNCTION_SCOPE) {
        result_scope = NULL;
        self_scope = NULL;
        head_scope = NULL;
        body_scope = NULL;
        exception_type = NULL;
        frame_base_offset = 0;
    }
    
    virtual ArgumentScope *add_result_scope() {
        result_scope = new ArgumentScope;
        add(result_scope);
        return result_scope;
    }
    
    virtual ArgumentScope *add_self_scope() {
        self_scope = new ArgumentScope;
        add(self_scope);
        return self_scope;
    }

    virtual ArgumentScope *add_head_scope() {
        head_scope = new ArgumentScope;
        add(head_scope);
        return head_scope;
    }
    
    virtual CodeScope *add_body_scope() {
        body_scope = new CodeScope;
        add(body_scope);
        return body_scope;
    }

    virtual void adjust_frame_base_offset(int fbo) {
        // NOTE: the frame base offset is 0 for compiling the plain function body.
        // But when compiling the retro scopes in it, RBP will point to the retro
        // frame, which is an artificial stack frame at a fixed offset from the enclosing
        // function frame. So accessing anything RBP-relative within the retro scopes
        // must take this offset into consideration.
        frame_base_offset += fbo;
    }

    virtual void set_exception_type(TreenumerationType *et) {
        exception_type = et;
    }
    
    virtual TreenumerationType *get_exception_type() {
        return exception_type;
    }

    virtual void make_forwarded_exception_storage() {
        forwarded_exception_storage.where = MEMORY;
    }
    
    virtual Storage get_forwarded_exception_storage() {
        if (forwarded_exception_storage.where == MEMORY)
            return forwarded_exception_storage + frame_base_offset;
        else
            return Storage();
    }

    virtual void make_result_alias_storage() {
        result_alias_storage.where = MEMORY;
    }
    
    virtual Storage get_result_alias_storage() {
        if (result_alias_storage.where == MEMORY)
            return result_alias_storage + frame_base_offset;
        else
            return Storage();
    }

    virtual void make_associated_offset_storage() {
        associated_offset_storage.where = MEMORY;
    }
    
    virtual Storage get_associated_offset_storage() {
        if (associated_offset_storage.where == MEMORY)
            return associated_offset_storage + frame_base_offset;
        else
            return Storage();
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
        current_size = current_size + s.stack_size();
        
        size.bytes = std::max(size.bytes, current_size.bytes);
        size.count1 = std::max(size.count1, current_size.count1);
        size.count2 = std::max(size.count2, current_size.count2);
        size.count3 = std::max(size.count3, current_size.count3);
        
        return current_size * (-1);
    }
    
    virtual void rollback(Allocation checkpoint) {
        current_size = checkpoint * (-1);
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

        // FIXME: this is technically not an ALIAS, just an address
        if (result_alias_storage.where == MEMORY) {
            Allocation a = reserve(Allocation { ADDRESS_SIZE, 0, 0, 0 });
            result_alias_storage.address = Address(RBP, a.concretize());
        }
        
        if (forwarded_exception_storage.where == MEMORY) {
            Allocation a = reserve(Allocation { INTEGER_SIZE, 0, 0, 0 });
            forwarded_exception_storage.address = Address(RBP, a.concretize());
        }

        if (associated_offset_storage.where == MEMORY) {
            Allocation a = reserve(Allocation { ADDRESS_SIZE, 0, 0, 0 });
            associated_offset_storage.address = Address(RBP, a.concretize());
        }
        
        body_scope->allocate();
        
        is_allocated = true;
    }

    virtual Storage get_local_storage() {
        return Storage(MEMORY, Address(RBP, frame_base_offset));
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

    virtual RetroScope *get_retro_scope() {
        return NULL;
    }
    
    virtual void be_unwindable() {
        // Finish the notification here
    }
    
    virtual std::vector<Variable *> get_result_variables() {
        std::vector<Variable *> vars;
        
        for (auto &d : result_scope->contents)
            vars.push_back(ptr_cast<Variable>(d.get()));
            
        return vars;
    }
};
