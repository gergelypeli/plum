
// Oops! Even in 64 bit mode the addressing offsets and immediate operands are limited to
// 32 bit! The only exceptions are:
//   REX A1   => MOV RAX, moffset64
//   REX A3   => MOV moffset64, RAX
//   REX B8+r => MOV reg64, imm64
// Other than these, immediate constants are always 32 bits!

static const int REGISTER_COUNT = 16;

enum Register {
    RAX=0, RCX, RDX, RBX, RSP, RBP, RSI, RDI, R8,  R9,  R10,  R11,  R12,  R13,  R14,  R15,
    EAX=0, ECX, EDX, EBX, ESP, EBP, ESI, EDI, R8D, R9D, R10D, R11D, R12D, R13D, R14D, R15D,
    AX=0, CX, DX, BX, SP, BP, SI, DI,         R8W, R9W, R10W, R11W, R12W, R13W, R14W, R15W,
    AL=0, CL, DL, BL, SPL, BPL, SIL, DIL,     R8B, R9B, R10B, R11B, R12B, R13B, R14B, R15B,
    NOREG=-1
};

const char *register_name(Register r);
std::ostream &operator << (std::ostream &os, const Register r);


enum HighByteRegister {
    AH=4, CH, DH, BH
};


enum SseRegister {
    XMM0=0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6, XMM7,
    XMM8, XMM9, XMM10, XMM11, XMM12, XMM13, XMM14, XMM15,
    NOSSE=-1
};

const char *sseregister_name(SseRegister r);
std::ostream &operator << (std::ostream &os, const SseRegister r);


enum ConditionCode {
    CC_OVERFLOW=0, CC_NOT_OVERFLOW,
    CC_BELOW, CC_ABOVE_EQUAL,
    CC_EQUAL, CC_NOT_EQUAL,
    CC_BELOW_EQUAL, CC_ABOVE,
    CC_SIGN, CC_NOT_SIGN,
    CC_PARITY, CC_NOT_PARITY,
    CC_LESS, CC_GREATER_EQUAL,
    CC_LESS_EQUAL, CC_GREATER,
    CC_NONE
};


std::ostream &operator << (std::ostream &os, const ConditionCode cc);
ConditionCode negated(ConditionCode cc);
ConditionCode swapped(ConditionCode cc);


enum Slash {
    // To represent constant values that go into the reg field, without interpreting
    // them as register numbers. This is to prevent accidental type conversions from
    // Register to int.
    SLASH_0,
    SLASH_1,
    SLASH_2,
    SLASH_3,
    SLASH_4,
    SLASH_5,
    SLASH_6,
    SLASH_7,
};


enum RegSubset {
    GPR_SUBSET,
    PTR_SUBSET,
    SSE_SUBSET
};

struct Regs {
private:
    // 16 general registers, except RSP (4, 0x10), RBP (5, 0x20), R10 (10, 0x400), R11 (11, 0x800).
    // 16 SSE registers, except XMM14 and XMM15 ({14,15}+16, 0xC0000000).
    static const int REGS_TOTAL = 32;
    static const unsigned64 GPR_MASK  = 0x00000000F3CF;
    static const unsigned64 PTR_MASK  = 0x00000000F3CF;
    static const unsigned64 SSE_MASK  = 0x00003FFF0000;
    static const unsigned64 STACKVARS = 0x000100000000;
    static const unsigned64 HEAPVARS  = 0x000200000000;
    static const unsigned64 ALL_MASK  = GPR_MASK | PTR_MASK | SSE_MASK | STACKVARS | HEAPVARS;
    
    unsigned64 available;
    
    Regs(unsigned64 a);

    void validate(Register r);
    void validate(SseRegister s);

public:
    Regs();
    static Regs all();
    static Regs stackvars();
    static Regs heapvars();
    Regs(Register r);
    Regs(Register r1, Register r2);
    Regs(Register r1, Register r2, Register r3);
    Regs(Register r1, Register r2, Register r3, Register r4);
    Regs(Register r1, Register r2, Register r3, Register r4, Register r5);
    Regs(SseRegister s);
    Regs operator |(Regs other);
    Regs operator &(Regs other);
    Regs operator ~();
    bool operator ==(Regs other);
    bool operator !=(Regs other);
    explicit operator bool();
    bool has_gpr();
    bool has_sse();
    int count_gpr();
    int count_sse();
    Register get_gpr();
    SseRegister get_sse();
    void reserve_gpr(int requested);
};


struct Label {
private:
    Label(int x) { def_index = x; }  // Fuck
    Label(void *) {}  // Fuuuck
    
public:
    enum LeaveUndefined {
        LEAVE_UNDEFINED
    };

    unsigned def_index;
    
    Label();
    Label(const Label &c);
    Label(LeaveUndefined);
};


struct Address {
    enum Scale {
        SCALE_1, SCALE_2, SCALE_4, SCALE_8
    };
    
    Register base;
    Register index;
    Scale scale;
    int offset;  // Offsets are never longer than 32 bits, except in some wicked cases
    Label label;

    Address();
    Address(Register b, int o);
    Address(Register b, Register i, int o);
    Address(Register b, Register i, Scale s, int o);
    Address(Label l, int o);
    Address operator + (int x) const;
};


std::ostream &operator << (std::ostream &os, const Address &a);
