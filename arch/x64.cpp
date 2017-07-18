#include <iostream>

#include "x64.h"

const int OPSIZE_NONBYTE = 4;
const int OPSIZE_DEFAULT = 6;

const int OPSIZE_WORD_PREFIX = 0x66;
const int OPSIZE_REX_PREFIX = 0x40;

const int REX_W = 0x08;
const int REX_R = 0x04;
const int REX_X = 0x02;
const int REX_B = 0x01;


std::ostream &operator << (std::ostream &os, const Register r) {
    switch (r) {
    case NOREG: os << "-"; break;
    case RAX: os << "RAX"; break;
    case RBX: os << "RBX"; break;
    case RCX: os << "RCX"; break;
    case RDX: os << "RDX"; break;
    case RSP: os << "RSP"; break;
    case RBP: os << "RBP"; break;
    case RSI: os << "RSI"; break;
    case RDI: os << "RDI"; break;
    default: os << "???"; break;
    }
    
    return os;
}


Address::Address() {
    base = NOREG;
    index = NOREG;
    offset = 0;
}


Address::Address(Register b, int o) {
    if (b == NOREG) {
        std::cerr << "Address without register!\n";
        throw X64_ERROR;
    }
          
    base = b;
    index = NOREG;
    offset = o;
}


Address::Address(Register b, Register i, int o) {
    if (b == NOREG) {
        std::cerr << "Address without register!\n";
        throw X64_ERROR;
    }
          
    base = b;
    index = i;
    offset = o;
}


Address Address::operator + (int x) {
    return Address(base, offset + x);
}


X64::X64() {
}


X64::~X64() {
}


void X64::init(std::string module_name) {
    ork = new Ork;  // New Ork, New Ork...

    // symbol table indexes
    code_symbol_index = ork->export_code(module_name + ".code", 0, 0, 1);
    data_symbol_index = ork->export_data(module_name + ".data", 0, 0, 1);

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
            ork->export_code(d.name, d.location, d.size, d.is_global);
            break;
        case DEF_DATA_EXPORT:
            ork->export_data(d.name, d.location, d.size, d.is_global);
            break;
        case DEF_ABSOLUTE_EXPORT:
            ork->export_absolute(d.name, d.location, d.size, d.is_global);
            break;
        default:
            std::cerr << "He?\n";
            throw X64_ERROR;
        }
    }

    for (auto &r : refs) {
        if (!defs.count(r.def_index)) {
            std::cerr << "Undefined label " << r.def_index << "!\n";
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
                *(int *)&code[r.location] += -4;
                ork->code_relocation(d.symbol_index, r.location, true);
                break;
            case DEF_DATA:
            case DEF_DATA_EXPORT:
                *(int *)&code[r.location] += d.location - 4;
                ork->code_relocation(data_symbol_index, r.location, true);
                break;
            default:
                std::cerr << "Can't relocate relative to this symbol!\n";
                throw X64_ERROR;
            }
            break;
            
        case REF_CODE_ABSOLUTE:
            switch (d.type) {
            case DEF_CODE:
            case DEF_CODE_EXPORT:
                *(int *)&code[r.location] += d.location;
                ork->code_relocation(code_symbol_index, r.location, false);
                break;
            case DEF_CODE_IMPORT:
                ork->code_relocation(d.symbol_index, r.location, false);
                break;
            case DEF_DATA:
            case DEF_DATA_EXPORT:
                *(int *)&code[r.location] += d.location;
                ork->code_relocation(data_symbol_index, r.location, false);
                break;
            case DEF_ABSOLUTE:
            case DEF_ABSOLUTE_EXPORT:
                *(int *)&code[r.location] += d.location;
                break;
            default:
                std::cerr << "Can't relocate absolute to this symbol!\n";
                throw X64_ERROR;
            }
            break;
            
        case REF_DATA_ABSOLUTE:
            switch (d.type) {
            case DEF_DATA:
            case DEF_DATA_EXPORT:
                *(int *)&data[r.location] += d.location;
                ork->data_relocation(data_symbol_index, r.location);
                break;
            default:
                std::cerr << "Can't relocate data absolute to this symbol!\n";
                throw X64_ERROR;
            }
        }
    }

    ork->set_code(code);
    ork->set_data(data);
    ork->done(filename);
    
    delete ork;
}


