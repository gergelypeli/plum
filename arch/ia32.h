#include <string>
#include <vector>

#include "ork.h"

enum Register {
    EAX=0, ECX, EDX, EBX, ESP, EBP, ESI, EDI,
    AX=0, CX, DX, BX, SP, BP, SI, DI,
    AL=0, CL, DL, BL, AH, CH, DH, BH,
    NOREG=-1
};


struct Label {
    unsigned def_index;
    
    Label();
    Label(int di);
    Label(const Label &c);
    
    explicit operator bool() const {
        return def_index > 0;
    }
};


struct Address {
    Label label;
    Register base;
    int offset;

    Address(Register r, int x = 0);
    Address(Label &c, int x = 0);
    Address operator + (int x);
    Address operator + (Label &c);
};


enum SimpleOp {
    CBW, CDQ, CLC, CLD, CLI, CLTS, CMC, CWD, CWDE, HLT, IRET, LAHF, NOP,
    POPA, POPF, PUSHA, PUSHF, RETF, RET, SAHF, STC, STD, STI, UD2, XLAT,
    FDIVP, FRNDINT, FMULP, FSUBRP, FINIT
};


enum UnaryOp {
    DECB=0, DECW, DECD,
    DIVB=4, DIVW, DIVD,
    IDIVB=8, IDIVW, IDIVD,
    IMULB=12, IMULW, IMULD,
    INCB=16, INCW, INCD,
    LLDT=20,
    LTR=24,
    MULB=28, MULW, MULD,
    NEGB=32, NEGW, NEGD,
    NOTB=36, NOTW, NOTD,
    SLDT=40,
    STR=44,
    VERR=48,
    VERW=52
};


enum PortOp {
    INB=0, INW, IND,
    OUTB=4, OUTW, OUTD
};


enum StringOp {
    INSB=0, INSW, INSD,
    LODSB=4, LODSW, LODSD,
    MOVSB=8, MOVSW, MOVSD,
    OUTSB=12, OUTSW, OUTSD,
    STOSB=16, STOSW, STOSD,
    REPINSB=20, REPINSW, REPINSD,
    REPLODSB=24, REPLODSW, REPLODSD,
    REPMOVSB=28, REPMOVSW, REPMOVSD,
    REPOUTSB=32, REPOUTSW, REPOUTSD,
    REPSTOSB=36, REPSTOSW, REPSTOSD,
    CMPSB=40, CMPSW, CMPSD,
    SCASB=44, SCASW, SCASD,
    REPECMPSB=48, REPECMPSW, REPECMPSD,
    REPESCASB=52, REPESCASW, REPESCASD,
    REPNECMPSB=56, REPNECMPSW, REPNECMPSD,
    REPNESCASB=60, REPNESCASW, REPNESCASD,
};


enum BinaryOp {
    ADCB=0, ADCW, ADCD,
    ADDB=4, ADDW, ADDD,
    ANDB=8, ANDW, ANDD,
    CMPB=12, CMPW, CMPD,
    MOVB=16, MOVW, MOVD,
    ORB=20, ORW, ORD,
    SBBB=24, SBBW, SBBD,
    SUBB=28, SUBW, SUBD,
    TESTB=32, TESTW, TESTD,
    XORB=36, XORW, XORD
};


enum ShiftOp {
    RCLB=0, RCLW, RCLD,
    RCRB=4, RCRW, RCRD,
    ROLB=8, ROLW, ROLD,
    RORB=12, RORW, RORD,
    SALB=16, SALW, SALD,
    SARB=20, SARW, SARD,
    SHLB=24, SHLW, SHLD,
    SHRB=28, SHRW, SHRD
};


enum ExchangeOp {
    XCHGB=0, XCHGW, XCHGD
};


enum StackOp {
    PUSHD, POPD
};


enum MemoryOp {
      LGDT, LIDT, SGDT, SIDT, FILDQ, FISTPQ, FSTCW, FLDCW
};


enum RegisterFirstOp {
    IMUL2D, LAR, LSL
};


enum RegisterConstantOp {
    IMUL3D
};


enum RegisterSecondOp {
    ARPL, SHLDD, SHRDD
};


