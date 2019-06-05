
enum SimpleOp {
    CBW, CDQ, CLC, CLD, CLI, CLTS, CMC, CQO,
    CWD, HLT, IRET, LAHF, NOP, POPFQ, PUSHFQ, RETF,
    RET, SAHF, STC, STD, STI, UD2
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

inline UnaryOp operator%(UnaryOp x, int y) { return (UnaryOp)((x & ~3) | (y & 3)); }


enum PortOp {
    INB=0, INW, IND, INQ,
    OUTB=4, OUTW, OUTD, OUTQ
};


enum StringOp {
    INSB=0, INSW, INSD, INSQ,
    LODSB=4, LODSW, LODSD, LODSQ,
    MOVSB=8, MOVSW, MOVSD_SORRY, MOVSQ,
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

inline BinaryOp operator%(BinaryOp x, int y) { return (BinaryOp)((x & ~3) | (y & 3)); }


enum MovabsOp {
    MOVABSQ
};


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

inline ShiftOp operator%(ShiftOp x, int y) { return (ShiftOp)((x & ~3) | (y & 3)); }


enum ExchangeOp {
    XCHGB=0, XCHGW, XCHGD, XCHGQ
};

inline ExchangeOp operator%(ExchangeOp x, int y) { return (ExchangeOp)((x & ~3) | (y & 3)); }


enum StackOp {
    PUSHQ, POPQ
};


enum MemoryOp {
    LGDT, LIDT, SGDT, SIDT, FILDQ, FISTPQ, FSTCW, FLDCW
};


enum RegisterFirstOp {
    IMUL2B_, IMUL2W, IMUL2D, IMUL2Q,
    MOVSXBB_, MOVSXBW, MOVSXBD, MOVSXBQ,
    MOVSXWB_, MOVSXWW_, MOVSXWD, MOVSXWQ,
    MOVSXDB_, MOVSXDW_, MOVSXDD_, MOVSXDQ,  // the DQ variant has QWORD size to sign extend
    MOVZXBB_, MOVZXBW, MOVZXBD, MOVZXBQ,
    MOVZXWB_, MOVZXWW_, MOVZXWD, MOVZXWQ,
    MOVZXDB_, MOVZXDW_, MOVZXDQ, MOVZXDQ_,  // the DQ variant has DWORD size to zero extend
};

inline RegisterFirstOp operator%(RegisterFirstOp x, int y) { return (RegisterFirstOp)((x & ~3) | (y & 3)); }


enum Imul3Op {
    IMUL3W=1, IMUL3D, IMUL3Q
};

inline Imul3Op operator%(Imul3Op x, int y) { return (Imul3Op)((x & ~3) | (y & 3)); }


enum RegisterMemoryOp {
    LEA
};


enum BranchOp {
    JO, JNO, JB, JAE, JE, JNE, JBE, JA,
    JS, JNS, JP, JNP, JL, JGE, JLE, JG
};


inline BranchOp branch(ConditionCode cc) {
    // Both enums are just condition bits, so converting between them is straightforward
    return cc != CC_NONE ? (BranchOp)cc : throw ASM_ERROR;
}


enum JumpOp {
    CALL, JMP
};


enum BitSetOp {
    SETO, SETNO, SETB, SETAE, SETE, SETNE, SETBE, SETA,
    SETS, SETNS, SETP, SETNP, SETL, SETGE, SETLE, SETG
};


inline BitSetOp bitset(ConditionCode cc) {
    // Both enums are just condition bits, so converting between them is straightforward
    return cc != CC_NONE ? (BitSetOp)cc : throw ASM_ERROR;
}


enum ConstantOp {
     INT, RETX, RETFX
};


enum SsememSsememOp {
    MOVQW, MOVSD, MOVSS
};


enum SseSsememOp {
    ADDSD, SUBSD, MULSD, DIVSD, COMISD, UCOMISD, CVTSS2SD, CVTSD2SS, MAXSD, MINSD, SQRTSD, PXOR
};


enum SseGprmemOp {
    CVTSI2SD
};


enum GprSsememOp {
    CVTSD2SI, CVTTSD2SI
};


class Asm {
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
        unsigned64 location;  // Can be arbitrary value for absolute symbols
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
    
    std::vector<char> code;
    std::vector<char> data;
    std::map<unsigned, Def> defs;
    
    Asm();
    virtual ~Asm();
    
    virtual void done(std::string filename) =0;

    virtual void add_def(Label label, const Def &def);
    
    virtual bool is_accounting();
    virtual void adjust_stack_usage(int mod);

    virtual void absolute_label(Label c, unsigned64 value, unsigned size = 0);

