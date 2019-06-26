#include "../plum.h"


// Zero register / official stack pointer (varies by instructions)
const Register XZR = (Register)31;
const Register XSP = (Register)31;

// Link register
const Register XLR = (Register)30;

// Frame Pointer
const Register XFP = (Register)29;

// Architectural scratch registers
const Register XADDRESS = (Register)17;
const Register XVALUE = (Register)16;


Emu_A64::Emu_A64(std::string module_name) {
    elf_a64 = new Elf_A64(module_name);
    elf = elf_a64;
    
    asm_a64 = new Asm_A64();
    asm_a64->set_referrer_a64(this);
}


void Emu_A64::add_ref(Ref r) {
    if (!r.def_index) {
        std::cerr << "Can't reference an undeclared label!\n";
        throw ASM_ERROR;
    }

    refs.push_back(r);
}


void Emu_A64::data_reference(Label label, int addend) {
    add_ref({ REF_DATA_ABSOLUTE, get_dc(), label.def_index, addend });
    data_qword(0);  // 64-bit relocations only
}


void Emu_A64::code_jump_reference(Label label, int addend) {
    add_ref({ REF_CODE_JUMP, get_pc(), label.def_index, addend });
    // No placeholder added
}


void Emu_A64::code_branch_reference(Label label, int addend) {
    add_ref({ REF_CODE_BRANCH, get_pc(), label.def_index, addend });
    // No placeholder added
}


void Emu_A64::code_adr_reference(Label label, int addend) {
    add_ref({ REF_CODE_ADR, get_pc(), label.def_index, addend });
    // No placeholder added
}


void Emu_A64::process_relocations() {
    for (auto &r : refs) {
        if (!defs.count(r.def_index)) {
            std::cerr << "Reference to undefined label " << r.def_index << "!\n";
            throw ASM_ERROR;
        }
        
        Def &d(defs.at(r.def_index));

        switch (r.type) {
        case REF_CODE_JUMP:
            // PC-relative offset imm26*4 at bits 25:0
            
            switch (d.type) {
            case DEF_CODE:
            case DEF_CODE_EXPORT: {
                int64 distance = (int64)(d.location - r.location + r.addend) / 4;
                    
                if (distance >= (1 << 25) || distance < -(1 << 25)) {
                    std::cerr << "REF_CODE_JUMP can't jump " << distance << " bytes!\n";
                    throw ASM_ERROR;
                }
                
                unsigned32 *u = (unsigned32 *)&code[r.location];
                *u = (*u & 0xfc000000) | ((distance << 0) & 0x03ffffff);
            }
                break;
            case DEF_CODE_IMPORT:
                elf_a64->code_jump_relocation(d.symbol_index, r.location, r.addend);
                break;
            default:
                std::cerr << "Can't code jump to this symbol!\n";
                throw ASM_ERROR;
            }
            break;
            
        case REF_CODE_BRANCH:
            // PC-relative offset imm19*4 at bits 23:5.
            
            switch (d.type) {
            case DEF_CODE:
            case DEF_CODE_EXPORT: {
                int64 distance = (int64)(d.location - r.location + r.addend) / 4;
                
                if (distance >= (1 << 18) || distance < -(1 << 18)) {
                    std::cerr << "REF_CODE_BRANCH can't jump " << distance << " bytes!\n";
                    throw ASM_ERROR;
                }

                unsigned32 *u = (unsigned32 *)&code[r.location];
                *u = (*u & 0xff00001f) | ((distance << 5) & 0x00ffffe0);
            }
                break;
            case DEF_CODE_IMPORT:
                elf_a64->code_branch_relocation(d.symbol_index, r.location, r.addend);
                break;
            default:
                std::cerr << "Can't code branch to this symbol!\n";
                throw ASM_ERROR;
            }
            break;

        case REF_CODE_ADR:
            // PC-relative offset imm19:imm2 at some batshit bit positions.
            
            switch (d.type) {
            case DEF_CODE:
            case DEF_CODE_EXPORT: {
                int64 distance = d.location - r.location + r.addend;
                
                if (distance >= (1 << 20) || distance < -(1 << 20)) {
                    std::cerr << "REF_CODE_ADR can't address " << distance << " bytes!\n";
                    throw ASM_ERROR;
                }

                int immlo = distance & 3;
                int immhi = distance >> 2;

                unsigned32 *u = (unsigned32 *)&code[r.location];
                *u = (*u & 0x9f00001f) | ((immhi << 5) & 0x00ffffe0) | ((immlo << 29) & 0x60000000);
            }
                break;
            case DEF_CODE_IMPORT:
                elf_a64->code_adr_relocation(d.symbol_index, r.location, r.addend);
                break;
            case DEF_DATA_EXPORT:
            case DEF_DATA:
                elf_a64->code_adr_relocation(elf_a64->data_start_sym, r.location, d.location + r.addend);
                break;
            default:
                std::cerr << "Can't code adr to this symbol!\n";
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
                elf_a64->data_relocation(elf_a64->data_start_sym, r.location, d.location + r.addend);
                break;
            case DEF_CODE:
            case DEF_CODE_EXPORT:
                elf_a64->data_relocation(elf_a64->code_start_sym, r.location, d.location + r.addend);
                break;
            case DEF_CODE_IMPORT:
                elf_a64->data_relocation(d.symbol_index, r.location, r.addend);
                break;
            default:
                std::cerr << "Can't relocate data absolute to this symbol!\n";
                throw ASM_ERROR;
            }
            break;
        }
    }
}


