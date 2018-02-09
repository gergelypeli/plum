
class OptionNoneValue: public Value {
public:
    OptionNoneValue(TypeSpec ts)
        :Value(ts) {
    }

    virtual Regs precompile(Regs preferred) {
        return Regs();
    }
    
    virtual Storage compile(X64 *x64) {
        ts.create(Storage(), Storage(STACK), x64);
        return Storage(STACK);
    }
};


class OptionSomeValue: public Value {
public:
    std::unique_ptr<Value> some;
    
    OptionSomeValue(TypeSpec ts)
        :Value(ts) {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        TypeSpec some_ts = ts.unprefix(option_type);
        return check_arguments(args, kwargs, {{ "some", &some_ts, scope, &some }});
    }

    virtual Regs precompile(Regs preferred) {
        return some->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        some->compile_and_store(x64, Storage(STACK));
        x64->op(PUSHQ, 1);
        return Storage(STACK);
    }
};
