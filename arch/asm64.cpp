#include "../plum.h"


Asm::Asm() {
}


Asm::~Asm() {
}


void Asm::add_def(Label label, const Def &def) {
    if (!label.def_index) {
        std::cerr << "Can't define an undeclared label!\n";
        throw ASM_ERROR;
    }

    if (defs.count(label.def_index)) {
        std::cerr << "Can't redefine label!\n";
        throw ASM_ERROR;
    }

    defs.insert(decltype(defs)::value_type(label.def_index, def));
}


void Asm::absolute_label(Label c, unsigned64 value, unsigned size) {
    add_def(c, Def(DEF_ABSOLUTE, value, size, "", false));
}


void Asm::data_align(int bytes) {
    data.resize((data.size() + (bytes - 1)) & ~(bytes - 1));
}


void Asm::data_blob(void *blob, int length) {
    data.resize(data.size() + length);
    memcpy(data.data() + data.size() - length, blob, length);
}


void Asm::data_byte(char x) {
    data.push_back(x);
}


void Asm::data_word(int16 x) {
    data.resize(data.size() + 2);
    *(short *)(data.data() + data.size() - 2) = x;
}


void Asm::data_dword(int x) {
    data.resize(data.size() + 4);
    *(int *)(data.data() + data.size() - 4) = x;
}


void Asm::data_qword(int64 x) {
    data.resize(data.size() + 8);
    *(int64 *)(data.data() + data.size() - 8) = x;
}


void Asm::data_zstring(std::string s) {
    for (char c : s)
        data.push_back(c);
        
    data.push_back(0);
}


void Asm::data_double(double x) {
    data.resize(data.size() + 8);
    *(double *)(data.data() + data.size() - 8) = x;
}


void Asm::data_label(Label c, unsigned size) {
    add_def(c, Def(DEF_DATA, data.size(), size, "", false));
}


void Asm::data_label_local(Label c, std::string name, unsigned size) {
    add_def(c, Def(DEF_DATA_EXPORT, data.size(), size, name, false));
}


void Asm::data_label_global(Label c, std::string name, unsigned size) {
    add_def(c, Def(DEF_DATA_EXPORT, data.size(), size, name, true));
}


void Asm::code_byte(char x) {
    code.push_back(x);
}


void Asm::code_word(int16 x) {
    code.resize(code.size() + 2);
    *(int16 *)(code.data() + code.size() - 2) = x;
}


void Asm::code_dword(int x) {
    code.resize(code.size() + 4);
    *(int *)(code.data() + code.size() - 4) = x;
}


void Asm::code_qword(int64 x) {
    code.resize(code.size() + 8);
    *(int64 *)(code.data() + code.size() - 8) = x;
}


void Asm::code_label(Label c, unsigned size) {
    add_def(c, Def(DEF_CODE, code.size(), size, "", false));
}


void Asm::code_label_import(Label c, std::string name) {
    add_def(c, Def(DEF_CODE_IMPORT, 0, 0, name, false));
}


void Asm::code_label_local(Label c, std::string name, unsigned size) {
    add_def(c, Def(DEF_CODE_EXPORT, code.size(), size, name, false));
}


void Asm::code_label_global(Label c, std::string name, unsigned size) {
    add_def(c, Def(DEF_CODE_EXPORT, code.size(), size, name, true));
}


int Asm::get_pc() {
    return code.size();
}


bool Asm::is_accounting() {
    return false;
}


void Asm::adjust_stack_usage(int mod) {
    throw ASM_ERROR;
}


// Asm_X64

Asm_X64::Opsize OPSIZE_LEGACY(int opcode) {
    return (
        (opcode & 3) == 0 ? Asm_X64::OPSIZE_LEGACY_BYTE :
        (opcode & 3) == 1 ? Asm_X64::OPSIZE_LEGACY_WORD :
        (opcode & 3) == 2 ? Asm_X64::OPSIZE_LEGACY_DWORD :
        (opcode & 3) == 3 ? Asm_X64::OPSIZE_LEGACY_QWORD :
        throw ASM_ERROR
    );
}

Asm_X64::Opsize OPSIZE_NONBYTE(int opcode) {
    return (
        (opcode & 3) == 0 ? throw ASM_ERROR :
        (opcode & 3) == 1 ? Asm_X64::OPSIZE_WORD :
        (opcode & 3) == 2 ? Asm_X64::OPSIZE_DEFAULT :
        (opcode & 3) == 3 ? Asm_X64::OPSIZE_QWORD :
        throw ASM_ERROR
    );
}

const int OPSIZE_WORD_PREFIX = 0x66;
const int OPSIZE_REX_PREFIX = 0x40;

Asm_X64::RexFlags operator |(Asm_X64::RexFlags x, Asm_X64::RexFlags y) { return (Asm_X64::RexFlags)((int)x | (int)y); }


Asm_X64::Asm_X64(Elf_X64 *e) {
    elf_x64 = e;
    
    op(UD2);  // Have fun jumping to address 0
}


Asm_X64::~Asm_X64() {
}