enum RegisterMemoryOp {
    LDS, LEA, LES
};


enum BranchOp {
    JO, JNO, JB, JAE, JE, JNE, JBE, JA, JS, JNS, JP, JNP, JL, JGE, JLE, JG
};


enum JumpOp {
    CALL, JMP, LOOP
};


enum BitSetOp {
    SETO, SETNO, SETB, SETAE, SETE, SETNE, SETBE, SETA,
    SETS, SETNS, SETP, SETNP, SETL, SETGE, SETLE, SETG
};


enum ConstantOp {
     INT, RETX, RETFX
};


class IA32 {
public:
    std::vector<char> code;
    std::vector<char> data;

    enum Def_target {
        DEF_NONE,
        DEF_CODE, DEF_DATA, DEF_ABSOLUTE,
        DEF_EXPORT_CODE, DEF_EXPORT_ABSOLUTE, DEF_EXPORT_DATA,
        DEF_IMPORT_CODE
    };
    
    struct Def {
        Def_target target;
        std::string name;
        int location;
        int size;
        int glob;
        int level;
    };
    
    std::vector<Def> defs;

    enum Ref_type {
        REF_CODE_SHORT, REF_CODE_RELATIVE, REF_CODE_ABSOLUTE, REF_DATA_ABSOLUTE
    };
    
    struct Ref {
        int location;
        Ref_type type;
        int def_index;
        int level;
    };
    
    std::vector<Ref> refs;

    int current_level;
    
    int ork_code_begin, ork_data_begin;
    Ork *ork;
    
    int last_opcode;

    Label make_label();
    void level_begin();
    void level_end();

    void data_byte(int x);
    void data_word(int x);
    void data_dword(int x);
    void data_label(Label c, int m = 0);
    int data_allocate(int m);
    void data_reference(Label c);

    void code_align();
    void code_byte(int x);
    void code_word(int x);
    void code_dword(int x);
    void effective_address(int modrm, Register x);
    void effective_address(int modrm, Address x);
    
    IA32();
    ~IA32();
    
    void init(std::string module_name);
    void done(std::string name);
    
    void code_label(Label c, int m = 0);
    void code_label_import(Label c, std::string name);
    void code_label_export(Label c, std::string name, int m, int glob);
    void absolute_label_export(Label c, std::string name, int value, int m, int glob);
    void data_label_export(Label c, std::string name, int m, int glob);
    void absolute_label(Label c, int value);
    int is_defined(Label c);
    void code_reference(Label c, Ref_type f, int offset = 0);

    void code_op(int opcode, int size);

    void movd(Register x, Register y);
    void addd(Address x, int y);
    void op(SimpleOp opcode);
    void op(UnaryOp opcode, Register x);
    void op(UnaryOp opcode, Address x);
    void op(PortOp opcode);
    void op(PortOp opcode, int x);
    void op(StringOp opcode);
    void op(BinaryOp opcode, Register x, Label c, int offset = 0);
    void op(BinaryOp opcode, Address x, Label c, int offset = 0);
    void op(BinaryOp opcode, Register x, int y);
    void op(BinaryOp opcode, Address x, int y);
    void op(BinaryOp opcode, Register x, Register y);
    void op(BinaryOp opcode, Address x, Register y);
    void op(BinaryOp opcode, Register x, Address y);
    void op(ShiftOp opcode, Register x);    // Ez a CL szerinti leptetes
    void op(ShiftOp opcode, Address x);    // Ez a CL szerinti leptetes
    void op(ShiftOp opcode, Register x, int y);
    void op(ShiftOp opcode, Address x, int y);
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
    void op(RegisterConstantOp opcode, Register x, Register y, int z);
    void op(RegisterSecondOp opcode, Register x, Register y);
    void op(RegisterSecondOp opcode, Address x, Register y);
    void op(RegisterMemoryOp opcode, Register x, Address y);
    void op(BitSetOp, Register x);
    void op(BitSetOp, Address x);
    void op(BranchOp opcode, Label c);
    void branch_instead_of_set(Label c, Register r);
    void op(JumpOp opcode, Label c);
    void op(JumpOp opcode, Address x);
    void op(ConstantOp opcode, int x);
};
