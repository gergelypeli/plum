#include "../plum.h"


Emu_X64::Emu_X64(std::string module_name) {
    elf_x64 = new Elf_X64(module_name);
    elf = elf_x64;
    
    asm_x64 = new Asm_X64();
    asm_x64->set_referrer_x64(this);
}


void Emu_X64::data_reference(Label label, int addend) {
    if (!label.def_index) {
        std::cerr << "Can't reference an undeclared label!\n";
        throw ASM_ERROR;
    }

    refs.push_back({ REF_DATA_ABSOLUTE, data.size(), label.def_index, addend });
    data_qword(0);  // 64-bit relocations only
}


void Emu_X64::code_reference(Label label, int addend) {
    if (!label.def_index) {
        std::cerr << "Can't reference an undeclared label!\n";
        throw ASM_ERROR;
    }

    refs.push_back({ REF_CODE_RELATIVE, code.size(), label.def_index, addend });
    code_dword(0);  // 32-bit offset only
}


void Emu_X64::process_relocations() {
    for (auto &r : refs) {
        if (!defs.count(r.def_index)) {
            std::cerr << "Reference to undefined label " << r.def_index << "!\n";
            throw ASM_ERROR;
        }
        
        Def &d(defs.at(r.def_index));

        switch (r.type) {
        case REF_CODE_SHORT:
            // 1-byte references from code to code.
            // May be used for RIP-relative short branching instructions.
            
            switch (d.type) {
            case DEF_CODE:
            case DEF_CODE_EXPORT: {
                int64 distance = d.location - r.location + r.addend;
                    
                if (distance > 127 || distance < -128) {
                    std::cerr << "REF_CODE_SHORT can't jump " << distance << " bytes!\n";
                    throw ASM_ERROR;
                }
                
                code[r.location] = (char)distance;
                }
                break;
            default:
                std::cerr << "Can't short jump to this symbol!\n";
                throw ASM_ERROR;
            }
            break;
            
        case REF_CODE_RELATIVE:
            // 4-byte relative references from code to code or data.
            // May be used for RIP-relative control transfer or data access.
            
            switch (d.type) {
            case DEF_CODE:
            case DEF_CODE_EXPORT: {
                int64 distance = d.location - r.location + r.addend;
                
                if (distance > 2147483647 || distance < -2147483648) {
                    std::cerr << "REF_CODE_RELATIVE can't jump " << distance << " bytes!\n";
                    throw ASM_ERROR;
                }
                
                *(int *)&code[r.location] = (int)distance;
                }
                break;
            case DEF_CODE_IMPORT:
                elf_x64->code_relocation(d.symbol_index, r.location, r.addend);
                break;
            case DEF_DATA:
            case DEF_DATA_EXPORT:
                elf_x64->code_relocation(elf_x64->data_start_sym, r.location, d.location + r.addend);
                break;
            default:
                std::cerr << "Can't relocate code relative to this symbol!\n";
                throw ASM_ERROR;
            }
            break;
            
        case REF_DATA_ABSOLUTE:
            // 8-byte absolute references from data to code, data or absolute values.
            // May be used for intra-data absolute addresses, or 8-byte constants.
            
            switch (d.type) {
            case DEF_ABSOLUTE:
            case DEF_ABSOLUTE_EXPORT:
                *(unsigned64 *)&data[r.location] = d.location + r.addend;
                break;
            case DEF_DATA_EXPORT:
            case DEF_DATA:
                elf_x64->data_relocation(elf_x64->data_start_sym, r.location, d.location + r.addend);
                break;
            case DEF_CODE:
            case DEF_CODE_EXPORT:
                elf_x64->data_relocation(elf_x64->code_start_sym, r.location, d.location + r.addend);
                break;
            case DEF_CODE_IMPORT:
                elf_x64->data_relocation(d.symbol_index, r.location, r.addend);
                break;
            default:
                std::cerr << "Can't relocate data absolute to this symbol!\n";
                throw ASM_ERROR;
            }
            break;
        }
    }
}


