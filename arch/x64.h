#include <string>
#include <vector>
#include <map>

// Oops! Even in 64 bit mode the addressing offsets and immediate operands are limited to
// 32 bit! The only exceptions are:
//   REX A1   => MOV RAX, moffset64
//   REX A3   => MOV moffset64, RAX
//   REX B8+r => MOV reg64, imm64
// We now don't support either forms, so for us these constants are never 64 bit!

// We force a REX prefix for all byte operations, so we gain access to SIL/DIL/BPL/SPL
// instead of AH/BH/CH/DH.

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


struct Regs {
    static const int ALL_MASK = 0xFFC7;
    //static const int GPR_MASK = 0xFF07;  // general purpose registers
    //static const int PTR_MASK = 0x00C0;  // registers for borrowed references, RSI and RDI
    static const int SCR_MASK = 0x0008;  // scratch registers, RBX only
    static const int RES_MASK = 0x0030;  // reserved registers, RSP and RBP
    
    int available;
    
    Regs(int a = 0) {
        available = a;
    }
    
    static Regs all() {
        return Regs(ALL_MASK);
    }

    //static Regs all_gprs() {
    //    return Regs(GPR_MASK);
    //}

    //static Regs all_ptrs() {
    //    return Regs(PTR_MASK);
    //}
    
    Regs add(Register r) {
        available |= 1 << (int)r;
        return *this;
    }

    Regs remove(Register r) {
        available &= ~(1 << (int)r);
        return *this;
    }

    Regs operator |(Regs other) {
        return Regs(available | other.available);
    }

    Regs operator &(Regs other) {
        return Regs(available & other.available);
    }

    Regs operator~() {
        return Regs(~available);
    }
    
    bool has(Register r) {
        return available & (1 << (int)r);
    }

    bool has_any() {
        return (available & ALL_MASK) != 0;
    }

    //bool has_ptr() {
    //    return (available & PTR_MASK) != 0;
    //}

    int count() {
        int n = 0;
        
        for (int i=0; i<REGISTER_COUNT; i++)
            if (available & ALL_MASK & (1 << i)) {
                n++;
            }
    
        return n;
    }

    Register get_any() {
        for (int i=0; i<REGISTER_COUNT; i++)
            if (available & ALL_MASK & (1 << i)) {
                return (Register)i;
            }
    
        std::cerr << "No available register!\n";
        throw X64_ERROR;
    }

    //Register get_ptr() {
    //    for (int i=0; i<REGISTER_COUNT; i++)
    //        if (available & PTR_MASK & (1 << i)) {
    //            return (Register)i;
    //        }
    //
    //    std::cerr << "No PTR in register set!\n";
    //    throw X64_ERROR;
    //}
};


struct Label {
private:
    Label(int) {}  // Fuck
    
public:
    unsigned def_index;
    
    Label(bool leave_uninitialized = false) {
        static unsigned last_def_index = 0;
        
        def_index = leave_uninitialized ? 0 : ++last_def_index;
        
        //if (def_index == 1675)
        //    abort();
    }
    
    Label(const Label &c) {
        def_index = c.def_index;
    }
};


struct Address {
    Register base;
    Register index;
    int scale;
    int offset;  // Offsets are never longer than 32 bits, except in some wicked cases

    Address();
    Address(Register b, int o);
    Address(Register b, Register i, int o);
    Address(Register b, Register i, int scale, int o);
    Address operator + (int o);
};

#define EXCEPTION_ADDRESS Address(RBP, -8)


enum SimpleOp {
    CBW, CDQ, CDQE, CLC, CLD, CLI, CLTS, CMC, CQO, CWD, CWDE, HLT, IRET, LAHF, NOP,
    POPA, POPF, PUSHA, PUSHF, RETF, RET, SAHF, STC, STD, STI, UD2, XLAT,
    FDIVP, FRNDINT, FMULP, FSUBRP, FINIT
};


