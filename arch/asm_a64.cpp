#include "../plum.h"


using namespace A;


Asm_A64::Asm_A64()
    :Asm() {
    referrer_a64 = NULL;
}


Asm_A64::~Asm_A64() {
}


void Asm_A64::set_referrer_a64(Referrer_A64 *r) {
    referrer_a64 = r;
}


void Asm_A64::data_reference(Label label, int addend) {
    referrer_a64->data_reference(label, addend);
}


void Asm_A64::code_jump_reference(Label label, int addend) {
    referrer_a64->code_jump_reference(label, addend);
}


void Asm_A64::code_branch_reference(Label label, int addend) {
    referrer_a64->code_branch_reference(label, addend);
}


void Asm_A64::code_adr_reference(Label label, int addend) {
    referrer_a64->code_adr_reference(label, addend);
}


int Asm_A64::uimm(int imm, int width, int unit) {
    if (imm % unit == 0 && imm / unit >= 0 && imm / unit < (1 << width))
        return (imm / unit) & ((1 << width) - 1);
    else
        throw ASM_ERROR;
}


int Asm_A64::simm(int imm, int width, int unit) {
    if (imm % unit == 0 && imm / unit >= -(1 << (width - 1)) && imm / unit < (1 << (width - 1)))
        return (imm / unit) & ((1 << width) - 1);
    else
        throw ASM_ERROR;
}

void Asm_A64::code_op(unsigned opcode) {
    // NOTE: host must be little endian, as Aarch64 is, too
    
    code_dword(opcode);
}


// The unscaled signed immediate addressing has these variants
enum {
    MEM_NORMAL = 0b00,
    MEM_POSTINDEX = 0b01,
    MEM_UNPRIVILEGED = 0b10,
    MEM_PREINDEX = 0b11
};


// MovImm

struct {
    unsigned op9;
} movimm_info[] = {
    0b110100101,  // MOVZ
    0b100100101,  // MOVN
    0b111100101   // MOVK
};

void Asm_A64::op(MovImmOpcode opcode, Register rd, int imm, Lsl hw) {
    if (rd == RSP)
        cant_account();
        
    code_op(movimm_info[opcode].op9 << 23 | hw << 21 | uimm(imm, 16) << 5 | rd << 0);
}


// Adr

void Asm_A64::op(A::AdrOpcode opcode, Register rd, Label l, int offset) {
    if (rd == RSP)
        cant_account();

    code_adr_reference(l, offset);  // needs imm2 + imm19 relocation
    
    int op1 = 0b0;
    int op5 = 0b10000;
    
    code_op(op1 << 31 | op5 << 24 | rd << 0);
}


// Pair

struct {
    unsigned op10;
} pair_info[] = {
    0b1010100001,  // LDP
    0b1010100000   // STP
};

void Asm_A64::op(PairOpcode opcode, Register r1, Register r2, Register rn, int imm) {
    if (r1 == RSP || r2 == RSP)
        cant_account();
        
    int op10 = pair_info[opcode].op10 | 0b10 << 1;
    
    code_op(op10 << 22 | simm(imm, 7, 8) << 15 | r2 << 10 | rn << 5 | r1 << 0);
}

void Asm_A64::op(PairOpcode opcode, Register r1, Register r2, Register rn, int imm, MemIncrement increment) {
    if (r1 == RSP || r2 == RSP)
        cant_account();
    else if (rn == RSP)
        account(-imm);
    
    int inc = (
        increment == INCREMENT_PRE ? MEM_PREINDEX :
        increment == INCREMENT_POST ? MEM_POSTINDEX :
        throw ASM_ERROR
    );
    int op10 = pair_info[opcode].op10 | inc << 1;
    
    code_op(op10 << 22 | simm(imm, 7, 8) << 15 | r2 << 10 | rn << 5 | r1 << 0);
}


// Mem

struct {
    unsigned op10;
} mem_info[] = {
    0b1111100001,  // LDR
    0b1011100001,  // LDRUW
    0b0111100001,  // LDRUH
    0b0011100001,  // LDRUB
    0b1011100010,  // LDRSW
    0b0111100010,  // LDRSH
    0b0011100010,  // LDRSB
    0b1111100000,  // STR
    0b1011100000,  // STRW
    0b0111100000,  // STRH
    0b0011100000,  // STRB
};


