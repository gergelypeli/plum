
// Oops! Even in 64 bit mode the addressing offsets and immediate operands are limited to
// 32 bit! The only exceptions are:
//   REX A1   => MOV RAX, moffset64
//   REX A3   => MOV moffset64, RAX
//   REX B8+r => MOV reg64, imm64
// Other than these, immediate constants are always 32 bits!

static const int REGISTER_COUNT = 16;
const char *REGISTER_NAMES[] = {
    "RAX", "RCX", "RDX", "RBX", "RSP", "RBP", "RSI", "RDI",
    "R8",  "R9",  "R10", "R11", "R12", "R13", "R14", "R15"
};

enum Register {
    RAX=0, RCX, RDX, RBX, RSP, RBP, RSI, RDI, R8,  R9,  R10,  R11,  R12,  R13,  R14,  R15,
    EAX=0, ECX, EDX, EBX, ESP, EBP, ESI, EDI, R8D, R9D, R10D, R11D, R12D, R13D, R14D, R15D,
    AX=0, CX, DX, BX, SP, BP, SI, DI,         R8W, R9W, R10W, R11W, R12W, R13W, R14W, R15W,
    AL=0, CL, DL, BL, SPL, BPL, SIL, DIL,     R8B, R9B, R10B, R11B, R12B, R13B, R14B, R15B,
    NOREG=-1
};


enum HighByteRegister {
    AH=4, CH, DH, BH
};


const char *SSE_REGISTER_NAMES[] = {
    "XMM0", "XMM1", "XMM2", "XMM3", "XMM4", "XMM5", "XMM6", "XMM7",
    "XMM8", "XMM9", "XMM10", "XMM11", "XMM12", "XMM13", "XMM14", "XMM15",
};

enum SseRegister {
    XMM0=0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6, XMM7,
    XMM8, XMM9, XMM10, XMM11, XMM12, XMM13, XMM14, XMM15,
    NOSSE=-1
};


const char *CONDITION_NAMES[] = {
    "OVERFLOW", "NOT_OVERFLOW",
    "BELOW", "ABOVE_EQUAL",
    "EQUAL", "NOT_EQUAL",
    "BELOW_EQUAL", "ABOVE",
    "SIGN", "NOT_SIGN",
    "PARITY", "NOT_PARITY",
    "LESS", "GREATER_EQUAL",
    "LESS_EQUAL", "GREATER"
};

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


ConditionCode negated(ConditionCode cc) {
    // The lowest bit negates the condition meaning
    return cc != CC_NONE ? (ConditionCode)(cc ^ 1) : throw X64_ERROR;
}


ConditionCode swapped(ConditionCode cc) {
    return (
        cc == CC_EQUAL ? CC_EQUAL :
        cc == CC_NOT_EQUAL ? CC_NOT_EQUAL :
        cc == CC_BELOW ? CC_ABOVE :
        cc == CC_ABOVE ? CC_BELOW :
        cc == CC_BELOW_EQUAL ? CC_ABOVE_EQUAL :
        cc == CC_ABOVE_EQUAL ? CC_BELOW_EQUAL :
        cc == CC_LESS ? CC_GREATER :
        cc == CC_GREATER ? CC_LESS :
        cc == CC_LESS_EQUAL ? CC_GREATER_EQUAL :
        cc == CC_GREATER_EQUAL ? CC_LESS_EQUAL :
        throw X64_ERROR
    );
}


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
    // 16 general registers, except RBX (3, 0x08), RSP (4, 0x10), RBP (5, 0x20).
    // 16 SSE registers, except XMM15 (15+16, 0x80000000).
    static const unsigned long GPR_MASK = 0x0000FFC7;
    static const unsigned long PTR_MASK = 0x0000FFC7;
    static const unsigned long SSE_MASK = 0x7FFF0000;
    static const unsigned long ALL_MASK = GPR_MASK | PTR_MASK | SSE_MASK;
    static const int REGS_TOTAL = 32;
    
    unsigned long available;
    
    Regs(unsigned long a) {
        available = a;
    }

    void validate(Register r) {
        if (r == NOREG || r == RBX || r == RSP || r == RBP)
            throw X64_ERROR;
    }

    void validate(SseRegister s) {
        if (s == NOSSE || s == XMM15)
            throw X64_ERROR;
    }

