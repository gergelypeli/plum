// Values
class Value {
public:
    TypeSpec ts;
    Token token;
    bool lvalue_needed;

    Value(TypeSpec t);

    virtual ~Value();
    virtual Value *set_token(Token t);
    virtual void set_context_ts(TypeSpec *c);
    bool check_arguments(Args &args, Kwargs &kwargs, const ArgInfos &arg_infos, bool is_function_call = false);
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual void need_lvalue();
    virtual Regs precompile(Regs);
    virtual Regs precompile_tail();
    virtual Storage compile(X64 *);
    virtual void compile_and_store(X64 *x64, Storage t);
    virtual Storage compile_lvalue(X64 *x64);
    virtual Declaration *declare(std::string name, Scope *scope);
    virtual void escape_statement_variables();
    virtual CodeScope *unwind(X64 *x64);
    virtual bool define_data();
    virtual bool define_code();
    virtual Value *lookup_inner(std::string name, Scope *scope);
    virtual void streamify(X64 *x64);
};

class Raiser {
public:
    RaisingDummy *raising_dummy;
    TreenumerationType *raised_type;
    bool am_implicit_matcher;

    Raiser();
    
    void be_implicit_matcher();
    virtual void make_raising_dummy(Scope *scope);
    virtual bool check_raise(TreenumerationType *exception_type, Scope *scope);
    virtual void raise(std::string keyword, X64 *x64);
};

class RvalueCastValue: public Value {
public:
    std::unique_ptr<Value> pivot;
    Storage rvalue_storage;
    Regs borrows_allowed;
    Register unalias_reg;
    Register container_reg;
    bool ralias_needed;

    RvalueCastValue(Value *v);

    virtual void need_ralias();
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};

class ContainedLvalue {
public:
    ContainedLvalue();
    virtual Regs precompile_contained_lvalue(Regs preferred, bool lvalue_needed, TypeSpec ts);
    virtual Storage compile_contained_lvalue(Address addr, Register container_ref, TypeSpec ts, X64 *x64);
};

class PassValue: public Value {
public:
    PassValue(Value *p, TypeMatch tm);
    
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};

class CastValue: public Value {
public:
    std::unique_ptr<Value> pivot;

    CastValue(Value *p, TypeSpec ts);

    virtual void need_lvalue();
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};

class PtrCastValue: public Value {
public:
    std::unique_ptr<Value> value;

    PtrCastValue(Value *p, TypeMatch tm);

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};

class VariableValue: public Value {
public:
    Variable *variable;
    std::unique_ptr<Value> pivot;
    //Register unalias_reg;
    TypeMatch match;
    TypeSpec pts;
    //bool is_rvalue_record;
    //bool is_single_record;
    //Storage value_storage;
    //Regs borrows_allowed;
    //bool ralias_needed;
    Register record_reg;
    Storage member_storage;

    VariableValue(Variable *v, Value *p, Scope *scope, TypeMatch &tm);

    virtual void need_lvalue();
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};

class SelfVariableValue: public VariableValue {
public:
    SelfInfo *self_info;

    SelfVariableValue(Variable *pv, Value *p, Scope *scope, TypeMatch &match, SelfInfo *si);

    virtual Value *lookup_inner(std::string name, Scope *scope);
};

class PartialVariableValue: public VariableValue {
public:
    PartialInfo *partial_info;

    PartialVariableValue(Variable *pv, Value *p, Scope *scope, TypeMatch &match, PartialInfo *pi);

    virtual Value *lookup_inner(std::string name, Scope *scope);
};

class EvaluableValue: public Value {
public:
    Evaluable *evaluable;

    EvaluableValue(Evaluable *e);

    virtual std::vector<Variable *> get_dvalue_variables();
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};

class EvaluateValue: public Value, public Raiser {
public:
    std::vector<Variable *> dvalue_variables;
    std::unique_ptr<Value> evaluable_value;
    std::vector<std::unique_ptr<Value>> arg_values;
    //std::vector<StorageWhere> retro_wheres;
    FunctionScope *fn_scope;
    static TypeSpec uncodify(TypeSpec ts);

    EvaluateValue(Value *p, TypeMatch &tm);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual void destroy_arguments(X64 *x64);
    virtual Storage compile(X64 *x64);
    virtual CodeScope *unwind(X64 *x64);
};

class RoleValue: public Value {
public:
    Associable *associable;
    std::unique_ptr<Value> pivot;
    Register reg;
    TypeMatch match;

    RoleValue(Associable *a, Value *p, TypeMatch &tm);

    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};