std::array<Register, 4> Emu_A64::abi_arg_regs() {
    return { (Register)0, (Register)1, (Register)2, (Register)3 };
}


std::array<SseRegister, 4> Emu_A64::abi_arg_sses() {
    return { (SseRegister)0, (SseRegister)1, (SseRegister)2, (SseRegister)3 };
}


std::array<Register, 2> Emu_A64::abi_res_regs() {
    return { (Register)0, (Register)1 };
}


std::array<SseRegister, 2> Emu_A64::abi_res_sses() {
    return { (SseRegister)0, (SseRegister)1 };
}


void Emu_A64::prologue() {
    pushq(XLR);
    pushq(RBP);
    
    op(MOVQ, RBP, RSP);
    op(MOVQ, XFP, RBP);  // for debuggers
}


void Emu_A64::epilogue() {
    popq(RBP);
    popq(XLR);
    
    op(MOVQ, XFP, RBP);  // for debuggers
    op(RET);
}


void Emu_A64::start() {
    // Shadow XSP as RSP
    asm_a64->op(A::ADD, RSP, XSP, 0);  // MOV (ORR) would use XZR instead of XSP
    
    // Shadow XFP as RBP
    op(MOVQ, RBP, XFP);
    
    // Adjust the original XSP so the to-be-created start frame fits above it
    asm_a64->op(A::SUB, XSP, XSP, 16);
}


int Emu_A64::dwarf_register_number(Register r) {
    return (int)r;
}


int Emu_A64::dwarf_sseregister_number(SseRegister s) {
    return 64 + (int)s;
}


int Emu_A64::dwarf_retaddr_number() {
    return 30;
}


A::MemOpcode Emu_A64::ldrs(int os) {
    return (
        os % 4 == 0 ? A::LDRSB :
        os % 4 == 1 ? A::LDRSH :
        os % 4 == 2 ? A::LDRSW :
        A::LDRQ
    );
}


A::MemOpcode Emu_A64::ldru(int os) {
    return (
        os % 4 == 0 ? A::LDRUB :
        os % 4 == 1 ? A::LDRUH :
        os % 4 == 2 ? A::LDRUW :
        A::LDRQ
    );
}


