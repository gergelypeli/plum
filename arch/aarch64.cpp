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

namespace A {
enum Lsl {
    LSL_0 = 0,
    LSL_16 = 1,
    LSL_32 = 2,
    LSL_48 = 3
};

enum ShiftDir {
    SHIFT_LSL = 0,
    SHIFT_LSR = 1,
    SHIFT_ASR = 2,
    SHIFT_ROR = 3  // not allowed for some instructions
};

enum Shift12 {
    SHIFT12_NO,
    SHIFT12_YES
};

struct BitMask {
    unsigned imms;  // index of the most significant bit to keep
    unsigned immr;  // number of right rotations (variant depends on the instruction)
    
    // NOTES:
    //   AND, ORR, EOR
    //     use an artificial all-1s input
    //     clear the higher bits
    //     then cyclic rotate to the right, low bits returning of the high side
    //   UBFM
    //     clears the higher bits
    //     logical shifts to the right, inserting 0 high bits
    //     but when all bits would disappear, they appear at the highest bits
    //   SBFM
    //     sign extends the higher bits
    //     arithmetic shifts to the right, inserting sign high bits
    //     but when all bits would disappear, they appear at the highest bits
    //   BFM
    //     ignores the higher bits
    //     shifts to the right, ignoring more high bits
    //     but when all bits would disappear, they appear at the highest bits
    //     only the significant bits are replaced in the destination, the rest is kept
    
    BitMask(unsigned s, unsigned r) {
        if (s >= 64 || r >= 64)
            throw ASM_ERROR;
            
        imms = s;
        immr = r;
    }
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


enum ArithOpcode {
    ADD, ADDS,
    SUB, SUBS,
    AND, ANDS,
    EOR, ORR
};

enum ArithNotOpcode {
    EON, ORN
};

enum MulOpcode {
    MADD
};

enum DivOpcode {
    SDIV, UDIV
};

enum ShiftOpcode {
    ASR, LSL, LSR, ROR
};

enum BitFieldOpcode {
    BFM, SBFM, UBFM
};

enum ExtrOpcode {
    EXTR
};

enum NopOpcode {
    NOP
};

enum JumpOpcode {
    B, BL
};

enum CondCode {
    CC_EQ, CC_NE,  // equal, not
    CC_HS, CC_LO,  // unsigned higher or same, lower
    CC_MI, CC_PL,  // minus, plus
    CC_VS, CC_VC,  // overflow, not
    CC_HI, CC_LS,  // unsigned higher, lower or same
    CC_GE, CC_LT,  // signed greater or equal, less
    CC_GT, CC_LE,  // signed greater, less or equal
    CC_AL, CC_NV   // always, always
};

enum RetOpcode {
    RET
};

enum RegLabelOpcode {
    CBZ, CBNZ
};

enum CondSelOpcode {
    CSEL, CSINC, CSINV, CSNEG
};


}


using namespace A;

// Asm_A64

class Asm_A64: public Asm {
public:
    enum Ref_type {
        REF_CODE_JUMP, REF_CODE_BRANCH
    };

    struct Ref {
        Ref_type type;
        unsigned64 location;
        unsigned def_index;
        int addend;
    };

    std::vector<Ref> refs;

    Elf_A64 *elf_a64;

    Asm_A64(Elf_A64 *e);

    void done(std::string filename);

    void code_reference(Ref_type rt, Label label, int addend = 0);

    int uimm(int imm, int width, int unit = 1);
    int simm(int imm, int width, int unit = 1);

    void code_op(unsigned op);

    void op(MovImmOpcode opcode, Register rd, int imm, Lsl hw);
    //void op(MovImmOpcode, Register rd, Register rm);

    void op(PairOpcode opcode, Register r1, Register r2, Register rn, int imm);
    void op(PairOpcode opcode, Register r1, Register r2, Register rn, int imm, bool post);

    void op(MemOpcode opcode, Register rt, Register rn, int imm);
    void op(MemOpcode opcode, Register rt, Register rn, int imm, bool post);
    void op(MemOpcode opcode, Register rt, Register rn, Register rm);

