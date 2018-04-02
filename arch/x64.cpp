#include <iostream>

#include "x64.h"
#include "heap.h"


Opsize OPSIZE_LEGACY(int opcode) {
    return (
        (opcode & 3) == 0 ? OPSIZE_LEGACY_BYTE :
        (opcode & 3) == 1 ? OPSIZE_LEGACY_WORD :
        (opcode & 3) == 2 ? OPSIZE_LEGACY_DWORD :
        (opcode & 3) == 3 ? OPSIZE_LEGACY_QWORD :
        throw X64_ERROR
    );
}

Opsize OPSIZE_NONBYTE(int opcode) {
    return (
        (opcode & 3) == 0 ? throw X64_ERROR :
        (opcode & 3) == 1 ? OPSIZE_WORD :
        (opcode & 3) == 2 ? OPSIZE_DWORD :
        (opcode & 3) == 3 ? OPSIZE_QWORD :
        throw X64_ERROR
    );
}

const int OPSIZE_WORD_PREFIX = 0x66;
const int OPSIZE_REX_PREFIX = 0x40;

const int REX_Q = 0x10;  // virtual flag, set if a register operand is SIL, DIL, SPL, BPL.
const int REX_W = 0x08;
const int REX_R = 0x04;
const int REX_X = 0x02;
const int REX_B = 0x01;


std::ostream &operator << (std::ostream &os, const Register r) {
    os << (r == NOREG ? "---" : REGISTER_NAMES[r]);
    return os;
}


Address::Address() {
    base = NOREG;
    index = NOREG;
    scale = 0;
    offset = 0;
}


Address::Address(Register b, int o) {
    if (b == NOREG) {
        std::cerr << "Address without register!\n";
        throw X64_ERROR;
    }
          
    base = b;
    index = NOREG;
    scale = 0;
    offset = o;
}


Address::Address(Register b, Register i, int o) {
    if (b == NOREG) {
        std::cerr << "Address without register!\n";
        throw X64_ERROR;
    }
          
    base = b;
    index = i;
    scale = 1;
    offset = o;
}


Address::Address(Register b, Register i, int s, int o) {
    if (b == NOREG) {
        std::cerr << "Address without register!\n";
        throw X64_ERROR;
    }
    
    if (i != NOREG && s != 1 && s != 2 && s != 4 && s != 8)
        throw X64_ERROR;
    
    base = b;
    index = i;
    scale = s;
    offset = o;
}


Address Address::operator + (int x) {
    return Address(base, index, scale, offset + x);
}


X64::X64() {
    unwind = NULL;
    once = NULL;
    runtime = NULL;
}


X64::~X64() {
}


void X64::init(std::string module_name) {
    ork = new Ork;  // New Ork, New Ork...

    // symbol table indexes
    code_symbol_index = ork->export_code(module_name + ".code", 0, 0, true);
    data_symbol_index = ork->export_data(module_name + ".data", 0, 0, true);

    op(UD2);  // Have fun jumping to address 0
}