void Asm_X64::relocate() {
    for (auto &kv : defs) {
        Def &d(kv.second);

        switch (d.type) {
        case DEF_CODE: break;
        case DEF_DATA: break;
        case DEF_ABSOLUTE: break;
        case DEF_CODE_IMPORT:
            d.symbol_index = elf_x64->import(d.name);
            break;
        case DEF_CODE_EXPORT:
            d.symbol_index = elf_x64->export_code(d.name, d.location, d.size, d.is_global);
            break;
        case DEF_DATA_EXPORT:
            d.symbol_index = elf_x64->export_data(d.name, d.location, d.size, d.is_global);
            break;
        case DEF_ABSOLUTE_EXPORT:
            d.symbol_index = elf_x64->export_absolute(d.name, d.location, d.size, d.is_global);
            break;
        default:
            std::cerr << "He?\n";
            throw ASM_ERROR;
        }
    }

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
                int64 distance = d.location - (r.location + 1);
                    
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
            // The relocated part is assumed to be the last part of the instruction,
            // that is, at -4 bytes from the beginning of the next instruction.
            
            switch (d.type) {
            case DEF_CODE:
            case DEF_CODE_EXPORT: {
                int64 distance = d.location - (r.location + 4);
                
                if (distance > 2147483647 || distance < -2147483648) {
                    std::cerr << "REF_CODE_RELATIVE can't jump " << distance << " bytes!\n";
                    throw ASM_ERROR;
                }
                
                *(int *)&code[r.location] = (int)distance;
                }
                break;
            case DEF_CODE_IMPORT:
                elf_x64->code_relocation(d.symbol_index, r.location, -4);
                break;
            case DEF_DATA:
            case DEF_DATA_EXPORT:
                elf_x64->code_relocation(elf_x64->data_start_sym, r.location, d.location - 4);
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
                *(unsigned64 *)&data[r.location] = d.location;
                break;
            case DEF_DATA_EXPORT:
            case DEF_DATA:
                elf_x64->data_relocation(elf_x64->data_start_sym, r.location, d.location);
                break;
            case DEF_CODE:
            case DEF_CODE_EXPORT:
                elf_x64->data_relocation(elf_x64->code_start_sym, r.location, d.location);
                break;
            case DEF_CODE_IMPORT:
                elf_x64->data_relocation(d.symbol_index, r.location, 0);
                break;
            default:
                std::cerr << "Can't relocate data absolute to this symbol!\n";
                throw ASM_ERROR;
            }
            break;
        }
    }
}


void Asm_X64::done(std::string filename) {
    relocate();
    
    elf_x64->set_code(code);
    elf_x64->set_data(data);
    
    elf_x64->done(filename);
    
    delete elf_x64;
}


void Asm_X64::data_reference(Label label) {
    if (!label.def_index) {
        std::cerr << "Can't reference an undeclared label!\n";
        throw ASM_ERROR;
    }

    refs.push_back(Ref());
    Ref &r = refs.back();
    
    r.location = data.size();  // Store the beginning
    r.type = REF_DATA_ABSOLUTE;
    r.def_index = label.def_index;
    
    data_qword(0);  // 64-bit relocations only
}


void Asm_X64::code_reference(Label label, int offset) {
    if (!label.def_index) {
        std::cerr << "Can't reference an undeclared label!\n";
        throw ASM_ERROR;
    }

    //if (label.def_index == 1347)
    //    abort();

    refs.push_back(Ref());
    Ref &r = refs.back();

    r.location = code.size();  // Store the beginning!
    r.type = REF_CODE_RELATIVE;
    r.def_index = label.def_index;

    code_dword(offset);  // 32-bit offset only
    
    // TODO: shall we store 0 only, and put the offset into the addend?
}


Asm_X64::RexFlags Asm_X64::q(Register r) {
    return (r == SIL || r == DIL || r == SPL || r == BPL ? REX_Q : REX_NONE);
}


Asm_X64::RexFlags Asm_X64::r(Register regfield) {
    return
        (regfield >= 8 ? REX_R : REX_NONE);
}


Asm_X64::RexFlags Asm_X64::r(SseRegister regfield) {
    return
        (regfield >= 8 ? REX_R : REX_NONE);
}


Asm_X64::RexFlags Asm_X64::xb(Address rm) {
    return
        (rm.index != NOREG && rm.index >= 8 ? REX_X : REX_NONE) |
        (rm.base != NOREG && rm.base >= 8 ? REX_B : REX_NONE);
}


Asm_X64::RexFlags Asm_X64::xb(Register rm) {
    return
        (rm >= 8 ? REX_B : REX_NONE);
}


Asm_X64::RexFlags Asm_X64::xb(SseRegister rm) {
    return
        (rm >= 8 ? REX_B : REX_NONE);
}


void Asm_X64::rex(RexFlags wrxb, bool force) {
    if (wrxb || force)
        code_byte(OPSIZE_REX_PREFIX | wrxb);
}