    void op(ArithOpcode opcode, Register rd, Register rn, int imm, Shift12 shift12 = SHIFT12_NO);
    void op(ArithOpcode opcode, Register rd, Register rn, BitMask bitmask);
    void op(ArithOpcode opcode, Register rd, Register rn, unsigned lowest_bit, unsigned bit_length);
    void op(ArithOpcode opcode, Register rd, Register rn, Register rm, ShiftDir shift_dir = SHIFT_LSL, int shift_amount = 0);

    void op(ArithNotOpcode opcode, Register rd, Register rn, Register rm, ShiftDir shift_dir = SHIFT_LSL, int shift_amount = 0);

    void op(MulOpcode opcode, Register rd, Register rn, Register rm, Register ra);

    void op(DivOpcode opcode, Register rd, Register rn, Register rm);

    void op(ShiftOpcode opcode, Register rd, Register rn, Register rm);

    void op(BitFieldOpcode opcode, Register rd, Register rn, BitMask bitmask);
    void op(ExtrOpcode opcode, Register rd, Register rn, Register rm, int lsb_index);
    
    void op(NopOpcode);
    
    void op(JumpOpcode opcode, Label label);
    void op(JumpOpcode opcode, Register rn);
    void op(JumpOpcode opcode, CondCode cc, Label label);

    void op(RetOpcode opcode, Register rn);

