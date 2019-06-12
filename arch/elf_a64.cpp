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
