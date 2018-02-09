
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


class OptionIsValue: public GenericValue {
public:
    OptionIsValue(Value *p, TypeMatch &match)
        :GenericValue(OPTIONSELECTOR_TS, BOOLEAN_TS, p) {
    }
    
    virtual Regs precompile(Regs preferred) {
        return left->precompile(preferred) | right->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        Storage ls = left->compile(x64);
        Storage rs = right->compile(x64);
        
        if (rs.where != CONSTANT)
            throw INTERNAL_ERROR;
            
        switch (ls.where) {
        case STACK:
            left->ts.destroy(Storage(MEMORY, Address(RSP, 0)), x64);
            x64->op(CMPB, Address(RSP, 0), rs.value);
            x64->op(LEA, RSP, Address(RSP, left->ts.measure_stack()));
            return Storage(FLAGS, SETE);
        case MEMORY:
            x64->op(CMPB, ls.address, rs.value);
            return Storage(FLAGS, SETE);
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class OptionAsValue: public GenericValue {
public:
    TypeSpec same_ts;
    
    OptionAsValue(Value *p, TypeMatch &match)
        :GenericValue(OPTIONSELECTOR_TS, VOID_TS, p) {
        same_ts = match[1];
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!GenericValue::check(args, kwargs, scope))
            return false;
            
        BasicValue *v = dynamic_cast<BasicValue *>(right.get());
        if (!v)
            throw INTERNAL_ERROR;
            
        switch (v->number) {
        case 0:
            break;
        case 1:
            ts = same_ts;
            break;
        default:
            throw INTERNAL_ERROR;
        }
        
        return true;
    }
    
    virtual Regs precompile(Regs preferred) {
        return left->precompile(preferred) | right->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        Storage ls = left->compile(x64);
        Storage rs = right->compile(x64);
        
        if (rs.where != CONSTANT)
            throw INTERNAL_ERROR;
            
        Label ok;
            
        switch (ls.where) {
        case STACK:
            if (rs.value == 0) {
                left->ts.destroy(Storage(MEMORY, Address(RSP, 0)), x64);
                x64->op(CMPB, Address(RSP, 0), 0);
                x64->op(LEA, RSP, Address(RSP, left->ts.measure_stack()));
                x64->op(JE, ok);
                
                x64->die("Option is not None!");
                
                x64->code_label(ok);
                return Storage();
            }
            else {
                x64->op(CMPB, Address(RSP, 0), 1);
                x64->op(JE, ok);

                x64->die("Option is not Some!");

                x64->code_label(ok);
                x64->op(ADDQ, RSP, 8);
                return Storage(STACK);
            }
        case MEMORY:
            x64->op(CMPB, ls.address, rs.value);
            x64->op(JE, ok);
            
            x64->die("Option is not that!");
            
            x64->code_label(ok);
            
            if (rs.value == 0)
                return Storage();
            else
                return Storage(MEMORY, ls.address + INTEGER_SIZE);
        default:
            throw INTERNAL_ERROR;
        }
    }
};