public:
    Regs() {
        available = 0;
    }
    
    static Regs all() {
        return Regs(ALL_MASK);
    }
    
    Regs(Register r) {
        validate(r);
        available = (1UL << (int)r);
    }

    Regs(Register r1, Register r2) {
        validate(r1);
        validate(r2);
        available = (1UL << (int)r1) | (1UL << (int)r2);
    }

    Regs(Register r1, Register r2, Register r3) {
        validate(r1);
        validate(r2);
        validate(r3);
        available = (1UL << (int)r1) | (1UL << (int)r2) | (1UL << (int)r3);
    }

    Regs(Register r1, Register r2, Register r3, Register r4) {
        validate(r1);
        validate(r2);
        validate(r3);
        validate(r4);
        available = (1UL << (int)r1) | (1UL << (int)r2) | (1UL << (int)r3) | (1UL << (int)r4);
    }

    Regs(Register r1, Register r2, Register r3, Register r4, Register r5) {
        validate(r1);
        validate(r2);
        validate(r3);
        validate(r4);
        validate(r5);
        available = (1UL << (int)r1) | (1UL << (int)r2) | (1UL << (int)r3) | (1UL << (int)r4) | (1UL << (int)r5);
    }

    Regs(SseRegister s) {
        validate(s);
        available = (1UL << ((int)s + 16));
    }

    Regs operator |(Regs other) {
        return Regs(available | other.available);
    }

    Regs operator &(Regs other) {
        return Regs(available & other.available);
    }

    Regs operator~() {
        return Regs(~available & ALL_MASK);
    }
    
    explicit operator bool() {
        return available != 0;
    }
    
    bool has_any() {
        return (available & GPR_MASK) != 0;
    }

    bool has_sse() {
        return (available & SSE_MASK) != 0;
    }

    int count() {
        int n = 0;
        
        for (int i=0; i<REGS_TOTAL; i++)
            if (available & GPR_MASK & (1UL << i)) {
                n++;
            }
    
        return n;
    }

    int count_sse() {
        int n = 0;
        
        for (int i=0; i<REGS_TOTAL; i++)
            if (available & SSE_MASK & (1UL << i)) {
                n++;
            }
    
        return n;
    }

    Register get_any() {
        for (int i=0; i<REGS_TOTAL; i++)
            if (available & GPR_MASK & (1UL << i)) {
                return (Register)i;
            }
    
        std::cerr << "No available register!\n";
        throw X64_ERROR;
    }

    SseRegister get_sse() {
        for (int i=0; i<REGS_TOTAL; i++)
            if (available & SSE_MASK & (1UL << i)) {
                return (SseRegister)(i - 16);
            }
    
        std::cerr << "No available SSE register!\n";
        throw X64_ERROR;
    }
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
    
    Label() {
        static unsigned last_def_index = 0;
        
        def_index = ++last_def_index;
        
        //if (def_index == 2095)
        //    abort();
    }
    
    Label(const Label &c) {
        def_index = c.def_index;
    }
    
    Label(LeaveUndefined) {
        def_index = 0;
    }
};


struct Address {
    Register base;
    Register index;
    int scale;
    int offset;  // Offsets are never longer than 32 bits, except in some wicked cases
    Label label;

    Address()
        :label(Label::LEAVE_UNDEFINED) {
        base = NOREG;
        index = NOREG;
        scale = 0;
        offset = 0;
    }

    Address(Register b, int o)
        :label(Label::LEAVE_UNDEFINED) {
        if (b == NOREG) {
            std::cerr << "Address without base register!\n";
            throw X64_ERROR;
        }
          
        base = b;
        index = NOREG;
        scale = 0;
        offset = o;
    }

    Address(Register b, Register i, int o)
        :label(Label::LEAVE_UNDEFINED) {
        if (b == NOREG) {
            std::cerr << "Address without base register!\n";
            throw X64_ERROR;
        }
          
        base = b;
        index = i;
        scale = 1;
        offset = o;
    }

    Address(Register b, Register i, int s, int o)
        :label(Label::LEAVE_UNDEFINED) {
        if (b == NOREG) {
            std::cerr << "Address without base register!\n";
            throw X64_ERROR;
        }
    
        if (i != NOREG && s != 1 && s != 2 && s != 4 && s != 8)
            throw X64_ERROR;
    
        base = b;
        index = i;
        scale = s;
        offset = o;
    }

    Address(Label l, int o)
        :label(l) {
        base = NOREG;
        index = NOREG;
        scale = 1;
        offset = o;
    }

    Address operator + (int x) {
        Address a(*this);
        a.offset += x;
        return a;
    }
};
