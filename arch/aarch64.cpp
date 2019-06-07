#include "../plum.h"


// Elf

class Elf_A64: public Elf {
public:
    Elf_A64(std::string module_name);

    virtual Elf64_Half get_machine();
    virtual void info_relocation32(unsigned index, Elf64_Addr location, int addend);
    virtual void info_relocation64(unsigned index, Elf64_Addr location, int addend);
    virtual void line_relocation64(unsigned index, Elf64_Addr location, int addend);
};


Elf_A64::Elf_A64(std::string module_name)
    :Elf(module_name) {
}


Elf64_Half Elf_A64::get_machine() {
    return EM_AARCH64;
}


void Elf_A64::info_relocation32(unsigned index, Elf64_Addr location, int addend) {
    add_relocation(index, location, addend, R_AARCH64_ABS32, info_relocations);
}


void Elf_A64::info_relocation64(unsigned index, Elf64_Addr location, int addend) {
    add_relocation(index, location, addend, R_AARCH64_ABS64, info_relocations);
}


void Elf_A64::line_relocation64(unsigned index, Elf64_Addr location, int addend) {
    add_relocation(index, location, addend, R_AARCH64_ABS64, line_relocations);
}


// Opcodes

enum Lsl {
    LSL_0 = 0,
    LSL_16 = 1,
    LSL_32 = 2,
    LSL_48 = 3
};

enum MovImmOpcode {
    MOVZ, MOVN, MOVK
};

enum PairOpcode {
    LDP, STP
};

enum MemOpcode {
    LDRQ,
    
    LDRUW,
    LDRUH,
    LDRUB,
    
    LDRSW,
    LDRSH,
    LDRSB,
    
    STRQ,
    STRW,
    STRH,
    STRB
};


// Asm_A64

class Asm_A64: public Asm {
public:
    Elf_A64 *elf_a64;

    Asm_A64(Elf_A64 *e);

    void done(std::string filename);
    
    int uimm16(int imm);
    int simm9(int imm);
    int simm7(int imm, int size);
    int uimm12(int imm, int size);

    void op(unsigned op);

    void op(MovImmOpcode opcode, Register rd, int imm, Lsl hw);
    //void op(MovImmOpcode, Register rd, Register rm);

    void op(PairOpcode opcode, Register r1, Register r2, Register rn, int imm);
    void op(PairOpcode opcode, Register r1, Register r2, Register rn, int imm, bool post);

    void op(MemOpcode opcode, Register rt, Register rn, int imm);
    void op(MemOpcode opcode, Register rt, Register rn, int imm, bool post);
    void op(MemOpcode opcode, Register rt, Register rn, Register rm);
};


Asm_A64::Asm_A64(Elf_A64 *e) {
    elf_a64 = e;
}


void Asm_A64::done(std::string filename) {
    //relocate();
    
    elf_a64->set_code(code);
    elf_a64->set_data(data);
    
    elf_a64->done(filename);
    
    delete elf_a64;
}


int Asm_A64::uimm16(int imm) {
    if (imm >= -32768 && imm <= 32767)
        return imm & 0xffff;
    else
        throw ASM_ERROR;
}

int Asm_A64::simm9(int imm) {
    if (imm >= -256 && imm <= 255)
        return imm & 0x1ff;
    else
        throw ASM_ERROR;
}

int Asm_A64::simm7(int imm, int size) {
    if (imm % size == 0 && imm / size >= -64 && imm / size <= 63)
        return (imm / size) & 0x7f;
    else
        throw ASM_ERROR;
}

int Asm_A64::uimm12(int imm, int size) {
    if (imm % size == 0 && imm / size >= 0 && imm / size <= 4095)
        return (imm / size) & 0xfff;
    else
        throw ASM_ERROR;
}

void Asm_A64::op(unsigned opcode) {
    // NOTE: host must be little endian, as Aarch64 is, too
    
    code_dword(opcode);  
}


struct {
    unsigned op9;
} movimm_info[] = {
    0b110100101,
    0b100100101,
    0b111100101
};

void Asm_A64::op(MovImmOpcode opcode, Register rd, int imm, Lsl hw) {
    op(movimm_info[opcode].op9 << 23 | hw << 21 | uimm16(imm) << 5 | rd << 0);
}


//void Asm_A64::op(MovImmOpcode, Register rd, Register rm) {
//    op(ORR, rd, XZR, rm, 0);
//}


struct {
    unsigned op10;
} pair_info[] = {
    0b1010100001,  // LDP
    0b1010100000   // STP
};