    virtual void data_align(int bytes);
    virtual void data_blob(void *blob, int length);
    virtual void data_byte(char x);
    virtual void data_word(int16 x);
    virtual void data_dword(int x);
    virtual void data_qword(int64 x);
    virtual void data_zstring(std::string s);
    virtual void data_double(double x);
    virtual void data_label(Label c, unsigned size = 0);
    virtual void data_label_local(Label c, std::string name, unsigned size = 0);
    virtual void data_label_global(Label c, std::string name, unsigned size = 0);

    virtual void code_byte(char x);
    virtual void code_word(int16 x);
    virtual void code_dword(int x);
    virtual void code_qword(int64 x);
    virtual void code_label(Label c, unsigned size = 0);
    virtual void code_label_import(Label c, std::string name);
    virtual void code_label_local(Label c, std::string name, unsigned size = 0);
    virtual void code_label_global(Label c, std::string name, unsigned size = 0);

    virtual int get_pc();
    
    virtual void op(SimpleOp opcode) =0;
    virtual void op(UnaryOp opcode, Register x) =0;
    virtual void op(UnaryOp opcode, Address x) =0;
    virtual void op(PortOp opcode) =0;
    virtual void op(PortOp opcode, int x) =0;
    virtual void op(StringOp opcode) =0;
    virtual void op(BinaryOp opcode, Register x, int y) =0;
    virtual void op(BinaryOp opcode, Address x, int y) =0;
    virtual void op(BinaryOp opcode, Register x, Register y) =0;
    virtual void op(BinaryOp opcode, Register x, HighByteRegister y) =0;
    virtual void op(BinaryOp opcode, Address x, Register y) =0;
    virtual void op(BinaryOp opcode, Register x, Address y) =0;
    virtual void op(MovabsOp opcode, Register x, int64 y) =0;  // 64-bit immediate capable
    virtual void op(ShiftOp opcode, Register x, Register cl) =0;
    virtual void op(ShiftOp opcode, Address x, Register cl) =0;
    virtual void op(ShiftOp opcode, Register x, char y) =0;
    virtual void op(ShiftOp opcode, Address x, char y) =0;
    virtual void op(ExchangeOp opcode, Register x, Register y) =0;
    virtual void op(ExchangeOp opcode, Address x, Register y) =0;
    virtual void op(ExchangeOp opcode, Register x, Address y) =0;
    virtual void op(StackOp opcode, int x) =0;
    virtual void op(StackOp opcode, Register x) =0;
    virtual void op(StackOp opcode, Address x) =0;
    virtual void op(MemoryOp opcode, Address x) =0;
    virtual void op(RegisterFirstOp opcode, Register x, Register y) =0;
    virtual void op(RegisterFirstOp opcode, Register x, Address y) =0;
    virtual void op(Imul3Op opcode, Register x, Register y, int z) =0;
    virtual void op(Imul3Op opcode, Register x, Address y, int z) =0;
    virtual void op(RegisterMemoryOp opcode, Register x, Address y) =0;
    virtual void op(BitSetOp, Register x) =0;
    virtual void op(BitSetOp, HighByteRegister x) =0;
    virtual void op(BitSetOp, Address x) =0;
    virtual void op(BranchOp opcode, Label c) =0;
    virtual void op(JumpOp opcode, Label c) =0;
    virtual void op(JumpOp opcode, Address x) =0;
    virtual void op(JumpOp opcode, Register x) =0;
    virtual void op(ConstantOp opcode, int x) =0;
    
    virtual void op(SsememSsememOp opcode, SseRegister x, SseRegister y) =0;
    virtual void op(SsememSsememOp opcode, SseRegister x, Address y) =0;
    virtual void op(SsememSsememOp opcode, Address x, SseRegister y) =0;
    
    virtual void op(SseSsememOp opcode, SseRegister x, SseRegister y) =0;
    virtual void op(SseSsememOp opcode, SseRegister x, Address y) =0;

    virtual void op(SseGprmemOp opcode, SseRegister x, Register y) =0;
    virtual void op(SseGprmemOp opcode, SseRegister x, Address y) =0;

    virtual void op(GprSsememOp opcode, Register x, SseRegister y) =0;
    virtual void op(GprSsememOp opcode, Register x, Address y) =0;
};


class Asm_X64: public Asm {
public:
    enum Opsize {
        OPSIZE_LEGACY_BYTE, OPSIZE_LEGACY_WORD, OPSIZE_LEGACY_DWORD, OPSIZE_LEGACY_QWORD,
        OPSIZE_HIGH_BYTE, OPSIZE_WORD, OPSIZE_DEFAULT, OPSIZE_QWORD
    };

    enum RexFlags {
        REX_NONE=0x00,
        REX_B=0x01,
        REX_X=0x02,
        REX_R=0x04,
        REX_W=0x08,
        REX_Q=0x10  // virtual flag, set if a register operand is SIL, DIL, SPL, BPL.
    };

    enum Ref_type {
        REF_CODE_SHORT,
        REF_CODE_RELATIVE,
        REF_DATA_ABSOLUTE,
    };
    