void X64::done(std::string filename) {
    for (auto &kv : defs) {
        Def &d(kv.second);

        switch (d.type) {
        case DEF_CODE: break;
        case DEF_DATA: break;
        case DEF_ABSOLUTE: break;
        case DEF_CODE_IMPORT:
            d.symbol_index = ork->import(d.name);
            break;
        case DEF_CODE_EXPORT:
            //debug << "PROCIEX: " << d.szoveg << "\n";
            d.symbol_index = ork->export_code(d.name, d.location, d.size, d.is_global);
            break;
        case DEF_DATA_EXPORT:
            d.symbol_index = ork->export_data(d.name, d.location, d.size, d.is_global);
            break;
        case DEF_ABSOLUTE_EXPORT:
            d.symbol_index = ork->export_absolute(d.name, d.location, d.size, d.is_global);
            break;
        default:
            std::cerr << "He?\n";
            throw X64_ERROR;
        }
    }

    for (auto &r : refs) {
        if (!defs.count(r.def_index)) {
            std::cerr << "Reference to undefined label " << r.def_index << "!\n";
            throw X64_ERROR;
        }
        
        Def &d(defs.at(r.def_index));

        switch (r.type) {
        case REF_CODE_SHORT:
            switch (d.type) {
            case DEF_CODE:
            case DEF_CODE_EXPORT: {
                int distance = d.location - (r.location + 1);
                    
                if (distance > 127 || distance < -128)
                    std::cerr << "REF_CODE_SHORT can't jump " << distance << " bytes!\n";
                else
                    code[r.location] = (char)distance;
                }
                break;
            default:
                std::cerr << "Can't short jump to this symbol!\n";
                throw X64_ERROR;
            }
            break;
            
        case REF_CODE_RELATIVE:
            // The offset must be the last part of the instruction
            
            switch (d.type) {
            case DEF_CODE:
            case DEF_CODE_EXPORT:
                *(int *)&code[r.location] += d.location - r.location - 4;
                break;
            case DEF_CODE_IMPORT:
                //*(int *)&code[r.location] += -4;
                ork->code_relocation(d.symbol_index, r.location, -4);
                break;
            case DEF_DATA:
            case DEF_DATA_EXPORT:
                ork->code_relocation(data_symbol_index, r.location, d.location - 4);
                break;
            default:
                std::cerr << "Can't relocate code relative to this symbol!\n";
                throw X64_ERROR;
            }
            break;
            
        case REF_DATA_ABSOLUTE:
            switch (d.type) {
            case DEF_ABSOLUTE:
            case DEF_ABSOLUTE_EXPORT:
                *(long *)&data[r.location] = d.location;
                break;
            case DEF_DATA_EXPORT:
            case DEF_DATA:
                ork->data_relocation(data_symbol_index, r.location, d.location);
                break;
            case DEF_CODE:
            case DEF_CODE_EXPORT:
                ork->data_relocation(code_symbol_index, r.location, d.location);
                break;
            default:
                std::cerr << "Can't relocate data absolute to this symbol!\n";
                throw X64_ERROR;
            }
            break;

        }
    }

    ork->set_code(code);
    ork->set_data(data);
    ork->done(filename);
    
    delete ork;
}


void X64::add_def(Label label, const Def &def) {
    if (!label.def_index) {
        std::cerr << "Can't define an undeclared label!\n";
        throw X64_ERROR;
    }

    if (defs.count(label.def_index)) {
        std::cerr << "Can't redefine label!\n";
        throw X64_ERROR;
    }

    //if (label.def_index == 108)
    //    throw X64_ERROR;

    defs.insert(decltype(defs)::value_type(label.def_index, def));
}


void X64::data_align() {
    data.resize((data.size() + 7) & ~7);  // 8-byte alignment
}


void X64::data_byte(char x) {
    data.push_back(x);
}


void X64::data_word(short x) {
    data.resize(data.size() + 2);
    *(short *)(data.data() + data.size() - 2) = x;
}


void X64::data_dword(int x) {
    data.resize(data.size() + 4);
    *(int *)(data.data() + data.size() - 4) = x;
}


void X64::data_qword(long x) {
    data.resize(data.size() + 8);
    *(long *)(data.data() + data.size() - 8) = x;
}


void X64::data_zstring(const char *s) {
    while (*s)
        data.push_back(*s++);
        
    data.push_back(0);
}


void X64::data_double(double x) {
    data.resize(data.size() + 8);
    *(double *)(data.data() + data.size() - 8) = x;
}


void X64::data_label(Label c, unsigned size) {
    add_def(c, Def(DEF_DATA, data.size(), size, "", false));
}


void X64::data_label_local(Label c, std::string name, unsigned size) {
    add_def(c, Def(DEF_DATA_EXPORT, data.size(), size, name, false));
}


