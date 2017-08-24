
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
        if (args.size() == 1 && kwargs.size() == 0) {
            Value *v = typize(args[0].get(), scope, &INTEGER_TS);
            TypeMatch match;
            
            if (!typematch(INTEGER_TS, v, match)) {
                std::cerr << "Integer unicode code point expected!\n";
                return false;
            }
            
            value.reset(v);
            return true;
        }
        else
            return false;
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
    Register reg;
    
    StringLiteralValue(std::string t)
        :Value(CHARACTER_ARRAY_REFERENCE_TS) {
        text = t;
    }

    virtual Regs precompile(Regs preferred) {
        reg = preferred.get_any();
        return Regs().add(reg);
        //return Regs().add(RAX).add(RCX).add(RSI).add(RDI);
    }

    virtual Storage compile(X64 *x64) {
        std::vector<unsigned short> characters = decode_utf8(text);
        Label l = x64->data_heap_string(characters);
        
        x64->op(LEARIP, reg, l);
        x64->incref(reg);  // This way we can return the same static string many times
        
        return Storage(REGISTER, reg);
        
        /*
        // Code to allocate a new buffer and return that
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
        */
    }
};


class StringBufferValue: public Value {
public:
    int length;
    Register reg;
    
    StringBufferValue(int l)
        :Value(CHARACTER_ARRAY_REFERENCE_TS) {
        length = l;
    }

    virtual Regs precompile(Regs preferred) {
        return Regs().add(RAX);
    }

    virtual Storage compile(X64 *x64) {
        x64->op(MOVQ, RAX, length);
        x64->op(MOVQ, RBX, 2);
        x64->alloc_array_RAX_RBX();
        
        return Storage(REGISTER, RAX);
    }
};


class StringStreamificationValue: public GenericValue {
public:
    StringStreamificationValue(Value *p, TypeMatch &match)
        :GenericValue(CHARACTER_ARRAY_REFERENCE_LVALUE_TS, VOID_TS, p) {
    }
    
    virtual Regs precompile(Regs preferred) {
        left->precompile(preferred);
        right->precompile(preferred);
            
        return Regs::all();  // We're Void
    }
    
    virtual Storage compile(X64 *x64) {
        Label ss_label = x64->once(compile_string_streamification);

        compile_and_store_both(x64, Storage(STACK), Storage(ALISTACK));
        
        x64->op(CALL, ss_label);
        
        right->ts.store(rs, Storage(), x64);
        left->ts.store(ls, Storage(), x64);
        
        return Storage();
    }
    
    static void compile_string_streamification(X64 *x64) {
        // RAX - target array, RBX - tmp, RCX - size, RDX - source array, RDI - alias
        x64->op(MOVQ, RDI, Address(RSP, 8));  // alias to the stream reference
        x64->op(MOVQ, RDX, Address(RSP, 16));  // reference to the string
        
        x64->op(MOVQ, RAX, Address(RDI, 0));
        x64->op(MOVQ, RBX, x64->array_length_address(RDX));

        x64->op(MOVQ, RCX, 2);
        x64->preappend_array_RAX_RBX_RCX();
        
        x64->op(MOVQ, Address(RDI, 0), RAX);  // RDI no longer needed

        x64->op(LEA, RDI, x64->array_items_address(RAX));
        x64->op(ADDQ, RDI, x64->array_length_address(RAX));
        x64->op(ADDQ, RDI, x64->array_length_address(RAX));  // Yes, added twice

        x64->op(LEA, RSI, x64->array_items_address(RDX));
        x64->op(MOVQ, RCX, x64->array_length_address(RDX));
        x64->op(ADDQ, x64->array_length_address(RAX), RCX);
        x64->op(SHLQ, RCX, 1);
        
        x64->op(REPMOVSB);
        
        x64->op(RET);
    }
};


class CharacterStreamificationValue: public GenericValue {
public:

    CharacterStreamificationValue(Value *p, TypeMatch &match)
        :GenericValue(CHARACTER_ARRAY_REFERENCE_LVALUE_TS, VOID_TS, p) {
    }
    
    virtual Regs precompile(Regs preferred) {
        left->precompile(preferred);
        right->precompile(preferred);
            
        return Regs::all();  // We're Void
    }
    
