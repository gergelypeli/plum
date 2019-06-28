// Declarations
class Declaration {
public:
    Scope *outer_scope;
    Label finalization_label;
    bool need_finalization_label;
    bool is_finalized;  // sanity check

    Declaration();

    virtual ~Declaration();
    virtual void set_outer_scope(Scope *os);
    virtual void outer_scope_entered();
    virtual void outer_scope_left();
    virtual Value *match(std::string name, Value *pivot, Scope *scope);
    virtual bool is_transient();
    virtual bool may_omit_finalization();
    virtual bool is_typedefinition(std::string n);
    virtual void allocate();
    virtual void finalize(Cx *cx);
    virtual void jump_to_finalization(Cx *cx);
    virtual DataScope *find_inner_scope(std::string name);
    virtual void debug(TypeMatch tm, Cx *cx);
};
