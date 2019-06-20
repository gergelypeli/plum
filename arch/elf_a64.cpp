#include "../plum.h"


Elf_A64::Elf_A64(std::string module_name)
    :Elf(module_name) {
}


Elf64_Half Elf_A64::get_machine() {
    return EM_AARCH64;
}


void Elf_A64::info_relocation32(unsigned index, Elf64_Addr location, int addend) {
    add_relocation(index, location, addend, R_AARCH64_ABS32, info_relocations);
}


void Elf_A64::info_relocation64(unsigned index, Elf64_Addr location, int addend) {
    add_relocation(index, location, addend, R_AARCH64_ABS64, info_relocations);
}


void Elf_A64::line_relocation64(unsigned index, Elf64_Addr location, int addend) {
    add_relocation(index, location, addend, R_AARCH64_ABS64, line_relocations);
}


void Elf_A64::code_jump_relocation(unsigned index, Elf64_Addr location, int addend) {
    add_relocation(index, location, addend, R_AARCH64_JUMP26, code_relocations);
}


void Elf_A64::code_branch_relocation(unsigned index, Elf64_Addr location, int addend) {
    add_relocation(index, location, addend, R_AARCH64_CONDBR19, code_relocations);
}


void Elf_A64::code_adr_relocation(unsigned index, Elf64_Addr location, int addend) {
    add_relocation(index, location, addend, R_AARCH64_ADR_PREL_LO21, code_relocations);
}


void Elf_A64::data_relocation(unsigned index, Elf64_Addr location, int addend) {
    add_relocation(index, location, addend, R_AARCH64_ABS64, data_relocations);
}