A::MemOpcode Emu_A64::str(int os) {
    return (
        os % 4 == 0 ? A::STRB :
        os % 4 == 1 ? A::STRH :
        os % 4 == 2 ? A::STRW :
        A::STRQ
    );
}


void Emu_A64::movimm(Register x, int64 y) {
    // TODO: can be more optimal
    
    if (y >= 0 && y < 65536)
        asm_a64->op(A::MOVZ, x, y & 0xffff, A::LSL_0);
    else if (y < 0 && y >= -65536)
        asm_a64->op(A::MOVN, x, ~y & 0xffff, A::LSL_0);
    else {
        asm_a64->op(A::MOVK, x, (y >> 48) & 0xffff, A::LSL_48);
        asm_a64->op(A::MOVK, x, (y >> 32) & 0xffff, A::LSL_32);
        asm_a64->op(A::MOVK, x, (y >> 16) & 0xffff, A::LSL_16);
        asm_a64->op(A::MOVK, x, (y >> 0) & 0xffff, A::LSL_0);
    }
}


void Emu_A64::pushq(Register r) {
    asm_a64->op(A::STRQ, r, RSP, -8, A::INCREMENT_PRE);
}


void Emu_A64::popq(Register r) {
    asm_a64->op(A::LDRQ, r, RSP, 8, A::INCREMENT_POST);
}


void Emu_A64::lea(Register x, Address y) {
    if (y.label.def_index) {
        if (y.base != NOREG || y.index != NOREG)
            throw ASM_ERROR;
            
        asm_a64->op(A::ADR, x, y.label, y.offset);
        return;
    }

    // TODO: this can be more optimal
    // NOTE: but must not set flags here
    // NOTE: x can be among the components of y!
    
    Register r;
    
    if (y.index != NOREG) {
        // Must read all register components first, as we clobber a register here
        asm_a64->op(A::ADD, x, y.base, y.index, A::SHIFT_LSL, (int)y.scale);
        r = x;
    }
    else {
        r = y.base;
    }

    // NOTE: this is different from the regular add immediate handling in two ways:
    //   * the flags are unchanged
    //   * we assume a writable destination register for multiple steps to work
    //   * uses no extra register
    //   * zero is optimized out
    
    int z = y.offset;
    
    if (z > 0) {
        asm_a64->op(A::ADD, x, r, z & 0xfff);
        r = x;
        
        if (z >= 4096) {
            asm_a64->op(A::ADD, x, x, (z >> 12) & 0xfff, A::SHIFT12_YES);
            
            if (z >= 4096 * 4096)
                throw ASM_ERROR;
        }
    }
    else if (z < 0) {
        asm_a64->op(A::SUB, x, r, -z & 0xfff);
        r = x;
        
        if (-z >= 4096) {
            asm_a64->op(A::SUB, x, x, (-z >> 12) & 0xfff, A::SHIFT12_YES);
            
            if (-z >= 4096 * 4096)
                throw ASM_ERROR;
        }
    }
    
    if (r != x) {
        op(MOVQ, x, r);
        r = x;
    }
}


Emu_A64::Addressing Emu_A64::prepare(int os, Address a) {
    if (a.index == NOREG && a.offset >= -256 && a.offset < 256 && a.label.def_index == 0)
        return { Addressing::OFFSET_UNSCALED, a.base, NOREG, a.offset };
        
    if (a.index == NOREG && a.offset >= 0 && (a.offset & ((1 << os) - 1)) == 0 && a.offset >> os < 4096 && a.label.def_index == 0)
        return { Addressing::OFFSET_SCALED, a.base, NOREG, a.offset };
        
    if (a.index != NOREG && a.scale == Address::SCALE_1 && a.offset == 0 && a.label.def_index == 0)
        return { Addressing::OFFSET_REGISTER, a.base, a.index, 0 };

    if (a.index != NOREG && (int)a.scale == os && a.offset == 0 && a.label.def_index == 0)
        return { Addressing::OFFSET_REGISTER_SHIFTED, a.base, a.index, 0 };
        
    lea(XADDRESS, a);
    
    return { Addressing::OFFSET_SCALED, XADDRESS, NOREG, 0 };
}


