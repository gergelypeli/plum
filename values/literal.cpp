
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


class StringLiteralValue: public Value {
public:
    std::string text;
    Register reg;
    
    StringLiteralValue(std::string t)
        :Value(CHARACTER_ARRAY_REFERENCE_TS) {
        text = t;
    }

    virtual Regs precompile(Regs preferred) {
        reg = preferred.get_gpr();
        return Regs().add(reg);
        //return Regs().add(RAX).add(RCX).add(RSI).add(RDI);
    }

    virtual Storage compile(X64 *x64) {
        int bytelen = text.size();
        std::vector<unsigned short> characters;
        characters.resize(bytelen);
        int charlen = decode_utf8_buffer(text.data(), bytelen, characters.data());
        characters.resize(charlen);
        //int size = 2;

        // Static headers with a refcount of 2
        // Not yet usable, since some of our code expects Character Array-s to be reallocable
        x64->data_heap_header();
        Label l;
        x64->data_label(l);
        x64->data_qword(charlen);
        x64->data_qword(charlen);

        for (unsigned short &c : characters)
            x64->data_word(c);
        
        x64->op(LEARIP, reg, l, 0);
        
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
        x64->alloc_array_RAX(2);
        
        return Storage(REGISTER, RAX);
    }
};


class StringStreamificationValue: public GenericOperationValue {
public:

    StringStreamificationValue(OperationType o, Value *p, TypeMatch &match)
        :GenericOperationValue(o, CHARACTER_ARRAY_REFERENCE_LVALUE_TS, VOID_TS, p) {
    }
    
    virtual void force_arg(Value *r) {
        // Necessary to support string interpolation
        
        right.reset(r);
    }
    
    virtual Regs precompile(Regs preferred) {
        GenericOperationValue::precompile(preferred);
            
        return Regs::all();  // We're Void
    }
    
    virtual Storage compile(X64 *x64) {
        // TODO: don't inline, may just use fixed RDX/RDI
        subcompile(x64);
        
        if (ls.where != REGISTER)
            throw INTERNAL_ERROR;
        
        if (rs.where != MEMORY)
            throw INTERNAL_ERROR;
            
        // RAX - target array, RBX - tmp, RCX - , RDX - source array
        x64->op(MOVQ, RDX, ls.reg);
        x64->op(MOVQ, RAX, rs.address);

        x64->op(MOVQ, RBX, x64->array_length_address(RDX));
        x64->preappend_array_RAX_RBX(2);
        x64->op(MOVQ, rs.address, RAX);  // rs.address is no longer needed, PTRs can be clobbed

        x64->op(LEA, RDI, x64->array_items_address(RAX));
        x64->op(ADDQ, RDI, x64->array_length_address(RAX));
        x64->op(ADDQ, RDI, x64->array_length_address(RAX));  // Yes, added twice

        x64->op(LEA, RSI, x64->array_items_address(RDX));
        x64->op(MOVQ, RCX, x64->array_length_address(RDX));
        x64->op(ADDQ, x64->array_length_address(RAX), RCX);
        x64->op(SHLQ, RCX, 1);
        
        x64->op(REPMOVSB);
        
        x64->decref(RDX);
        
        return Storage();
    }
};