    struct Ref {
        Ref_type type;
        unsigned64 location;
        unsigned def_index;
    };

    std::vector<Ref> refs;
    
    Elf_X64 *elf_x64;

    Asm_X64(Elf_X64 *e);
    virtual ~Asm_X64();

    virtual void relocate();
    virtual void done(std::string filename);

    virtual void data_reference(Label c);

    virtual void code_reference(Label c, int offset = 0);
    
    virtual void effective_address(int regfield, Register rm);
    virtual void effective_address(int regfield, SseRegister rm);
    virtual void effective_address(int regfield, Address rm);

    virtual RexFlags q(Register r);
    virtual RexFlags r(Register regfield);
    virtual RexFlags r(SseRegister regfield);
    virtual RexFlags xb(Register regfield);
    virtual RexFlags xb(SseRegister regfield);
    virtual RexFlags xb(Address rm);
    
    virtual void rex(RexFlags wrxb, bool force = false);
    virtual void prefixless_op(int opcode);
    virtual void prefixed_op(int opcode, Opsize opsize, RexFlags rxbq = REX_NONE);
    
    virtual void code_op(int opcode, Opsize opsize, Slash regfield, Register rm);
    virtual void code_op(int opcode, Opsize opsize, Register regfield, Register rm);
    virtual void code_op(int opcode, Opsize opsize, Slash regfield, Address rm);
    virtual void code_op(int opcode, Opsize opsize, Register regfield, Address rm);
    virtual void code_op(int opcode, Opsize opsize, SseRegister regfield, SseRegister rm);
    virtual void code_op(int opcode, Opsize opsize, SseRegister regfield, Address rm);
    virtual void code_op(int opcode, Opsize opsize, SseRegister regfield, Register rm);
    virtual void code_op(int opcode, Opsize opsize, Register regfield, SseRegister rm);

    virtual void op(SimpleOp opcode);
    virtual void op(UnaryOp opcode, Register x);
    virtual void op(UnaryOp opcode, Address x);
    virtual void op(PortOp opcode);
    virtual void op(PortOp opcode, int x);
    virtual void op(StringOp opcode);
    virtual void op(BinaryOp opcode, Register x, int y);
    virtual void op(BinaryOp opcode, Address x, int y);
    virtual void op(BinaryOp opcode, Register x, Register y);
    virtual void op(BinaryOp opcode, Register x, HighByteRegister y);
    virtual void op(BinaryOp opcode, Address x, Register y);
    virtual void op(BinaryOp opcode, Register x, Address y);
    virtual void op(MovabsOp opcode, Register x, int64 y);  // 64-bit immediate capable
    virtual void op(ShiftOp opcode, Register x, Register cl);
    virtual void op(ShiftOp opcode, Address x, Register cl);
    virtual void op(ShiftOp opcode, Register x, char y);
    virtual void op(ShiftOp opcode, Address x, char y);
    virtual void op(ExchangeOp opcode, Register x, Register y);
    virtual void op(ExchangeOp opcode, Address x, Register y);
    virtual void op(ExchangeOp opcode, Register x, Address y);
    virtual void op(StackOp opcode, int x);
    virtual void op(StackOp opcode, Register x);
    virtual void op(StackOp opcode, Address x);
    virtual void op(MemoryOp opcode, Address x);
    virtual void op(RegisterFirstOp opcode, Register x, Register y);
    virtual void op(RegisterFirstOp opcode, Register x, Address y);
    virtual void op(Imul3Op opcode, Register x, Register y, int z);
    virtual void op(Imul3Op opcode, Register x, Address y, int z);
    virtual void op(RegisterMemoryOp opcode, Register x, Address y);
    virtual void op(BitSetOp, Register x);
    virtual void op(BitSetOp, HighByteRegister x);
    virtual void op(BitSetOp, Address x);
    virtual void op(BranchOp opcode, Label c);
    virtual void op(JumpOp opcode, Label c);
    virtual void op(JumpOp opcode, Address x);
    virtual void op(JumpOp opcode, Register x);
    virtual void op(ConstantOp opcode, int x);
    
    virtual void op(SsememSsememOp opcode, SseRegister x, SseRegister y);
    virtual void op(SsememSsememOp opcode, SseRegister x, Address y);
    virtual void op(SsememSsememOp opcode, Address x, SseRegister y);
    
    virtual void op(SseSsememOp opcode, SseRegister x, SseRegister y);
    virtual void op(SseSsememOp opcode, SseRegister x, Address y);

    virtual void op(SseGprmemOp opcode, SseRegister x, Register y);
    virtual void op(SseGprmemOp opcode, SseRegister x, Address y);

    virtual void op(GprSsememOp opcode, Register x, SseRegister y);
    virtual void op(GprSsememOp opcode, Register x, Address y);
};