void Emu_A64::mem(A::MemOpcode opcode, Register rt, Addressing a) {
    switch (a.mode) {
    case Addressing::OFFSET_SCALED:
        asm_a64->op(opcode, rt, a.base, a.imm, A::UNSIGNED_SCALED);
        break;
    case Addressing::OFFSET_UNSCALED:
        asm_a64->op(opcode, rt, a.base, a.imm, A::SIGNED_UNSCALED);
        break;
    case Addressing::OFFSET_REGISTER:
        asm_a64->op(opcode, rt, a.base, a.index, A::INDEX_UNSHIFTED);
        break;
    case Addressing::OFFSET_REGISTER_SHIFTED:
        asm_a64->op(opcode, rt, a.base, a.index, A::INDEX_SHIFTED);
        break;
    default:
        throw ASM_ERROR;
    }
}


void Emu_A64::op(SimpleOp opcode) {
    switch (opcode) {
    case NOP:
        asm_a64->op(A::NOP);
        break;
    case POPFQ:
        popq(XVALUE);
        asm_a64->op(A::MSRW, A::SPECREG_CONDFLAGS, XVALUE);
        break;
    case PUSHFQ:
        asm_a64->op(A::MSRR, A::SPECREG_CONDFLAGS, XVALUE);
        pushq(XVALUE);
        break;
    case RET:
        asm_a64->op(A::RET, XLR);
        break;
    case UD2:
        asm_a64->op(A::UDF);
        break;
    default:
        throw ASM_ERROR;
    }
}


void Emu_A64::op(UnaryOp opcode, Register x) {
    switch (opcode % 3) {
    case DECQ:
        asm_a64->op(A::SUBS, x, x, 1);
        break;
    case INCQ:
        asm_a64->op(A::ADDS, x, x, 1);
        break;
    case NEGQ:
        asm_a64->op(A::SUBS, x, XZR, x);
        break;
    case NOTQ:
        asm_a64->op(A::ORN, x, XZR, x);
        break;
    default:
        throw ASM_ERROR;
    }
}


void Emu_A64::op(UnaryOp opcode, Address x) {
    int os = opcode & 3;
    Addressing aing = prepare(os, x);
    
    mem(ldrs(os), XVALUE, aing);
    
    switch (opcode % 3) {
    case DECQ:
        asm_a64->op(A::SUBS, XVALUE, XVALUE, 1);
        break;
    case INCQ:
        asm_a64->op(A::ADDS, XVALUE, XVALUE, 1);
        break;
    case NEGQ:
        asm_a64->op(A::SUBS, XVALUE, XZR, XVALUE);
        break;
    case NOTQ:
        asm_a64->op(A::ORN, XVALUE, XZR, XVALUE);
        break;
    default:
        throw ASM_ERROR;
    }
    
    mem(str(os), XVALUE, aing);
}


void Emu_A64::op(StringOp opcode) {
    Label start, end;
    
    asm_a64->op(A::SUBS, XZR, RCX, 0);
    asm_a64->op(A::B, A::CC_EQ, end);
    
    code_label(start);
    
    switch (opcode) {
    case REPMOVSB:
        asm_a64->op(A::LDRUB, XVALUE, RSI, 1, A::INCREMENT_POST);
        asm_a64->op(A::STRB, XVALUE, RDI, 1, A::INCREMENT_POST);
        break;
    case REPECMPSW:
        asm_a64->op(A::LDRUH, XVALUE, RSI, 2, A::INCREMENT_POST);
        asm_a64->op(A::LDRUH, XADDRESS, RDI, 2, A::INCREMENT_POST);  // Abuse
        asm_a64->op(A::SUBS, XZR, XVALUE, XADDRESS);
        asm_a64->op(A::B, A::CC_NE, end);
        break;
    default:
        throw ASM_ERROR;
    }

    asm_a64->op(A::SUBS, RCX, RCX, 1);
    asm_a64->op(A::B, A::CC_NE, start);
    
    code_label(end);
}