void Asm_X64::prefixless_op(int code) {
    // Multi-byte opcodes must be emitted MSB first
    
    if (code & 0xFF00)
        code_byte((code >> 8) & 0xFF);

    code_byte(code & 0xFF);
}


void Asm_X64::prefixed_op(int code, Opsize opsize, RexFlags rxbq) {
    // size == 0 => byte  =>      _RXB op0
    // size == 1 => word  => 0x66 _RXB op1
    // size == 2 => dword =>      _RXB op1
    // size == 3 => qword =>      WRXB op1
    // size == 4 => INVALID
    // size == 5 => word  => 0x66 _RXB opc
    // size == 6 => dword =>      _RXB opc
    // size == 7 => qword =>      WRXB opc

    // SSE instructions may have fake operand size (0x66) and repeat (0xF2, 0xF3) prefixes
    // that are actually part of the opcode, but must be emitted before the REX prefix.
    // Those opcodes are effectively 3 bytes long.
    if (code & 0xFF0000) {
        code_byte((code >> 16) & 0xFF);
        code &= 0xFFFF;
    }

    bool questionable = rxbq & REX_Q;
    RexFlags rxb = (RexFlags)(rxbq & ~REX_Q);

    switch (opsize) {
    case OPSIZE_LEGACY_BYTE:
        // We force a REX prefix for byte operations to allow access to SIL and DIL.
        rex(rxb, questionable);
        code &= ~1;
        break;
    case OPSIZE_LEGACY_WORD:
        code_byte(OPSIZE_WORD_PREFIX);
        rex(rxb);
        code |= 1;
        break;
    case OPSIZE_LEGACY_DWORD:
        rex(rxb);
        code |= 1;
        break;
    case OPSIZE_LEGACY_QWORD:
        rex(REX_W | rxb);
        code |= 1;
        break;
    case OPSIZE_HIGH_BYTE:
        // Don't alter the lowest bit, assume byte operands, but don't use REX prefix,
        // because we want to use a high byte register. But check for the expected usage,
        // since we can only suppress the REX if the questionable flag is the only reason.
        if (rxb != REX_NONE || !questionable)
            throw ASM_ERROR;
        break;
    case OPSIZE_WORD:
        code_byte(OPSIZE_WORD_PREFIX);
        rex(rxb);
        break;
    case OPSIZE_DEFAULT:
        // The default opsize may be byte for some operations (bitsets!), and using the
        // questionable registers needs a forced REX prefix.
        rex(rxb, questionable);
        break;
    case OPSIZE_QWORD:
        rex(REX_W | rxb);
        break;
    default:
        throw ASM_ERROR;
    }

    prefixless_op(code);
}


void Asm_X64::effective_address(int regfield, Register rm) {
    // The cut off bits belong to the REX prefix
    code_byte(0xC0 | ((regfield & 7) << 3) | (rm & 7));
}


void Asm_X64::effective_address(int regfield, SseRegister rm) {
    // The cut off bits belong to the REX prefix
    code_byte(0xC0 | ((regfield & 7) << 3) | (rm & 7));
}