#define MAP(OP) x == OP ? X::OP : 

static X::SimpleOp map(SimpleOp x) {
    return
        MAP(CBW) MAP(CBW) MAP(CDQ) MAP(CLC) MAP(CLD) MAP(CLI) MAP(CLTS) MAP(CMC) MAP(CQO)
        MAP(CWD) MAP(HLT) MAP(IRET) MAP(LAHF) MAP(NOP) MAP(POPFQ) MAP(PUSHFQ) MAP(RETF)
        MAP(RET) MAP(SAHF) MAP(STC) MAP(STD) MAP(STI) MAP(UD2)
        throw ASM_ERROR;
}

static X::UnaryOp map(UnaryOp x) {
    return
        MAP(DECB) MAP(DECW) MAP(DECD) MAP(DECQ)
        MAP(DIVB) MAP(DIVW) MAP(DIVD) MAP(DIVQ)
        MAP(IDIVB) MAP(IDIVW) MAP(IDIVD) MAP(IDIVQ)
        MAP(IMULB) MAP(IMULW) MAP(IMULD) MAP(IMULQ)
        MAP(INCB) MAP(INCW) MAP(INCD) MAP(INCQ)
        MAP(LLDT)
        MAP(LTR)
        MAP(MULB) MAP(MULW) MAP(MULD) MAP(MULQ)
        MAP(NEGB) MAP(NEGW) MAP(NEGD) MAP(NEGQ)
        MAP(NOTB) MAP(NOTW) MAP(NOTD) MAP(NOTQ)
        MAP(SLDT)
        MAP(STR)
        MAP(VERR)
        MAP(VERW)
        throw ASM_ERROR;
};


static X::PortOp map(PortOp x) {
    return
        MAP(INB) MAP(INW) MAP(IND) MAP(INQ)
        MAP(OUTB) MAP(OUTW) MAP(OUTD) MAP(OUTQ)
        throw ASM_ERROR;
};


static X::StringOp map(StringOp x) {
    return
        MAP(INSB) MAP(INSW) MAP(INSD) MAP(INSQ)
        MAP(LODSB) MAP(LODSW) MAP(LODSD) MAP(LODSQ)
        MAP(MOVSB) MAP(MOVSW) MAP(MOVSD_SORRY) MAP(MOVSQ)
        MAP(OUTSB) MAP(OUTSW) MAP(OUTSD) MAP(OUTSQ)
        MAP(STOSB) MAP(STOSW) MAP(STOSD) MAP(STOSQ)
        MAP(REPINSB) MAP(REPINSW) MAP(REPINSD) MAP(REPINSQ)
        MAP(REPLODSB) MAP(REPLODSW) MAP(REPLODSD) MAP(REPLODSQ)
        MAP(REPMOVSB) MAP(REPMOVSW) MAP(REPMOVSD) MAP(REPMOVSQ)
        MAP(REPOUTSB) MAP(REPOUTSW) MAP(REPOUTSD) MAP(REPOUTSQ)
        MAP(REPSTOSB) MAP(REPSTOSW) MAP(REPSTOSD) MAP(REPSTOSQ)
        MAP(CMPSB) MAP(CMPSW) MAP(CMPSD) MAP(CMPSQ)
        MAP(SCASB) MAP(SCASW) MAP(SCASD) MAP(SCASQ)
        MAP(REPECMPSB) MAP(REPECMPSW) MAP(REPECMPSD) MAP(REPECMPSQ)
        MAP(REPESCASB) MAP(REPESCASW) MAP(REPESCASD) MAP(REPESCASQ)
        MAP(REPNECMPSB) MAP(REPNECMPSW) MAP(REPNECMPSD) MAP(REPNECMPSQ)
        MAP(REPNESCASB) MAP(REPNESCASW) MAP(REPNESCASD) MAP(REPNESCASQ)
        throw ASM_ERROR;
};