void Asm_A64::op(PairOpcode opcode, Register r1, Register r2, Register rn, int imm) {
    int op10 = pair_info[opcode].op10 | 0b10 << 1;
    
    op(op10 << 22 | simm7(imm, 8) << 15 | r2 << 10 | rn << 5 | r1 << 0);
}

void Asm_A64::op(PairOpcode opcode, Register r1, Register r2, Register rn, int imm, bool post) {
    int op10 = pair_info[opcode].op10 | (post ? 0b01 : 0b11) << 1;
    
    op(op10 << 22 | simm7(imm, 8) << 15 | r2 << 10 | rn << 5 | r1 << 0);
}


struct {
    unsigned op10;
} mem_info[] = {
    0b1111100001,  // LDR
    0b1011100001,  // LDRUW
    0b0111100001,
    0b0011100001,
    0b1011100010,  // LDRSW
    0b0111100010,
    0b0011100010,
    0b1111100000,  // STR
    0b1011100000,  // STRUW
    0b0111100000,
    0b0011100000,
};

enum {
    MEM_NORMAL = 0b00,
    MEM_POSTINDEX = 0b01,
    MEM_UNPRIVILEGED = 0b10,
    MEM_PREINDEX = 0b11
};

/*
struct {
    unsigned op10;
    unsigned op2;
} ldrimm_info[] = {
    { 0b1111100101, 0 },  // LDR
    { 0b1111100001, 0b11 },
    { 0b1111100001, 0b01 },

    { 0b1011100101, 0 },  // LDRUW
    { 0b1011100001, 0b11 },
    { 0b1011100001, 0b01 },

    { 0b0111100101, 0 },  // LDRUH
    { 0b0111100001, 0b11 },
    { 0b0111100001, 0b01 },
    
    { 0b0011100101, 0 },  // LDRUB
    { 0b0011100001, 0b11 },
    { 0b0011100001, 0b01 },
    
    { 0b1011100110, 0 },  // LDRSW
    { 0b1011100010, 0b11 },
    { 0b1011100010, 0b01 },
    
    { 0b0111100110, 0 },  // LDRSH
    { 0b0111100010, 0b11 },
    { 0b0111100010, 0b01 },
    
    { 0b0011100110, 0 },  // LDRSB
    { 0b0011100010, 0b11 },
    { 0b0011100010, 0b01 }
}
*/
void Asm_A64::op(MemOpcode opcode, Register rt, Register rn, int imm) {
    int op10 = mem_info[opcode].op10;
    int imm12;
    int size = 1 << (op10 >> 8);
    
    if (imm >= 0 && imm % size == 0) {
        // Use the scaled unsigned immediate version
        op10 |= 0b100;
        imm12 = uimm12(imm, size);
    }
    else {
        // Use the unscaled signed immediate version
        imm12 = 0 | simm9(imm) << 2 | MEM_NORMAL;
    }
    
    op(op10 << 22 | imm12 << 10 | rn << 5 | rt << 0);
}

void Asm_A64::op(MemOpcode opcode, Register rt, Register rn, int imm, bool post) {
    int op10 = mem_info[opcode].op10;
    int imm12 = 0 | simm9(imm) << 2 | (post ? MEM_POSTINDEX : MEM_PREINDEX);
    
    op(op10 << 22 | imm12 << 10 | rn << 5 | rt << 0);
}

void Asm_A64::op(MemOpcode opcode, Register rt, Register rn, Register rm) {
    int op10 = mem_info[opcode].op10;
    int option = 0b011;  // LSL
    int scale = 0b0;

    op(op10 << 22 | 0b1 << 21 | rm << 16 | option << 13 | scale << 12 | 0b10 << 10 | rn << 5 | rt << 0);
}





/*
void Asm_A64::movz(Register rd, int imm16, Lsl hw) {
    op(0b110100101 << 23 | hw << 21 | imm16 << 5 | rd << 0);
}


void Asm_A64::movn(Register rd, int imm16, Lsl hw) {
    op(0b100100101 << 23 | hw << 21 | imm16 << 5 | rd << 0);
}


void Asm_A64::movk(Register rd, int imm16, Lsl hw) {
    op(0b111100101 << 23 | hw << 21 | imm16 << 5 | rd << 0);
}
*/



class A64: public Asm_A64 {
public:
    A64(std::string module_name)
        :Asm_A64(new Elf_A64(module_name)) {
    }
    
    virtual bool is_accounting() {
        return false;
    }
    
    virtual void adjust_stack_usage(int mod) {
    }

    // FIXME
    using Asm_A64::op;
    