enum UnaryOp {
    DECB=0, DECW, DECD, DECQ,
    DIVB=4, DIVW, DIVD, DIVQ,
    IDIVB=8, IDIVW, IDIVD, IDIVQ,
    IMULB=12, IMULW, IMULD, IMULQ,
    INCB=16, INCW, INCD, INCQ,
    LLDT=20,
    LTR=24,
    MULB=28, MULW, MULD, MULQ,
    NEGB=32, NEGW, NEGD, NEGQ,
    NOTB=36, NOTW, NOTD, NOTQ,
    SLDT=40,
    STR=44,
    VERR=48,
    VERW=52
};

UnaryOp operator%(UnaryOp x, int y) { return (UnaryOp)((x & ~3) | (y & 3)); }


enum PortOp {
    INB=0, INW, IND, INQ,
    OUTB=4, OUTW, OUTD, OUTQ
};


enum StringOp {
    INSB=0, INSW, INSD, INSQ,
    LODSB=4, LODSW, LODSD, LODSQ,
    MOVSB=8, MOVSW, MOVSD, MOVSQ,
    OUTSB=12, OUTSW, OUTSD, OUTSQ,
    STOSB=16, STOSW, STOSD, STOSQ,
    REPINSB=20, REPINSW, REPINSD, REPINSQ,
    REPLODSB=24, REPLODSW, REPLODSD, REPLODSQ,
    REPMOVSB=28, REPMOVSW, REPMOVSD, REPMOVSQ,
    REPOUTSB=32, REPOUTSW, REPOUTSD, REPOUTSQ,
    REPSTOSB=36, REPSTOSW, REPSTOSD, REPSTOSQ,
    CMPSB=40, CMPSW, CMPSD, CMPSQ,
    SCASB=44, SCASW, SCASD, SCASQ,
    REPECMPSB=48, REPECMPSW, REPECMPSD, REPECMPSQ,
    REPESCASB=52, REPESCASW, REPESCASD, REPESCASQ,
    REPNECMPSB=56, REPNECMPSW, REPNECMPSD, REPNECMPSQ,
    REPNESCASB=60, REPNESCASW, REPNESCASD, REPNESCASQ
};


enum BinaryOp {
    ADCB=0, ADCW, ADCD, ADCQ,
    ADDB=4, ADDW, ADDD, ADDQ,
    ANDB=8, ANDW, ANDD, ANDQ,
    CMPB=12, CMPW, CMPD, CMPQ,
    MOVB=16, MOVW, MOVD, MOVQ,
    ORB=20, ORW, ORD, ORQ,
    SBBB=24, SBBW, SBBD, SBBQ,
    SUBB=28, SUBW, SUBD, SUBQ,
    TESTB=32, TESTW, TESTD, TESTQ,
    XORB=36, XORW, XORD, XORQ
};

BinaryOp operator%(BinaryOp x, int y) { return (BinaryOp)((x & ~3) | (y & 3)); }


enum ShiftOp {
    RCLB=0, RCLW, RCLD, RCLQ,
    RCRB=4, RCRW, RCRD, RCRQ,
    ROLB=8, ROLW, ROLD, ROLQ,
    RORB=12, RORW, RORD, RORQ,
    SALB=16, SALW, SALD, SALQ,
    SARB=20, SARW, SARD, SARQ,
    SHLB=24, SHLW, SHLD, SHLQ,
    SHRB=28, SHRW, SHRD, SHRQ
};

ShiftOp operator%(ShiftOp x, int y) { return (ShiftOp)((x & ~3) | (y & 3)); }


enum ExchangeOp {
    XCHGB=0, XCHGW, XCHGD, XCHGQ
};

ExchangeOp operator%(ExchangeOp x, int y) { return (ExchangeOp)((x & ~3) | (y & 3)); }


enum StackOp {
    PUSHQ, POPQ
};


enum MemoryOp {
    LGDT, LIDT, SGDT, SIDT, FILDQ, FISTPQ, FSTCW, FLDCW
};


