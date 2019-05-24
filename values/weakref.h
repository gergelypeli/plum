// Weakref
void compile_nosyref_callback(Label label, X64 *x64);
void compile_nosyref_finalizer(Label label, X64 *x64);
void alloc_nosyref(X64 *x64);

class WeakrefToValue: public Value {
public:
    std::unique_ptr<Value> target;
    TypeSpec heap_ts;

    WeakrefToValue(TypeSpec hts);

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};

class WeakrefDeadMatcherValue: public GenericValue, public Raiser {
public:
    TypeSpec param_heap_ts;

    WeakrefDeadMatcherValue(Value *p, TypeMatch &match);

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};

class WeakrefLiveMatcherValue: public GenericValue, public Raiser {
public:
    TypeSpec param_heap_ts;

    WeakrefLiveMatcherValue(Value *p, TypeMatch &match);

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};
