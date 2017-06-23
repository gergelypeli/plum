#include <iostream>

#include "x64.h"

std::ostream &operator << (std::ostream &os, const Register r) {
    switch (r) {
    case NOREG: os << "-"; break;
    case EAX: os << "EAX"; break;
    case EBX: os << "EBX"; break;
    case ECX: os << "ECX"; break;
    case EDX: os << "EDX"; break;
    case ESP: os << "ESP"; break;
    case EBP: os << "EBP"; break;
    case ESI: os << "ESI"; break;
    case EDI: os << "EDI"; break;
    default: os << "???"; break;
    }
    
    return os;
}


Address::Address(Register x, int y) {
     if (x == NOREG)
          std::cerr << "Address without register!\n";
          
    base = x;
    offset = y;
}


Address::Address(Label &c, int x)
    :label(c) {
    base = NOREG;
    offset = x;
}


Address Address::operator + (int x) {
    return Address(base, offset + x);
}


Address Address::operator + (Label &c) {
     Address m(*this);
     
     if (m.label)
        std::cerr << "This Address is too small for the two of these labels!\n";
        
     m.label.def_index = c.def_index;
     return m;
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
        case DEF_NONE: 
            break;
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
        
        switch (d.type) {
        case DEF_NONE:
            std::cerr << "Ref to undefined Def!\n";
            break;
        case DEF_ABSOLUTE:
        case DEF_ABSOLUTE_EXPORT:
            if (r.type == REF_CODE_ABSOLUTE)
                *(int *)&code[r.location] += d.location;
            else
                std::cerr << "Only ABSOLUTE Ref can point to an ABSOLUTE Def!\n";
            break;
        case DEF_DATA:
        case DEF_DATA_EXPORT:
            if (r.type == REF_CODE_ABSOLUTE) {
                *(int *)&code[r.location] += d.location;  // Can also be an offset, too
                ork->code_relocation(data_symbol_index, r.location, false);
            }
            else if (r.type == REF_DATA_ABSOLUTE) {
                *(int *)&data[r.location] += d.location;  // Just do it
                ork->data_relocation(data_symbol_index, r.location);
            }
            else
                std::cerr << "Only ABSOLUTE Ref can point to a data Def!\n";
            break;
        case DEF_CODE_IMPORT:
            switch (r.type) {
            case REF_CODE_SHORT:
            case REF_DATA_ABSOLUTE:
                std::cerr << "Can't refer to imported address like this!\n";
                break;
            case REF_CODE_RELATIVE:
                // FIXME: we hope this is the last thing in the instruction!
                *(int *)&code[r.location] = -4;  // Relative to the end of the instruction
                ork->code_relocation(d.symbol_index, r.location, true);
                break;
            case REF_CODE_ABSOLUTE:
                //*(int *)(tar + re->hely) = de->hely;
                ork->code_relocation(d.symbol_index, r.location, false);
                break;
            }
            break;
        case DEF_CODE:
        case DEF_CODE_EXPORT:
            switch (r.type) {
                case REF_CODE_SHORT: {
                    int distance = d.location - (r.location + 1);
                    
                    if (distance > 127 || distance < -128)
                        std::cerr << "REF_CODE_SHORT can't jump " << distance << " bytes!\n";
                    else
                        code[r.location] = (char)distance;
                    }
                    break;
                case REF_CODE_RELATIVE:
                    // FIXME: we hope this is the last thing in the instruction!
                    *(int *)&code[r.location] += d.location - (r.location + 4);
                    break;
                case REF_CODE_ABSOLUTE:
                    *(int *)&code[r.location] += d.location;
                    ork->code_relocation(code_symbol_index, r.location, false);
                    break;
                case REF_DATA_ABSOLUTE:
                    std::cerr << "A Ref in data can't point to a Def in code!\n";
                    break;
            }
            break;
        default:
            std::cerr << "He??\n";
            throw X64_ERROR;
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

    defs[label.def_index] = def;
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
    refs.push_back(Ref());
    Ref &r = refs.back();

    r.location = code.size();  // Store the beginning!
    r.type = f;
    r.def_index = c.def_index;

    if (f != REF_CODE_ABSOLUTE && offset != 0)
        std::cerr << "Only absolute refs can have offsets!\n";
    
    if (f == REF_CODE_SHORT)
        code_byte(offset);
    else
        code_dword(offset);  // 32-bit offset only
}


void X64::code_op(int code, int size) {
    // size: 0=byte, 1=word, 2=dword, 3=qword

    if (size == 1)
        code_byte(0x66);
    else if (size == 3)
        code_byte(0x48);  // REX prefix for 64 operands, but no Rn registers

    if (code > 255)  // Two-byte opcodes must be emitted MSB first
        code_byte(code >> 8);

    code_byte((code & 0xFF) | (size >= 1 ? 1 : 0));
}


void X64::effective_address(int modrm, Register x) {
    code_byte(0xC0 | (modrm << 3) | x);
}


// Fix addresses and scaling and index are not yet supported...
// TODO: modrm should be called reg officially, the mod and rm fields are generated here.
void X64::effective_address(int modrm, Address x) {
     if (x.base == NOREG && x.label) {  // Must have a label here
        // TODO: in 64-bit mode fake EBP base will mean RIP base, must use SIB always!
        code_byte(0x00 | (modrm << 3) | 0x05);  // fake EBP base means no base only offset
        code_reference(x.label, REF_CODE_ABSOLUTE, x.offset);
    }
    else if (x.offset == 0 && !x.label && x.base != BP) {  // SIB EBP base needs offset
        if (x.base == SP) {
            code_byte(0x00 | (modrm << 3) | 0x04);  // Using SIB
            code_byte(0x00 | (SP << 3) | x.base);  // fake SP index means no index
        }
        else 
            code_byte(0x00 | (modrm << 3) | x.base);
    }
    else if (x.offset <= 127 && x.offset >= -128 && !x.label) {
        if (x.base == SP) {
            code_byte(0x40 | (modrm << 3) | 0x04);  // Using SIB
            code_byte(0x00 | (SP << 3) | x.base);  // fake SP index means no index
        }
        else
            code_byte(0x40 | (modrm << 3) | x.base);
            
        code_byte((char)x.offset);
    }
    else {
        if (x.base == SP) {
            code_byte(0x80 | (modrm << 3) | 0x04); // Using SIB
            code_byte(0x00 | (SP << 3) | x.base); // fake SP index means no index
        }
        else
            code_byte(0x80 | (modrm << 3) | x.base);
            
        if (!x.label)
            code_dword(x.offset);  // 32-bit offsets only
        else
            code_reference(x.label, REF_CODE_ABSOLUTE, x.offset);
    }
}


int simple_info[] = {
     0x6698, 0x99, 0xF8, 0xFC, 0xFA, 0x0F06, 0xF5, 0x6699, 0x98, 0xF4, 0xCF, 0x9F, 0x90,
     0x61, 0x9D, 0x60, 0x9C, 0xCB, 0xC3, 0x9E, 0xF9, 0xFD, 0xFB, 0x0F0B, 0xD7,
     0xDEF9, 0xD9FC, 0xDEC9, 0xDEE1, 0xDBE3
};


void X64::op(SimpleOp opcode) {
    code_op(simple_info[opcode], 0);
}




struct {
    int op;
    int modrm;
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
    code_op(info.op, opcode & 3);
    effective_address(info.modrm, x);
}

void X64::op(UnaryOp opcode, Address x) {
    auto &info = unary_info[opcode >> 2];
    code_op(info.op, opcode & 3);
    effective_address(info.modrm, x);
}




void X64::op(PortOp opcode) {
    if (opcode < 4)
        code_op(0xEC, opcode & 3);
    else
        code_op(0xEE, opcode & 3);
}


void X64::op(PortOp opcode, int x) {
    if (opcode < 4)
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
    code_op(string_info[opcode >> 2], opcode & 3);
}




struct {
    int op1;
    int modrm1;
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


// constant to r/m

void X64::op(BinaryOp opcode, Register x, Label c, int offset) {
    // Even if the immediate operand will only be 32 bits, it will be sign-extended,
    // and since it's semantically an address, it should be moved into a 64-bit register.
    if ((opcode & 3) != 3)
        std::cerr << "Label addresses are qword constants!\n";
        
    code_op(binary_info[opcode >> 2].op1, opcode & 3);
    effective_address(binary_info[opcode >> 2].modrm1, x);
    code_reference(c, REF_CODE_ABSOLUTE, offset);
}


void X64::op(BinaryOp opcode, Address x, Label c, int offset) {
    if ((opcode & 3) != 3)
        std::cerr << "Label addresses are qword constants!\n";

    code_op(binary_info[opcode >> 2].op1, opcode & 3);
    effective_address(binary_info[opcode >> 2].modrm1, x);
    code_reference(c, REF_CODE_ABSOLUTE, offset);
}


void X64::op(BinaryOp opcode, Register x, int y) {
    code_op(binary_info[opcode >> 2].op1, opcode & 3);
    effective_address(binary_info[opcode >> 2].modrm1, x);
    
    switch (opcode & 3) {
    case 0: code_byte(y); break;
    case 1: code_word(y); break;
    case 2: code_dword(y); break;
    case 3: code_dword(y); break;  // 32-bit immediate only
    }
}

void X64::op(BinaryOp opcode, Address x, int y) {
    code_op(binary_info[opcode >> 2].op1, opcode & 3);
    effective_address(binary_info[opcode >> 2].modrm1, x);
    
    switch (opcode & 3) {
    case 0: code_byte(y); break;
    case 1: code_word(y); break;
    case 2: code_dword(y); break;
    case 3: code_dword(y); break;  // 32-bit immediate only
    }
}

void X64::op(BinaryOp opcode, Register x, Register y) {
    code_op(binary_info[opcode >> 2].op2, opcode & 3);
    effective_address(y, x);
}

void X64::op(BinaryOp opcode, Address x, Register y) {
    code_op(binary_info[opcode >> 2].op2, opcode & 3);
    effective_address(y, x);
}

void X64::op(BinaryOp opcode, Register x, Address y) {
    code_op(binary_info[opcode >> 2].op3, opcode & 3);
    effective_address(x, y);
}




int shift_info[] = {
    2, 3, 0, 1, 4, 7, 4, 5
};


void X64::op(ShiftOp opcode, Register x) {  // by CL
    code_op(0xD2, opcode & 3);
    effective_address(shift_info[opcode >> 2], x);
}

void X64::op(ShiftOp opcode, Address x) {  // by CL
    code_op(0xD2, opcode & 3);
    effective_address(shift_info[opcode >> 2], x);
}

void X64::op(ShiftOp opcode, Register x, char y) {
    if (y == 1) {
        code_op(0xD0, opcode & 3);
        effective_address(shift_info[opcode >> 2], x);
    }
    else {
        code_op(0xC0, opcode & 3);
        effective_address(shift_info[opcode >> 2], x);
        code_byte(y);
    }
}

void X64::op(ShiftOp opcode, Address x, char y) {
    if (y == 1) {
        code_op(0xD0, opcode & 3);
        effective_address(shift_info[opcode >> 2], x);
    }
    else {
        code_op(0xC0, opcode & 3);
        effective_address(shift_info[opcode >> 2], x);
        code_byte(y);
    }
}




void X64::op(ExchangeOp opcode, Register x, Register y) {
    code_op(0x86, opcode);
    effective_address(x, y);
}

void X64::op(ExchangeOp opcode, Address x, Register y) {
    code_op(0x86, opcode);
    effective_address(y, x);
}

void X64::op(ExchangeOp opcode, Register x, Address y) {
    code_op(0x86, opcode);
    effective_address(x, y);
}



void X64::op(StackOp opcode, Label c, int offset) {
    if (opcode == PUSHQ) {
        code_byte(0x68);
        code_reference(c, REF_CODE_ABSOLUTE, offset);
    }
}

void X64::op(StackOp opcode, int x) {
    if (opcode == PUSHQ) {
        code_byte(0x68);
        code_dword(x);  // 32-bit immediate only
    }
    else
        std::cerr << "WAT?\n";
}

void X64::op(StackOp opcode, Register x) {
    if (opcode == PUSHQ)
        code_byte(0x50 + x);
    else
        code_byte(0x58 + x);
}

void X64::op(StackOp opcode, Address x) {
    if (opcode == PUSHQ) {
        code_byte(0xFF);
        effective_address(6, x);
    }
    else {
        code_byte(0x8F);
        effective_address(0, x);
    }
}




struct {
        int op;
        int modrm;
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
    code_op(memory_info[opcode].op, 0);
    effective_address(memory_info[opcode].modrm, x);
}




int registerfirst_info[] = {
    0x0FAF, 0x0F02, 0x0F03
};

void X64::op(RegisterFirstOp opcode, Register x, Register y) {
    code_op(registerfirst_info[opcode], 0);
    effective_address(x, y);
}

void X64::op(RegisterFirstOp opcode, Register x, Address y) {
    code_op(registerfirst_info[opcode], 0);
    effective_address(x, y);
}




void X64::op(RegisterConstantOp opcode, Register x, Register y, int z) {
    if (opcode == IMUL3Q) {
        // The lowest bit does not work here the usual way, so...
        code_byte(0x48);  // Hardcoded REX
        code_byte(0x69);  // Hardcoded opcode
        effective_address(x, y);
        code_dword(z);  // 32-bit immediate only
    }
}




int registersecond_info[] = {
    0x63, 0x0FA5, 0x0FAD
};

void X64::op(RegisterSecondOp opcode, Register x, Register y) {
    code_op(registersecond_info[opcode], 0);
    effective_address(y, x);
}

void X64::op(RegisterSecondOp opcode, Address x, Register y) {
    code_op(registersecond_info[opcode], 0);
    effective_address(y, x);
}




int registermemory_info[] = {
    0xC5, 0x8D, 0xC4
};


void X64::op(RegisterMemoryOp opcode, Register x, Address y) {
    //std::cerr << "HIHI\n";
    code_op(registermemory_info[opcode], 0);
    effective_address(x, y);
}




void X64::op(BitSetOp opcode, Register x) {
    code_op(0x0F90 | opcode, 0);
    effective_address(0, x);  // 0 is modrm
}


void X64::op(BitSetOp opcode, Address x) {
    code_op(0x0F90 | opcode, 0);
    effective_address(0, x);  // 0 is modrm
}




void X64::op(BranchOp opcode, Label c) {
    code_op(0x0F80 | opcode, 0);  // use 32-bit offset at first
    code_reference(c, REF_CODE_RELATIVE);
}




void X64::op(JumpOp opcode, Label c) {
    if (opcode == CALL) {
        code_op(0xE8, 0);
        code_reference(c, REF_CODE_RELATIVE);
    }
    else if (opcode == JMP) {
        code_op(0xE9, 0);
        code_reference(c, REF_CODE_RELATIVE);
    }
    else if (opcode == LOOP) {
        code_op(0xE2, 0);
        code_reference(c, REF_CODE_RELATIVE);
    }
}


void X64::op(JumpOp opcode, Address x) {
    if (opcode == CALL) {
        code_op(0xFF, 0);
        effective_address(2, x);
    }
    else
        std::cerr << "Whacky jump!\n\a";
}


void X64::op(ConstantOp opcode, int x) {
    if (opcode == INT) {
        code_op(0xCD, 0);
        code_byte(x);
    }
    else if (opcode == RETX) {
        code_op(0xC2, 0);
        code_word(x);
    }
    else if (opcode == RETFX) {
        code_op(0xCA, 0);
        code_word(x);
    }
}