void Emu_A64::op(BinaryOp opcode, Register x, int y) {
    switch (opcode % 3) {
    case ADCQ:
        throw ASM_ERROR;
        break;
    case ADDQ:
        if (y >= 0 && y < 4096)
            asm_a64->op(A::ADDS, x, x, y);
        else if (-y > 0 && -y < 4096)
            asm_a64->op(A::SUBS, x, x, -y);
        else {
            movimm(XVALUE, y);
            asm_a64->op(A::ADDS, x, x, XVALUE);
        }
        break;
    case ANDQ:
        // TODO: may be optimized for consecutive bits
        movimm(XVALUE, y);
        asm_a64->op(A::ANDS, x, x, XVALUE);
        break;
    case CMPQ:
        if (y >= 0 && y < 4096)
            asm_a64->op(A::SUBS, XZR, x, y);
        else if (-y > 0 && -y < 4096)
            asm_a64->op(A::ADDS, XZR, x, -y);
        else {
            movimm(XVALUE, y);
            asm_a64->op(A::SUBS, XZR, x, XVALUE);
        }
        break;
    case MOVQ:
        movimm(x, y);
        break;
    case ORQ:
        // TODO: may be optimized for consecutive bits
        movimm(XVALUE, y);
        asm_a64->op(A::ORR, x, x, XVALUE);
        break;
    case SBBQ:
        throw ASM_ERROR;
        break;
    case SUBQ:
        if (y >= 0 && y < 4096)
            asm_a64->op(A::SUBS, x, x, y);
        else if (-y > 0 && -y < 4096)
            asm_a64->op(A::ADDS, x, x, -y);
        else {
            movimm(XVALUE, y);
            asm_a64->op(A::SUBS, x, x, XVALUE);
        }
        break;
    case TESTQ:
        // TODO: may be optimized for consecutive bits
        movimm(XVALUE, y);
        asm_a64->op(A::ANDS, XZR, x, XVALUE);
        break;
    case XORQ:
        // TODO: may be optimized for consecutive bits
        movimm(XVALUE, y);
        asm_a64->op(A::EOR, x, x, XVALUE);
        break;
    default:
        throw ASM_ERROR;
    }
}


void Emu_A64::op(BinaryOp opcode, Address x, int y) {
    int os = opcode & 3;
    Addressing aing = prepare(os, x);
    
    mem(ldrs(os), XVALUE, aing);
    
    op(opcode, XVALUE, y);
    
    mem(str(os), XVALUE, aing);
}


void Emu_A64::op(BinaryOp opcode, Register x, Register y) {
    switch (opcode % 3) {
    case ADCQ:
        throw ASM_ERROR;
        break;
    case ADDQ:
        asm_a64->op(A::ADDS, x, x, y);
        break;
    case ANDQ:
        asm_a64->op(A::ANDS, x, x, y);
        break;
    case CMPQ:
        asm_a64->op(A::SUBS, XZR, x, y);
        break;
    case MOVQ:
        asm_a64->op(A::ORR, x, XZR, y);
        break;
    case ORQ:
        asm_a64->op(A::ORR, x, x, y);
        break;
    case SBBQ:
        throw ASM_ERROR;
        break;
    case SUBQ:
        asm_a64->op(A::SUBS, x, x, y);
        break;
    case TESTQ:
        asm_a64->op(A::ANDS, XZR, x, y);
        break;
    case XORQ:
        asm_a64->op(A::EOR, x, x, y);
        break;
    default:
        throw ASM_ERROR;
    }
}


