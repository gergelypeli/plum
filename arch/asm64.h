
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

inline RexFlags operator |(RexFlags x, RexFlags y) { return (RexFlags)((int)x | (int)y); }


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


class Asm64 {
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


    std::vector<char> code;
    std::vector<char> data;
    std::map<unsigned, Def> defs;
    std::vector<Ref> refs;
    
    Elf *elf;

    Asm64(std::string module_name);
    ~Asm64();
    
    void relocate();
    void done(std::string filename);

    void add_def(Label label, const Def &def);

    void absolute_label(Label c, unsigned64 value, unsigned size = 0);

    void data_align(int bytes);
    void data_blob(void *blob, int length);
    void data_byte(char x);
    void data_word(int16 x);
    void data_dword(int x);
    void data_qword(int64 x);
    void data_zstring(std::string s);
    void data_double(double x);
    void data_label(Label c, unsigned size = 0);
    void data_label_local(Label c, std::string name, unsigned size = 0);
    void data_label_global(Label c, std::string name, unsigned size = 0);
    void data_reference(Label c);

    void code_byte(char x);
    void code_word(int16 x);
    void code_dword(int x);
    void code_qword(int64 x);
    void code_label(Label c, unsigned size = 0);
    void code_label_import(Label c, std::string name);
    void code_label_local(Label c, std::string name, unsigned size = 0);
    void code_label_global(Label c, std::string name, unsigned size = 0);
    void code_reference(Label c, int offset = 0);
    int get_pc();
    
    void effective_address(int regfield, Register rm);
    void effective_address(int regfield, SseRegister rm);
    void effective_address(int regfield, Address rm);

    RexFlags q(Register r);
    RexFlags r(Register regfield);
    RexFlags r(SseRegister regfield);
    RexFlags xb(Register regfield);
    RexFlags xb(SseRegister regfield);
    RexFlags xb(Address rm);
    
    void rex(RexFlags wrxb, bool force = false);
    void prefixless_op(int opcode);
    void prefixed_op(int opcode, Opsize opsize, RexFlags rxbq = REX_NONE);
    
    void code_op(int opcode, Opsize opsize, Slash regfield, Register rm);
    void code_op(int opcode, Opsize opsize, Register regfield, Register rm);
    void code_op(int opcode, Opsize opsize, Slash regfield, Address rm);
    void code_op(int opcode, Opsize opsize, Register regfield, Address rm);
    void code_op(int opcode, Opsize opsize, SseRegister regfield, SseRegister rm);
    void code_op(int opcode, Opsize opsize, SseRegister regfield, Address rm);
    void code_op(int opcode, Opsize opsize, SseRegister regfield, Register rm);
    void code_op(int opcode, Opsize opsize, Register regfield, SseRegister rm);

    void op(SimpleOp opcode);
    void op(UnaryOp opcode, Register x);
    void op(UnaryOp opcode, Address x);
    void op(PortOp opcode);
    void op(PortOp opcode, int x);
    void op(StringOp opcode);
    void op(BinaryOp opcode, Register x, int y);
    void op(BinaryOp opcode, Address x, int y);
    void op(BinaryOp opcode, Register x, Register y);
    void op(BinaryOp opcode, Register x, HighByteRegister y);
    void op(BinaryOp opcode, Address x, Register y);
    void op(BinaryOp opcode, Register x, Address y);
    void op(MovabsOp opcode, Register x, int64 y);  // 64-bit immediate capable
    void op(ShiftOp opcode, Register x, Register cl);
    void op(ShiftOp opcode, Address x, Register cl);
    void op(ShiftOp opcode, Register x, char y);
    void op(ShiftOp opcode, Address x, char y);
    void op(ExchangeOp opcode, Register x, Register y);
    void op(ExchangeOp opcode, Address x, Register y);
    void op(ExchangeOp opcode, Register x, Address y);
    void op(StackOp opcode, int x);
    void op(StackOp opcode, Register x);
    void op(StackOp opcode, Address x);
    void op(MemoryOp opcode, Address x);
    void op(RegisterFirstOp opcode, Register x, Register y);
    void op(RegisterFirstOp opcode, Register x, Address y);
    void op(Imul3Op opcode, Register x, Register y, int z);
    void op(Imul3Op opcode, Register x, Address y, int z);
    void op(RegisterMemoryOp opcode, Register x, Address y);
    void op(BitSetOp, Register x);
    void op(BitSetOp, HighByteRegister x);
    void op(BitSetOp, Address x);
    void op(BranchOp opcode, Label c);
    void op(JumpOp opcode, Label c);
    void op(JumpOp opcode, Address x);
    void op(JumpOp opcode, Register x);
    void op(ConstantOp opcode, int x);
    
    void op(SsememSsememOp opcode, SseRegister x, SseRegister y);
    void op(SsememSsememOp opcode, SseRegister x, Address y);
    void op(SsememSsememOp opcode, Address x, SseRegister y);
    
    void op(SseSsememOp opcode, SseRegister x, SseRegister y);
    void op(SseSsememOp opcode, SseRegister x, Address y);

    void op(SseGprmemOp opcode, SseRegister x, Register y);
    void op(SseGprmemOp opcode, SseRegister x, Address y);

    void op(GprSsememOp opcode, Register x, SseRegister y);
    void op(GprSsememOp opcode, Register x, Address y);
    
    virtual bool is_accounting();
    virtual void adjust_stack_usage(int mod);
};
