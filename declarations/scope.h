
enum ScopeType {
    ROOT_SCOPE, DATA_SCOPE, CODE_SCOPE, ARGUMENT_SCOPE, FUNCTION_SCOPE, MODULE_SCOPE, EXPORT_SCOPE, SINGLETON_SCOPE
};

enum Unwound {
    NOT_UNWOUND = 0,
    EXCEPTION_UNWOUND = 1,
    YIELD_UNWOUND = 2,
    BOTH_UNWOUND = 3
};

class Scope: virtual public Declaration {
public:
    std::vector<std::unique_ptr<Declaration>> contents;
    Allocation size;
    bool is_allocated;  // for sanity checks
    bool is_left;
    ScopeType type;
    TypeSpec pivot_ts;

    Scope(ScopeType st);

    virtual void set_pivot_ts(TypeSpec t);
    virtual TypeSpec get_pivot_ts();
    virtual void add(Declaration *decl);
    virtual void remove(Declaration *decl);
    virtual void remove_internal(Declaration *d);
    virtual void outer_scope_entered();
    virtual void outer_scope_left();
    virtual void enter();
    virtual void leave();
    virtual Allocation get_size(TypeMatch tm);
    virtual Value *lookup(std::string name, Value *pivot, Scope *scope);
    virtual DataScope *get_data_scope();
    virtual FunctionScope *get_function_scope();
    virtual SwitchScope *get_switch_scope();
    virtual TryScope *get_try_scope();
    virtual EvalScope *get_eval_scope();
    virtual RetroScope *get_retro_scope();
    virtual ModuleScope *get_module_scope();
    virtual RootScope *get_root_scope();
    virtual void be_unwindable(Unwound u);
    virtual void allocate();
    virtual Storage get_local_storage();
    virtual Allocation reserve(Allocation size);
    virtual void rollback(Allocation checkpoint);
    virtual std::string fully_qualify(std::string n);
    virtual bool is_typedefinition(std::string n);
    virtual void debug_contents(TypeMatch tm, Cx *cx);
    virtual void debug(TypeMatch tm, Cx *cx);
};

class NamedScope: public Scope {
public:
    std::string name;
    Scope *meta_scope;
    std::vector<Scope *> export_scopes;

    NamedScope(ScopeType st);

    virtual void set_name(std::string n);
    virtual void set_meta_scope(Scope *ms);
    virtual Value *lookup(std::string name, Value *pivot, Scope *scope);
    virtual Allocation reserve(Allocation s);
    virtual std::string fully_qualify(std::string n);
    virtual void push_scope(Scope *s);
    virtual void pop_scope(Scope *s);
};

class DataScope: public NamedScope {
public:
    devector<VirtualEntry *> virtual_table;
    bool am_virtual_scope;
    bool am_abstract_scope;

    DataScope(ScopeType st = DATA_SCOPE);

    virtual void be_virtual_scope();
    virtual bool is_virtual_scope();
    virtual void be_abstract_scope();
    virtual bool is_abstract_scope();
    virtual DataScope *get_data_scope();
    virtual void allocate();
    virtual void virtual_initialize(devector<VirtualEntry *> vt);
    virtual int virtual_reserve(VirtualEntry * ve);
    virtual devector<VirtualEntry *> get_virtual_table();
    virtual void set_virtual_entry(int i, VirtualEntry *entry);
};

class RootScope: public NamedScope {
public:
    Label application_label;
    std::vector<GlobalVariable *> global_variables;

    RootScope();

    virtual RootScope *get_root_scope();
    virtual void set_application_label(Label al);
    virtual Storage get_local_storage();
    virtual std::string fully_qualify(std::string n);
    virtual void register_global_variable(GlobalVariable *g);
    virtual std::vector<GlobalVariable *> list_global_variables();
};

class ModuleScope: public NamedScope {
public:
    Allocation offset;

    ModuleScope(std::string mn, RootScope *rs);

    virtual void allocate();
    virtual ModuleScope *get_module_scope();
    virtual Storage get_local_storage();
    virtual std::string fully_qualify(std::string n);
};

class ExtensionScope: public DataScope {
public:
    DataScope *target_scope;

    ExtensionScope(DataScope *ts);

    virtual void outer_scope_entered();
    virtual void outer_scope_left();
};

class ExportScope: public Scope {
public:
    NamedScope *target_scope;

    ExportScope(NamedScope *ts);

    virtual void outer_scope_entered();
    virtual void outer_scope_left();
};

class ImportScope: public ExportScope {
public:
    std::string prefix;
    ModuleScope *source_scope;
    std::set<std::string> identifiers;

    ImportScope(ModuleScope *ss, ModuleScope *ts);