    void op(CondSelOpcode opcode, CondCode cc, Register rd, Register rn, Register rm);
    void op(RegLabelOpcode opcode, Register rn, Label label);
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

void Asm_A64::code_reference(Ref_type rt, Label label, int addend) {
    if (!label.def_index) {
        std::cerr << "Can't reference an undeclared label!\n";
        throw ASM_ERROR;
    }

    refs.push_back({ rt, code.size(), label.def_index, addend });
}

int Asm_A64::uimm(int imm, int width, int unit) {
    if (imm % unit == 0 && imm / unit >= 0 && imm / unit < (1 << width))
        return (imm / unit) & ((1 << width) - 1);
    else
        throw ASM_ERROR;
}


int Asm_A64::simm(int imm, int width, int unit) {
    if (imm % unit == 0 && imm / unit >= (-1 << (width - 1)) && imm / unit < (1 << (width - 1)))
        return (imm / unit) & ((1 << width) - 1);
    else
        throw ASM_ERROR;
}

void Asm_A64::code_op(unsigned opcode) {
    // NOTE: host must be little endian, as Aarch64 is, too
    
    code_dword(opcode);
}


// MovImm

struct {
    unsigned op9;
} movimm_info[] = {
    0b110100101,
    0b100100101,
    0b111100101
};

void Asm_A64::op(MovImmOpcode opcode, Register rd, int imm, Lsl hw) {
    code_op(movimm_info[opcode].op9 << 23 | hw << 21 | uimm(imm, 16) << 5 | rd << 0);
}


//void Asm_A64::op(MovImmOpcode, Register rd, Register rm) {
//    code_op(ORR, rd, XZR, rm, 0);
//}


// Pair

struct {
    unsigned op10;
} pair_info[] = {
    0b1010100001,  // LDP
    0b1010100000   // STP
};

void Asm_A64::op(PairOpcode opcode, Register r1, Register r2, Register rn, int imm) {
    int op10 = pair_info[opcode].op10 | 0b10 << 1;
    
    code_op(op10 << 22 | simm(imm, 7, 8) << 15 | r2 << 10 | rn << 5 | r1 << 0);
}

void Asm_A64::op(PairOpcode opcode, Register r1, Register r2, Register rn, int imm, bool post) {
    int op10 = pair_info[opcode].op10 | (post ? 0b01 : 0b11) << 1;
    
    code_op(op10 << 22 | simm(imm, 7, 8) << 15 | r2 << 10 | rn << 5 | r1 << 0);
}


// Mem

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

void Asm_A64::op(MemOpcode opcode, Register rt, Register rn, int imm) {
    int op10 = mem_info[opcode].op10;
    int imm12;
    int size = 1 << (op10 >> 8);
    
    if (imm >= 0 && imm % size == 0) {
        // Use the scaled unsigned immediate version
        op10 |= 0b100;
        imm12 = uimm(imm, 12, size);
    }
    else {
        // Use the unscaled signed immediate version
        imm12 = 0 | simm(imm, 9) << 2 | MEM_NORMAL;
    }
    
    code_op(op10 << 22 | imm12 << 10 | rn << 5 | rt << 0);
}

void Asm_A64::op(MemOpcode opcode, Register rt, Register rn, int imm, bool post) {
    int op10 = mem_info[opcode].op10;
    int imm12 = 0 | simm(imm, 9) << 2 | (post ? MEM_POSTINDEX : MEM_PREINDEX);
    
    code_op(op10 << 22 | imm12 << 10 | rn << 5 | rt << 0);
}

void Asm_A64::op(MemOpcode opcode, Register rt, Register rn, Register rm) {
    int op10 = mem_info[opcode].op10;
    int option = 0b011;  // LSL
    int scale = 0b0;

    code_op(op10 << 22 | 0b1 << 21 | rm << 16 | option << 13 | scale << 12 | 0b10 << 10 | rn << 5 | rt << 0);
}


// Arith

struct {
    unsigned imm_op8;
    unsigned reg_op8;
} arith_info[] = {
    { 0b10010001, 0b10001011 }, // ADD
    { 0b10110001, 0b10101011 }, // ADDS
    { 0b11010001, 0b11001011 }, // SUB
    { 0b11110001, 0b11101011 }, // SUBS
    { 0b10010010, 0b10001010 }, // AND
    { 0b11110010, 0b11101010 }, // ANDS
    { 0b11010010, 0b11001010 }, // EOR
    { 0b10110010, 0b10101010 }, // ORR
};

static bool is_logical(ArithOpcode opcode) {
    return (opcode == A::AND || opcode == ANDS || opcode == EOR || opcode == ORR);
}

void Asm_A64::op(ArithOpcode opcode, Register rd, Register rn, int imm, Shift12 shift12) {
    if (is_logical(opcode))
        throw ASM_ERROR;  // should use the bitmask operand version instead

    int op8 = arith_info[opcode].imm_op8;
    
    code_op(op8 << 24 | shift12 << 22 | uimm(imm, 12) << 10 | rn << 5 | rd << 0);
}

void Asm_A64::op(ArithOpcode opcode, Register rd, Register rn, BitMask bitmask) {
    if (!is_logical(opcode))
        throw ASM_ERROR;  // should use the numeric operand version instead

    if (bitmask.imms >= 63 || bitmask.immr >= 64)
        throw ASM_ERROR;

    int op8 = arith_info[opcode].imm_op8;
    int n = 0b01;  // we always use 64-bit wide patterns
    
    code_op(op8 << 24 | n << 22 | bitmask.immr << 16 | bitmask.imms << 10 | rn << 5 | rd << 0);
}

void Asm_A64::op(ArithOpcode opcode, Register rd, Register rn, unsigned lowest_bit, unsigned bit_length) {
    if (!is_logical(opcode))
        throw ASM_ERROR;  // should use the numeric operand version instead

    if (lowest_bit >= 64 || bit_length >= 64 || bit_length == 0)
        throw ASM_ERROR;

    op(opcode, rd, rn, BitMask(bit_length - 1, (64 - lowest_bit) % 64));
}

void Asm_A64::op(ArithOpcode opcode, Register rd, Register rn, Register rm, ShiftDir shift_dir, int shift_amount) {
    if (shift_dir == SHIFT_ROR && !is_logical(opcode))
        throw ASM_ERROR;

    int op8 = arith_info[opcode].reg_op8;
    int neg = 0b0;
    
    code_op(op8 << 24 | shift_dir << 22 | neg << 21 | rm << 16 | uimm(shift_amount, 6) << 10 | rn << 5 | rd << 0);
}


struct {
    unsigned reg_op8;
} arithnot_info[] = {
    0b11001010, // EON  (practically EOR)
    0b10101010, // ORN  (practically ORR)
};

void Asm_A64::op(ArithNotOpcode opcode, Register rd, Register rn, Register rm, ShiftDir shift_dir, int shift_amount) {
    int op8 = arithnot_info[opcode].reg_op8;
    int neg = 0b1;
    
    code_op(op8 << 24 | shift_dir << 22 | neg << 21 | rm << 16 | uimm(shift_amount, 6) << 10 | rn << 5 | rd << 0);
}



// Mul

void Asm_A64::op(MulOpcode opcode, Register rd, Register rn, Register rm, Register ra) {
    int op8 = 0b10011011;  // MADD
    
    code_op(op8 << 24 | 0b000 << 21 | rm << 16 | 0b0 << 15 | ra << 10 | rn << 5 | rd << 0);
}


// Div

void Asm_A64::op(DivOpcode opcode, Register rd, Register rn, Register rm) {
    int op11 = 0b10011010110;
    int op6 = (opcode == SDIV ? 0b000011 : 0b000010);
    
    code_op(op11 << 21 | rm << 16 | op6 << 10 | rn << 5 | rd << 0);
}


// Shift (register)

struct {
    unsigned op11;
    unsigned op6;
} shift_info[] = {
    { 0b10011010110, 0b001010 },  // ASR
    { 0b10011010110, 0b001000 },  // LSL
    { 0b10011010110, 0b001001 },  // LSR
    { 0b10011010110, 0b001011 }   // ROR
};

void Asm_A64::op(ShiftOpcode opcode, Register rd, Register rn, Register rm) {
    int op11 = shift_info[opcode].op11;
    int op6 = shift_info[opcode].op6;
    
    code_op(op11 << 21 | rm << 16 | op6 << 10 | rn << 5 | rd << 0);
}


// BitField

struct {
    unsigned imm_op8;
} bitfield_info[] = {
    { 0b10110011 },  // BFM
    { 0b10010011 },  // SBFM
    { 0b11010011 },  // UBFM
};

void Asm_A64::op(BitFieldOpcode opcode, Register rd, Register rn, BitMask bitmask) {
    int op8 = bitfield_info[opcode].imm_op8;
    int op2 = 0b01;
    
    code_op(op8 << 24 | op2 << 22 | bitmask.immr << 16 | bitmask.imms << 10 | rn << 5 | rd << 0);
}

void Asm_A64::op(ExtrOpcode opcode, Register rd, Register rn, Register rm, int lsb_index) {
    int op11 = 0b10010011110;
    int imm6 = uimm(lsb_index, 6);
    
    code_op(op11 << 21 | rm << 16 | imm6 << 10 | rn << 5 | rd << 0);
}


// Nop

void Asm_A64::op(NopOpcode opcode) {
    code_op(0b11010101000000110010000000011111);
}


// Branch

void Asm_A64::op(JumpOpcode opcode, Label label) {
    code_reference(REF_CODE_JUMP, label);  // needs imm26

    int op6 = (opcode == B ? 0b000101 : opcode == A::BL ? 0b100101 : throw ASM_ERROR);

    code_op(op6 << 26);
}

void Asm_A64::op(JumpOpcode opcode, Register rn) {
    int op16 = (opcode == B ? 0b1101011000011111 : opcode == A::BL ? 0b1101011000111111 : throw ASM_ERROR);

    code_op(op16 << 16 | 0b000000 << 10 | rn << 5 | 0b00000 << 0);
}

void Asm_A64::op(JumpOpcode opcode, CondCode cc, Label label) {
    if (opcode != B)
        throw ASM_ERROR;
        
    code_reference(REF_CODE_BRANCH, label);  // needs imm19

    int op8 = 0b01010100;

    code_op(op8 << 24 | 0b0 << 4 | cc);
}


// Ret

void Asm_A64::op(RetOpcode opcode, Register rn) {
    int op22 = 0b1101011001011111000000;
    int op5 = 0b00000;

    code_op(op22 << 10 | rn << 5 | op5 << 0);
}


// CondSel

struct {
    unsigned op11;
    unsigned op2;
} condsel_info[] = {
    { 0b10011010100, 0b00 },  // CSEL
    { 0b10011010100, 0b01 },  // CSINC
    { 0b11011010100, 0b00 },  // CSINV
    { 0b11011010100, 0b01 },  // CSNEG
};

void Asm_A64::op(CondSelOpcode opcode, CondCode cc, Register rd, Register rn, Register rm) {
    int op11 = condsel_info[opcode].op11;
    int op2 = condsel_info[opcode].op2;
    
    code_op(op11 << 21 | rm << 16 | cc << 12 | op2 << 10 | rn << 5 | rd << 0);
}

void Asm_A64::op(RegLabelOpcode opcode, Register rn, Label label) {
    code_reference(REF_CODE_BRANCH, label);  // needs imm19
    
    int op8 = (opcode == CBZ ? 0b10110100 : opcode == CBNZ ? 0b10110101 : throw ASM_ERROR);
    
    code_op(op8 << 24 | rn << 0);
}




// NOTES:
//   CMP A, B => SUBS XZR, A, B
//   TST A, B => ANDS XZR, A, B
//   MOV A, B => ORR A, B, XZR


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
    
