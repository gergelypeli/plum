
// This class must only return 32-bit constants as CONSTANT storage class,
// because that is what we can treat as immediate value in instructions.
// If a greater value is specified, it must be loaded into a register, and
// returned as REGISTER.

class BasicValue: public Value {
public:
    Register reg;
    int64 number;
    
    BasicValue(TypeSpec ts, int64 n)
        :Value(ts) {
        reg = NOREG;
        number = n;
    }

    virtual Regs precompile(Regs preferred) {
        if (number < -2147483648 || number > 2147483647) {
            reg = preferred.get_any();
            return Regs(reg);
        }
        else
            return Regs();
    }

    virtual Storage compile(X64 *x64) {
        if (number < -2147483648 || number > 2147483647) {
            x64->op(MOVABSQ, reg, number);
            return Storage(REGISTER, reg);
        }
        else {
            // 32-bit signed integers fit in immediate operands
            return Storage(CONSTANT, (int)number);  
        }
    }
};


class FloatValue: public Value {
public:
    double number;
    
    FloatValue(TypeSpec ts, double n)
        :Value(ts) {
        number = n;
    }

    virtual Regs precompile(Regs preferred) {
        return Regs();
    }

    virtual Storage compile(X64 *x64) {
        Label label;
        
        x64->data_label(label);
        x64->data_double(number);
        
        return Storage(MEMORY, Address(label, 0));
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
    std::ustring utext;
    
    StringLiteralValue(std::ustring ut)
        :Value(STRING_TS) {
        utext = ut;
    }

    virtual Regs precompile(Regs preferred) {
        return Regs(RAX);
    }

    virtual Storage compile(X64 *x64) {
        Label l = x64->runtime->data_heap_string(utext);
        
        x64->op(LEA, RAX, Address(l, 0));
        return Storage(BREGISTER, RAX);
    }
};


class StringTemplateValue: public Value {
public:
    std::vector<std::ustring> fragments;
    
    StringTemplateValue(std::vector<std::ustring> f)
        :Value(STRINGTEMPLATE_TS) {
        fragments = f;
    }

    virtual Regs precompile(Regs preferred) {
        throw INTERNAL_ERROR;
    }

    virtual Storage compile(X64 *x64) {
        throw INTERNAL_ERROR;
    }
};




class TreenumerationMatcherValue: public GenericValue, public Raiser {
public:
    int index;
    
    TreenumerationMatcherValue(int i, Value *p)
        :GenericValue(NO_TS, p->ts.rvalue(), p) {
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
        TreenumerationType *t = ptr_cast<TreenumerationType>(left->ts.rvalue()[0]);
        Label parents_label = t->get_parents_label(x64);
        Label loop, cond, match;
        
        ls = left->compile(x64);
        
        switch (ls.where) {
        case CONSTANT:
            x64->op(MOVQ, R11, ls.value);
            break;
        case REGISTER:
            x64->op(MOVZXBQ, R11, ls.reg);
            break;
        case STACK:
            x64->op(MOVZXBQ, R11, Address(RSP, 0));
            break;
        case MEMORY:
            x64->op(MOVZXBQ, R11, ls.address);
            break;
        default:
            throw INTERNAL_ERROR;
        }
        
        // R11 always contains one byte of nonzero data, so we can use it for addressing
        x64->op(LEA, R10, Address(parents_label, 0));
        
        x64->code_label(loop);
        x64->op(CMPQ, R11, index);
        x64->op(JE, match);
        
        x64->op(MOVB, R11B, Address(R10, R11, 0));
        
        x64->op(CMPQ, R11, 0);
        x64->op(JNE, loop);
        
        raise("UNMATCHED", x64);
        
        x64->code_label(match);
        
        return ls;
    }
};


// FIXME: Raiser is only needed as long as :is is stupid enough to think that non-raising
// expressions are equality matchers.
class TreenumerationAnyMatcherValue: public GenericValue, public Raiser {
public:
    TreenumerationAnyMatcherValue(Value *p)
        :GenericValue(NO_TS, p->ts.rvalue(), p) {
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
        return left->compile(x64);
    }
};
