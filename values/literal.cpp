
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
            x64->op(MOVABS, reg, number);
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
    std::string text;
    
    StringLiteralValue(std::string t)
        :Value(STRING_TS) {
        text = t;
    }

    virtual Regs precompile(Regs preferred) {
        return Regs(RAX);
        //return Regs().add(RAX).add(RCX).add(RSI).add(RDI);
    }

    virtual Storage compile(X64 *x64) {
        std::vector<unsigned16> characters = decode_utf8(text);
        Label l = x64->runtime->data_heap_string(characters);
        
        x64->op(LEA, RAX, Address(l, 0));
        return Storage(BREGISTER, RAX);
    }
};




class TreenumerationMatcherValue: public GenericValue, public Raiser {
public:
    int index;
    Register reg;
    
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
        Regs clob = left->precompile(preferred);
        reg = clob.has_any() ? clob.get_any() : AL;
        return clob | reg;
    }
    
    virtual Storage compile(X64 *x64) {
        TreenumerationType *t = ptr_cast<TreenumerationType>(left->ts.rvalue()[0]);
        Label parents_label = t->get_parents_label(x64);
        Label loop, cond, match;
        
        ls = left->compile(x64);
        
        switch (ls.where) {
        case CONSTANT:
            x64->op(MOVB, reg, ls.value);
            break;
        case REGISTER:
            reg = ls.reg;
            break;
        case MEMORY:
            x64->op(MOVB, reg, ls.address);
            break;
        default:
            throw INTERNAL_ERROR;
        }
        
        x64->op(LEA, R10, Address(parents_label, 0));
        
        x64->code_label(loop);
        x64->op(CMPB, reg, index);
        x64->op(JE, match);
        
        x64->op(ANDQ, reg, 255);
        x64->op(MOVB, reg, Address(R10, reg, 0));
        
        x64->op(CMPB, reg, 0);
        x64->op(JNE, loop);
        
        raise("UNMATCHED", x64);
        
        x64->code_label(match);
        
        return Storage();
    }
};
