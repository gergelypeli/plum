
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


class TreenumCoveringValue: public GenericOperationValue {
public:
    TreenumCoveringValue(OperationType o, Value *p, TypeMatch &match)
        :GenericOperationValue(o, match[1], BOOLEAN_TS, p) {
    }
    
    virtual Storage compile(X64 *x64) {
        subcompile(x64);
        
        TreenumerationType *t = dynamic_cast<TreenumerationType *>(left->ts.rvalue()[0]);
        x64->op(LEARIP, RBX, t->get_tails_label(x64));  // table start
        
        switch (ls.where) {
        case CONSTANT:
            x64->op(MOVQ, reg, ls.value);
            break;
        case REGISTER:
            x64->op(ANDQ, ls.reg, 255);
            if (reg != ls.reg)
                throw INTERNAL_ERROR;
            break;
        case MEMORY:
            x64->op(MOVB, reg, ls.address);
            x64->op(ANDQ, reg, 255);
            break;
        default:
            throw INTERNAL_ERROR;
        }
        
        x64->op(MOVB, BL, Address(RBX, reg, 0));  // BL is the tail
        
        switch (rs.where) {
        case CONSTANT:
            x64->op(CMPB, reg, rs.value);
            x64->op(SETBE, reg);
            x64->op(CMPB, BL, rs.value);
            x64->op(SETAE, BL);
            break;
        case REGISTER:
            x64->op(CMPB, reg, rs.reg);
            x64->op(SETBE, reg);
            x64->op(CMPB, BL, rs.reg);
            x64->op(SETAE, BL);
            break;
        case MEMORY:
            x64->op(CMPB, reg, rs.address);
            x64->op(SETBE, reg);
            x64->op(CMPB, BL, rs.address);
            x64->op(SETAE, BL);
            break;
        default:
            throw INTERNAL_ERROR;
        }
        
        x64->op(ANDB, reg, BL);
        
        return Storage(FLAGS, SETNE);
    }
};