    virtual void add(std::string id);
    virtual Value *lookup(std::string name, Value *pivot, Scope *scope);
};

class CodeScope: public Scope {
public:
    Allocation offset;
    bool contents_finalized;  // for sanity check
    bool is_taken;  // too
    Unwound unwound;  // for optimizing out checks
    int low_pc;
    int high_pc;
    Label got_nothing_label;
    Label got_exception_label;
    Label got_yield_label;

    CodeScope();

    virtual void be_taken();
    virtual bool is_transient();
    virtual bool may_omit_finalization();
    virtual void be_unwindable(Unwound u);
    virtual void leave();
    virtual Allocation reserve(Allocation s);
    virtual void rollback(Allocation checkpoint);
    virtual void allocate();
    virtual Storage get_local_storage();
    virtual TypeSpec get_pivot_ts();
    virtual bool may_omit_content_finalization();
    virtual void initialize_contents(Cx *cx);
    virtual void finalize_contents(Cx *cx);
    virtual void finalize_contents_and_unwind(Cx *cx);
    virtual void jump_to_content_finalization(Declaration *last, Cx *cx);
    virtual void finalize(Cx *cx);
    virtual void debug(TypeMatch tm, Cx *cx);
};

class RetroScope: public CodeScope {
public:
    Allocation header_offset;

    RetroScope();

    virtual RetroScope *get_retro_scope();
    virtual void allocate();
    virtual int get_frame_offset();
};

class SwitchScope: public CodeScope {
public:
    Variable *switch_variable;

    SwitchScope();

    virtual void set_switch_variable(Variable *sv);
    virtual Variable *get_variable();
    virtual SwitchScope *get_switch_scope();
    virtual void debug(TypeMatch tm, Cx *cx);
};

class TryScope: public CodeScope {
public:
    TreenumerationType *exception_type;
    bool have_implicit_matcher;

    TryScope();

    virtual TryScope *get_try_scope();
    virtual bool set_exception_type(TreenumerationType *et, bool is_implicit_matcher);
    virtual TreenumerationType *get_exception_type();
    virtual bool has_implicit_matcher();
    virtual void finalize_contents_and_unwind(Cx *cx);
    virtual void jump_to_content_finalization(Declaration *last, Cx *cx);
    virtual void debug(TypeMatch tm, Cx *cx);
};

class TransparentTryScope: public TryScope {
public:
    TransparentTryScope();

    virtual void add(Declaration *d);
    virtual void jump_to_content_finalization(Declaration *last, Cx *cx);
};

class EvalScope: public CodeScope {
public:
    EvalScope();

    virtual int get_yield_value();
    virtual EvalScope *get_eval_scope();
    virtual void finalize_contents_and_unwind(Cx *cx);
    virtual void jump_to_content_finalization(Declaration *last, Cx *cx);
};

class ArgumentScope: public Scope {
public:
    ArgumentScope();

    virtual void allocate();
    virtual Allocation reserve(Allocation s);
    virtual Storage get_local_storage();
    virtual TypeSpec get_pivot_ts();
};

class RetroArgumentScope: public ArgumentScope {
public:
    TypeSpec tuple_ts;
    Allocation offset;

    RetroArgumentScope(TypeSpec tts);

    virtual Value *match(std::string name, Value *pivot, Scope *scope);
    virtual void allocate();
    virtual Storage get_local_storage();
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

    FunctionScope();

    virtual ArgumentScope *add_result_scope();
    virtual ArgumentScope *add_self_scope();
    virtual ArgumentScope *add_head_scope();
    virtual CodeScope *add_body_scope();
    virtual void adjust_frame_base_offset(int fbo);
    virtual void set_exception_type(TreenumerationType *et);
    virtual TreenumerationType *get_exception_type();
    virtual void make_forwarded_exception_storage();
    virtual Storage get_forwarded_exception_storage();
    virtual void make_result_alias_storage();
    virtual Storage get_result_alias_storage();
    virtual void make_associated_offset_storage();
    virtual Storage get_associated_offset_storage();
    virtual TypeSpec get_pivot_ts();
    virtual Value *lookup(std::string name, Value *pivot, Scope *scope);
    virtual Allocation reserve(Allocation s);
    virtual void rollback(Allocation checkpoint);
    virtual void allocate();
    virtual Storage get_local_storage();
    virtual unsigned get_frame_size();
    virtual FunctionScope *get_function_scope();
    virtual SwitchScope *get_switch_scope();
    virtual TryScope *get_try_scope();
    virtual EvalScope *get_eval_scope();
    virtual RetroScope *get_retro_scope();
    virtual void be_unwindable(Unwound u);
    virtual std::vector<Variable *> get_result_variables();
};