void Asm_X64::effective_address(int regfield, Address x) {
    // Quirks:
    // Offsetless RBP/R13 in r/m is interpreted as [RIP + disp32]
    // Offsetless RBP in SIB base is interpreted as [disp32]
    // RSP/R12 in r/m is interpreted as SIB byte following
    // RSP in SIB index means no index
    
    const int DISP0 = 0;
    const int DISP8 = 1;
    const int DISP32 = 2;
    
    const int SCALE1 = 0;
    
    const Register USE_SIB = RSP;
    const Register NO_INDEX = RSP;
    
    if (x.index == NO_INDEX) {
        std::cerr << "Oops, can't use RSP as SIB index register!\n";
        throw ASM_ERROR;
    }

    if (x.label.def_index != 0 && (x.base != NOREG || x.index != NOREG)) {
        std::cerr << "Oops, can't use base or index for RIP relative addressing!\n";
        throw ASM_ERROR;
    }

    if (x.base == NOREG && x.index == NOREG && x.offset == 0 && x.label.def_index == 0) {
        // This is likely an uninitialized Address, disallow
        std::cerr << "Null address used in instruction!\n";
        throw ASM_ERROR;
    }

    // The cut off bits belong to the REX prefix
    regfield &= 7;
    int base = x.base == NOREG ? NOREG : x.base & 7;  // RSP and R12 need a SIB
    int index = x.index == NOREG ? NOREG : x.index & 7;
    int scale = (x.scale == Address::SCALE_1 ? 0 : x.scale == Address::SCALE_2 ? 1 : x.scale == Address::SCALE_4 ? 2 : 3);
    int offset = x.offset;
    
    if (x.label.def_index != 0) {
        // Accessing a fixed memory location is only allowed using RIP-relative addressing,
        // so our code can be position independent.
        // Specifying DISP0 offsets with RBP/R13 base in r/m means [RIP+disp32].

        code_byte((DISP0 << 6) | (regfield << 3) | RBP);
        code_reference(x.label, offset);
    }
    else if (base == NOREG) {
        // Specifying DISP0 offsets with SIB with RBP/R13 base means [disp32].

        if (index != NOREG) {
            code_byte((DISP0 << 6) | (regfield << 3) | USE_SIB);
            code_byte((scale << 6) | (index << 3)    | RBP);  // R12 can be index
        }
        else {
            code_byte((DISP0 << 6)  | (regfield << 3) | USE_SIB);
            code_byte((SCALE1 << 6) | (NO_INDEX << 3) | RBP);  // disp32 only
        }
                
        code_dword(offset);
    }
    else if (offset == 0 && base != RBP) {
        // A RBP/R13 base means [RIP+disp32] in r/m base, and [disp32] in SIB base!
        // Omit offset
        
        if (index != NOREG) {  // need a SIB for index
            code_byte((DISP0 << 6)  | (regfield << 3) | USE_SIB);
            code_byte((scale << 6)  | (index << 3)    | base);  // R12 can be index
        }
        else if (base == USE_SIB) {  // need a SIB for RSP and R12 base
            code_byte((DISP0 << 6)  | (regfield << 3) | USE_SIB);
            code_byte((SCALE1 << 6) | (NO_INDEX << 3) | base);
        }
        else  // base only can be encoded in r/m if not RBP/R13/RSP/R12
            code_byte((DISP0 << 6)  | (regfield << 3) | base);
    }
    else if (offset <= 127 && offset >= -128) {
        // Byte offset
        
        if (index != NOREG) {  // need a SIB for index
            code_byte((DISP8 << 6)  | (regfield << 3) | USE_SIB);
            code_byte((scale << 6)  | (index << 3)    | base);  // R12 can be index
        }
        else if (base == USE_SIB) {  // need a SIB for RSP and R12 base
            code_byte((DISP8 << 6)  | (regfield << 3) | USE_SIB);
            code_byte((SCALE1 << 6) | (NO_INDEX << 3) | base);
        }
        else  // base with disp8 can be encoded in r/m if not RSP/R12
            code_byte((DISP8 << 6)  | (regfield << 3) | base);
            
        code_byte((char)offset);
    }
    else {
        // Dword offset
        
        if (index != NOREG) {  // need a SIB for index
            code_byte((DISP32 << 6) | (regfield << 3) | USE_SIB);
            code_byte((scale << 6)  | (index << 3)    | base);  // R12 can be index
        }
        else if (base == USE_SIB) {  // need a SIB for RSP and R12 base
            code_byte((DISP32 << 6) | (regfield << 3) | USE_SIB);
            code_byte((SCALE1 << 6) | (NO_INDEX << 3) | base);
        }
        else  // base with disp32 can be encoded in r/m if not RSP/R12
            code_byte((DISP32 << 6) | (regfield << 3) | base);

        code_dword(offset);
    }
}


void Asm_X64::code_op(int opcode, Opsize opsize, Slash regfield, Register rm) {
    prefixed_op(opcode, opsize, xb(rm) | q(rm));
    effective_address(regfield, rm);
}


void Asm_X64::code_op(int opcode, Opsize opsize, Register regfield, Register rm) {
    prefixed_op(opcode, opsize, r(regfield) | xb(rm) | q(regfield) | q(rm));
    effective_address(regfield, rm);
}


void Asm_X64::code_op(int opcode, Opsize opsize, Slash regfield, Address rm) {
    prefixed_op(opcode, opsize, xb(rm));
    effective_address(regfield, rm);
}


void Asm_X64::code_op(int opcode, Opsize opsize, Register regfield, Address rm) {
    prefixed_op(opcode, opsize, r(regfield) | xb(rm) | q(regfield));
    effective_address(regfield, rm);
}


void Asm_X64::code_op(int opcode, Opsize opsize, SseRegister regfield, SseRegister rm) {
    prefixed_op(opcode, opsize, r(regfield) | xb(rm));
    effective_address(regfield, rm);
}


void Asm_X64::code_op(int opcode, Opsize opsize, SseRegister regfield, Address rm) {
    prefixed_op(opcode, opsize, r(regfield) | xb(rm));
    effective_address(regfield, rm);
}


void Asm_X64::code_op(int opcode, Opsize opsize, SseRegister regfield, Register rm) {
    prefixed_op(opcode, opsize, r(regfield) | xb(rm) | q(rm));
    effective_address(regfield, rm);
}


void Asm_X64::code_op(int opcode, Opsize opsize, Register regfield, SseRegister rm) {
    prefixed_op(opcode, opsize, r(regfield) | xb(rm) | q(regfield));
    effective_address(regfield, rm);
}




int simple_info[] = {
     0x6698, 0x99, 0xF8, 0xFC, 0xFA, 0x0F06, 0xF5, 0x4899,
     0x6699, 0xF4, 0xCF, 0x9F, 0x90, 0x9D, 0x9C, 0xCB,
     0xC3, 0x9E, 0xF9, 0xFD, 0xFB, 0x0F0B
};


void Asm_X64::op(SimpleOp opcode) {
    prefixless_op(simple_info[opcode]);
}