void X64::data_label_global(Label c, std::string name, unsigned size) {
    add_def(c, Def(DEF_DATA_EXPORT, data.size(), size, name, true));
}


void X64::data_reference(Label label) {
    if (!label.def_index) {
        std::cerr << "Can't reference an undeclared label!\n";
        throw X64_ERROR;
    }

    refs.push_back(Ref());
    Ref &r = refs.back();
    
    r.location = data.size();  // Store the beginning
    r.type = REF_DATA_ABSOLUTE;
    r.def_index = label.def_index;
    
    data_qword(0);  // 64-bit relocations only
}


void X64::code_align() {
    code.resize((code.size() + 7) & ~7);  // 8-byte alignment
}


void X64::code_byte(char x) {
    code.push_back(x);
}


void X64::code_word(short x) {
    code.resize(code.size() + 2);
    *(short *)(code.data() + code.size() - 2) = x;
}


void X64::code_dword(int x) {
    code.resize(code.size() + 4);
    *(int *)(code.data() + code.size() - 4) = x;
}


void X64::code_qword(long x) {
    code.resize(code.size() + 8);
    *(long *)(code.data() + code.size() - 8) = x;
}


void X64::code_label(Label c, unsigned size) {
    add_def(c, Def(DEF_CODE, code.size(), size, "", false));
}


void X64::code_label_import(Label c, std::string name) {
    add_def(c, Def(DEF_CODE_IMPORT, 0, 0, name, false));
}


void X64::code_label_local(Label c, std::string name, unsigned size) {
    add_def(c, Def(DEF_CODE_EXPORT, code.size(), size, name, false));
}


void X64::code_label_global(Label c, std::string name, unsigned size) {
    add_def(c, Def(DEF_CODE_EXPORT, code.size(), size, name, true));
}


void X64::code_reference(Label label, int offset) {
    if (!label.def_index) {
        std::cerr << "Can't reference an undeclared label!\n";
        throw X64_ERROR;
    }

    refs.push_back(Ref());
    Ref &r = refs.back();

    r.location = code.size();  // Store the beginning!
    r.type = REF_CODE_RELATIVE;
    r.def_index = label.def_index;

    code_dword(offset);  // 32-bit offset only
}


int X64::q(Register r) {
    return (r == SIL || r == DIL || r == SPL || r == BPL ? REX_Q : 0);
}


int X64::r(Register regfield) {
    return
        (regfield >= 8 ? REX_R : 0x00);
}


int X64::r(SseRegister regfield) {
    return
        (regfield >= 8 ? REX_R : 0x00);
}


int X64::xb(Address rm) {
    return
        (rm.index != NOREG && rm.index >= 8 ? REX_X : 0x00) |
        (rm.base != NOREG && rm.base >= 8 ? REX_B : 0x00);
}


int X64::xb(Register rm) {
    return
        (rm >= 8 ? REX_B : 0x00);
}


int X64::xb(SseRegister rm) {
    return
        (rm >= 8 ? REX_B : 0x00);
}


void X64::rex(int wrxb, bool force) {
    if (wrxb || force)
        code_byte(OPSIZE_REX_PREFIX | wrxb);
}


void X64::code_op(int code) {
    // Multi-byte opcodes must be emitted MSB first
    
    if (code & 0xFF00)
        code_byte((code >> 8) & 0xFF);

    code_byte(code & 0xFF);
}


void X64::code_op(int code, Opsize opsize, int rxbq) {
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
    int rxb = rxbq & ~REX_Q;

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
    case OPSIZE_WORD:
        code_byte(OPSIZE_WORD_PREFIX);
        rex(rxb);
        break;
    case OPSIZE_DWORD:
        // The default opsize may be byte for some operations (bitsets!), and using the
        // questionable registers needs a forced REX prefix.
        rex(rxb, questionable);
        break;
    case OPSIZE_QWORD:
        rex(REX_W | rxb);
        break;
    default:
        throw X64_ERROR;
    }

    code_op(code);
}