static X::BinaryOp map(BinaryOp x) {
    return
        MAP(ADCB) MAP(ADCW) MAP(ADCD) MAP(ADCQ)
        MAP(ADDB) MAP(ADDW) MAP(ADDD) MAP(ADDQ)
        MAP(ANDB) MAP(ANDW) MAP(ANDD) MAP(ANDQ)
        MAP(CMPB) MAP(CMPW) MAP(CMPD) MAP(CMPQ)
        MAP(MOVB) MAP(MOVW) MAP(MOVD) MAP(MOVQ)
        MAP(ORB) MAP(ORW) MAP(ORD) MAP(ORQ)
        MAP(SBBB) MAP(SBBW) MAP(SBBD) MAP(SBBQ)
        MAP(SUBB) MAP(SUBW) MAP(SUBD) MAP(SUBQ)
        MAP(TESTB) MAP(TESTW) MAP(TESTD) MAP(TESTQ)
        MAP(XORB) MAP(XORW) MAP(XORD) MAP(XORQ)
        throw ASM_ERROR;
};


static X::MovabsOp map(MovabsOp x) {
    return
        MAP(MOVABSQ)
        throw ASM_ERROR;
};


static X::ShiftOp map(ShiftOp x) {
    return
        MAP(RCLB) MAP(RCLW) MAP(RCLD) MAP(RCLQ)
        MAP(RCRB) MAP(RCRW) MAP(RCRD) MAP(RCRQ)
        MAP(ROLB) MAP(ROLW) MAP(ROLD) MAP(ROLQ)
        MAP(RORB) MAP(RORW) MAP(RORD) MAP(RORQ)
        MAP(SALB) MAP(SALW) MAP(SALD) MAP(SALQ)
        MAP(SARB) MAP(SARW) MAP(SARD) MAP(SARQ)
        MAP(SHLB) MAP(SHLW) MAP(SHLD) MAP(SHLQ)
        MAP(SHRB) MAP(SHRW) MAP(SHRD) MAP(SHRQ)
        throw ASM_ERROR;
};


static X::ExchangeOp map(ExchangeOp x) {
    return
        MAP(XCHGB) MAP(XCHGW) MAP(XCHGD) MAP(XCHGQ)
        throw ASM_ERROR;
};


static X::StackOp map(StackOp x) {
    return
        MAP(PUSHQ) MAP(POPQ)
        throw ASM_ERROR;
};


static X::MemoryOp map(MemoryOp x) {
    return
        MAP(LGDT) MAP(LIDT) MAP(SGDT) MAP(SIDT) MAP(FILDQ) MAP(FISTPQ) MAP(FSTCW) MAP(FLDCW)
        throw ASM_ERROR;
};


static X::RegisterFirstOp map(RegisterFirstOp x) {
    return
        MAP(IMUL2B_) MAP(IMUL2W) MAP(IMUL2D) MAP(IMUL2Q)
        MAP(MOVSXBB_) MAP(MOVSXBW) MAP(MOVSXBD) MAP(MOVSXBQ)
        MAP(MOVSXWB_) MAP(MOVSXWW_) MAP(MOVSXWD) MAP(MOVSXWQ)
        MAP(MOVSXDB_) MAP(MOVSXDW_) MAP(MOVSXDD_) MAP(MOVSXDQ)  // the DQ variant has QWORD size to sign extend
        MAP(MOVZXBB_) MAP(MOVZXBW) MAP(MOVZXBD) MAP(MOVZXBQ)
        MAP(MOVZXWB_) MAP(MOVZXWW_) MAP(MOVZXWD) MAP(MOVZXWQ)
        MAP(MOVZXDB_) MAP(MOVZXDW_) MAP(MOVZXDQ) MAP(MOVZXDQ_)  // the DQ variant has DWORD size to zero extend
        throw ASM_ERROR;
};


static X::Imul3Op map(Imul3Op x) {
    return
        MAP(IMUL3W) MAP(IMUL3D) MAP(IMUL3Q)
        throw ASM_ERROR;
};


