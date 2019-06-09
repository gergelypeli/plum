#include "../plum.h"


Cc_X64::Cc_X64(std::string module_name) {
    elf_x64 = new Elf_X64(module_name);
    asm_x64 = new Asm_X64(elf_x64);
}

void Cc_X64::absolute_label(Label c, unsigned64 value, unsigned size) { asm_x64->absolute_label(c, value, size); }

void Cc_X64::data_align(int bytes) { asm_x64->data_align(bytes); }
void Cc_X64::data_blob(void *blob, int length) { asm_x64->data_blob(blob, length); }
void Cc_X64::data_byte(char x) { asm_x64->data_byte(x); }
void Cc_X64::data_word(int16 x) { asm_x64->data_word(x); }
void Cc_X64::data_dword(int x) { asm_x64->data_dword(x); }
void Cc_X64::data_qword(int64 x) { asm_x64->data_qword(x); }
void Cc_X64::data_zstring(std::string s) { asm_x64->data_zstring(s); }
void Cc_X64::data_double(double x) { asm_x64->data_double(x); }
void Cc_X64::data_label(Label c, unsigned size) { asm_x64->data_label(c, size); }
void Cc_X64::data_label_local(Label c, std::string name, unsigned size) { asm_x64->data_label_local(c, name, size); }
void Cc_X64::data_label_global(Label c, std::string name, unsigned size) { asm_x64->data_label_global(c, name, size); }

void Cc_X64::data_reference(Label c, int addend) { asm_x64->data_reference(c, addend); }
    
void Cc_X64::code_label(Label c, unsigned size) { return asm_x64->code_label(c, size); }
void Cc_X64::code_label_import(Label c, std::string name) { return asm_x64->code_label_import(c, name); };
void Cc_X64::code_label_local(Label c, std::string name, unsigned size) { return asm_x64->code_label_local(c, name, size); };
void Cc_X64::code_label_global(Label c, std::string name, unsigned size) { return asm_x64->code_label_global(c, name, size); };

int Cc_X64::get_pc() { return asm_x64->get_pc(); }
int Cc_X64::get_dc() { return asm_x64->get_dc(); }

void Cc_X64::done(std::string output) { asm_x64->done(output); }
    
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