struct {
    int op;
    Slash regfield;
} unary_info[] = {
    {0xFE,   SLASH_1},
    {0xF6,   SLASH_6},
    {0xF6,   SLASH_7},
    {0xF6,   SLASH_5},
    {0xFE,   SLASH_0},
    {0x0F00, SLASH_2},
    {0x0F00, SLASH_3},
    {0xF6,   SLASH_4},
    {0xF6,   SLASH_3},
    {0xF6,   SLASH_2},
    {0x0F00, SLASH_0},
    {0x0F00, SLASH_1},
    {0x0F00, SLASH_4},
    {0x0F00, SLASH_5}
};


void Asm_X64::op(UnaryOp opcode, Register x) {
    if (is_accounting() && x == RSP)
        throw ASM_ERROR;

    auto &info = unary_info[opcode >> 2];
    code_op(info.op, OPSIZE_LEGACY(opcode), info.regfield, x);
}

void Asm_X64::op(UnaryOp opcode, Address x) {
    auto &info = unary_info[opcode >> 2];
    code_op(info.op, OPSIZE_LEGACY(opcode), info.regfield, x);
}




void Asm_X64::op(PortOp opcode) {
    if ((opcode | 3) == INQ)
        prefixed_op(0xEC, OPSIZE_LEGACY(opcode));
    else
        prefixed_op(0xEE, OPSIZE_LEGACY(opcode));
}


void Asm_X64::op(PortOp opcode, int x) {
    if ((opcode | 3) == INQ)
        prefixed_op(0xE4, OPSIZE_LEGACY(opcode));
    else
        prefixed_op(0xE6, OPSIZE_LEGACY(opcode));
     
    code_byte(x);
}




int string_info[] = {
    0x6C, 0xAC, 0xA4, 0x6E, 0xAA,
    0xF36C, 0xF3AC, 0xF3A4, 0xF36E, 0xF3AA,
    0xA6, 0xAE,
    0xF3A6, 0xF3AE,
    0xF2A6, 0xF2AE,
};


void Asm_X64::op(StringOp opcode) {
    // 64-bit mode uses the RCX, RSI, RDI registers because of using 64-bit ADDRESS size.
    // The REP prefixes must precede the REX prefix, so we must encode it manually.
    // NOTE: REP MOVSB/STOSB is really fast on post Ivy Bridge processors, even if they
    // have a relatively high (~35 cycles?) setup cost. Even faster than MOVSQ/STOSQ.
    int info = string_info[opcode >> 2];
    
    if (info & 0xFF00)
        code_byte(info >> 8);
        
    prefixed_op(info & 0xFF, OPSIZE_LEGACY(opcode));
}




struct {
    int op1;
    Slash regfield1;
    int op2;
    int op3;
} binary_info[] = {
    {0x80, SLASH_2, 0x10, 0x12},
    {0x80, SLASH_0, 0x00, 0x02},
    {0x80, SLASH_4, 0x20, 0x22},
    {0x80, SLASH_7, 0x38, 0x3A},
    {0xC6, SLASH_0, 0x88, 0x8A},
    {0x80, SLASH_1, 0x08, 0x0A},
    {0x80, SLASH_3, 0x18, 0x1A},
    {0x80, SLASH_5, 0x28, 0x2A},
    {0xF6, SLASH_0, 0x84, 0x84},  // Look, it's symmetric!
    {0x80, SLASH_6, 0x30, 0x32}
};


void Asm_X64::op(BinaryOp opcode, Register x, int y) {
    if (is_accounting() && x == RSP) {
        if (opcode == ADDQ)
            adjust_stack_usage(-y);
        else if (opcode == SUBQ)
            adjust_stack_usage(y);
        else
            throw ASM_ERROR;
    }

    auto &info = binary_info[opcode >> 2];
    code_op(info.op1, OPSIZE_LEGACY(opcode), info.regfield1, x);
    
    switch (opcode & 3) {
    case 0: code_byte(y); break;
    case 1: code_word(y); break;
    case 2: code_dword(y); break;
    case 3: code_dword(y); break;  // 32-bit immediate only
    }
}

void Asm_X64::op(BinaryOp opcode, Address x, int y) {
    if (x.label.def_index) {
        // Must adjust RIP-relative offset with trailing immediate operand
        switch (opcode & 3) {
        case 0: x.offset -= 1; break;
        case 1: x.offset -= 2; break;
        case 2: x.offset -= 4; break;
        case 3: x.offset -= 4; break;  // 32-bit immediate only
        }
    }

    auto &info = binary_info[opcode >> 2];
    code_op(info.op1, OPSIZE_LEGACY(opcode), info.regfield1, x);
    
    switch (opcode & 3) {
    case 0: code_byte(y); break;
    case 1: code_word(y); break;
    case 2: code_dword(y); break;
    case 3: code_dword(y); break;  // 32-bit immediate only
    }
}