void X64::effective_address(int regfield, Register rm) {
    // The cut off bits belong to the REX prefix
    code_byte(0xC0 | ((regfield & 7) << 3) | (rm & 7));
}


void X64::effective_address(int regfield, SseRegister rm) {
    // The cut off bits belong to the REX prefix
    code_byte(0xC0 | ((regfield & 7) << 3) | (rm & 7));
}


void X64::effective_address(int regfield, Address x) {
    // Quirks:
    // Offsetless RBP in r/m is interpreted as [RIP + disp32]
    // Offsetless RBP in SIB base is interpreted as [disp32]
    // RSP in r/m is interpreted as SIB byte following
    // RSP in SIB index means no index
    
    const int DISP0 = 0;
    const int DISP8 = 1;
    const int DISP32 = 2;
    
    const int SCALE1 = 0;
    
    const Register USE_SIB = RSP;
    const Register NO_INDEX = RSP;
    
    if (x.index == NO_INDEX) {
        std::cerr << "Oops, can't use RSP as SIB index register!\n";
        throw X64_ERROR;
    }

    if (x.base == NOREG) {
        // We no longer use absolute addresses, so it's better to throw something instead.
        std::cerr << "Address without base register used for addressing!\n";
        throw X64_ERROR;
    }
    
    // The cut off bits belong to the REX prefix
    regfield &= 7;
    int base = x.base == NOREG ? NOREG : x.base & 7;  // RSP and R12 need a SIB
    int index = x.index == NOREG ? NOREG : x.index & 7;
    int scale = (x.scale == 1 ? 0 : x.scale == 2 ? 1 : x.scale == 4 ? 2 : x.scale == 8 ? 3 : x.index == NOREG ? 0 : throw X64_ERROR);
    int offset = x.offset;
    
    if (base == NOREG) {
        if (index != NOREG) {
            std::cerr << "Not funny.\n";
            throw X64_ERROR;
        }
        
        // Must encode as offsetless RBP base with SIB
        code_byte((DISP0 << 6)  | (regfield << 3) | USE_SIB);
        code_byte((SCALE1 << 6) | (NO_INDEX << 3) | RBP);
        code_dword(offset);
    }
    else if (offset == 0 && base != RBP) {  // Can't encode [RBP] and [R13] without offset
        // Omit offset
        
        if (index != NOREG) {
            code_byte((DISP0 << 6)  | (regfield << 3) | USE_SIB);
            code_byte((scale << 6)  | (index << 3)    | base);  // R12 can be index
        }
        else if (base == USE_SIB) {
            code_byte((DISP0 << 6)  | (regfield << 3) | USE_SIB);
            code_byte((SCALE1 << 6) | (NO_INDEX << 3) | base);
        }
        else 
            code_byte((DISP0 << 6)  | (regfield << 3) | base);
    }
    else if (offset <= 127 && offset >= -128) {
        // Byte offset
        
        if (index != NOREG) {
            code_byte((DISP8 << 6)  | (regfield << 3) | USE_SIB);
            code_byte((scale << 6)  | (index << 3)    | base);  // R12 can be index
        }
        else if (base == USE_SIB) {
            code_byte((DISP8 << 6)  | (regfield << 3) | USE_SIB);
            code_byte((SCALE1 << 6) | (NO_INDEX << 3) | base);
        }
        else
            code_byte((DISP8 << 6)  | (regfield << 3) | base);
            
        code_byte((char)offset);
    }
    else {
        // Dword offset
        
        if (index != NOREG) {
            code_byte((DISP32 << 6) | (regfield << 3) | USE_SIB);
            code_byte((scale << 6)  | (index << 3)    | base);  // R12 can be index
        }
        else if (base == USE_SIB) {
            code_byte((DISP32 << 6) | (regfield << 3) | USE_SIB);
            code_byte((SCALE1 << 6) | (NO_INDEX << 3) | base);
        }
        else
            code_byte((DISP32 << 6) | (regfield << 3) | base);
            
        code_dword(offset);  // 32-bit offsets only
    }
}