    a64->op(A::ADD, R10, R11, 100);
    a64->op(ADDS, R10, R11, 100);
    a64->op(SUB, R10, R11, 100);
    a64->op(SUBS, R10, R11, 100);
    
    a64->op(A::ADD, R10, R11, R12, SHIFT_LSL, 0);
    a64->op(ADDS, R10, R11, R12, SHIFT_LSL, 10);
    a64->op(SUB, R10, R11, R12, SHIFT_LSR, 10);
    a64->op(SUBS, R10, R11, R12, SHIFT_ASR, 10);
    
    a64->op(A::AND, R10, R11, R12, SHIFT_ROR, 10);
    a64->op(ANDS, R10, R11, R12, SHIFT_ROR, 10);
    a64->op(EOR, R10, R11, R12, SHIFT_ROR, 10);
    a64->op(ORR, R10, R11, R12, SHIFT_ROR, 10);
    
    a64->op(A::AND, R10, R11, 0, 4);
    a64->op(ANDS, R10, R11, 4, 8);
    a64->op(EOR, R10, R11, 8, 12);
    a64->op(ORR, R10, R11, 12, 16);

    a64->op(EON, R10, R11, R12, SHIFT_ROR, 10);
    a64->op(ORN, R10, R11, R12, SHIFT_ROR, 10);
    