void Asm_X64::op(BinaryOp opcode, Register x, Register y) {
    if (is_accounting() && x == RSP)
        throw ASM_ERROR;

    if ((opcode | 3) == MOVQ && x == y)
        return;  // Don't embarrass ourselves

    auto &info = binary_info[opcode >> 2];
    code_op(info.op2, OPSIZE_LEGACY(opcode), y, x);
}

void Asm_X64::op(BinaryOp opcode, Register x, HighByteRegister y) {
    if (is_accounting() && x == RSP)
        throw ASM_ERROR;

    if ((opcode & 3) != 0)
        throw ASM_ERROR;  // Must use byte operands for this combination
        
    if (q(x))
        throw ASM_ERROR;  // Regular register mustn't be questionable

    auto &info = binary_info[opcode >> 2];
    code_op(info.op2, OPSIZE_HIGH_BYTE, (Register)y, x);
}

void Asm_X64::op(BinaryOp opcode, Address x, Register y) {
    auto &info = binary_info[opcode >> 2];
    code_op(info.op2, OPSIZE_LEGACY(opcode), y, x);
}

void Asm_X64::op(BinaryOp opcode, Register x, Address y) {
    if (is_accounting() && x == RSP)
        throw ASM_ERROR;

    auto &info = binary_info[opcode >> 2];
    code_op(info.op3, OPSIZE_LEGACY(opcode), x, y);
}




void Asm_X64::op(MovabsOp opcode, Register x, int64 y) {
    if (is_accounting() && x == RSP)
        throw ASM_ERROR;

    prefixed_op(0xB8 + (x & 7), OPSIZE_QWORD, xb(x));
    code_qword(y);
}




Slash shift_info[] = {
    SLASH_2,
    SLASH_3,
    SLASH_0,
    SLASH_1,
    SLASH_4,
    SLASH_7,
    SLASH_4,
    SLASH_5
};


// We explicitly take CL as an argument, because by convention we always
// spell it out. But CL would automatically convert to char, and encode
// the constant shifts! So calling these function with a second operand of CL
// would encode shifts by 1 (CL numeric value)!
void Asm_X64::op(ShiftOp opcode, Register x, Register cl) {
    if (is_accounting() && x == RSP)
        throw ASM_ERROR;

    if (cl != CL)
        throw ASM_ERROR;
        
    auto &info = shift_info[opcode >> 2];
    code_op(0xD2, OPSIZE_LEGACY(opcode), info, x);
}

void Asm_X64::op(ShiftOp opcode, Address x, Register cl) {
    if (cl != CL)
        throw ASM_ERROR;

    auto &info = shift_info[opcode >> 2];
    code_op(0xD2, OPSIZE_LEGACY(opcode), info, x);
}

void Asm_X64::op(ShiftOp opcode, Register x, char y) {
    if (is_accounting() && x == RSP)
        throw ASM_ERROR;

    auto &info = shift_info[opcode >> 2];

    if (y == 1) {
        code_op(0xD0, OPSIZE_LEGACY(opcode), info, x);
    }
    else {
        code_op(0xC0, OPSIZE_LEGACY(opcode), info, x);
        code_byte(y);
    }
}

void Asm_X64::op(ShiftOp opcode, Address x, char y) {
    auto &info = shift_info[opcode >> 2];

    if (y == 1) {
        code_op(0xD0, OPSIZE_LEGACY(opcode), info, x);
    }
    else {
        if (x.label.def_index) {
            // Must adjust RIP-relative offset with trailing immediate operand
            x.offset -= 1;
        }

        code_op(0xC0, OPSIZE_LEGACY(opcode), info, x);
        code_byte(y);
    }
}




void Asm_X64::op(ExchangeOp opcode, Register x, Register y) {
    if (is_accounting() && x == RSP)
        throw ASM_ERROR;

    code_op(0x86, OPSIZE_LEGACY(opcode), x, y);
}

void Asm_X64::op(ExchangeOp opcode, Address x, Register y) {
    code_op(0x86, OPSIZE_LEGACY(opcode), y, x);
}

void Asm_X64::op(ExchangeOp opcode, Register x, Address y) {
    if (is_accounting() && x == RSP)
        throw ASM_ERROR;

    code_op(0x86, OPSIZE_LEGACY(opcode), x, y);
}




void Asm_X64::op(StackOp opcode, int x) {
    if (opcode == PUSHQ) {
        if (is_accounting())
            adjust_stack_usage(8);

        code_byte(0x68);  // Defaults to 64-bit operand size
        code_dword(x);  // 32-bit immediate only
    }
    else
        throw ASM_ERROR;
}

void Asm_X64::op(StackOp opcode, Register x) {
    if (opcode == PUSHQ) {
        if (is_accounting())
            adjust_stack_usage(8);
            
        prefixed_op(0x50 | (x & 0x07), OPSIZE_DEFAULT, xb(x));
    }
    else if (opcode == POPQ) {
        if (is_accounting()) {
            if (x == RSP)
                throw ASM_ERROR;
            
            adjust_stack_usage(-8);
        }

        prefixed_op(0x58 | (x & 0x07), OPSIZE_DEFAULT, xb(x));
    }
    else
        throw ASM_ERROR;
}