void X64::effective_address(int regfield, Label l, int offset) {
    const int DISP0 = 0;
    code_byte((DISP0 << 6) | ((regfield & 7) << 3) | RBP);  // means [RIP + disp32]
    code_reference(l, offset);
}


void X64::code_op(int opcode, Opsize opsize, Slash regfield, Register rm) {
    code_op(opcode, opsize, xb(rm) | q(rm));
    effective_address(regfield, rm);
}


void X64::code_op(int opcode, Opsize opsize, Register regfield, Register rm) {
    code_op(opcode, opsize, r(regfield) | xb(rm) | q(regfield) | q(rm));
    effective_address(regfield, rm);
}


void X64::code_op(int opcode, Opsize opsize, Slash regfield, Address rm) {
    code_op(opcode, opsize, xb(rm));
    effective_address(regfield, rm);
}


void X64::code_op(int opcode, Opsize opsize, Register regfield, Address rm) {
    code_op(opcode, opsize, r(regfield) | xb(rm) | q(regfield));
    effective_address(regfield, rm);
}


void X64::code_op(int opcode, Opsize opsize, Slash regfield, Label l, int offset) {
    code_op(opcode, opsize, 0);
    effective_address(regfield, l, offset);
}


void X64::code_op(int opcode, Opsize opsize, Register regfield, Label l, int offset) {
    code_op(opcode, opsize, r(regfield) | q(regfield));
    effective_address(regfield, l, offset);
}


void X64::code_op(int opcode, Opsize opsize, SseRegister regfield, SseRegister rm) {
    code_op(opcode, opsize, r(regfield) | xb(rm));
    effective_address(regfield, rm);
}


void X64::code_op(int opcode, Opsize opsize, SseRegister regfield, Address rm) {
    code_op(opcode, opsize, r(regfield) | xb(rm));
    effective_address(regfield, rm);
}


void X64::code_op(int opcode, Opsize opsize, SseRegister regfield, Register rm) {
    code_op(opcode, opsize, r(regfield) | xb(rm) | q(rm));
    effective_address(regfield, rm);
}


void X64::code_op(int opcode, Opsize opsize, Register regfield, SseRegister rm) {
    code_op(opcode, opsize, r(regfield) | xb(rm) | q(regfield));
    effective_address(regfield, rm);
}




int simple_info[] = {
     0x6698, 0x99, 0x4898, 0xF8, 0xFC, 0xFA, 0x0F06, 0xF5, 0x4899, 0x6699, 0x98, 0xF4, 0xCF, 0x9F, 0x90,
     0x61, 0x9D, 0x60, 0x9C, 0xCB, 0xC3, 0x9E, 0xF9, 0xFD, 0xFB, 0x0F0B, 0xD7,
     0xDEF9, 0xD9FC, 0xDEC9, 0xDEE1, 0xDBE3
};