enum RegisterFirstOp {
    IMUL2W=1, IMUL2D, IMUL2Q,
    MOVSXDQ=7
};

RegisterFirstOp operator%(RegisterFirstOp x, int y) { return (RegisterFirstOp)((x & ~3) | (y & 3)); }


enum Imul3Op {
    IMUL3W=1, IMUL3D, IMUL3Q
};

Imul3Op operator%(Imul3Op x, int y) { return (Imul3Op)((x & ~3) | (y & 3)); }


enum RegisterMemoryOp {
    LEA
};


enum LeaRipOp {
    LEARIP  // home made instruction for LEA r, [RIP + disp32]
};


enum BranchOp {
    JO, JNO, JB, JAE, JE, JNE, JBE, JA,
    JS, JNS, JP, JNP, JL, JGE, JLE, JG
};


enum JumpOp {
    CALL, JMP
};


enum BitSetOp {
    SETO, SETNO, SETB, SETAE, SETE, SETNE, SETBE, SETA,
    SETS, SETNS, SETP, SETNP, SETL, SETGE, SETLE, SETG,
    NOSET=-1
};


BitSetOp negate(BitSetOp opcode) {
    // The lowest bit negates the condition meaning
    return (BitSetOp)(opcode ^ 1);
}


BitSetOp negate_ordering(BitSetOp opcode) {
    return opcode == SETE || opcode == SETNE ? opcode : opcode != NOSET ? negate(opcode) : throw X64_ERROR;
}


BranchOp branchize(BitSetOp opcode) {
    // Both enums are just condition bits, so converting between them is straightforward
    return opcode != NOSET ? (BranchOp)opcode : throw X64_ERROR;
}


enum ConstantOp {
     INT, RETX, RETFX
};


class Once;
class Unwind;


class X64 {
public:
    enum Def_type {
        DEF_CODE,
        DEF_CODE_EXPORT,
        DEF_CODE_IMPORT,
        DEF_DATA,
        DEF_DATA_EXPORT,
        DEF_ABSOLUTE,
        DEF_ABSOLUTE_EXPORT
    };
    
    struct Def {
        Def_type type;
        int location;  // Can be arbitrary value for absolute symbols
        unsigned size;
        std::string name;
        bool is_global;

        unsigned symbol_index;  // To be filled during importing
        
        Def(Def_type t, int l = 0, unsigned s = 0, const std::string &n = "", bool ig = false) {
            type = t;
            location = l;
            size = s;
            name = n;
            is_global = ig;
            
            symbol_index = 0;
        }
    };
    
    enum Ref_type {
        REF_CODE_SHORT,
        REF_CODE_RELATIVE,
        REF_DATA_ABSOLUTE,
    };
    
    struct Ref {
        Ref_type type;
        int location;
        unsigned def_index;
    };

    std::vector<char> code;
    std::vector<char> data;
    std::map<unsigned, Def> defs;
    std::vector<Ref> refs;
    unsigned code_symbol_index, data_symbol_index;
    Ork *ork;
    Label alloc_RAX_RBX_label, realloc_RAX_RBX_label;
    Label memalloc_label, memfree_label, memrealloc_label, log_label, dump_label, die_label, dies_label, sort_label, empty_function_label, weak_finalized_die_label;
    Label finalize_label, alloc_fcb_label, free_fcb_label;
    std::vector<Label> incref_labels, decref_labels;

    Once *once;
    Unwind *unwind;
    
    void add_def(Label label, const Def &def);

    void data_align();
    void data_byte(char x);
    void data_word(short x);
    void data_dword(int x);
    void data_qword(long x);
    void data_zstring(const char *s);
    void data_label(Label c, unsigned size = 0);
    void data_label_local(Label c, std::string name, unsigned size = 0);
    void data_label_global(Label c, std::string name, unsigned size = 0);
    unsigned data_allocate(unsigned size);
    void data_reference(Label c);
    void data_heap_header();
    Label data_heap_string(std::vector<unsigned short> characters);