void Asm_A64::op(MemOpcode opcode, Register rt, Register rn, int imm, MemScaling scaling) {
    if (rt == RSP)
        cant_account();

    int op10 = mem_info[opcode].op10;
    int imm12;
    int size = 1 << (op10 >> 8);
    
    if (scaling == UNSIGNED_SCALED) {
        op10 |= 0b100;
        imm12 = uimm(imm, 12, size);
    }
    else if (scaling == SIGNED_UNSCALED) {
        imm12 = 0 | simm(imm, 9) << 2 | MEM_NORMAL;
    }
    else
        throw ASM_ERROR;
    
    code_op(op10 << 22 | imm12 << 10 | rn << 5 | rt << 0);
}

void Asm_A64::op(MemOpcode opcode, Register rt, Register rn, int imm, MemIncrement increment) {
    if (rt == RSP) {
        switch (opcode) {
            case LDRQ:
            case LDRUW:
            case LDRUH:
            case LDRUB:
            case LDRSW:
            case LDRSH:
            case LDRSB:
                cant_account();
                break;
            default:
                ;
        }
    }
    
    if (rn == RSP)
        account(-imm);
    
    int op10 = mem_info[opcode].op10;
    int inc = (
        increment == INCREMENT_PRE ? MEM_PREINDEX :
        increment == INCREMENT_POST ? MEM_POSTINDEX :
        throw ASM_ERROR
    );
    int imm12 = 0 | simm(imm, 9) << 2 | inc << 0;
    
    code_op(op10 << 22 | imm12 << 10 | rn << 5 | rt << 0);
}

void Asm_A64::op(MemOpcode opcode, Register rt, Register rn, Register rm, IndexShift indexshift) {
    if (rt == RSP)
        cant_account();

    // The shift amount depends on the memory operand size
    int op10 = mem_info[opcode].op10;
    int option = 0b011;  // LSL
    int scale = (indexshift == INDEX_SHIFTED ? 0b1 : indexshift == INDEX_UNSHIFTED ? 0b0 : throw ASM_ERROR);

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
};

void Asm_A64::op(ArithOpcode opcode, Register rd, Register rn, int imm, Shift12 shift12) {
    if (rd == RSP) {
        if (rn != RSP)
            cant_account();
        else {
            int sign = (opcode == A::ADD || opcode == A::ADDS ? -1 : 1);
            int amount = imm << (shift12 == SHIFT12_YES ? 12 : 0);
            
            account(sign * amount);
        }
    }
        
    int op8 = arith_info[opcode].imm_op8;
    
    code_op(op8 << 24 | shift12 << 22 | uimm(imm, 12) << 10 | rn << 5 | rd << 0);
}

void Asm_A64::op(ArithOpcode opcode, Register rd, Register rn, Register rm, ShiftDir shift_dir, int shift_amount) {
    if (rd == RSP)
        cant_account();

    if (shift_dir == SHIFT_ROR)
        throw ASM_ERROR;

    int op8 = arith_info[opcode].reg_op8;
    int neg = 0b0;
    
    code_op(op8 << 24 | shift_dir << 22 | neg << 21 | rm << 16 | uimm(shift_amount, 6) << 10 | rn << 5 | rd << 0);
}


// Logical

struct {
    unsigned imm_op8;
    unsigned reg_op8;
} logical_info[] = {
    { 0b10010010, 0b10001010 }, // AND
    { 0b11110010, 0b11101010 }, // ANDS
    { 0b11010010, 0b11001010 }, // EOR
    { 0b10110010, 0b10101010 }, // ORR
};

void Asm_A64::op(LogicalOpcode opcode, Register rd, Register rn, BitMask bitmask) {
    if (rd == RSP)
        cant_account();

    if (bitmask.imms >= 63 || bitmask.immr >= 64)
        throw ASM_ERROR;

    int op8 = logical_info[opcode].imm_op8;
    int n = 0b01;  // we always use 64-bit wide patterns
    
    code_op(op8 << 24 | n << 22 | bitmask.immr << 16 | bitmask.imms << 10 | rn << 5 | rd << 0);
}

