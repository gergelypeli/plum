
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

enum IndexShift {
    INDEX_UNSHIFTED,
    INDEX_SHIFTED
};

enum MemIncrement {
    INCREMENT_PRE,
    INCREMENT_POST
};

enum MemScaling {
    UNSIGNED_SCALED,
    SIGNED_UNSCALED
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

enum SpecReg {
    SPECREG_CONDFLAGS = 0b1101101000010000
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
};

enum LogicalOpcode {
    AND, ANDS,
    EOR, ORR
};

enum LogicalNotOpcode {
    EON, ORN
};

enum MulOpcode {
    MADD, MSUB
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

enum SimpleOpcode {
    NOP, UDF
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

enum SpecRegOpcode {
    MSRR, MSRW
};

}


class Referrer_A64 {
public:
    virtual void data_reference(Label label, int addend) =0;
    virtual void code_jump_reference(Label label, int addend = 0) =0;
    virtual void code_branch_reference(Label label, int addend = 0) =0;
};


// Asm_A64

class Asm_A64: public Asm {
public:
    Referrer_A64 *referrer_a64;
    
    Asm_A64();
    virtual ~Asm_A64();

    virtual void set_referrer_a64(Referrer_A64 *r);
    
    virtual void data_reference(Label label, int addend = 0);
    virtual void code_jump_reference(Label label, int addend = 0);
    virtual void code_branch_reference(Label label, int addend = 0);

    virtual int uimm(int imm, int width, int unit = 1);
    virtual int simm(int imm, int width, int unit = 1);

    virtual void code_op(unsigned op);

    virtual void op(A::MovImmOpcode opcode, Register rd, int imm, A::Lsl hw);

    virtual void op(A::PairOpcode opcode, Register r1, Register r2, Register rn, int imm);
    virtual void op(A::PairOpcode opcode, Register r1, Register r2, Register rn, int imm, A::MemIncrement increment);

    virtual void op(A::MemOpcode opcode, Register rt, Register rn, int imm, A::MemScaling scaling);
    virtual void op(A::MemOpcode opcode, Register rt, Register rn, int imm, A::MemIncrement increment);
    virtual void op(A::MemOpcode opcode, Register rt, Register rn, Register rm, A::IndexShift indexshift = A::INDEX_UNSHIFTED);

    virtual void op(A::ArithOpcode opcode, Register rd, Register rn, int imm, A::Shift12 shift12 = A::SHIFT12_NO);
    virtual void op(A::ArithOpcode opcode, Register rd, Register rn, Register rm, A::ShiftDir shift_dir = A::SHIFT_LSL, int shift_amount = 0);

    virtual void op(A::LogicalOpcode opcode, Register rd, Register rn, A::BitMask bitmask);
    virtual void op(A::LogicalOpcode opcode, Register rd, Register rn, Register rm, A::ShiftDir shift_dir = A::SHIFT_LSL, int shift_amount = 0);

    virtual void op(A::LogicalNotOpcode opcode, Register rd, Register rn, Register rm, A::ShiftDir shift_dir = A::SHIFT_LSL, int shift_amount = 0);

    virtual void op(A::MulOpcode opcode, Register rd, Register rn, Register rm, Register ra);

    virtual void op(A::DivOpcode opcode, Register rd, Register rn, Register rm);

    virtual void op(A::ShiftOpcode opcode, Register rd, Register rn, Register rm);

    virtual void op(A::BitFieldOpcode opcode, Register rd, Register rn, A::BitMask bitmask);
    virtual void op(A::ExtrOpcode opcode, Register rd, Register rn, Register rm, int lsb_index);

    virtual void op(A::SimpleOpcode);

    virtual void op(A::JumpOpcode opcode, Label label);
    virtual void op(A::JumpOpcode opcode, Register rn);
    virtual void op(A::JumpOpcode opcode, A::CondCode cc, Label label);

    virtual void op(A::RetOpcode opcode, Register rn);

    virtual void op(A::CondSelOpcode opcode, A::CondCode cc, Register rd, Register rn, Register rm);
    virtual void op(A::RegLabelOpcode opcode, Register rn, Label label);
    
    virtual void op(A::SpecRegOpcode opcode, A::SpecReg specreg16, Register rt);
};
