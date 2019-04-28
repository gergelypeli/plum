
class OptionNoneValue: public Value {
public:
    OptionNoneValue(TypeSpec ts)
        :Value(ts) {
    }

    virtual Regs precompile(Regs preferred) {
        return Regs();
    }
    
    virtual Storage compile(X64 *x64) {
        ts.store(Storage(), Storage(STACK), x64);
        return Storage(STACK);
    }
};


class OptionSomeValue: public Value {
public:
    std::unique_ptr<Value> some;
    int flag_size;
    
    OptionSomeValue(TypeSpec ts)
        :Value(ts) {
        flag_size = OptionType::get_flag_size(ts.unprefix(option_type));
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
        
        if (flag_size == ADDRESS_SIZE)
            x64->op(PUSHQ, OPTION_FLAG_NONE + 1);
        else if (flag_size)
            throw INTERNAL_ERROR;
            
        return Storage(STACK);
    }
};


class OptionOperationValue: public GenericOperationValue {
public:
    OptionOperationValue(OperationType o, Value *p, TypeMatch &match)
        :GenericOperationValue(o, op_arg_ts(o, match), op_ret_ts(o, match), p) {
    }
};


class OptionNoneMatcherValue: public GenericValue, public Raiser {
public:
    OptionNoneMatcherValue(Value *p, TypeMatch &match)
        :GenericValue(NO_TS, VOID_TS, p) {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!check_raise(match_unmatched_exception_type, scope))
            return false;
        
        return GenericValue::check(args, kwargs, scope);
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
            x64->op(CMPQ, Address(RSP, 0), OPTION_FLAG_NONE);
            x64->op(LEA, RSP, Address(RSP, left->ts.measure_stack()));  // discard STACK, keep flags
            x64->op(JE, ok);
            
            raise("UNMATCHED", x64);

            x64->code_label(ok);
            return Storage();
        case MEMORY:
            x64->op(CMPQ, ls.address, OPTION_FLAG_NONE);
            x64->op(JE, ok);

            raise("UNMATCHED", x64);
            
            x64->code_label(ok);
            return Storage();
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class OptionSomeMatcherValue: public GenericValue, public Raiser {
public:
    int flag_size;
    
    OptionSomeMatcherValue(Value *p, TypeMatch &match)
        :GenericValue(NO_TS, match[1], p) {
        flag_size = OptionType::get_flag_size(match[1]);
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!check_raise(match_unmatched_exception_type, scope))
            return false;
        
        return GenericValue::check(args, kwargs, scope);
    }

    virtual Regs precompile(Regs preferred) {
        return left->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        Storage ls = left->compile(x64);
        Label ok;
            
        switch (ls.where) {
        case STACK: {
            x64->op(CMPQ, Address(RSP, 0), OPTION_FLAG_NONE);
            x64->op(JNE, ok);

            int old_stack_usage = x64->accounting->mark();
            left->ts.store(ls, Storage(), x64);
            raise("UNMATCHED", x64);
            x64->accounting->rewind(old_stack_usage);

            x64->code_label(ok);
            if (flag_size == ADDRESS_SIZE)
                x64->op(ADDQ, RSP, ADDRESS_SIZE);
            else if (flag_size)
                throw INTERNAL_ERROR;
                
            return Storage(STACK);
        }
        case MEMORY:
            x64->op(CMPQ, ls.address, OPTION_FLAG_NONE);
            x64->op(JNE, ok);
            
            raise("UNMATCHED", x64);
            
            x64->code_label(ok);
            return Storage(MEMORY, ls.address + flag_size);
        default:
            throw INTERNAL_ERROR;
        }
    }
};



class UnionValue: public Value {
public:
    TypeSpec arg_ts;
    int tag_index;
    std::unique_ptr<Value> value;
    
    UnionValue(TypeSpec ts, TypeSpec ats, int ti)
        :Value(ts) {
        arg_ts = ats;
        tag_index = ti;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        return check_arguments(args, kwargs, {{ "value", &arg_ts, scope, &value }});
    }

    virtual Regs precompile(Regs preferred) {
        if (value)
            return value->precompile(preferred);
        else
            return Regs();
    }
    
    virtual Storage compile(X64 *x64) {
        unsigned flag_size = UnionType::get_flag_size();
        unsigned full_size = ts.measure_stack();
        unsigned arg_size = arg_ts.measure_stack();
        unsigned pad_count = (full_size - flag_size - arg_size) / ADDRESS_SIZE;
        
        for (unsigned i = 0; i < pad_count; i++)
            x64->op(PUSHQ, 0);
            
        if (value)
            value->compile_and_store(x64, Storage(STACK));
        
        x64->op(PUSHQ, tag_index);
            
        return Storage(STACK);
    }
};


class UnionMatcherValue: public GenericValue, public Raiser {
public:
    TypeSpec union_ts;
    int tag_index;
    
    UnionMatcherValue(Value *p, TypeSpec uts, TypeSpec rts, int ti)
        :GenericValue(NO_TS, rts, p) {
        union_ts = uts;
        tag_index = ti;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!check_raise(match_unmatched_exception_type, scope))
            return false;
        
        return GenericValue::check(args, kwargs, scope);
    }

    virtual Regs precompile(Regs preferred) {
        return left->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        Storage ls = left->compile(x64);
        Label ok;
            
        switch (ls.where) {
        case STACK: {
            x64->op(CMPQ, Address(RSP, 0), tag_index);
            x64->op(JE, ok);

            int old_stack_usage = x64->accounting->mark();
            left->ts.store(ls, Storage(), x64);
            raise("UNMATCHED", x64);
            x64->accounting->rewind(old_stack_usage);

            x64->code_label(ok);
            
            // Since the matched type may be smaller than the maximum of the member sizes,
            // we may need to shift the value up a bit on the stack. Not nice.
            
            int shift_count = (left->ts.measure_stack() - ADDRESS_SIZE - ts.measure_stack()) / ADDRESS_SIZE;
            int copy_count = ts.measure_stack() / ADDRESS_SIZE;
            
            for (int i = 0; i < copy_count; i++) {
                x64->op(MOVQ, R10, Address(RSP, ADDRESS_SIZE * (copy_count - i)));
                x64->op(MOVQ, Address(RSP, ADDRESS_SIZE * (copy_count - i + shift_count)), R10);
            }

            x64->op(ADDQ, RSP, ADDRESS_SIZE * (1 + shift_count));
            
            return Storage(STACK);
        }
        case MEMORY:
            x64->op(CMPQ, ls.address, tag_index);
            x64->op(JE, ok);
            
            raise("UNMATCHED", x64);
            
            x64->code_label(ok);
            return Storage(MEMORY, ls.address + ADDRESS_SIZE);
        default:
            throw INTERNAL_ERROR;
        }
    }
};