void Asm_X64::op(StackOp opcode, Address x) {
    if (opcode == PUSHQ) {
        if (is_accounting())
            adjust_stack_usage(8);
            
        code_op(0xFF, OPSIZE_DEFAULT, SLASH_6, x);
    }
    else if (opcode == POPQ) {
        if (is_accounting())
            adjust_stack_usage(-8);
            
        // NOTE: POP with an RSP based address uses the RSP value after the pop!
        code_op(0x8F, OPSIZE_DEFAULT, SLASH_0, x);
    }
    else
        throw ASM_ERROR;
}




struct {
        int op;
        Slash regfield;
} memory_info[] = {
        {0x0F01, SLASH_2},
        {0x0F01, SLASH_3},
        {0x0F01, SLASH_0},
        {0x0F01, SLASH_1},
        {0xDF,   SLASH_5},
        {0xDF,   SLASH_7},
        {0xD9,   SLASH_7},
        {0xD9,   SLASH_5}
};

void Asm_X64::op(MemoryOp opcode, Address x) {
    auto &info = memory_info[opcode];
    code_op(info.op, OPSIZE_DEFAULT, info.regfield, x);
}




int registerfirst_info[] = {
    0x0FAF,
    0x0FBE,
    0x0FBF,
    0x63,  // MOVSXD with QWORD size sign extends a DWORD to QWORD
    0x0FB6,
    0x0FB7,
    0x63   // MOVSXD with DWORD size zero extends a DWORD to QWORD (like a normal MOVD)
};

void Asm_X64::op(RegisterFirstOp opcode, Register x, Register y) {
    if (is_accounting() && x == RSP)
        throw ASM_ERROR;

    auto &info = registerfirst_info[opcode >> 2];
    code_op(info, OPSIZE_NONBYTE(opcode), x, y);
}

void Asm_X64::op(RegisterFirstOp opcode, Register x, Address y) {
    if (is_accounting() && x == RSP)
        throw ASM_ERROR;

    auto &info = registerfirst_info[opcode >> 2];
    code_op(info, OPSIZE_NONBYTE(opcode), x, y);
}




void Asm_X64::op(Imul3Op opcode, Register x, Register y, int z) {
    if (is_accounting() && x == RSP)
        throw ASM_ERROR;

    if (z >= -128 && z <= 127) {
        code_op(0x6B, OPSIZE_NONBYTE(opcode), x, y);
        code_byte(z);
    }
    else {
        code_op(0x69, OPSIZE_NONBYTE(opcode), x, y);

        switch (opcode & 3) {
        case 0: throw ASM_ERROR;
        case 1: code_word(z); break;
        case 2: code_dword(z); break;
        case 3: code_dword(z); break;  // 32-bit immediate only
        }
    }
}

void Asm_X64::op(Imul3Op opcode, Register x, Address y, int z) {
    if (is_accounting() && x == RSP)
        throw ASM_ERROR;

    if (y.label.def_index) {
        // Must adjust RIP-relative offset with trailing immediate operand
        if (z >= -128 && z <= 127) {
            y.offset -= 1;
        }
        else switch (opcode & 3) {
            case 0: throw ASM_ERROR;
            case 1: y.offset -= 2; break;
            case 2: y.offset -= 4; break;
            case 3: y.offset -= 4; break;  // 32-bit immediate only
        }
    }

    if (z >= -128 && z <= 127) {
        code_op(0x6B, OPSIZE_NONBYTE(opcode), x, y);
        code_byte(z);
    }
    else {
        code_op(0x69, OPSIZE_NONBYTE(opcode), x, y);

        switch (opcode & 3) {
        case 0: throw ASM_ERROR;
        case 1: code_word(z); break;
        case 2: code_dword(z); break;
        case 3: code_dword(z); break;  // 32-bit immediate only
        }
    }
}




int registermemory_info[] = {
    0x8D
};


void Asm_X64::op(RegisterMemoryOp opcode, Register x, Address y) {
    if (opcode == LEA) {
        if (is_accounting() && x == RSP) {
            if (y.base == RSP && y.index == NOREG)
                adjust_stack_usage(-y.offset);
            else
                throw ASM_ERROR;
        }

        auto &info = registermemory_info[opcode];
        code_op(info, OPSIZE_QWORD, x, y);
    }
    else
        throw ASM_ERROR;
}




void Asm_X64::op(BitSetOp opcode, Register x) {
    if (is_accounting() && x == RSP)
        throw ASM_ERROR;

    code_op(0x0F90 | opcode, OPSIZE_DEFAULT, SLASH_0, x);
}


void Asm_X64::op(BitSetOp opcode, HighByteRegister x) {
    code_op(0x0F90 | opcode, OPSIZE_HIGH_BYTE, SLASH_0, (Register)x);
}


void Asm_X64::op(BitSetOp opcode, Address x) {
    code_op(0x0F90 | opcode, OPSIZE_DEFAULT, SLASH_0, x);
}




void Asm_X64::op(BranchOp opcode, Label c) {
    prefixless_op(0x0F80 | opcode);
    code_reference(c);
}




