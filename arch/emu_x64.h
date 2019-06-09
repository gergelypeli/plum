
class Emu_X64: public Emu, public Referrer_X64 {
public:
    enum Ref_type {
        REF_CODE_SHORT,
        REF_CODE_RELATIVE,
        REF_DATA_ABSOLUTE,
    };
    
    struct Ref {
        Ref_type type;
        unsigned64 location;
        unsigned def_index;
        int addend;
    };

    std::vector<Ref> refs;

    Elf_X64 *elf_x64;
    Asm_X64 *asm_x64;
    
    Emu_X64(std::string module_name);

    void data_reference(Label label, int addend = 0);
    void code_reference(Label label, int addend = 0);

    virtual void process_relocations();
    
    virtual void op(SimpleOp opcode);
    virtual void op(UnaryOp opcode, Register x);
    virtual void op(UnaryOp opcode, Address x);
    virtual void op(StringOp opcode);
    virtual void op(BinaryOp opcode, Register x, int y);
    virtual void op(BinaryOp opcode, Address x, int y);
    virtual void op(BinaryOp opcode, Register x, Register y);
    virtual void op(BinaryOp opcode, Register x, HighByteRegister y);
    virtual void op(BinaryOp opcode, Address x, Register y);
    virtual void op(BinaryOp opcode, Register x, Address y);
    virtual void op(MovabsOp opcode, Register x, int64 y);  // 64-bit immediate capable
    virtual void op(ShiftOp opcode, Register x, Register cl);
    virtual void op(ShiftOp opcode, Address x, Register cl);
    virtual void op(ShiftOp opcode, Register x, char y);
    virtual void op(ShiftOp opcode, Address x, char y);
    virtual void op(ExchangeOp opcode, Register x, Register y);
    virtual void op(ExchangeOp opcode, Address x, Register y);
    virtual void op(ExchangeOp opcode, Register x, Address y);
    virtual void op(StackOp opcode, int x);
    virtual void op(StackOp opcode, Register x);
    virtual void op(StackOp opcode, Address x);
    virtual void op(RegisterFirstOp opcode, Register x, Register y);
    virtual void op(RegisterFirstOp opcode, Register x, Address y);
    virtual void op(Imul3Op opcode, Register x, Register y, int z);
    virtual void op(Imul3Op opcode, Register x, Address y, int z);
    virtual void op(RegisterMemoryOp opcode, Register x, Address y);
    virtual void op(BitSetOp, Register x);
    virtual void op(BitSetOp, HighByteRegister x);
    virtual void op(BitSetOp, Address x);
    virtual void op(BranchOp opcode, Label c);
    virtual void op(JumpOp opcode, Label c);
    virtual void op(JumpOp opcode, Address x);
    virtual void op(JumpOp opcode, Register x);
    
    virtual void op(SsememSsememOp opcode, SseRegister x, SseRegister y);
    virtual void op(SsememSsememOp opcode, SseRegister x, Address y);
    virtual void op(SsememSsememOp opcode, Address x, SseRegister y);
    
    virtual void op(SseSsememOp opcode, SseRegister x, SseRegister y);
    virtual void op(SseSsememOp opcode, SseRegister x, Address y);

    virtual void op(SseGprmemOp opcode, SseRegister x, Register y);
    virtual void op(SseGprmemOp opcode, SseRegister x, Address y);

    virtual void op(GprSsememOp opcode, Register x, SseRegister y);
    virtual void op(GprSsememOp opcode, Register x, Address y);
};
