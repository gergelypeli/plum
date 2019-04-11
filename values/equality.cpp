
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
            return Storage(FLAGS, negated(s.cc));
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
    ConditionCode cc;
    std::unique_ptr<Value> value;

    ComparisonValue(ConditionCode c, Value *v)
        :Value(BOOLEAN_TS) {
        cc = c;
        value.reset(v);
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        return value->check(args, kwargs, scope);
    }
    
    virtual Regs precompile(Regs preferred) {
        return value->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        // Value returns an integer representing the ordering of the arguments
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

        return Storage(FLAGS, cc);
    }
};


class BulkEqualityMatcherValue: public Value, public Raiser {
public:
    std::unique_ptr<Value> pivot_value;
    std::vector<std::unique_ptr<Value>> values;
    
    BulkEqualityMatcherValue(Value *p)
        :Value(p->ts.rvalue()) {
        pivot_value.reset(p);
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!check_raise(match_unmatched_exception_type, scope))
            return false;

        if (kwargs.size() != 0)
            return false;
            
        TypeSpec arg_ts = pivot_value->ts.rvalue();
            
        for (auto &a : args) {
            Value *v = typize(a.get(), scope, &arg_ts);
            
            values.push_back(std::unique_ptr<Value>(v));
        }

        return true;
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = pivot_value->precompile(preferred);
        
        for (auto &v : values)
            clob = clob | v->precompile(preferred);
            
        return clob | EQUAL_CLOB;
    }
    
    virtual Storage compile(X64 *x64) {
        Label equal;
        Storage ps = pivot_value->compile(x64);
        
        // The switch variables are read only, no need to push a copy
        if (!am_implicit_matcher) {
            pivot_value->ts.store(ps, Storage(STACK), x64);
            ps = Storage(STACK);
        }
            
        for (auto &v : values) {
            int pop = 0;
            Storage vs = v->compile(x64);
            
            if (vs.where == STACK) {
                // NOTE: we use that destroy does not change values
                vs = Storage(MEMORY, Address(RSP, 0));
                v->ts.destroy(vs, x64);
                pop = v->ts.measure_stack();
            }
            
            pivot_value->ts.equal(ps, vs, x64);
            
            if (pop)
                x64->op(LEA, RSP, Address(RSP, pop));  // preserve flags
            
            x64->op(JE, equal);
        }

        int old_stack_usage = x64->accounting->mark();
        pivot_value->ts.store(ps, Storage(), x64);
        raise("UNMATCHED", x64);
        x64->accounting->rewind(old_stack_usage);
        
        x64->code_label(equal);
        
        return ps;
    }
};