void Asm_X64::op(JumpOp opcode, Label c) {
    if (opcode == CALL) {
        if (is_accounting()) {
            adjust_stack_usage(16);
            adjust_stack_usage(-16);
        }
            
        prefixless_op(0xE8);
        code_reference(c);
    }
    else if (opcode == JMP) {
        prefixless_op(0xE9);
        code_reference(c);
    }
    else
        throw ASM_ERROR;
}


void Asm_X64::op(JumpOp opcode, Address x) {
    if (opcode == CALL) {
        if (is_accounting()) {
            adjust_stack_usage(16);
            adjust_stack_usage(-16);
        }
            
        code_op(0xFF, OPSIZE_DEFAULT, SLASH_2, x);
    }
    else if (opcode == JMP) {
        code_op(0xFF, OPSIZE_DEFAULT, SLASH_4, x);
    }
    else
        throw ASM_ERROR;
}


void Asm_X64::op(JumpOp opcode, Register x) {
    if (opcode == CALL) {
        if (is_accounting()) {
            adjust_stack_usage(16);
            adjust_stack_usage(-16);
        }
            
        code_op(0xFF, OPSIZE_DEFAULT, SLASH_2, x);
    }
    else if (opcode == JMP) {
        code_op(0xFF, OPSIZE_DEFAULT, SLASH_4, x);
    }
    else
        throw ASM_ERROR;
}


void Asm_X64::op(ConstantOp opcode, int x) {
    if (is_accounting())
        throw ASM_ERROR;

    if (opcode == INT) {
        prefixless_op(0xCD);
        code_byte(x);
    }
    else if (opcode == RETX) {
        prefixless_op(0xC2);
        code_word(x);
    }
    else if (opcode == RETFX) {
        prefixless_op(0xCA);
        code_word(x);
    }
}


struct {
    int op1;
    int op2;
} ssemem_ssemem_info[] = {   // xmm1, xmm2/mem64    xmm1/mem64, xmm2
    { 0xF30F7E, 0x660FD6 },  // MOVQW
    { 0xF20F10, 0xF20F11 },  // MOVSD
    { 0xF30F10, 0xF30F11 },  // MOVSS
};

void Asm_X64::op(SsememSsememOp opcode, SseRegister x, SseRegister y) {
    code_op(ssemem_ssemem_info[opcode].op1, OPSIZE_DEFAULT, x, y);
}


void Asm_X64::op(SsememSsememOp opcode, SseRegister x, Address y) {
    code_op(ssemem_ssemem_info[opcode].op1, OPSIZE_DEFAULT, x, y);
}


void Asm_X64::op(SsememSsememOp opcode, Address x, SseRegister y) {
    code_op(ssemem_ssemem_info[opcode].op2, OPSIZE_DEFAULT, y, x);
}


int sse_ssemem_info[] = {  // xmm1, xmm2/mem64  Test REX placement!
    0xF20F58,  // ADDSD
    0xF20F5C,  // SUBSD
    0xF20F59,  // MULSD
    0xF20F5E,  // DIVSD
    0x660F2F,  // COMISD
    0x660F2E,  // UCOMISD
    0xF30F5A,  // CVTSS2SD
    0xF20F5A,  // CVTSD2SS
    0xF20F5F,  // MAXSD
    0xF20F5D,  // MINSD
    0xF20F51,  // SQRTSD
    0x660FEF,  // PXOR
};

void Asm_X64::op(SseSsememOp opcode, SseRegister x, SseRegister y) {
    code_op(sse_ssemem_info[opcode], OPSIZE_DEFAULT, x, y);
}

void Asm_X64::op(SseSsememOp opcode, SseRegister x, Address y) {
    code_op(sse_ssemem_info[opcode], OPSIZE_DEFAULT, x, y);
}


int sse_gprmem_info[] = {  // xmm1, reg64/mem64
    0xF20F2A,  // CVTSI2SD
};

void Asm_X64::op(SseGprmemOp opcode, SseRegister x, Register y) {
    code_op(sse_gprmem_info[opcode], OPSIZE_DEFAULT, x, y);
}

void Asm_X64::op(SseGprmemOp opcode, SseRegister x, Address y) {
    code_op(sse_gprmem_info[opcode], OPSIZE_DEFAULT, x, y);
}


int gpr_ssemem_info[] = {  // reg64, xmm1/mem64
    0xF20F2D,  // CVTSD2SI
    0xF20F2C,  // CVTTSD2SI
};

void Asm_X64::op(GprSsememOp opcode, Register x, SseRegister y) {
    if (is_accounting() && x == RSP)
        throw ASM_ERROR;

    code_op(gpr_ssemem_info[opcode], OPSIZE_DEFAULT, x, y);
}

void Asm_X64::op(GprSsememOp opcode, Register x, Address y) {
    if (is_accounting() && x == RSP)
        throw ASM_ERROR;

    code_op(gpr_ssemem_info[opcode], OPSIZE_DEFAULT, x, y);
}