static X::RegisterMemoryOp map(RegisterMemoryOp x) {
    return
        MAP(LEA)
        throw ASM_ERROR;
};


static X::BranchOp map(BranchOp x) {
    return
        MAP(JO) MAP(JNO) MAP(JB) MAP(JAE) MAP(JE) MAP(JNE) MAP(JBE) MAP(JA)
        MAP(JS) MAP(JNS) MAP(JP) MAP(JNP) MAP(JL) MAP(JGE) MAP(JLE) MAP(JG)
        throw ASM_ERROR;
};


static X::JumpOp map(JumpOp x) {
    return
        MAP(CALL) MAP(JMP)
        throw ASM_ERROR;
};


static X::BitSetOp map(BitSetOp x) {
    return
        MAP(SETO) MAP(SETNO) MAP(SETB) MAP(SETAE) MAP(SETE) MAP(SETNE) MAP(SETBE) MAP(SETA)
        MAP(SETS) MAP(SETNS) MAP(SETP) MAP(SETNP) MAP(SETL) MAP(SETGE) MAP(SETLE) MAP(SETG)
        throw ASM_ERROR;
};


static X::ConstantOp map(ConstantOp x) {
    return
         MAP(INT) MAP(RETX) MAP(RETFX)
        throw ASM_ERROR;
};


static X::SsememSsememOp map(SsememSsememOp x) {
    return
        MAP(MOVQW) MAP(MOVSD) MAP(MOVSS)
        throw ASM_ERROR;
};


static X::SseSsememOp map(SseSsememOp x) {
    return
        MAP(ADDSD) MAP(SUBSD) MAP(MULSD) MAP(DIVSD) MAP(COMISD) MAP(UCOMISD) MAP(CVTSS2SD) MAP(CVTSD2SS) MAP(MAXSD) MAP(MINSD) MAP(SQRTSD) MAP(PXOR)
        throw ASM_ERROR;
};


static X::SseGprmemOp map(SseGprmemOp x) {
    return
        MAP(CVTSI2SD)
        throw ASM_ERROR;
};


static X::GprSsememOp map(GprSsememOp x) {
    return
        MAP(CVTSD2SI) MAP(CVTTSD2SI)
        throw ASM_ERROR;
};


