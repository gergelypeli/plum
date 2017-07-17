
// This class must only return 32-bit constants as CONSTANT storage class,
// because that is what we can treat as immediate value in instructions.
// If a greater value is specified, it must be loaded into a register, and
// returned as REGISTER.

class NumberValue: public Value {
public:
    int number;
    
    NumberValue(std::string t)
        :Value(type(t)) {
        number = std::stoi(t);
    }

    static TypeSpec type(std::string &s) {
        TypeSpec ts;
        
        ts.push_back(
            ends_with(s, "s32") ? integer32_type :
            ends_with(s, "s16") ? integer16_type :
            ends_with(s, "s8") ? integer8_type :
            ends_with(s, "u32") ? unsigned_integer32_type :
            ends_with(s, "u16") ? unsigned_integer16_type :
            ends_with(s, "u8") ? unsigned_integer8_type :
            ends_with(s, "u") ? unsigned_integer_type :
            integer_type
        );
        
        return ts;
    }

    virtual Regs precompile(Regs regs) {
        return regs;
    }

    virtual Storage compile(X64 *) {
        return Storage(CONSTANT, number);
    }
};


class StringValue: public Value {
public:
    std::string text;
    Register reg;
    
    StringValue(std::string t)
        :Value(UNSIGNED_INTEGER8_ARRAY_TS) {
        text = t;
    }

    virtual Regs precompile(Regs regs) {
        reg = regs.remove_any();
        return regs;
    }

    virtual Storage compile(X64 *x64) {
        Label l;
        l.allocate();
        
        x64->data_label(l);
        x64->data_qword(text.size());
        for (char &c : text)
            x64->data_byte(c);

        x64->op(LEARIP, reg, l, 0);
        return Storage(REGISTER, reg);
    }
};
