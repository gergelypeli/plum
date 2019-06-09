#include "../plum.h"


Cc_X64::Cc_X64(std::string module_name) {
    elf_x64 = new Elf_X64(module_name);
    elf = elf_x64;
    
    asm_x64 = new Asm_X64();
    asm_x64->set_referrer_x64(this);
}


void Cc_X64::data_reference(Label label, int addend) {
    if (!label.def_index) {
        std::cerr << "Can't reference an undeclared label!\n";
        throw ASM_ERROR;
    }

    refs.push_back({ REF_DATA_ABSOLUTE, data.size(), label.def_index, addend });
    data_qword(0);  // 64-bit relocations only
}


void Cc_X64::code_reference(Label label, int addend) {
    if (!label.def_index) {
        std::cerr << "Can't reference an undeclared label!\n";
        throw ASM_ERROR;
    }

    refs.push_back({ REF_CODE_RELATIVE, code.size(), label.def_index, addend });
    code_dword(0);  // 32-bit offset only
}


void Cc_X64::process_relocations() {
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


void Cc_X64::op(SimpleOp opcode) { asm_x64->op((X::SimpleOp)opcode); }
void Cc_X64::op(UnaryOp opcode, Register x) { asm_x64->op((X::UnaryOp)opcode, x); }
void Cc_X64::op(UnaryOp opcode, Address x) { asm_x64->op((X::UnaryOp)opcode, x); }
void Cc_X64::op(PortOp opcode) { asm_x64->op((X::PortOp)opcode); }
void Cc_X64::op(PortOp opcode, int x) { asm_x64->op((X::PortOp)opcode, x); }
void Cc_X64::op(StringOp opcode) { asm_x64->op((X::StringOp)opcode); }
void Cc_X64::op(BinaryOp opcode, Register x, int y) { asm_x64->op((X::BinaryOp)opcode, x, y); }
void Cc_X64::op(BinaryOp opcode, Address x, int y) { asm_x64->op((X::BinaryOp)opcode, x, y); }
void Cc_X64::op(BinaryOp opcode, Register x, Register y) { asm_x64->op((X::BinaryOp)opcode, x, y); }
void Cc_X64::op(BinaryOp opcode, Register x, HighByteRegister y) { asm_x64->op((X::BinaryOp)opcode, x, y); }
void Cc_X64::op(BinaryOp opcode, Address x, Register y) { asm_x64->op((X::BinaryOp)opcode, x, y); }
void Cc_X64::op(BinaryOp opcode, Register x, Address y) { asm_x64->op((X::BinaryOp)opcode, x, y); }
void Cc_X64::op(MovabsOp opcode, Register x, int64 y) { asm_x64->op((X::MovabsOp)opcode, x, y); }  // 64-bit immediate capable
void Cc_X64::op(ShiftOp opcode, Register x, Register cl) { asm_x64->op((X::ShiftOp)opcode, x, cl); }
void Cc_X64::op(ShiftOp opcode, Address x, Register cl) { asm_x64->op((X::ShiftOp)opcode, x, cl); }
void Cc_X64::op(ShiftOp opcode, Register x, char y) { asm_x64->op((X::ShiftOp)opcode, x, y); }
void Cc_X64::op(ShiftOp opcode, Address x, char y) { asm_x64->op((X::ShiftOp)opcode, x, y); }
void Cc_X64::op(ExchangeOp opcode, Register x, Register y) { asm_x64->op((X::ExchangeOp)opcode, x, y); }
void Cc_X64::op(ExchangeOp opcode, Address x, Register y) { asm_x64->op((X::ExchangeOp)opcode, x, y); }
void Cc_X64::op(ExchangeOp opcode, Register x, Address y) { asm_x64->op((X::ExchangeOp)opcode, x, y); }
void Cc_X64::op(StackOp opcode, int x) { asm_x64->op((X::StackOp)opcode, x); }
void Cc_X64::op(StackOp opcode, Register x) { asm_x64->op((X::StackOp)opcode, x); }
void Cc_X64::op(StackOp opcode, Address x) { asm_x64->op((X::StackOp)opcode, x); }
void Cc_X64::op(MemoryOp opcode, Address x) { asm_x64->op((X::MemoryOp)opcode, x); }
void Cc_X64::op(RegisterFirstOp opcode, Register x, Register y) { asm_x64->op((X::RegisterFirstOp)opcode, x, y); }
void Cc_X64::op(RegisterFirstOp opcode, Register x, Address y) { asm_x64->op((X::RegisterFirstOp)opcode, x, y); }
void Cc_X64::op(Imul3Op opcode, Register x, Register y, int z) { asm_x64->op((X::Imul3Op)opcode, x, y, z); }
void Cc_X64::op(Imul3Op opcode, Register x, Address y, int z) { asm_x64->op((X::Imul3Op)opcode, x, y, z); }
void Cc_X64::op(RegisterMemoryOp opcode, Register x, Address y) { asm_x64->op((X::RegisterMemoryOp)opcode, x, y); }
void Cc_X64::op(BitSetOp opcode, Register x) { asm_x64->op((X::BitSetOp)opcode, x); }
void Cc_X64::op(BitSetOp opcode, HighByteRegister x) { asm_x64->op((X::BitSetOp)opcode, x); }
void Cc_X64::op(BitSetOp opcode, Address x) { asm_x64->op((X::BitSetOp)opcode, x); }
void Cc_X64::op(BranchOp opcode, Label c) { asm_x64->op((X::BranchOp)opcode, c); }
void Cc_X64::op(JumpOp opcode, Label c) { asm_x64->op((X::JumpOp)opcode, c); }
void Cc_X64::op(JumpOp opcode, Address x) { asm_x64->op((X::JumpOp)opcode, x); }
void Cc_X64::op(JumpOp opcode, Register x) { asm_x64->op((X::JumpOp)opcode, x); }
void Cc_X64::op(ConstantOp opcode, int x) { asm_x64->op((X::ConstantOp)opcode, x); }

void Cc_X64::op(SsememSsememOp opcode, SseRegister x, SseRegister y) { asm_x64->op((X::SsememSsememOp)opcode, x, y); }
void Cc_X64::op(SsememSsememOp opcode, SseRegister x, Address y) { asm_x64->op((X::SsememSsememOp)opcode, x, y); }
void Cc_X64::op(SsememSsememOp opcode, Address x, SseRegister y) { asm_x64->op((X::SsememSsememOp)opcode, x, y); }

void Cc_X64::op(SseSsememOp opcode, SseRegister x, SseRegister y) { asm_x64->op((X::SseSsememOp)opcode, x, y); }
void Cc_X64::op(SseSsememOp opcode, SseRegister x, Address y) { asm_x64->op((X::SseSsememOp)opcode, x, y); }

void Cc_X64::op(SseGprmemOp opcode, SseRegister x, Register y) { asm_x64->op((X::SseGprmemOp)opcode, x, y); }
void Cc_X64::op(SseGprmemOp opcode, SseRegister x, Address y) { asm_x64->op((X::SseGprmemOp)opcode, x, y); }

void Cc_X64::op(GprSsememOp opcode, Register x, SseRegister y) { asm_x64->op((X::GprSsememOp)opcode, x, y); }
void Cc_X64::op(GprSsememOp opcode, Register x, Address y) { asm_x64->op((X::GprSsememOp)opcode, x, y); }
