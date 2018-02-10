
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


class OptionOperationValue: public GenericOperationValue {
public:
    OptionOperationValue(OperationType o, Value *p, TypeMatch &match)
        :GenericOperationValue(o, op_arg_ts(o, match), op_ret_ts(o, match), p) {
    }
};


class OptionIsNoneValue: public GenericValue {
public:
    TypeSpec option_ts;
    
    OptionIsNoneValue(Value *p, TypeMatch &match)
        :GenericValue(NO_TS, BOOLEAN_TS, p) {
        option_ts = match[1].prefix(option_type);
    }
    
    virtual Regs precompile(Regs preferred) {
        return left->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        Storage ls = left->compile(x64);
            
        switch (ls.where) {
        case STACK:
            option_ts.destroy(Storage(MEMORY, Address(RSP, 0)), x64);
            x64->op(CMPB, Address(RSP, 0), 0);
            x64->op(LEA, RSP, Address(RSP, option_ts.measure_stack()));
            return Storage(FLAGS, SETE);
        case MEMORY:
            x64->op(CMPB, ls.address, 0);
            return Storage(FLAGS, SETE);
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class OptionIsSomeValue: public GenericValue {
public:
    TypeSpec option_ts;
    
    OptionIsSomeValue(Value *p, TypeMatch &match)
        :GenericValue(NO_TS, BOOLEAN_TS, p) {
        option_ts = match[1].prefix(option_type);
    }
    
    virtual Regs precompile(Regs preferred) {
        return left->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        Storage ls = left->compile(x64);
            
        switch (ls.where) {
        case STACK:
            option_ts.destroy(Storage(MEMORY, Address(RSP, 0)), x64);
            x64->op(CMPB, Address(RSP, 0), 1);
            x64->op(LEA, RSP, Address(RSP, option_ts.measure_stack()));
            return Storage(FLAGS, SETE);
        case MEMORY:
            x64->op(CMPB, ls.address, 1);
            return Storage(FLAGS, SETE);
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class OptionAsNoneValue: public GenericValue {
public:
    OptionAsNoneValue(Value *p, TypeMatch &match)
        :GenericValue(NO_TS, VOID_TS, p) {
    }

    virtual Regs precompile(Regs preferred) {
        return left->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        Storage ls = left->compile(x64);
        Label ok;
            
        switch (ls.where) {
        case STACK:
            left->ts.destroy(Storage(MEMORY, Address(RSP, 0)), x64);
            x64->op(CMPB, Address(RSP, 0), 0);
            x64->op(LEA, RSP, Address(RSP, left->ts.measure_stack()));
            x64->op(JE, ok);
            
            x64->die("Option is not `none!");
            
            x64->code_label(ok);
            return Storage();
        case MEMORY:
            x64->op(CMPB, ls.address, 0);
            x64->op(JE, ok);
            
            x64->die("Option is not `none!");
            
            x64->code_label(ok);
            return Storage();
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class OptionAsSomeValue: public GenericValue {
public:
    OptionAsSomeValue(Value *p, TypeMatch &match)
        :GenericValue(NO_TS, match[1].varvalue(), p) {
    }

    virtual Regs precompile(Regs preferred) {
        return left->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        Storage ls = left->compile(x64);
        Label ok;
            
        switch (ls.where) {
        case STACK:
            x64->op(CMPB, Address(RSP, 0), 1);
            x64->op(JE, ok);

            x64->die("Option is not `some!");

            x64->code_label(ok);
            x64->op(ADDQ, RSP, INTEGER_SIZE);
            return Storage(STACK);
        case MEMORY:
            x64->op(CMPB, ls.address, 1);
            x64->op(JE, ok);
            
            x64->die("Option is not `some!");
            
            x64->code_label(ok);
            return Storage(MEMORY, ls.address + INTEGER_SIZE);
        default:
            throw INTERNAL_ERROR;
        }
    }
};