    a64->op(MADD, R10, R11, R12, R13);
    a64->op(SDIV, R10, R11, R12);
    a64->op(UDIV, R10, R11, R12);

    a64->op(ASR, R10, R11, R12);
    a64->op(LSL, R10, R11, R12);
    a64->op(LSR, R10, R11, R12);
    a64->op(ROR, R10, R11, R12);

    a64->op(BFM, R10, R11, BitMask(3, 0));
    a64->op(SBFM, R10, R11, BitMask(3, 0));
    a64->op(UBFM, R10, R11, BitMask(3, 0));

    a64->op(A::NOP);
        
    Label somewhere;
    a64->op(B, somewhere);
    a64->op(B, R10);
    a64->op(A::BL, somewhere);
    a64->op(A::BL, R10);
    
    a64->op(B, CC_EQ, somewhere);
    a64->op(B, CC_LT, somewhere);
    a64->op(B, CC_GT, somewhere);
    a64->op(B, CC_AL, somewhere);

    a64->op(A::RET, R10);
    
    a64->op(CSEL, CC_EQ, R10, R11, R12);
    a64->op(CSINC, CC_EQ, R10, R11, R12);
    a64->op(CSINV, CC_EQ, R10, R11, R12);
    a64->op(CSNEG, CC_EQ, R10, R11, R12);

    a64->op(CBZ, R10, somewhere);
    a64->op(CBNZ, R10, somewhere);

    a64->done("/tmp/aatest.o");
    
    return 0;
}