void Emu_A64::op(BinaryOp opcode, Address x, Register y) {
    int os = opcode & 3;
    Addressing aing = prepare(os, x);
    
    if (opcode % 3 == MOVQ) {
        // Don't load the former value just to be overwritten
        mem(str(os), y, aing);
    }
    else {
        mem(ldrs(os), XVALUE, aing);
        op(opcode, XVALUE, y);
        mem(str(os), XVALUE, aing);
    }
        
}


void Emu_A64::op(BinaryOp opcode, Register x, Address y) {
    int os = opcode & 3;
    Addressing aing = prepare(os, y);

    if (opcode % 3 == MOVQ) {
        // Don't load the value to a scratch
        mem(ldrs(os), x, aing);
    }
    else {
        mem(ldrs(os), XVALUE, aing);
        op(opcode, x, XVALUE);
    }
}


void Emu_A64::op(MovabsOp opcode, Register x, int64 y) {
    // TODO: can be more optimal
    movimm(x, y);
}


void Emu_A64::op(ShiftOp opcode, Register x, Register y) {
    // TODO: we don't do smaller shifts
    switch (opcode) {
    case RCLQ:
        throw ASM_ERROR;
        break;
    case RCRQ:
        throw ASM_ERROR;
        break;
    case ROLQ:
        asm_a64->op(A::SUB, XVALUE, XZR, y);
        asm_a64->op(A::ROR, x, x, XVALUE);
        break;
    case RORQ:
        asm_a64->op(A::ROR, x, x, y);
        break;
    case SALQ:
        asm_a64->op(A::LSL, x, x, y);
        break;
    case SARQ:
        asm_a64->op(A::ASR, x, x, y);
        break;
    case SHLQ:
        asm_a64->op(A::LSL, x, x, y);
        break;
    case SHRQ:
        asm_a64->op(A::LSR, x, x, y);
        break;
    default:
        throw ASM_ERROR;
    }
}


void Emu_A64::op(ShiftOp opcode, Register x, char y) {
    // TODO: we don't do smaller shifts
    switch (opcode) {
    case RCLQ:
        throw ASM_ERROR;
        break;
    case RCRQ:
        throw ASM_ERROR;
        break;
    case ROLQ:
        asm_a64->op(A::EXTR, x, x, x, -y % 64);
        break;
    case RORQ:
        asm_a64->op(A::EXTR, x, x, x, y % 64);
        break;
    case SALQ:
        asm_a64->op(A::SBFM, x, x, A::BitMask(63 - y, 64 - y));
        break;
    case SARQ:
        asm_a64->op(A::SBFM, x, x, A::BitMask(63, y));
        break;
    case SHLQ:
        asm_a64->op(A::UBFM, x, x, A::BitMask(63 - y, 64 - y));
        break;
    case SHRQ:
        asm_a64->op(A::UBFM, x, x, A::BitMask(63, y));
        break;
    default:
        throw ASM_ERROR;
    }

}


void Emu_A64::op(ShiftOp opcode, Address x, char y) {
    int os = opcode & 3;
    Addressing aing = prepare(os, x);
    
    mem(ldrs(os), XVALUE, aing);
    
    op(opcode, XVALUE, y);
    
    mem(str(os), XVALUE, aing);
}


void Emu_A64::op(ExchangeOp opcode, Register x, Register y) {
    op(MOVQ, XVALUE, x);
    op(MOVQ, x, y);
    op(MOVQ, y, XVALUE);
}


void Emu_A64::op(ExchangeOp opcode, Address x, Register y) {
    // TODO: use exclusive load and store!

    int os = opcode & 3;
    Addressing aing = prepare(os, x);
    
    mem(ldrs(os), XVALUE, aing);
    
    mem(str(os), y, aing);
    
    op(MOVQ, y, XVALUE);
}


void Emu_A64::op(ExchangeOp opcode, Register x, Address y) {
    op(opcode, y, x);
}


