
class Cc_X64 {
public:
    Elf_X64 *elf_x64;
    Asm_X64 *asm_x64;
    
    Cc_X64(std::string module_name);

    virtual void absolute_label(Label c, unsigned64 value, unsigned size = 0);

    virtual void data_align(int bytes);
    virtual void data_blob(void *blob, int length);
    virtual void data_byte(char x);
    virtual void data_word(int16 x);
    virtual void data_dword(int x);
    virtual void data_qword(int64 x);
    virtual void data_zstring(std::string s);
    virtual void data_double(double x);
    virtual void data_label(Label c, unsigned size = 0);
    virtual void data_label_local(Label c, std::string name, unsigned size = 0);
    virtual void data_label_global(Label c, std::string name, unsigned size = 0);

    virtual void data_reference(Label c, int addend = 0);
    
    virtual void code_label(Label c, unsigned size = 0);
    virtual void code_label_import(Label c, std::string name);
    virtual void code_label_local(Label c, std::string name, unsigned size = 0);
    virtual void code_label_global(Label c, std::string name, unsigned size = 0);

    virtual int get_pc();
    virtual int get_dc();
        
    virtual void done(std::string output);
    
    virtual void op(SimpleOp opcode);
    virtual void op(UnaryOp opcode, Register x);
    virtual void op(UnaryOp opcode, Address x);
    virtual void op(PortOp opcode);
    virtual void op(PortOp opcode, int x);
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
    virtual void op(MemoryOp opcode, Address x);
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
    virtual void op(ConstantOp opcode, int x);
    
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