    virtual void op(SimpleOp opcode) {};
    virtual void op(UnaryOp opcode, Register x) {};
    virtual void op(UnaryOp opcode, Address x) {};
    virtual void op(PortOp opcode) {};
    virtual void op(PortOp opcode, int x) {};
    virtual void op(StringOp opcode) {};
    virtual void op(BinaryOp opcode, Register x, int y) {};
    virtual void op(BinaryOp opcode, Address x, int y) {};
    virtual void op(BinaryOp opcode, Register x, Register y) {};
    virtual void op(BinaryOp opcode, Register x, HighByteRegister y) {};
    virtual void op(BinaryOp opcode, Address x, Register y) {};
    virtual void op(BinaryOp opcode, Register x, Address y) {};
    virtual void op(MovabsOp opcode, Register x, int64 y) {};  // 64-bit immediate capable
    virtual void op(ShiftOp opcode, Register x, Register cl) {};
    virtual void op(ShiftOp opcode, Address x, Register cl) {};
    virtual void op(ShiftOp opcode, Register x, char y) {};
    virtual void op(ShiftOp opcode, Address x, char y) {};
    virtual void op(ExchangeOp opcode, Register x, Register y) {};
    virtual void op(ExchangeOp opcode, Address x, Register y) {};
    virtual void op(ExchangeOp opcode, Register x, Address y) {};
    virtual void op(StackOp opcode, int x) {};
    virtual void op(StackOp opcode, Register x) {};
    virtual void op(StackOp opcode, Address x) {};
    virtual void op(MemoryOp opcode, Address x) {};
    virtual void op(RegisterFirstOp opcode, Register x, Register y) {};
    virtual void op(RegisterFirstOp opcode, Register x, Address y) {};
    virtual void op(Imul3Op opcode, Register x, Register y, int z) {};
    virtual void op(Imul3Op opcode, Register x, Address y, int z) {};
    virtual void op(RegisterMemoryOp opcode, Register x, Address y) {};
    virtual void op(BitSetOp, Register x) {};
    virtual void op(BitSetOp, HighByteRegister x) {};
    virtual void op(BitSetOp, Address x) {};
    virtual void op(BranchOp opcode, Label c) {};
    virtual void op(JumpOp opcode, Label c) {};
    virtual void op(JumpOp opcode, Address x) {};
    virtual void op(JumpOp opcode, Register x) {};
    virtual void op(ConstantOp opcode, int x) {};
    
    virtual void op(SsememSsememOp opcode, SseRegister x, SseRegister y) {};
    virtual void op(SsememSsememOp opcode, SseRegister x, Address y) {};
    virtual void op(SsememSsememOp opcode, Address x, SseRegister y) {};
    
    virtual void op(SseSsememOp opcode, SseRegister x, SseRegister y) {};
    virtual void op(SseSsememOp opcode, SseRegister x, Address y) {};

    virtual void op(SseGprmemOp opcode, SseRegister x, Register y) {};
    virtual void op(SseGprmemOp opcode, SseRegister x, Address y) {};

    virtual void op(GprSsememOp opcode, Register x, SseRegister y) {};
    virtual void op(GprSsememOp opcode, Register x, Address y) {};
};


int main() {
    A64 *a64 = new A64("aatest");
    
    a64->op(MOVZ, R10, 0x1234, LSL_16);
    a64->op(MOVK, R10, 0x1234, LSL_16);
    a64->op(MOVN, R10, 0x1234, LSL_16);
    
    a64->op(LDP, R10, R11, R12, 0x18);
    a64->op(LDP, R10, R11, R12, 0x18, false);
    a64->op(LDP, R10, R11, R12, 0x18, true);

    a64->op(STP, R10, R11, R12, 0x18);
    a64->op(STP, R10, R11, R12, 0x18, false);
    a64->op(STP, R10, R11, R12, 0x18, true);

    a64->op(LDRQ, R10, R12, 0x18);
    a64->op(LDRQ, R10, R12, 0x18, false);
    a64->op(LDRQ, R10, R12, 0x18, true);
    a64->op(LDRQ, R10, R12, R13);

    a64->op(LDRUW, R10, R12, 0x18);
    a64->op(LDRUH, R10, R12, 0x18);
    a64->op(LDRUB, R10, R12, 0x18);

    a64->op(LDRSW, R10, R12, 0x18);
    a64->op(LDRSH, R10, R12, 0x18);
    a64->op(LDRSB, R10, R12, 0x18);

    a64->op(STRQ, R10, R12, 0x19);
    a64->op(STRW, R10, R12, 0x19);
    a64->op(STRH, R10, R12, 0x19);
    a64->op(STRB, R10, R12, 0x19);
    
    a64->done("aatest.o");
    
    return 0;
}
