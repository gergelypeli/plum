
class Cc {
public:
    virtual void absolute_label(Label c, unsigned64 value, unsigned size = 0) =0;

    virtual void data_align(int bytes) =0;
    virtual void data_blob(void *blob, int length) =0;
    virtual void data_byte(char x) =0;
    virtual void data_word(int16 x) =0;
    virtual void data_dword(int x) =0;
    virtual void data_qword(int64 x) =0;
    virtual void data_zstring(std::string s) =0;
    virtual void data_double(double x) =0;
    virtual void data_label(Label c, unsigned size = 0) =0;
    virtual void data_label_local(Label c, std::string name, unsigned size = 0) =0;
    virtual void data_label_global(Label c, std::string name, unsigned size = 0) =0;

    virtual void code_label(Label c, unsigned size = 0) =0;
    virtual void code_label_import(Label c, std::string name) =0;
    virtual void code_label_local(Label c, std::string name, unsigned size = 0) =0;
    virtual void code_label_global(Label c, std::string name, unsigned size = 0) =0;

    virtual int get_pc() =0;
    virtual int get_dc() =0;

    virtual void op(SimpleOp opcode) =0;
    virtual void op(UnaryOp opcode, Register x) =0;
    virtual void op(UnaryOp opcode, Address x) =0;
    virtual void op(PortOp opcode) =0;
    virtual void op(PortOp opcode, int x) =0;
    virtual void op(StringOp opcode) =0;
    virtual void op(BinaryOp opcode, Register x, int y) =0;
    virtual void op(BinaryOp opcode, Address x, int y) =0;
    virtual void op(BinaryOp opcode, Register x, Register y) =0;
    virtual void op(BinaryOp opcode, Register x, HighByteRegister y) =0;
    virtual void op(BinaryOp opcode, Address x, Register y) =0;
    virtual void op(BinaryOp opcode, Register x, Address y) =0;
    virtual void op(MovabsOp opcode, Register x, int64 y) =0;  // 64-bit immediate capable
    virtual void op(ShiftOp opcode, Register x, Register cl) =0;
    virtual void op(ShiftOp opcode, Address x, Register cl) =0;
    virtual void op(ShiftOp opcode, Register x, char y) =0;
    virtual void op(ShiftOp opcode, Address x, char y) =0;
    virtual void op(ExchangeOp opcode, Register x, Register y) =0;
    virtual void op(ExchangeOp opcode, Address x, Register y) =0;
    virtual void op(ExchangeOp opcode, Register x, Address y) =0;
    virtual void op(StackOp opcode, int x) =0;
    virtual void op(StackOp opcode, Register x) =0;
    virtual void op(StackOp opcode, Address x) =0;
    virtual void op(MemoryOp opcode, Address x) =0;
    virtual void op(RegisterFirstOp opcode, Register x, Register y) =0;
    virtual void op(RegisterFirstOp opcode, Register x, Address y) =0;
    virtual void op(Imul3Op opcode, Register x, Register y, int z) =0;
    virtual void op(Imul3Op opcode, Register x, Address y, int z) =0;
    virtual void op(RegisterMemoryOp opcode, Register x, Address y) =0;
    virtual void op(BitSetOp, Register x) =0;
    virtual void op(BitSetOp, HighByteRegister x) =0;
    virtual void op(BitSetOp, Address x) =0;
    virtual void op(BranchOp opcode, Label c) =0;
    virtual void op(JumpOp opcode, Label c) =0;
    virtual void op(JumpOp opcode, Address x) =0;
    virtual void op(JumpOp opcode, Register x) =0;
    virtual void op(ConstantOp opcode, int x) =0;
    
    virtual void op(SsememSsememOp opcode, SseRegister x, SseRegister y) =0;
    virtual void op(SsememSsememOp opcode, SseRegister x, Address y) =0;
    virtual void op(SsememSsememOp opcode, Address x, SseRegister y) =0;
    
    virtual void op(SseSsememOp opcode, SseRegister x, SseRegister y) =0;
    virtual void op(SseSsememOp opcode, SseRegister x, Address y) =0;

    virtual void op(SseGprmemOp opcode, SseRegister x, Register y) =0;
    virtual void op(SseGprmemOp opcode, SseRegister x, Address y) =0;

    virtual void op(GprSsememOp opcode, Register x, SseRegister y) =0;
    virtual void op(GprSsememOp opcode, Register x, Address y) =0;
};
