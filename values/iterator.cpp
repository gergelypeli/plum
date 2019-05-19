
class SimpleRecordValue: public GenericValue {
public:
    SimpleRecordValue(TypeSpec ret_ts, Value *pivot)
        :GenericValue(NO_TS, ret_ts, pivot) {
    }

    virtual Regs precompile(Regs preferred) {
        return left->precompile(preferred);
    }
};


// Counters

class CountupNextValue: public GenericValue, public Raiser {
public:
    Regs clob;
    
    CountupNextValue(Value *l, TypeMatch &match)
        :GenericValue(NO_TS, INTEGER_TUPLE1_TS, l) {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!check_arguments(args, kwargs, {}))
            return false;
            
        if (!check_raise(iterator_done_exception_type, scope))
            return false;

        return true;
    }

    virtual Regs precompile(Regs preferred) {
        clob = left->precompile(preferred);

        clob.reserve_gpr(3);
        
        return clob;
    }

    virtual void advance(Address addr, X64 *x64) {
        x64->op(INCQ, addr);
    }

    virtual Storage compile(X64 *x64) {
        ls = left->compile(x64);
        Register reg = (clob & ~ls.regs()).get_gpr();
        Label ok;
        
        switch (ls.where) {
        case MEMORY:
            x64->op(MOVQ, reg, ls.address + INTEGER_SIZE);  // value
            x64->op(CMPQ, reg, ls.address); // limit
            x64->op(JNE, ok);
            
            raise("ITERATOR_DONE", x64);
            
            x64->code_label(ok);
            advance(ls.address + INTEGER_SIZE, x64);
            return Storage(REGISTER, reg);
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class CountdownNextValue: public CountupNextValue {
public:

    CountdownNextValue(Value *l, TypeMatch &match)
        :CountupNextValue(l, match) {
    }
    
    virtual void advance(Address addr, X64 *x64) {
        x64->op(DECQ, addr);
    }
};


class CountupValue: public SimpleRecordValue {
public:
    CountupValue(Value *l, TypeMatch &match)
        :SimpleRecordValue(COUNTUP_TS, l) {
    }

    virtual Storage compile(X64 *x64) {
        ls = left->compile(x64);  // integer limit
        
        x64->op(PUSHQ, 0);  // value
        
        switch (ls.where) {
        case CONSTANT:
            x64->op(PUSHQ, ls.value);
            break;
        case REGISTER:
            x64->op(PUSHQ, ls.reg);
            break;
        case MEMORY:
            x64->op(PUSHQ, ls.address);
            break;
        default:
            throw INTERNAL_ERROR;
        }
        
        return Storage(STACK);
    }
};


class CountdownValue: public SimpleRecordValue {
public:
    CountdownValue(Value *l, TypeMatch &match)
        :SimpleRecordValue(COUNTDOWN_TS, l) {
    }

    virtual Storage compile(X64 *x64) {
        ls = left->compile(x64);
        
        switch (ls.where) {  // value
        case CONSTANT:
            x64->op(PUSHQ, ls.value - 1);
            break;
        case REGISTER:
            x64->op(DECQ, ls.reg);
            x64->op(PUSHQ, ls.reg);
            break;
        case MEMORY:
            x64->op(MOVQ, R10, ls.address);
            x64->op(DECQ, R10);
            x64->op(PUSHQ, R10);
            break;
        default:
            throw INTERNAL_ERROR;
        }
        
        x64->op(PUSHQ, -1);
        
        return Storage(STACK);
    }
};
