
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
        TypeSpec ts = {
            ends_with(s, "s32") ? integer32_type :
            ends_with(s, "s16") ? integer16_type :
            ends_with(s, "s8") ? integer8_type :
            ends_with(s, "u32") ? unsigned_integer32_type :
            ends_with(s, "u16") ? unsigned_integer16_type :
            ends_with(s, "u8") ? unsigned_integer8_type :
            ends_with(s, "u") ? unsigned_integer_type :
            integer_type
        };
        
        return ts;
    }

    virtual Regs precompile(Regs) {
        return Regs();
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
        :Value(CHARACTER_ARRAY_REFERENCE_TS) {
        text = t;
    }

    virtual Regs precompile(Regs) {
        return Regs().add(RAX).add(RCX).add(RSI).add(RDI);
    }

    virtual Storage compile(X64 *x64) {
        int bytelen = text.size();
        std::vector<unsigned short> characters;
        characters.resize(bytelen);
        int charlen = decode_utf8_raw(text.data(), bytelen, characters.data());
        characters.resize(charlen);
        int size = 2;

        Label l;
        
        x64->data_label(l);
        for (unsigned short &c : characters)
            x64->data_word(c);

        x64->op(MOVQ, RAX, charlen * size + ARRAY_HEADER_SIZE);
        
        x64->alloc();

        x64->op(MOVQ, Address(RAX, ARRAY_RESERVATION_OFFSET), charlen);
        x64->op(MOVQ, Address(RAX, ARRAY_LENGTH_OFFSET), charlen);
        
        x64->op(LEA, RDI, Address(RAX, ARRAY_ITEMS_OFFSET));
        x64->op(LEARIP, RSI, l, 0);
        x64->op(MOVQ, RCX, charlen * size);
        x64->op(REPMOVSB);
        
        return Storage(REGISTER, RAX);
    }
};
