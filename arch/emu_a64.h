
class Emu_A64: public virtual Emu, public virtual Referrer_A64 {
public:
    struct Addressing {
        enum Mode {
            OFFSET_SCALED, OFFSET_UNSCALED, OFFSET_REGISTER, OFFSET_REGISTER_SHIFTED
        } mode;
        Register base;
        Register index;
        int imm;
    };

    enum Ref_type {
        REF_CODE_JUMP, REF_CODE_BRANCH, REF_CODE_ADR, REF_DATA_ABSOLUTE
    };

    struct Ref {
        Ref_type type;
        unsigned64 location;
        unsigned def_index;
        int addend;
    };

    std::vector<Ref> refs;

    Elf_A64 *elf_a64;
    Asm_A64 *asm_a64;
    
    Emu_A64(std::string module_name);

    virtual void process_relocations();
    virtual std::array<Register, 4> abi_arg_regs();
    virtual std::array<SseRegister, 4> abi_arg_sses();
    virtual std::array<Register, 2> abi_res_regs();
    virtual std::array<SseRegister, 2> abi_res_sses();
    virtual void prologue();
    virtual void epilogue();
    virtual void welcome();
    virtual void goodbye();
    virtual int dwarf_register_number(Register r);
    virtual int dwarf_sseregister_number(SseRegister s);
    virtual int dwarf_retaddr_number();

    virtual void add_ref(Ref r);
    virtual void data_reference(Label label, int addend = 0);
    virtual void code_jump_reference(Label label, int addend = 0);
    virtual void code_branch_reference(Label label, int addend = 0);
    virtual void code_adr_reference(Label label, int addend = 0);

    virtual A::MemOpcode ldrs(int os);
    virtual A::MemOpcode ldru(int os);
    virtual A::MemOpcode str(int os);
    virtual void movimm(Register x, int64 y);
    virtual void pushq(Register r);
    virtual void popq(Register r);
    virtual void lea(Register x, Address y);

    virtual Addressing prepare(int os, Address a);
    virtual void mem(A::MemOpcode opcode, Register rt, Addressing a);

    virtual void op(SimpleOp opcode);
    virtual void op(UnaryOp opcode, Register x);
    virtual void op(UnaryOp opcode, Address x);
    virtual void op(StringOp opcode);
    virtual void op(BinaryOp opcode, Register x, int y);
    virtual void op(BinaryOp opcode, Address x, int y);
    virtual void op(BinaryOp opcode, Register x, Register y);
    virtual void op(BinaryOp opcode, Address x, Register y);
    virtual void op(BinaryOp opcode, Register x, Address y);
    virtual void op(MovabsOp opcode, Register x, int64 y);  // 64-bit immediate capable
    virtual void op(ShiftOp opcode, Register x, Register y);
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
    
    virtual void op(DivModOp opcode, Register x, Register y);

    virtual void floatcmp(ConditionCode cc, SseRegister x, SseRegister y);
    virtual void floatorder(SseRegister x, SseRegister y);
};
