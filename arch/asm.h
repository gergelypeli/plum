
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


class Accounter {
public:
    virtual bool is_on() =0;
    virtual void adjust_stack_usage(int mod) =0;
};


class Emitter {
public:
    virtual void code_byte(char x) =0;
    virtual void code_word(int16 x) =0;
    virtual void code_dword(int x) =0;
    virtual void code_qword(int64 x) =0;
};


class Asm {
public:
    Accounter *accounter;
    Emitter *emitter;
    
    Asm();
    virtual ~Asm();

    virtual void setup(Accounter *a, Emitter *e);
    
    virtual void code_byte(char x);
    virtual void code_word(int16 x);
    virtual void code_dword(int x);
    virtual void code_qword(int64 x);

    virtual bool is_accounting();
    virtual void adjust_stack_usage(int mod);
};