void Asm_A64::op(LogicalOpcode opcode, Register rd, Register rn, Register rm, ShiftDir shift_dir, int shift_amount) {
    if (rd == RSP)
        cant_account();

    int op8 = logical_info[opcode].reg_op8;
    int neg = 0b0;
    
    code_op(op8 << 24 | shift_dir << 22 | neg << 21 | rm << 16 | uimm(shift_amount, 6) << 10 | rn << 5 | rd << 0);
}


// LogicalNot 

struct {
    unsigned reg_op8;
} logicalnot_info[] = {
    0b11001010, // EON  (practically EOR)
    0b10101010, // ORN  (practically ORR)
};

void Asm_A64::op(LogicalNotOpcode opcode, Register rd, Register rn, Register rm, ShiftDir shift_dir, int shift_amount) {
    if (rd == RSP)
        cant_account();

    int op8 = logicalnot_info[opcode].reg_op8;
    int neg = 0b1;
    
    code_op(op8 << 24 | shift_dir << 22 | neg << 21 | rm << 16 | uimm(shift_amount, 6) << 10 | rn << 5 | rd << 0);
}


// Mul

void Asm_A64::op(MulOpcode opcode, Register rd, Register rn, Register rm, Register ra) {
    if (rd == RSP)
        cant_account();

    int op11 = 0b10011011000;
    int op1 = (opcode == MADD ? 0b0 : opcode == MSUB ? 0b1 : throw ASM_ERROR);
    
    code_op(op11 << 21 | rm << 16 | op1 << 15 | ra << 10 | rn << 5 | rd << 0);
}


// Div

void Asm_A64::op(DivOpcode opcode, Register rd, Register rn, Register rm) {
    if (rd == RSP)
        cant_account();

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
    if (rd == RSP)
        cant_account();

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
    if (rd == RSP)
        cant_account();

    int op8 = bitfield_info[opcode].imm_op8;
    int op2 = 0b01;
    
    code_op(op8 << 24 | op2 << 22 | bitmask.immr << 16 | bitmask.imms << 10 | rn << 5 | rd << 0);
}

void Asm_A64::op(ExtrOpcode opcode, Register rd, Register rn, Register rm, int lsb_index) {
    if (rd == RSP)
        cant_account();

    int op11 = 0b10010011110;
    int imm6 = uimm(lsb_index, 6);
    
    code_op(op11 << 21 | rm << 16 | imm6 << 10 | rn << 5 | rd << 0);
}


// Simple

void Asm_A64::op(SimpleOpcode opcode) {
    switch (opcode) {
    case A::NOP:
        code_op(0b11010101000000110010000000011111);
        break;
    case A::UDF:
        code_op(0b00000000000000000000000000000000);
        break;
    default:
        throw ASM_ERROR;
    }
}


// Branch

void Asm_A64::op(JumpOpcode opcode, Label label) {
    code_jump_reference(label);  // needs imm26

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
        
    code_branch_reference(label);  // needs imm19

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
    if (rd == RSP)
        cant_account();

    int op11 = condsel_info[opcode].op11;
    int op2 = condsel_info[opcode].op2;
    
    code_op(op11 << 21 | rm << 16 | cc << 12 | op2 << 10 | rn << 5 | rd << 0);
}

void Asm_A64::op(RegLabelOpcode opcode, Register rn, Label label) {
    code_branch_reference(label);  // needs imm19
    
    int op8 = (opcode == CBZ ? 0b10110100 : opcode == CBNZ ? 0b10110101 : throw ASM_ERROR);
    
    code_op(op8 << 24 | rn << 0);
}


// SpecReg

void Asm_A64::op(SpecRegOpcode opcode, SpecReg specreg16, Register rt) {
    if (rt == RSP)
        cant_account();

    int op10 = 0b1101010100;
    int l = (opcode == MSRR ? 0b1 : 0b0);
    // specreg16 is o0op1CRnCRmop2
    
    code_op(op10 << 22 | l << 21 | specreg16 << 5 | rt << 0);
}