void Emu_X64::op(SimpleOp opcode) { asm_x64->op(map(opcode)); }
void Emu_X64::op(UnaryOp opcode, Register x) { asm_x64->op(map(opcode), x); }
void Emu_X64::op(UnaryOp opcode, Address x) { asm_x64->op(map(opcode), x); }
void Emu_X64::op(PortOp opcode) { asm_x64->op(map(opcode)); }
void Emu_X64::op(PortOp opcode, int x) { asm_x64->op(map(opcode), x); }
void Emu_X64::op(StringOp opcode) { asm_x64->op(map(opcode)); }
void Emu_X64::op(BinaryOp opcode, Register x, int y) { asm_x64->op(map(opcode), x, y); }
void Emu_X64::op(BinaryOp opcode, Address x, int y) { asm_x64->op(map(opcode), x, y); }
void Emu_X64::op(BinaryOp opcode, Register x, Register y) { asm_x64->op(map(opcode), x, y); }
void Emu_X64::op(BinaryOp opcode, Register x, HighByteRegister y) { asm_x64->op(map(opcode), x, y); }
void Emu_X64::op(BinaryOp opcode, Address x, Register y) { asm_x64->op(map(opcode), x, y); }
void Emu_X64::op(BinaryOp opcode, Register x, Address y) { asm_x64->op(map(opcode), x, y); }
void Emu_X64::op(MovabsOp opcode, Register x, int64 y) { asm_x64->op(map(opcode), x, y); }  // 64-bit immediate capable
void Emu_X64::op(ShiftOp opcode, Register x, Register cl) { asm_x64->op(map(opcode), x, cl); }
void Emu_X64::op(ShiftOp opcode, Address x, Register cl) { asm_x64->op(map(opcode), x, cl); }
void Emu_X64::op(ShiftOp opcode, Register x, char y) { asm_x64->op(map(opcode), x, y); }
void Emu_X64::op(ShiftOp opcode, Address x, char y) { asm_x64->op(map(opcode), x, y); }
void Emu_X64::op(ExchangeOp opcode, Register x, Register y) { asm_x64->op(map(opcode), x, y); }
void Emu_X64::op(ExchangeOp opcode, Address x, Register y) { asm_x64->op(map(opcode), x, y); }
void Emu_X64::op(ExchangeOp opcode, Register x, Address y) { asm_x64->op(map(opcode), x, y); }
void Emu_X64::op(StackOp opcode, int x) { asm_x64->op(map(opcode), x); }
void Emu_X64::op(StackOp opcode, Register x) { asm_x64->op(map(opcode), x); }
void Emu_X64::op(StackOp opcode, Address x) { asm_x64->op(map(opcode), x); }
void Emu_X64::op(MemoryOp opcode, Address x) { asm_x64->op(map(opcode), x); }
void Emu_X64::op(RegisterFirstOp opcode, Register x, Register y) { asm_x64->op(map(opcode), x, y); }
void Emu_X64::op(RegisterFirstOp opcode, Register x, Address y) { asm_x64->op(map(opcode), x, y); }
void Emu_X64::op(Imul3Op opcode, Register x, Register y, int z) { asm_x64->op(map(opcode), x, y, z); }
void Emu_X64::op(Imul3Op opcode, Register x, Address y, int z) { asm_x64->op(map(opcode), x, y, z); }
void Emu_X64::op(RegisterMemoryOp opcode, Register x, Address y) { asm_x64->op(map(opcode), x, y); }
void Emu_X64::op(BitSetOp opcode, Register x) { asm_x64->op(map(opcode), x); }
void Emu_X64::op(BitSetOp opcode, HighByteRegister x) { asm_x64->op(map(opcode), x); }
void Emu_X64::op(BitSetOp opcode, Address x) { asm_x64->op(map(opcode), x); }
void Emu_X64::op(BranchOp opcode, Label c) { asm_x64->op(map(opcode), c); }
void Emu_X64::op(JumpOp opcode, Label c) { asm_x64->op(map(opcode), c); }
void Emu_X64::op(JumpOp opcode, Address x) { asm_x64->op(map(opcode), x); }
void Emu_X64::op(JumpOp opcode, Register x) { asm_x64->op(map(opcode), x); }
void Emu_X64::op(ConstantOp opcode, int x) { asm_x64->op(map(opcode), x); }

void Emu_X64::op(SsememSsememOp opcode, SseRegister x, SseRegister y) { asm_x64->op(map(opcode), x, y); }
void Emu_X64::op(SsememSsememOp opcode, SseRegister x, Address y) { asm_x64->op(map(opcode), x, y); }
void Emu_X64::op(SsememSsememOp opcode, Address x, SseRegister y) { asm_x64->op(map(opcode), x, y); }

void Emu_X64::op(SseSsememOp opcode, SseRegister x, SseRegister y) { asm_x64->op(map(opcode), x, y); }
void Emu_X64::op(SseSsememOp opcode, SseRegister x, Address y) { asm_x64->op(map(opcode), x, y); }

void Emu_X64::op(SseGprmemOp opcode, SseRegister x, Register y) { asm_x64->op(map(opcode), x, y); }
void Emu_X64::op(SseGprmemOp opcode, SseRegister x, Address y) { asm_x64->op(map(opcode), x, y); }

void Emu_X64::op(GprSsememOp opcode, Register x, SseRegister y) { asm_x64->op(map(opcode), x, y); }
void Emu_X64::op(GprSsememOp opcode, Register x, Address y) { asm_x64->op(map(opcode), x, y); }
