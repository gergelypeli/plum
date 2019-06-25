
enum SimpleOp {
    NOP, POPFQ, PUSHFQ, RET, UD2
};


enum UnaryOp {
    DECB=0, DECW, DECD, DECQ,
    INCB=4, INCW, INCD, INCQ,
    NEGB=8, NEGW, NEGD, NEGQ,
    NOTB=16, NOTW, NOTD, NOTQ,
};

inline UnaryOp operator%(UnaryOp x, int y) { return (UnaryOp)((x & ~3) | (y & 3)); }


enum DivModOp {
    DIVB=0, DIVW, DIVD, DIVQ,
    MODB=4, MODW, MODD, MODQ,
    IDIVB=8, IDIVW, IDIVD, IDIVQ,
    IMODB=12, IMODW, IMODD, IMODQ,
};

inline DivModOp operator%(DivModOp x, int y) { return (DivModOp)((x & ~3) | (y & 3)); }


enum StringOp {
    REPMOVSB,
    REPECMPSW
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


enum RegisterFirstOp {
    IMUL2B_=0, IMUL2W, IMUL2D, IMUL2Q,
    MOVSXBQ=7,
    MOVSXWQ=11,
    MOVSXDQ=15,
    MOVZXBQ=19,
    MOVZXWQ=23,
    MOVZXDQ=27,
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


class Emu: public Emitter {
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

    std::map<unsigned, Def> defs;
    
    std::vector<char> code;
    std::vector<char> data;

    Elf *elf;

    Emu();
    virtual ~Emu();
    
    virtual void process_definitions();
    virtual void process_relocations() =0;
    virtual void done(std::string filename);

    void add_def(Label label, const Def &def);

    virtual void data_reference(Label label, int addend = 0) =0;

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

    virtual unsigned get_pc();
    virtual unsigned get_dc();

    virtual std::array<Register, 4> abi_arg_regs() =0;
    virtual std::array<SseRegister, 4> abi_arg_sses() =0;
    virtual std::array<Register, 2> abi_res_regs() =0;
    virtual std::array<SseRegister, 2> abi_res_sses() =0;
    virtual void prologue() =0;
    virtual void epilogue() =0;
    virtual void start() =0;
    virtual int dwarf_register_number(Register r) =0;
    virtual int dwarf_sseregister_number(SseRegister s) =0;

    virtual void op(SimpleOp opcode) =0;
    virtual void op(UnaryOp opcode, Register x) =0;
    virtual void op(UnaryOp opcode, Address x) =0;
    virtual void op(DivModOp opcode, Register x, Register y) =0;
    virtual void op(StringOp opcode) =0;
    virtual void op(BinaryOp opcode, Register x, int y) =0;
    virtual void op(BinaryOp opcode, Address x, int y) =0;
    virtual void op(BinaryOp opcode, Register x, Register y) =0;
    virtual void op(BinaryOp opcode, Address x, Register y) =0;
    virtual void op(BinaryOp opcode, Register x, Address y) =0;
    virtual void op(MovabsOp opcode, Register x, int64 y) =0;  // 64-bit immediate capable
    virtual void op(ShiftOp opcode, Register x, Register y) =0;
    virtual void op(ShiftOp opcode, Register x, char y) =0;
    virtual void op(ShiftOp opcode, Address x, char y) =0;
    virtual void op(ExchangeOp opcode, Register x, Register y) =0;
    virtual void op(ExchangeOp opcode, Address x, Register y) =0;
    virtual void op(ExchangeOp opcode, Register x, Address y) =0;
    virtual void op(StackOp opcode, int x) =0;
    virtual void op(StackOp opcode, Register x) =0;
    virtual void op(StackOp opcode, Address x) =0;
    virtual void op(RegisterFirstOp opcode, Register x, Register y) =0;
    virtual void op(RegisterFirstOp opcode, Register x, Address y) =0;
    virtual void op(Imul3Op opcode, Register x, Register y, int z) =0;
    virtual void op(Imul3Op opcode, Register x, Address y, int z) =0;
    virtual void op(RegisterMemoryOp opcode, Register x, Address y) =0;
    virtual void op(BitSetOp, Register x) =0;
    virtual void op(BitSetOp, Address x) =0;
    virtual void op(BranchOp opcode, Label c) =0;
    virtual void op(JumpOp opcode, Label c) =0;
    virtual void op(JumpOp opcode, Address x) =0;
    virtual void op(JumpOp opcode, Register x) =0;
    
    virtual void op(SsememSsememOp opcode, SseRegister x, SseRegister y) =0;
    virtual void op(SsememSsememOp opcode, SseRegister x, Address y) =0;
    virtual void op(SsememSsememOp opcode, Address x, SseRegister y) =0;
    
    virtual void op(SseSsememOp opcode, SseRegister x, SseRegister y) =0;
    virtual void op(SseSsememOp opcode, SseRegister x, Address y) =0;

    virtual void op(SseGprmemOp opcode, SseRegister x, Register y) =0;
    virtual void op(SseGprmemOp opcode, SseRegister x, Address y) =0;

    virtual void op(GprSsememOp opcode, Register x, SseRegister y) =0;
    virtual void op(GprSsememOp opcode, Register x, Address y) =0;
    
    virtual void floatcmp(ConditionCode cc, SseRegister x, SseRegister y) =0;
    virtual void floatorder(SseRegister x, SseRegister y) =0;
};
