#include "../plum.h"


Emu_A64::Emu_A64(std::string module_name) {
    elf_a64 = new Elf_A64(module_name);
    elf = elf_a64;
    
    asm_a64 = new Asm_A64();
    asm_a64->set_referrer_a64(this);
}


void Emu_A64::code_jump_reference(Label label, int addend) {
    if (!label.def_index) {
        std::cerr << "Can't reference an undeclared label!\n";
        throw ASM_ERROR;
    }

    refs.push_back({ REF_CODE_JUMP, get_pc(), label.def_index, addend });
}


void Emu_A64::code_branch_reference(Label label, int addend) {
    if (!label.def_index) {
        std::cerr << "Can't reference an undeclared label!\n";
        throw ASM_ERROR;
    }

    refs.push_back({ REF_CODE_BRANCH, get_pc(), label.def_index, addend });
}


void Emu_A64::process_relocations() {
    // TODO
}


void Emu_A64::op(SimpleOp opcode) {};
void Emu_A64::op(UnaryOp opcode, Register x) {};
void Emu_A64::op(UnaryOp opcode, Address x) {};
void Emu_A64::op(StringOp opcode) {};
void Emu_A64::op(BinaryOp opcode, Register x, int y) {};
void Emu_A64::op(BinaryOp opcode, Address x, int y) {};
void Emu_A64::op(BinaryOp opcode, Register x, Register y) {};
void Emu_A64::op(BinaryOp opcode, Register x, HighByteRegister y) {};
void Emu_A64::op(BinaryOp opcode, Address x, Register y) {};
void Emu_A64::op(BinaryOp opcode, Register x, Address y) {};
void Emu_A64::op(MovabsOp opcode, Register x, int64 y) {};  // 64-bit immediate capable
void Emu_A64::op(ShiftOp opcode, Register x, Register y) {};
void Emu_A64::op(ShiftOp opcode, Register x, char y) {};
void Emu_A64::op(ShiftOp opcode, Address x, char y) {};
void Emu_A64::op(ExchangeOp opcode, Register x, Register y) {};
void Emu_A64::op(ExchangeOp opcode, Address x, Register y) {};
void Emu_A64::op(ExchangeOp opcode, Register x, Address y) {};
void Emu_A64::op(StackOp opcode, int x) {};
void Emu_A64::op(StackOp opcode, Register x) {};
void Emu_A64::op(StackOp opcode, Address x) {};
void Emu_A64::op(RegisterFirstOp opcode, Register x, Register y) {};
void Emu_A64::op(RegisterFirstOp opcode, Register x, Address y) {};
void Emu_A64::op(Imul3Op opcode, Register x, Register y, int z) {};
void Emu_A64::op(Imul3Op opcode, Register x, Address y, int z) {};
void Emu_A64::op(RegisterMemoryOp opcode, Register x, Address y) {};
void Emu_A64::op(BitSetOp, Register x) {};
void Emu_A64::op(BitSetOp, HighByteRegister x) {};
void Emu_A64::op(BitSetOp, Address x) {};
void Emu_A64::op(BranchOp opcode, Label c) {};
void Emu_A64::op(JumpOp opcode, Label c) {};
void Emu_A64::op(JumpOp opcode, Address x) {};
void Emu_A64::op(JumpOp opcode, Register x) {};

void Emu_A64::op(SsememSsememOp opcode, SseRegister x, SseRegister y) {};
void Emu_A64::op(SsememSsememOp opcode, SseRegister x, Address y) {};
void Emu_A64::op(SsememSsememOp opcode, Address x, SseRegister y) {};

void Emu_A64::op(SseSsememOp opcode, SseRegister x, SseRegister y) {};
void Emu_A64::op(SseSsememOp opcode, SseRegister x, Address y) {};

void Emu_A64::op(SseGprmemOp opcode, SseRegister x, Register y) {};
void Emu_A64::op(SseGprmemOp opcode, SseRegister x, Address y) {};

void Emu_A64::op(GprSsememOp opcode, Register x, SseRegister y) {};
void Emu_A64::op(GprSsememOp opcode, Register x, Address y) {};

void Emu_A64::op(DivModOp opcode, Register x, Register y) {};
