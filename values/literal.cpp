
// This class must only return 32-bit constants as CONSTANT storage class,
// because that is what we can treat as immediate value in instructions.
// If a greater value is specified, it must be loaded into a register, and
// returned as REGISTER.

class BasicValue: public Value {
public:
    int number;
    
    BasicValue(TypeSpec ts, int n)
        :Value(ts) {
        number = n;
    }

    virtual Regs precompile(Regs) {
        return Regs();
    }

    virtual Storage compile(X64 *) {
        return Storage(CONSTANT, number);
    }
};


class UnicodeCharacterValue: public Value {
public:
    std::unique_ptr<Value> value;

    UnicodeCharacterValue()
        :Value(CHARACTER_TS) {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        return check_arguments(args, kwargs, { { "code", &INTEGER_TS, scope, &value } });
    }

    virtual Regs precompile(Regs preferred) {
        return value->precompile(preferred);
    }

    virtual Storage compile(X64 *x64) {
        Storage s = value->compile(x64);
        
        switch (s.where) {
        case CONSTANT:
            return Storage(CONSTANT, s.value & 0xFFFF);
        case REGISTER:
            return Storage(REGISTER, s.reg);
        case MEMORY:
            return Storage(MEMORY, s.address);
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class StringLiteralValue: public Value {
public:
    std::string text;
    
    StringLiteralValue(std::string t)
        :Value(STRING_TS) {
        text = t;
    }

    virtual Regs precompile(Regs preferred) {
        return Regs();
        //return Regs().add(RAX).add(RCX).add(RSI).add(RDI);
    }

    virtual Storage compile(X64 *x64) {
        std::vector<unsigned short> characters = decode_utf8(text);
        Label l = x64->data_heap_string(characters);
        
        x64->op(LEARIP, RBX, l);
        x64->incref(RBX);  // This way we can return the same static string many times
        x64->op(PUSHQ, RBX);
        
        return Storage(STACK);
    }
};




class TreenumerationMatcherValue: public GenericValue, public Raiser {
public:
    int index;
    
    TreenumerationMatcherValue(int i, Value *p)
        :GenericValue(NO_TS, VOID_TS, p) {
        index = i;
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
        TreenumerationType *t = dynamic_cast<TreenumerationType *>(left->ts.rvalue()[0]);
        int tail = t->get_tail(index);
        Label match, unmatch;
        
        ls = left->compile(x64);
        
        switch (ls.where) {
        case CONSTANT:
            if (ls.value >= index && ls.value <= tail)
                return Storage();
            else {
                raise("UNMATCHED", x64);
                return Storage();
            }
        case REGISTER:
            x64->op(CMPB, ls.reg, index);
            x64->op(JB, unmatch);
            x64->op(CMPB, ls.reg, tail);
            x64->op(JBE, match);
            break;
        case MEMORY:
            x64->op(CMPB, ls.address, index);
            x64->op(JB, unmatch);
            x64->op(CMPB, ls.address, tail);
            x64->op(JBE, match);
            break;
        default:
            throw INTERNAL_ERROR;
        }
        
        x64->code_label(unmatch);
        raise("UNMATCHED", x64);
        x64->code_label(match);
        
        return Storage();
    }
};
