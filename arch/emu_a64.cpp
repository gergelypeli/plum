#include "../plum.h"


// Zero register / official stack pointer
const Register XZR = (Register)31;

// Link register
const Register XLR = (Register)30;

// Architectural scratch registers
const Register XADDRESS = (Register)29;
const Register XVALUE = (Register)28;


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


A::MemOpcode ldrs(int os) {
    return (
        os % 4 == 0 ? A::LDRSB :
        os % 4 == 1 ? A::LDRSH :
        os % 4 == 2 ? A::LDRSW :
        A::LDRQ
    );
}


A::MemOpcode ldru(int os) {
    return (
        os % 4 == 0 ? A::LDRUB :
        os % 4 == 1 ? A::LDRUH :
        os % 4 == 2 ? A::LDRUW :
        A::LDRQ
    );
}


A::MemOpcode str(int os) {
    return (
        os % 4 == 0 ? A::STRB :
        os % 4 == 1 ? A::STRH :
        os % 4 == 2 ? A::STRW :
        A::STRQ
    );
}


Emu_A64::Addressing Emu_A64::prepare(int os, Address a) {
    if (a.index == NOREG && a.offset >= -256 && a.offset < 256)
        return { Addressing::OFFSET_UNSCALED, a.base, NOREG, a.offset };
        
    if (a.index == NOREG && a.offset >= 0 && (a.offset & ((1 << os) - 1)) == 0 && a.offset >> os < 4096)
        return { Addressing::OFFSET_SCALED, a.base, NOREG, a.offset };
        
    if (a.index != NOREG && a.scale == Address::SCALE_1 && a.offset == 0)
        return { Addressing::OFFSET_REGISTER, a.base, a.index, 0 };
        
    // TODO: this can be more optimal
    Register r = a.base;
    
    if (a.index != NOREG) {
        asm_a64->op(A::ADD, XADDRESS, r, a.index, A::SHIFT_LSL, (int)a.scale);
        r = XADDRESS;
    }

    // We can make this work up to 2*12 bits, that should be enough for us    
    if (a.offset > 0) {
        asm_a64->op(A::ADD, XADDRESS, r, a.offset & 0xfff);
        r = XADDRESS;
        
        if (a.offset >= 4096)
            asm_a64->op(A::ADD, r, r, (a.offset >> 12) & 0xfff, A::SHIFT12_YES);
    }
    else if (a.offset < 0) {
        asm_a64->op(A::SUB, XADDRESS, r, (-a.offset) & 0xfff);
        r = XADDRESS;
        
        if (-a.offset >= 4096)
            asm_a64->op(A::ADD, r, r, ((-a.offset) >> 12) & 0xfff, A::SHIFT12_YES);
    }
    
    return { Addressing::OFFSET_SCALED, r, NOREG, 0 };
}


void Emu_A64::mem(A::MemOpcode opcode, Register rt, Addressing a) {
    switch (a.mode) {
    case Addressing::OFFSET_SCALED:
        asm_a64->op(opcode, rt, a.base, a.imm, A::UNSIGNED_SCALED);
        break;
    case Addressing::OFFSET_UNSCALED:
        asm_a64->op(opcode, rt, a.base, a.imm, A::SIGNED_UNSCALED);
        break;
    case Addressing::OFFSET_REGISTER:
        asm_a64->op(opcode, rt, a.base, a.index);
        break;
    default:
        throw ASM_ERROR;
    }
}


void Emu_A64::op(SimpleOp opcode) {
    switch (opcode) {
    case NOP:
        asm_a64->op(A::NOP);
        break;
    case POPFQ:
        asm_a64->popq(XVALUE);
        asm_a64->op(A::MSRW, A::SPECREG_CONDFLAGS, XVALUE);
        break;
    case PUSHFQ:
        asm_a64->op(A::MSRR, A::SPECREG_CONDFLAGS, XVALUE);
        asm_a64->pushq(XVALUE);
        break;
    case RET:
        asm_a64->op(A::RET, XLR);
        break;
    case UD2:
        asm_a64->op(A::UDF);
        break;
    default:
        throw ASM_ERROR;
    }
}


void Emu_A64::op(UnaryOp opcode, Register x) {
    switch (opcode % 3) {
    case DECQ:
        asm_a64->op(A::SUBS, x, x, 1);
        break;
    case INCQ:
        asm_a64->op(A::ADDS, x, x, 1);
        break;
    case NEGQ:
        asm_a64->op(A::SUBS, x, XZR, x);
        break;
    case NOTQ:
        asm_a64->op(A::ORN, x, XZR, x);
        break;
    default:
        throw ASM_ERROR;
    }
}


void Emu_A64::op(UnaryOp opcode, Address x) {
}


void Emu_A64::op(StringOp opcode) {}
void Emu_A64::op(BinaryOp opcode, Register x, int y) {}
void Emu_A64::op(BinaryOp opcode, Address x, int y) {}
void Emu_A64::op(BinaryOp opcode, Register x, Register y) {}
void Emu_A64::op(BinaryOp opcode, Address x, Register y) {}
void Emu_A64::op(BinaryOp opcode, Register x, Address y) {}
void Emu_A64::op(MovabsOp opcode, Register x, int64 y) {}  // 64-bit immediate capable
void Emu_A64::op(ShiftOp opcode, Register x, Register y) {}
void Emu_A64::op(ShiftOp opcode, Register x, char y) {}
void Emu_A64::op(ShiftOp opcode, Address x, char y) {}
void Emu_A64::op(ExchangeOp opcode, Register x, Register y) {}
void Emu_A64::op(ExchangeOp opcode, Address x, Register y) {}
void Emu_A64::op(ExchangeOp opcode, Register x, Address y) {}
void Emu_A64::op(StackOp opcode, int x) {}
void Emu_A64::op(StackOp opcode, Register x) {}
void Emu_A64::op(StackOp opcode, Address x) {}
void Emu_A64::op(RegisterFirstOp opcode, Register x, Register y) {}
void Emu_A64::op(RegisterFirstOp opcode, Register x, Address y) {}
void Emu_A64::op(Imul3Op opcode, Register x, Register y, int z) {}
void Emu_A64::op(Imul3Op opcode, Register x, Address y, int z) {}
void Emu_A64::op(RegisterMemoryOp opcode, Register x, Address y) {}
void Emu_A64::op(BitSetOp, Register x) {}
void Emu_A64::op(BitSetOp, Address x) {}
void Emu_A64::op(BranchOp opcode, Label c) {}
void Emu_A64::op(JumpOp opcode, Label c) {}
void Emu_A64::op(JumpOp opcode, Address x) {}
void Emu_A64::op(JumpOp opcode, Register x) {}

void Emu_A64::op(SsememSsememOp opcode, SseRegister x, SseRegister y) {}
void Emu_A64::op(SsememSsememOp opcode, SseRegister x, Address y) {}
void Emu_A64::op(SsememSsememOp opcode, Address x, SseRegister y) {}

void Emu_A64::op(SseSsememOp opcode, SseRegister x, SseRegister y) {}
void Emu_A64::op(SseSsememOp opcode, SseRegister x, Address y) {}

void Emu_A64::op(SseGprmemOp opcode, SseRegister x, Register y) {}
void Emu_A64::op(SseGprmemOp opcode, SseRegister x, Address y) {}

void Emu_A64::op(GprSsememOp opcode, Register x, SseRegister y) {}
void Emu_A64::op(GprSsememOp opcode, Register x, Address y) {}

void Emu_A64::op(DivModOp opcode, Register x, Register y) {}