    void code_align();
    void code_byte(char x);
    void code_word(short x);
    void code_dword(int x);
    void code_qword(long x);
    void effective_address(int regfield, Register rm);
    void effective_address(int regfield, Address rm);
    void effective_address(int regfield, Label l, int offset);
    
    X64();
    ~X64();
    
    void init(std::string module_name);
    void done(std::string name);
    
    void code_label(Label c, unsigned size = 0);
    void code_label_import(Label c, std::string name);
    void code_label_local(Label c, std::string name, unsigned size = 0);
    void code_label_global(Label c, std::string name, unsigned size = 0);
    void code_reference(Label c, int offset = 0);

    int q(Register r);
    int r(Register regfield);
    int xb(Register regfield);
    int xb(Address rm);
    
    void rex(int wrxb, bool force = false);

    void code_op(int opcode);
    void code_op(int opcode, int opsize, int rxb = 0);
    void code_op(int opcode, int opsize, Slash regfield, Register rm);
    void code_op(int opcode, int opsize, Register regfield, Register rm);
    void code_op(int opcode, int opsize, Slash regfield, Address rm);
    void code_op(int opcode, int opsize, Register regfield, Address rm);
    void code_op(int opcode, int opsize, Slash regfield, Label l, int offset);
    void code_op(int opcode, int opsize, Register regfield, Label l, int offset);

    void op(SimpleOp opcode);
    void op(UnaryOp opcode, Register x);
    void op(UnaryOp opcode, Address x);
    void op(PortOp opcode);
    void op(PortOp opcode, int x);
    void op(StringOp opcode);
    void op(BinaryOp opcode, Register x, int y);
    void op(BinaryOp opcode, Address x, int y);
    void op(BinaryOp opcode, Register x, Register y);
    void op(BinaryOp opcode, Address x, Register y);
    void op(BinaryOp opcode, Register x, Address y);
    void op(BinaryOp opcode, Register x, Label y);
    void op(BinaryOp opcode, Label x, Register y);
    void op(BinaryOp opcode, Label x, int y);
    void op(ShiftOp opcode, Register x, Register cl);
    void op(ShiftOp opcode, Address x, Register cl);
    void op(ShiftOp opcode, Register x, char y);
    void op(ShiftOp opcode, Address x, char y);
    void op(ExchangeOp opcode, Register x, Register y);
    void op(ExchangeOp opcode, Address x, Register y);
    void op(ExchangeOp opcode, Register x, Address y);
    void op(StackOp opcode, int x);
    void op(StackOp opcode, Label c, int offset = 0);
    void op(StackOp opcode, Register x);
    void op(StackOp opcode, Address x);
    void op(MemoryOp opcode, Address x);
    void op(RegisterFirstOp opcode, Register x, Register y);
    void op(RegisterFirstOp opcode, Register x, Address y);
    void op(Imul3Op opcode, Register x, Register y, int z);
    void op(Imul3Op opcode, Register x, Address y, int z);
    void op(RegisterMemoryOp opcode, Register x, Address y);
    void op(LeaRipOp opcode, Register r, Label l, int offset = 0);
    void op(BitSetOp, Register x);
    void op(BitSetOp, Address x);
    void op(BranchOp opcode, Label c);
    void op(JumpOp opcode, Label c);
    void op(JumpOp opcode, Address x);
    void op(JumpOp opcode, Register x);
    void op(ConstantOp opcode, int x);

    int pusha(bool except_rax = false);
    void popa(bool except_rax = false);
    void incref(Register reg);
    void decref(Register reg);
    void incweakref(Register reg);
    void decweakref(Register reg);
    void init_memory_management();
    void alloc_RAX_RBX();
    void realloc_RAX_RBX();
    void memfree(Register reg);

    void lock(Register r, Label ok);
    void log(const char *message);
    void dump(const char *message);
    void die(const char *message);
    void dies(Register r);
};