void X64::add_def(Label label, const Def &def) {
    if (!label) {
        std::cerr << "Unallocated label!\n";
        throw X64_ERROR;
    }
    
    if (defs.count(label.def_index)) {
        std::cerr << "Double label definition!\n";
        throw X64_ERROR;
    }

    defs.insert(decltype(defs)::value_type(label.def_index, def));
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


void X64::data_label(Label c, unsigned size) {
    add_def(c, Def(DEF_DATA, data.size(), size, "", false));
}


void X64::data_label_export(Label c, std::string name, unsigned size, bool is_global) {
    add_def(c, Def(DEF_DATA_EXPORT, data.size(), size, name, is_global));
}


unsigned X64::data_allocate(unsigned size) {
    data.resize(data.size() + size);
    return data.size() - size;
}


void X64::data_reference(Label c) {
    if (!c.def_index) {
        std::cerr << "Reference to unallocated label!\n";
        throw X64_ERROR;
    }
    
    refs.push_back(Ref());
    Ref &r = refs.back();
    
    r.location = data.size();  // Store the beginning
    r.type = REF_DATA_ABSOLUTE;
    r.def_index = c.def_index;
    
    data_dword(0);  // 32-bit relocations only
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


void X64::code_label_export(Label c, std::string name, unsigned size, bool is_global) {
    add_def(c, Def(DEF_CODE_EXPORT, code.size(), size, name, is_global));
}


void X64::absolute_label(Label c, int value) {
    add_def(c, Def(DEF_ABSOLUTE, value, 0, "", false));
}


void X64::absolute_label_export(Label c, std::string name, int value, unsigned size, bool is_global) {
    add_def(c, Def(DEF_ABSOLUTE_EXPORT, value, size, name, is_global));
}


void X64::code_reference(Label c, Ref_type f, int offset) {
    if (!c.def_index) {
        std::cerr << "Reference to unallocated label!\n";
        throw X64_ERROR;
    }

    refs.push_back(Ref());
    Ref &r = refs.back();

    r.location = code.size();  // Store the beginning!
    r.type = f;
    r.def_index = c.def_index;

    //if (f != REF_CODE_ABSOLUTE && offset != 0)
    //    std::cerr << "Only absolute refs can have offsets!\n";
    
    if (f == REF_CODE_SHORT)
        code_byte(offset);
    else
        code_dword(offset);  // 32-bit offset only
}


int X64::rxb(int regfield, Register rm) {
    return
        (regfield >= 8 ? REX_R : 0x00) |
        (rm >= 8 ? REX_B : 0x00);
}


int X64::rxb(int regfield, Address rm) {
    return
        (regfield >= 8 ? REX_R : 0x00) |
        (rm.index != NOREG && rm.index >= 8 ? REX_X : 0x00) |
        (rm.base != NOREG && rm.base >= 8 ? REX_B : 0x00);
}


void X64::rex(int wrxb) {
    if (wrxb)
        code_byte(OPSIZE_REX_PREFIX | wrxb);
}


void X64::code_op(int code) {
    if (code & 0xFF00)  // Two-byte opcodes must be emitted MSB first
        code_byte((code >> 8) & 0xFF);

    code_byte(code & 0xFF);
}


void X64::code_op(int code, int size, int rxb) {
    // size == 0 => byte  =>      _RXB op0
    // size == 1 => word  => 0x66 _RXB op1
    // size == 2 => dword =>      _RXB op1
    // size == 3 => qword =>      WRXB op1
    // size == 4 => INVALID
    // size == 5 => word  => 0x66 _RXB opc
    // size == 6 => dword =>      _RXB opc
    // size == 7 => qword =>      WRXB opc

    switch (size) {
    case 0:
        rex(rxb);
        code &= ~1;
        break;
    case 1:
        code_byte(OPSIZE_WORD_PREFIX);
        rex(rxb);
        code |= 1;
        break;
    case 2:
        rex(rxb);
        code |= 1;
        break;
    case 3:
        rex(REX_W | rxb);
        code |= 1;
        break;
    case 4:
        throw X64_ERROR;
    case 5:
        code_byte(OPSIZE_WORD_PREFIX);
        rex(rxb);
        break;
    case 6:
        rex(rxb);
        break;
    case 7:
        rex(REX_W | rxb);
        break;
    default:
        throw X64_ERROR;
    }

    code_op(code);
}


void X64::effective_address(int regfield, Register x) {
    // The cut off bits belong to the REX prefix
    regfield &= 7;
    int rm = x & 7;
    
    code_byte(0xC0 | (regfield << 3) | rm);
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
    //const int SCALE2 = 1;
    //const int SCALE4 = 2;
    //const int SCALE8 = 3;
    
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
            code_byte((SCALE1 << 6) | (index << 3)    | base);  // R12 can be index
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
            code_byte((SCALE1 << 6) | (index << 3)    | base);  // R12 can be index
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
            code_byte((SCALE1 << 6) | (index << 3)    | base);  // R12 can be index
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


void X64::code_op(int opcode, int opsize, int regfield, Register rm) {
    code_op(opcode, opsize, rxb(regfield, rm));
    effective_address(regfield, rm);
}


void X64::code_op(int opcode, int opsize, int regfield, Address rm) {
    code_op(opcode, opsize, rxb(regfield, rm));
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
    int regfield;
} unary_info[] = {
    {0xFE, 1},
    {0xF6, 6},
    {0xF6, 7},
    {0xF6, 5},
    {0xFE, 0},
    {0x0F00, 2},
    {0x0F00, 3},
    {0xF6, 4},
    {0xF6, 3},
    {0xF6, 2},
    {0x0F00, 0},
    {0x0F00, 1},
    {0x0F00, 4},
    {0x0F00, 5}
};


void X64::op(UnaryOp opcode, Register x) {
    auto &info = unary_info[opcode >> 2];
    code_op(info.op, opcode & 3, info.regfield, x);
    //effective_address(info.regfield, x);
}

void X64::op(UnaryOp opcode, Address x) {
    auto &info = unary_info[opcode >> 2];
    code_op(info.op, opcode & 3, info.regfield, x);
    //effective_address(info.regfield, x);
}




void X64::op(PortOp opcode) {
    if ((opcode | 3) == INQ)
        code_op(0xEC, opcode & 3);
    else
        code_op(0xEE, opcode & 3);
}


void X64::op(PortOp opcode, int x) {
    if ((opcode | 3) == INQ)
        code_op(0xE4, opcode & 3);
    else
        code_op(0xE6, opcode & 3);
     
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
    // 64-bit mode uses the RCX, RSI, RDI registers because of using 64-bit ADDRESS size
    code_op(string_info[opcode >> 2], opcode & 3);
}




struct {
    int op1;
    int regfield1;
    int op2;
    int op3;
} binary_info[] = {
    {0x80, 2, 0x10, 0x12},
    {0x80, 0, 0x00, 0x02},
    {0x80, 4, 0x20, 0x22},
    {0x80, 7, 0x38, 0x3A},
    {0xC6, 0, 0x88, 0x8A},
    {0x80, 1, 0x08, 0x0A},
    {0x80, 3, 0x18, 0x1A},
    {0x80, 5, 0x28, 0x2A},
    {0xF6, 0, 0x84, 0x84},  // Look, it's symmetric!
    {0x80, 6, 0x30, 0x32}
};


void X64::op(BinaryOp opcode, Register x, int y) {
    auto &info = binary_info[opcode >> 2];
    code_op(info.op1, opcode & 3, info.regfield1, x);
    
    switch (opcode & 3) {
    case 0: code_byte(y); break;
    case 1: code_word(y); break;
    case 2: code_dword(y); break;
    case 3: code_dword(y); break;  // 32-bit immediate only
    }
}

void X64::op(BinaryOp opcode, Address x, int y) {
    auto &info = binary_info[opcode >> 2];
    code_op(info.op1, opcode & 3, info.regfield1, x);
    
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
    code_op(info.op2, opcode & 3, y, x);
}

void X64::op(BinaryOp opcode, Address x, Register y) {
    auto &info = binary_info[opcode >> 2];
    code_op(info.op2, opcode & 3, y, x);
}

void X64::op(BinaryOp opcode, Register x, Address y) {
    auto &info = binary_info[opcode >> 2];
    code_op(info.op3, opcode & 3, x, y);
}




int shift_info[] = {
    2, 3, 0, 1, 4, 7, 4, 5
};


void X64::op(ShiftOp opcode, Register x) {  // by CL
    auto &info = shift_info[opcode >> 2];
    code_op(0xD2, opcode & 3, info, x);
}

void X64::op(ShiftOp opcode, Address x) {  // by CL
    auto &info = shift_info[opcode >> 2];
    code_op(0xD2, opcode & 3, info, x);
}

void X64::op(ShiftOp opcode, Register x, char y) {
    auto &info = shift_info[opcode >> 2];

    if (y == 1) {
        code_op(0xD0, opcode & 3, info, x);
    }
    else {
        code_op(0xC0, opcode & 3, info, x);
        code_byte(y);
    }
}

void X64::op(ShiftOp opcode, Address x, char y) {
    auto &info = shift_info[opcode >> 2];

    if (y == 1) {
        code_op(0xD0, opcode & 3, info, x);
    }
    else {
        code_op(0xC0, opcode & 3, info, x);
        code_byte(y);
    }
}




void X64::op(ExchangeOp opcode, Register x, Register y) {
    code_op(0x86, opcode & 3, x, y);
}

void X64::op(ExchangeOp opcode, Address x, Register y) {
    code_op(0x86, opcode & 3, y, x);
}

void X64::op(ExchangeOp opcode, Register x, Address y) {
    code_op(0x86, opcode & 3, x, y);
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
        code_op(0xFF, OPSIZE_DEFAULT, 6, x);
    }
    else {
        code_op(0x8F, OPSIZE_DEFAULT, 0, x);
    }
}




struct {
        int op;
        int regfield;
} memory_info[] = {
        {0x0F01, 2},
        {0x0F01, 3},
        {0x0F01, 0},
        {0x0F01, 1},
        {0xDF,    5},
        {0xDF,    7},
        {0xD9,    7},
        {0xD9,    5}
};

void X64::op(MemoryOp opcode, Address x) {
    auto &info = memory_info[opcode];
    code_op(info.op, 0, info.regfield, x);
}




int registerfirst_info[] = {
    0x0FAF
};

void X64::op(RegisterFirstOp opcode, Register x, Register y) {
    auto &info = registerfirst_info[opcode >> 2];
    code_op(info, (opcode & 3) | OPSIZE_NONBYTE, x, y);
}

void X64::op(RegisterFirstOp opcode, Register x, Address y) {
    auto &info = registerfirst_info[opcode >> 2];
    code_op(info, (opcode & 3) | OPSIZE_NONBYTE, x, y);
}




int registerfirstconstantthird_info[] = {
    0x69
};

void X64::op(RegisterFirstConstantThirdOp opcode, Register x, Register y, int z) {
    auto &info = registerfirstconstantthird_info[opcode >> 2];
    code_op(info, (opcode & 3) | OPSIZE_NONBYTE, x, y);

    switch (opcode & 3) {
    case 0: throw X64_ERROR;
    case 1: code_word(z); break;
    case 2: code_dword(z); break;
    case 3: code_dword(z); break;  // 32-bit immediate only
    }
}

void X64::op(RegisterFirstConstantThirdOp opcode, Register x, Address y, int z) {
    auto &info = registerfirstconstantthird_info[opcode >> 2];
    code_op(info, (opcode & 3) | OPSIZE_NONBYTE, x, y);

    switch (opcode & 3) {
    case 0: throw X64_ERROR;
    case 1: code_word(z); break;
    case 2: code_dword(z); break;
    case 3: code_dword(z); break;  // 32-bit immediate only
    }
}




int registersecond_info[] = {
    0x63, 0x0FA5, 0x0FAD
};

void X64::op(RegisterSecondOp opcode, Register x, Register y) {
    auto &info = registersecond_info[opcode];
    code_op(info, 0, y, x);
}

void X64::op(RegisterSecondOp opcode, Address x, Register y) {
    auto &info = registersecond_info[opcode];
    code_op(info, 0, y, x);
}




int registermemory_info[] = {
    0xC5, 0x8D, 0xC4
};


void X64::op(RegisterMemoryOp opcode, Register x, Address y) {
    auto &info = registermemory_info[opcode];
    code_op(info, 0, x, y);
}




void X64::op(LeaRipOp, Register r, Label l, int o) {
    const int DISP0 = 0;
    
    code_op(0x8D, 3 | OPSIZE_NONBYTE, r & 8 ? REX_R : 0);  // must use 64-bit opsize
    
    // Can't specify RIP base in Address, encode it explicitly
    code_byte((DISP0 << 6) | ((r & 7) << 3) | RBP);  // means [RIP + disp32]
    code_reference(l, REF_CODE_RELATIVE, o);
}




void X64::op(BitSetOp opcode, Register x) {
    code_op(0x0F90 | opcode, OPSIZE_DEFAULT, 0, x);
}


void X64::op(BitSetOp opcode, Address x) {
    code_op(0x0F90 | opcode, OPSIZE_DEFAULT, 0, x);
}




void X64::op(BranchOp opcode, Label c) {
    code_op(0x0F80 | opcode);
    code_reference(c, REF_CODE_RELATIVE);
}




void X64::op(JumpOp opcode, Label c) {
    if (opcode == CALL) {
        code_op(0xE8);
        code_reference(c, REF_CODE_RELATIVE);
    }
    else if (opcode == JMP) {
        code_op(0xE9);
        code_reference(c, REF_CODE_RELATIVE);
    }
    else if (opcode == LOOP) {
        code_op(0xE2);
        code_reference(c, REF_CODE_RELATIVE);
    }
}


void X64::op(JumpOp opcode, Address x) {
    if (opcode == CALL) {
        code_op(0xFF, OPSIZE_DEFAULT, 2, x);
    }
    else
        std::cerr << "Whacky jump!\n\a";
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