void Emu_A64::op(StackOp opcode, int x) {
    switch (opcode) {
    case PUSHQ:
        movimm(XVALUE, x);
        pushq(XVALUE);
        break;
    default:
        throw ASM_ERROR;
    }
}


void Emu_A64::op(StackOp opcode, Register x) {
    switch (opcode) {
    case PUSHQ:
        pushq(x);
        break;
    case POPQ:
        popq(x);
        break;
    default:
        throw ASM_ERROR;
    }
}


void Emu_A64::op(StackOp opcode, Address x) {
    int os = 3;
    Addressing aing = prepare(os, x);
    
    switch (opcode) {
    case PUSHQ:
        mem(ldrs(os), XVALUE, aing);
        pushq(XVALUE);
        break;
    case POPQ:
        popq(XVALUE);
        mem(str(os), XVALUE, aing);
        break;
    default:
        throw ASM_ERROR;
    }
}

void Emu_A64::op(RegisterFirstOp opcode, Register x, Register y) {
    switch (opcode) {
    case IMUL2W:
    case IMUL2D:
    case IMUL2Q:
        asm_a64->op(A::MADD, x, x, y, XZR);
        break;
    case MOVSXBQ:
        asm_a64->op(A::SBFM, x, y, A::BitMask(7, 0));
        break;
    case MOVSXWQ:
        asm_a64->op(A::SBFM, x, y, A::BitMask(15, 0));
        break;
    case MOVSXDQ:
        asm_a64->op(A::SBFM, x, y, A::BitMask(31, 0));
        break;
    case MOVZXBQ:
        asm_a64->op(A::UBFM, x, y, A::BitMask(7, 0));
        break;
    case MOVZXWQ:
        asm_a64->op(A::UBFM, x, y, A::BitMask(15, 0));
        break;
    case MOVZXDQ:
        asm_a64->op(A::UBFM, x, y, A::BitMask(31, 0));
        break;
    default:
        throw ASM_ERROR;
    }
}

void Emu_A64::op(RegisterFirstOp opcode, Register x, Address y) {
    int os = opcode & 3;
    Addressing aing = prepare(os, y);

    switch (opcode) {
    case IMUL2W:
    case IMUL2D:
    case IMUL2Q:
        mem(ldrs(os), XVALUE, aing);
        asm_a64->op(A::MADD, x, x, XVALUE, XZR);
        break;
    case MOVSXBQ:
    case MOVSXWQ:
    case MOVSXDQ:
        mem(ldrs(os), x, aing);
        break;
    case MOVZXBQ:
    case MOVZXWQ:
    case MOVZXDQ:
        mem(ldru(os), x, aing);
        break;
    default:
        throw ASM_ERROR;
    }
}


void Emu_A64::op(Imul3Op opcode, Register x, Register y, int z) {
    movimm(XVALUE, z);
    asm_a64->op(A::MADD, x, x, XVALUE, XZR);
}


void Emu_A64::op(Imul3Op opcode, Register x, Address y, int z) {
    int os = opcode & 3;
    Addressing aing = prepare(os, y);

    mem(ldrs(os), x, aing);
    movimm(XVALUE, z);
    asm_a64->op(A::MADD, x, x, XVALUE, XZR);
}


void Emu_A64::op(RegisterMemoryOp opcode, Register x, Address y) {
    if (opcode != LEA)
        throw ASM_ERROR;
        
    lea(x, y);
}


