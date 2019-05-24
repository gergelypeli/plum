class ControlValue: public Value {
public:
    std::string name;
    TypeSpec context_ts;

    ControlValue(std::string n);

    virtual TypeSpec codify(TypeSpec ts);
    virtual void set_context_ts(TypeSpec *c);
    virtual bool has_fuzzy_context_ts();
    virtual bool check_args(Args &args, ArgInfo arg_info);
    virtual bool check_kwargs(Kwargs &kwargs, ArgInfos arg_infos);
    virtual Storage get_context_storage();
    virtual Storage nonsense_result(X64 *x64);
};

class IfValue: public ControlValue {
public:
    std::unique_ptr<Value> condition;
    std::unique_ptr<Value> then_branch;
    std::unique_ptr<Value> else_branch;
    Register reg;
    
    IfValue(OperationType o, Value *pivot, TypeMatch &match);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};

class YieldableValue: public ControlValue {
public:
    std::string yield_name;
    EvalScope *eval_scope;
    Variable *yield_var;

    YieldableValue(std::string n, std::string yn);

    virtual std::string get_yield_label();
    virtual bool setup_yieldable(Scope *scope);
    virtual Storage get_yield_storage();
    virtual void store_yield(Storage s, X64 *x64);
};

class RepeatValue: public ControlValue {
public:
    std::unique_ptr<Value> setup, condition, step, body;
    Label start, end;
    
    RepeatValue(Value *pivot, TypeMatch &match);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};
class ForEachValue: public ControlValue {
public:
    std::unique_ptr<Value> iterator, each, body, next;
    Variable *iterator_var;
    TryScope *next_try_scope;
    TypeSpec each_ts;

    ForEachValue(Value *pivot, TypeMatch &match);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
    virtual CodeScope *unwind(X64 *x64);
};

class SwitchValue: public ControlValue {
public:
    std::unique_ptr<Value> value, body;
    SwitchScope *switch_scope;
    Variable *switch_var;

    SwitchValue(Value *v, TypeMatch &m);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
    virtual CodeScope *unwind(X64 *x64);
};

class IsValue: public ControlValue {
public:
    std::unique_ptr<Value> match, then_branch, else_branch;
    //Variable *matched_var;
    CodeScope *then_scope;
    TryScope *match_try_scope;
    Label end;
    bool matching;
    const int CAUGHT_UNMATCHED_EXCEPTION = -255;  // hopefully outside of yield range

    IsValue(Value *v, TypeMatch &m);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
    virtual CodeScope *unwind(X64 *x64);
};

// This class is not a subclass of Raiser, because that is for incoming exceptions
// that must be in try scopes, while this is for outgoing exceptions that must not be
// in try scopes.
class RaiseValue: public ControlValue {
public:
    Declaration *dummy;
    std::unique_ptr<Value> value;
    int exception_value;

    RaiseValue(Value *v, TypeMatch &m);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};

class TryValue: public ControlValue {
public:
    std::unique_ptr<Value> body, handler;
    TryScope *try_scope;
    Variable *switch_var;
    SwitchScope *switch_scope;
    bool handling;
    
    TryValue(Value *v, TypeMatch &m);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
    virtual CodeScope *unwind(X64 *x64);
};

// The Eval-Yield pairs use a different numeric exception range from normal exceptions,
// so their unwind paths are always distinct.
class EvalValue: public YieldableValue {
public:
    std::unique_ptr<Value> body;

    EvalValue(std::string en);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
    virtual CodeScope *unwind(X64 *x64);
};

class YieldValue: public ControlValue {
public:
    Declaration *dummy;
    std::unique_ptr<Value> value;
    YieldableValue *yieldable_value;

    YieldValue(YieldableValue *yv);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};
