
class EqualityValue: public Value {
public:
    bool no;
    std::unique_ptr<Value> value;

    EqualityValue(bool n, Value *v)
        :Value(BOOLEAN_TS) {
        no = n;
        value.reset(v);
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        return value->check(args, kwargs, scope);
    }
    
    virtual Regs precompile(Regs preferred) {
        return value->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        // Returns a boolean if the arguments were equal
        Storage s = value->compile(x64);

        if (!no)
            return s;
        
        switch (s.where) {
        case CONSTANT:
            return Storage(CONSTANT, !s.value);
        case FLAGS:
            return Storage(FLAGS, negate(s.bitset));
        case REGISTER:
            x64->op(CMPB, s.reg, 0);
            return Storage(FLAGS, SETE);
        case MEMORY:
            x64->op(CMPB, s.address, 0);
            return Storage(FLAGS, SETE);
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class ComparisonValue: public Value {
public:
    BitSetOp bitset;
    std::unique_ptr<Value> value;

    ComparisonValue(BitSetOp b, Value *v)
        :Value(BOOLEAN_TS) {
        bitset = b;
        value.reset(v);
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        return value->check(args, kwargs, scope);
    }
    
    virtual Regs precompile(Regs preferred) {
        return value->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        // Returns an integer representing the ordering of the arguments
        Storage s = value->compile(x64);

        switch (s.where) {
        case REGISTER:
            x64->op(CMPB, s.reg, 0);
            break;
        case MEMORY:
            x64->op(CMPB, s.address, 0);
            break;
        default:
            throw INTERNAL_ERROR;
        }

        return Storage(FLAGS, bitset);
    }
};


class EqualityMatcherValue: public GenericValue, public Raiser {
public:
    EqualityMatcherValue(Value *v)
        :GenericValue(v->ts.rvalue(), VOID_TS, v) {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!check_raise(match_unmatched_exception_type, scope))
            return false;
            
        return GenericValue::check(args, kwargs, scope);
    }

    virtual Regs precompile(Regs preferred) {
        return left->precompile(preferred) | right->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        Label less, greater, equal;
        
        compile_and_store_both(x64, Storage(STACK), Storage(STACK));
        
        // Since the switch variable is on the right, use its type just in case
        right->ts.compare(Storage(STACK), Storage(STACK), x64, less, greater);
        
        x64->op(JMP, equal);
        
        x64->code_label(less);
        x64->code_label(greater);
        
        raise("UNMATCHED", x64);
        
        x64->code_label(equal);
        return Storage();
    }
};


class ImplicitEqualityMatcherValue: public Value, public Raiser {
public:
    std::unique_ptr<Value> switch_var_value, value;
    
    ImplicitEqualityMatcherValue(Value *v)
        :Value(VOID_TS) {
        value.reset(v);
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!check_raise(match_unmatched_exception_type, scope))
            return false;

        switch_var_value.reset(lookup_switch(scope, token));
            
        if (args.size() != 0 || kwargs.size() != 0)
            return false;

        return true;
    }

    virtual Regs precompile(Regs preferred) {
        return switch_var_value->precompile(preferred) | value->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        Label less, greater, equal;
        
        switch_var_value->compile_and_store(x64, Storage(STACK));
        value->compile_and_store(x64, Storage(STACK));
        
        switch_var_value->ts.compare(Storage(STACK), Storage(STACK), x64, less, greater);
        
        x64->op(JMP, equal);
        
        x64->code_label(less);
        x64->code_label(greater);
        
        raise("UNMATCHED", x64);
        
        x64->code_label(equal);
        return Storage();
    }
};


class InitializerEqualityMatcherValue: public Value, public Raiser {
public:
    std::unique_ptr<Value> switch_var_value, initializer_value;
    
    InitializerEqualityMatcherValue(Value *iv)
        :Value(VOID_TS) {
        initializer_value.reset(iv);
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!check_raise(match_unmatched_exception_type, scope))
            return false;
            
        switch_var_value.reset(lookup_switch(scope, token));
            
        return initializer_value->check(args, kwargs, scope);
    }

    virtual Regs precompile(Regs preferred) {
        return switch_var_value->precompile(preferred) | initializer_value->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        Label less, greater, equal;
        
        Storage ss = switch_var_value->compile(x64);
        if (ss.where != MEMORY)
            throw INTERNAL_ERROR;
        
        Storage is = initializer_value->compile(x64);
            
        switch_var_value->ts.compare(ss, is, x64, less, greater);
        
        x64->op(JMP, equal);
        
        x64->code_label(less);
        x64->code_label(greater);
        
        raise("UNMATCHED", x64);
        
        x64->code_label(equal);
        return Storage();
    }
};


class BulkEqualityMatcherValue: public Value, public Raiser {
public:
    std::unique_ptr<Value> switch_var_value;
    std::vector<std::unique_ptr<Value>> values;
    
    BulkEqualityMatcherValue()
        :Value(VOID_TS) {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!check_raise(match_unmatched_exception_type, scope))
            return false;

        switch_var_value.reset(lookup_switch(scope, token));
            
        if (kwargs.size() != 0)
            return false;
            
        TypeSpec ts = switch_var_value->ts.rvalue();
            
        for (auto &a : args) {
            Value *v = typize(a.get(), scope, &ts);
            
            values.push_back(std::unique_ptr<Value>(v));
        }

        return true;
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = switch_var_value->precompile(preferred);
        
        for (auto &v : values)
            clob = clob | v->precompile(preferred);
            
        return clob;
    }
    
    virtual Storage compile(X64 *x64) {
        Label equal;
        
        Storage s = switch_var_value->compile(x64);
        if (s.where != MEMORY)
            throw INTERNAL_ERROR;
            
        for (auto &v : values) {
            Label less, greater;
            Storage t = v->compile(x64);
            
            switch_var_value->ts.compare(s, t, x64, less, greater);
            
            x64->op(JMP, equal);
            
            x64->code_label(less);
            x64->code_label(greater);
        }
        
        raise("UNMATCHED", x64);
        
        x64->code_label(equal);
        return Storage();
    }
};