void X64::op(SimpleOp opcode) {
    code_op(simple_info[opcode]);
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


void X64::op(UnaryOp opcode, Register x) {
    auto &info = unary_info[opcode >> 2];
    code_op(info.op, OPSIZE_LEGACY(opcode), info.regfield, x);
    //effective_address(info.regfield, x);
}

void X64::op(UnaryOp opcode, Address x) {
    auto &info = unary_info[opcode >> 2];
    code_op(info.op, OPSIZE_LEGACY(opcode), info.regfield, x);
    //effective_address(info.regfield, x);
}




void X64::op(PortOp opcode) {
    if ((opcode | 3) == INQ)
        code_op(0xEC, OPSIZE_LEGACY(opcode));
    else
        code_op(0xEE, OPSIZE_LEGACY(opcode));
}


void X64::op(PortOp opcode, int x) {
    if ((opcode | 3) == INQ)
        code_op(0xE4, OPSIZE_LEGACY(opcode));
    else
        code_op(0xE6, OPSIZE_LEGACY(opcode));
     
    code_byte(x);
}




int string_info[] = {
    0x6C, 0xAC, 0xA4, 0x6E, 0xAA,
    0xF36C, 0xF3AC, 0xF3A4, 0xF36E, 0xF3AA,
    0xA6, 0xAE,
    0xF3A6, 0xF3AE,
    0xF2A6, 0xF2AE,
};


void X64::op(StringOp opcode) {
    // 64-bit mode uses the RCX, RSI, RDI registers because of using 64-bit ADDRESS size.
    // The REP prefixes must precede the REX prefix, so we must encode it manually.
    // NOTE: REP MOVSB/STOSB is really fast on post Ivy Bridge processors, even if they
    // have a relatively high (~35 cycles?) setup cost. Even faster than MOVSQ/STOSQ.
    int info = string_info[opcode >> 2];
    
    if (info & 0xFF00)
        code_byte(info >> 8);
        
    code_op(info & 0xFF, OPSIZE_LEGACY(opcode));
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


void X64::op(BinaryOp opcode, Register x, int y) {
    auto &info = binary_info[opcode >> 2];
    code_op(info.op1, OPSIZE_LEGACY(opcode), info.regfield1, x);
    
    switch (opcode & 3) {
    case 0: code_byte(y); break;
    case 1: code_word(y); break;
    case 2: code_dword(y); break;
    case 3: code_dword(y); break;  // 32-bit immediate only
    }
}

void X64::op(BinaryOp opcode, Address x, int y) {
    auto &info = binary_info[opcode >> 2];
    code_op(info.op1, OPSIZE_LEGACY(opcode), info.regfield1, x);
    
    switch (opcode & 3) {
    case 0: code_byte(y); break;
    case 1: code_word(y); break;
    case 2: code_dword(y); break;
    case 3: code_dword(y); break;  // 32-bit immediate only
    }
}

void X64::op(BinaryOp opcode, Register x, Register y) {
    if ((opcode | 3) == MOVQ && x == y)
        return;  // Don't embarrass ourselves

    auto &info = binary_info[opcode >> 2];
    code_op(info.op2, OPSIZE_LEGACY(opcode), y, x);
}

void X64::op(BinaryOp opcode, Address x, Register y) {
    auto &info = binary_info[opcode >> 2];
    code_op(info.op2, OPSIZE_LEGACY(opcode), y, x);
}

void X64::op(BinaryOp opcode, Register x, Address y) {
    auto &info = binary_info[opcode >> 2];
    code_op(info.op3, OPSIZE_LEGACY(opcode), x, y);
}

void X64::op(BinaryOp opcode, Register x, Label y) {
    auto &info = binary_info[opcode >> 2];
    code_op(info.op3, OPSIZE_LEGACY(opcode), x, y, 0);
}




void X64::op(MovabsOp opcode, Register x, long y) {
    code_op(0xB8 + (x & 7), OPSIZE_QWORD, xb(x));
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
void X64::op(ShiftOp opcode, Register x, Register cl) {
    if (cl != CL)
        throw X64_ERROR;
        
    auto &info = shift_info[opcode >> 2];
    code_op(0xD2, OPSIZE_LEGACY(opcode), info, x);
}

void X64::op(ShiftOp opcode, Address x, Register cl) {
    if (cl != CL)
        throw X64_ERROR;

    auto &info = shift_info[opcode >> 2];
    code_op(0xD2, OPSIZE_LEGACY(opcode), info, x);
}

void X64::op(ShiftOp opcode, Register x, char y) {
    auto &info = shift_info[opcode >> 2];

    if (y == 1) {
        code_op(0xD0, OPSIZE_LEGACY(opcode), info, x);
    }
    else {
        code_op(0xC0, OPSIZE_LEGACY(opcode), info, x);
        code_byte(y);
    }
}

void X64::op(ShiftOp opcode, Address x, char y) {
    auto &info = shift_info[opcode >> 2];

    if (y == 1) {
        code_op(0xD0, OPSIZE_LEGACY(opcode), info, x);
    }
    else {
        code_op(0xC0, OPSIZE_LEGACY(opcode), info, x);
        code_byte(y);
    }
}




void X64::op(ExchangeOp opcode, Register x, Register y) {
    code_op(0x86, OPSIZE_LEGACY(opcode), x, y);
}

void X64::op(ExchangeOp opcode, Address x, Register y) {
    code_op(0x86, OPSIZE_LEGACY(opcode), y, x);
}

void X64::op(ExchangeOp opcode, Register x, Address y) {
    code_op(0x86, OPSIZE_LEGACY(opcode), x, y);
}




void X64::op(StackOp opcode, int x) {
    if (opcode == PUSHQ) {
        code_byte(0x68);  // Defaults to 64-bit operand size
        code_dword(x);  // 32-bit immediate only
    }
    else
        throw X64_ERROR;
}

void X64::op(StackOp opcode, Register x) {
    if (opcode == PUSHQ)
        code_op(0x50 | (x & 0x07), OPSIZE_DEFAULT, (x & 0x08 ? REX_B : 0));
    else
        code_op(0x58 | (x & 0x07), OPSIZE_DEFAULT, (x & 0x08 ? REX_B : 0));
}

void X64::op(StackOp opcode, Address x) {
    if (opcode == PUSHQ) {
        code_op(0xFF, OPSIZE_DEFAULT, SLASH_6, x);
    }
    else {
        code_op(0x8F, OPSIZE_DEFAULT, SLASH_0, x);
    }
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

void X64::op(MemoryOp opcode, Address x) {
    auto &info = memory_info[opcode];
    code_op(info.op, OPSIZE_DEFAULT, info.regfield, x);
}




int registerfirst_info[] = {
    0x0FAF, 0x63
};

void X64::op(RegisterFirstOp opcode, Register x, Register y) {
    auto &info = registerfirst_info[opcode >> 2];
    code_op(info, OPSIZE_NONBYTE(opcode), x, y);
}

void X64::op(RegisterFirstOp opcode, Register x, Address y) {
    auto &info = registerfirst_info[opcode >> 2];
    code_op(info, OPSIZE_NONBYTE(opcode), x, y);
}




void X64::op(Imul3Op opcode, Register x, Register y, int z) {
    if (z >= -128 && z <= 127) {
        code_op(0x6B, OPSIZE_NONBYTE(opcode), x, y);
        code_byte(z);
    }
    else {
        code_op(0x69, OPSIZE_NONBYTE(opcode), x, y);

        switch (opcode & 3) {
        case 0: throw X64_ERROR;
        case 1: code_word(z); break;
        case 2: code_dword(z); break;
        case 3: code_dword(z); break;  // 32-bit immediate only
        }
    }
}

void X64::op(Imul3Op opcode, Register x, Address y, int z) {
    if (z >= -128 && z <= 127) {
        code_op(0x6B, OPSIZE_NONBYTE(opcode), x, y);
        code_byte(z);
    }
    else {
        code_op(0x69, OPSIZE_NONBYTE(opcode), x, y);

        switch (opcode & 3) {
        case 0: throw X64_ERROR;
        case 1: code_word(z); break;
        case 2: code_dword(z); break;
        case 3: code_dword(z); break;  // 32-bit immediate only
        }
    }
}




int registermemory_info[] = {
    0x8D
};


void X64::op(RegisterMemoryOp opcode, Register x, Address y) {
    auto &info = registermemory_info[opcode];
    code_op(info, OPSIZE_QWORD, x, y);
}




void X64::op(LeaRipOp, Register r, Label l, int offset) {
    code_op(0x8D, OPSIZE_QWORD, r, l, offset);  // must use 64-bit opsize
}




void X64::op(BitSetOp opcode, Register x) {
    code_op(0x0F90 | opcode, OPSIZE_DEFAULT, SLASH_0, x);
}


void X64::op(BitSetOp opcode, Address x) {
    code_op(0x0F90 | opcode, OPSIZE_DEFAULT, SLASH_0, x);
}




void X64::op(BranchOp opcode, Label c) {
    code_op(0x0F80 | opcode);
    code_reference(c);
}




void X64::op(JumpOp opcode, Label c) {
    if (opcode == CALL) {
        code_op(0xE8);
        code_reference(c);
    }
    else if (opcode == JMP) {
        code_op(0xE9);
        code_reference(c);
    }
    else
        throw X64_ERROR;
}


void X64::op(JumpOp opcode, Address x) {
    if (opcode == CALL) {
        code_op(0xFF, OPSIZE_DEFAULT, SLASH_2, x);
    }
    else if (opcode == JMP) {
        code_op(0xFF, OPSIZE_DEFAULT, SLASH_4, x);
    }
    else
        throw X64_ERROR;
}


void X64::op(JumpOp opcode, Register x) {
    if (opcode == CALL) {
        code_op(0xFF, OPSIZE_DEFAULT, SLASH_2, x);
    }
    else if (opcode == JMP) {
        code_op(0xFF, OPSIZE_DEFAULT, SLASH_4, x);
    }
    else
        throw X64_ERROR;
}


void X64::op(ConstantOp opcode, int x) {
    if (opcode == INT) {
        code_op(0xCD);
        code_byte(x);
    }
    else if (opcode == RETX) {
        code_op(0xC2);
        code_word(x);
    }
    else if (opcode == RETFX) {
        code_op(0xCA);
        code_word(x);
    }
}


struct {
    int op1;
    int op2;
} ssemem_ssemem_info[] = {   // xmm1, xmm2/mem64    xmm1/mem64, xmm2
    { 0xF30F7E, 0x660FD6 },  // MOVQ
    { 0xF20F10, 0xF20F11 },  // MOVSD
    { 0xF30F10, 0xF30F11 },  // MOVSS
};

void X64::op(SsememSsememOp opcode, SseRegister x, SseRegister y) {
    code_op(ssemem_ssemem_info[opcode].op1, OPSIZE_DEFAULT, x, y);
}


void X64::op(SsememSsememOp opcode, SseRegister x, Address y) {
    code_op(ssemem_ssemem_info[opcode].op1, OPSIZE_DEFAULT, x, y);
}


void X64::op(SsememSsememOp opcode, Address x, SseRegister y) {
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
};

void X64::op(SseSsememOp opcode, SseRegister x, SseRegister y) {
    code_op(sse_ssemem_info[opcode], OPSIZE_DEFAULT, x, y);
}

void X64::op(SseSsememOp opcode, SseRegister x, Address y) {
    code_op(sse_ssemem_info[opcode], OPSIZE_DEFAULT, x, y);
}


int sse_gprmem_info[] = {  // xmm1, reg64/mem64
    0xF20F2A,  // CVTSI2SD
};

void X64::op(SseGprmemOp opcode, SseRegister x, Register y) {
    code_op(sse_gprmem_info[opcode], OPSIZE_DEFAULT, x, y);
}

void X64::op(SseGprmemOp opcode, SseRegister x, Address y) {
    code_op(sse_gprmem_info[opcode], OPSIZE_DEFAULT, x, y);
}


int gpr_ssemem_info[] = {  // reg64, xmm1/mem64
    0xF20F2D,  // CVTSD2SI
    0xF20F2C,  // CVTTSD2SI
};

void X64::op(GprSsememOp opcode, Register x, SseRegister y) {
    code_op(gpr_ssemem_info[opcode], OPSIZE_DEFAULT, x, y);
}

void X64::op(GprSsememOp opcode, Register x, Address y) {
    code_op(gpr_ssemem_info[opcode], OPSIZE_DEFAULT, x, y);
}