    virtual Storage compile(X64 *x64) {
        Label cs_label = x64->once(compile_character_streamification);

        compile_and_store_both(x64, Storage(STACK), Storage(ALISTACK));
        
        x64->op(CALL, cs_label);
        
        right->ts.store(rs, Storage(), x64);
        left->ts.store(ls, Storage(), x64);
        
        return Storage();
    }
    
    static void compile_character_streamification(X64 *x64) {
        // RAX - target array, RBX - tmp, RCX - size, RDX - source character, RDI - alias
        x64->op(MOVQ, RDI, Address(RSP, 8));  // alias to the stream reference
        x64->op(MOVQ, RDX, Address(RSP, 16));  // the character

        x64->op(MOVQ, RAX, Address(RDI, 0));
        x64->op(MOVQ, RBX, 1);
        
        x64->op(MOVQ, RCX, 2);
        x64->preappend_array_RAX_RBX_RCX();
        
        x64->op(MOVQ, Address(RDI, 0), RAX);  // RDI no longer needed

        x64->op(LEA, RDI, x64->array_items_address(RAX));
        x64->op(ADDQ, RDI, x64->array_length_address(RAX));
        x64->op(ADDQ, RDI, x64->array_length_address(RAX));  // Yes, added twice

        x64->op(MOVW, Address(RDI, 0), DX);
            
        x64->op(ADDQ, x64->array_length_address(RAX), 1);

        x64->op(RET);
    }
};


class EnumStreamificationValue: public GenericValue {
public:

    EnumStreamificationValue(Value *p, TypeMatch &match)
        :GenericValue(CHARACTER_ARRAY_REFERENCE_LVALUE_TS, VOID_TS, p) {
    }
    
    virtual Regs precompile(Regs preferred) {
        left->precompile(preferred);
        right->precompile(preferred);
            
        return Regs::all();  // We're Void
    }
    
    virtual Storage compile(X64 *x64) {
        Label es_label = x64->once(compile_enum_streamification);

        compile_and_store_both(x64, Storage(STACK), Storage(ALISTACK));
        
        EnumerationType *t = dynamic_cast<EnumerationType *>(left->ts.rvalue()[0]);
        x64->op(LEARIP, RBX, t->stringifications_label);  // table start
        x64->op(CALL, es_label);
        
        right->ts.store(rs, Storage(), x64);
        left->ts.store(ls, Storage(), x64);
        
        return Storage();
    }
    
    static void compile_enum_streamification(X64 *x64) {
        // RAX - target array, RBX - table start, RCX - size, RDX - source enum, RDI - alias
        x64->op(MOVQ, RDI, Address(RSP, 8));  // alias to the stream reference
        x64->op(MOVQ, RDX, Address(RSP, 16));  // the enum

        // Find the string for this enum value
        x64->op(ANDQ, RDX, 0xFF);
        x64->op(SHLQ, RDX, 2);  // 32-bit relative offsets are stored in our table
        x64->op(ADDQ, RBX, RDX);  // entry start
        x64->op(MOVSXQ, RDX, Address(RBX, 0));  // offset to string
        x64->op(ADDQ, RDX, RBX);  // absolute address of string
            
        x64->op(MOVQ, RAX, Address(RDI, 0));
        x64->op(MOVQ, RBX, x64->array_length_address(RDX));
        x64->op(MOVQ, RCX, 2);
        
        x64->preappend_array_RAX_RBX_RCX();
        
        x64->op(MOVQ, Address(RDI, 0), RAX);  // RDI no longer needed

        x64->op(LEA, RDI, x64->array_items_address(RAX));
        x64->op(ADDQ, RDI, x64->array_length_address(RAX));
        x64->op(ADDQ, RDI, x64->array_length_address(RAX));  // Yes, added twice

        x64->op(LEA, RSI, x64->array_items_address(RDX));
        x64->op(MOVQ, RCX, x64->array_length_address(RDX));
        x64->op(ADDQ, x64->array_length_address(RAX), RCX);
        x64->op(SHLQ, RCX, 1);
        
        x64->op(REPMOVSB);
        
        x64->op(RET);
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
        x64->op(LEARIP, RBX, t->tails_label);  // table start
        
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