static A::CondCode cc(ConditionCode c) {
    return (
        c == CC_OVERFLOW ? A::CC_VS :
        c == CC_NOT_OVERFLOW ? A::CC_VC :
        c == CC_BELOW ? A::CC_LO :
        c == CC_ABOVE_EQUAL ? A::CC_HS :
        c == CC_EQUAL ? A::CC_EQ :
        c == CC_NOT_EQUAL ? A::CC_NE :
        c == CC_BELOW_EQUAL ? A::CC_LS :
        c == CC_ABOVE ? A::CC_HI :
        c == CC_SIGN ? A::CC_MI :
        c == CC_NOT_SIGN ? A::CC_PL :
        c == CC_PARITY ? throw ASM_ERROR :
        c == CC_NOT_PARITY ? throw ASM_ERROR :
        c == CC_LESS ? A::CC_LT :
        c == CC_GREATER_EQUAL ? A::CC_GE :
        c == CC_LESS_EQUAL ? A::CC_LE :
        c == CC_GREATER ? A::CC_GT :
        throw ASM_ERROR
    );
}

void Emu_A64::op(BitSetOp opcode, Register x) {
    if (opcode == SETP || opcode == SETNP)
        asm_a64->op(A::UDF);
    else
        asm_a64->op(A::CSINC, cc(negated((ConditionCode)opcode)), x, XZR, XZR);
}


void Emu_A64::op(BitSetOp opcode, Address x) {
    int os = 0;
    Addressing aing = prepare(os, x);

    op(opcode, XVALUE);

    mem(str(os), XVALUE, aing);
}


void Emu_A64::op(BranchOp opcode, Label c) {
    if (opcode == JP || opcode == JNP)
        asm_a64->op(A::UDF);
    else
        asm_a64->op(A::B, cc((ConditionCode)opcode), c);
}


void Emu_A64::op(JumpOp opcode, Label c) {
    switch (opcode) {
    case JMP:
        asm_a64->op(A::B, c);
        break;
    case CALL:
        asm_a64->op(A::BL, c);
        break;
    default:
        throw ASM_ERROR;
    }
}


void Emu_A64::op(JumpOp opcode, Address x) {
    int os = 3;
    Addressing aing = prepare(os, x);

    mem(A::LDRQ, XVALUE, aing);
    
    op(opcode, XVALUE);
}


void Emu_A64::op(JumpOp opcode, Register x) {
    switch (opcode) {
    case JMP:
        asm_a64->op(A::B, x);
        break;
    case CALL:
        asm_a64->op(A::BL, x);
        break;
    default:
        throw ASM_ERROR;
    }
}


void Emu_A64::op(SsememSsememOp opcode, SseRegister x, SseRegister y) {}
void Emu_A64::op(SsememSsememOp opcode, SseRegister x, Address y) {}
void Emu_A64::op(SsememSsememOp opcode, Address x, SseRegister y) {}

void Emu_A64::op(SseSsememOp opcode, SseRegister x, SseRegister y) {}
void Emu_A64::op(SseSsememOp opcode, SseRegister x, Address y) {}

void Emu_A64::op(SseGprmemOp opcode, SseRegister x, Register y) {}
void Emu_A64::op(SseGprmemOp opcode, SseRegister x, Address y) {}

void Emu_A64::op(GprSsememOp opcode, Register x, SseRegister y) {}
void Emu_A64::op(GprSsememOp opcode, Register x, Address y) {}

void Emu_A64::op(DivModOp opcode, Register x, Register y) {
    switch (opcode % 3) {
    case DIVQ:
        asm_a64->op(A::UDIV, x, x, y);
        break;
    case MODQ:
        asm_a64->op(A::UDIV, XVALUE, x, y);
        asm_a64->op(A::MSUB, x, XVALUE, y, x);
        break;
    case IDIVQ:
        asm_a64->op(A::SDIV, x, x, y);
        break;
    case IMODQ:
        asm_a64->op(A::SDIV, XVALUE, x, y);
        asm_a64->op(A::MSUB, x, XVALUE, y, x);
        break;
    default:
        throw ASM_ERROR;
    }
}

void Emu_A64::floatcmp(ConditionCode cc, SseRegister x, SseRegister y) {
}


void Emu_A64::floatorder(SseRegister x, SseRegister y) {
}
