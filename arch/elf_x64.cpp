#include "../plum.h"


Elf_X64::Elf_X64(std::string module_name)
    :Elf(module_name) {
}


Elf64_Half Elf_X64::get_machine() {
    return EM_X86_64;
}


void Elf_X64::info_relocation32(unsigned index, Elf64_Addr location, int addend) {
    add_relocation(index, location, addend, R_X86_64_32, info_relocations);
}


void Elf_X64::info_relocation64(unsigned index, Elf64_Addr location, int addend) {
    add_relocation(index, location, addend, R_X86_64_64, info_relocations);
}


void Elf_X64::line_relocation64(unsigned index, Elf64_Addr location, int addend) {
    add_relocation(index, location, addend, R_X86_64_64, line_relocations);
}


void Elf_X64::frame_relocation64(unsigned index, Elf64_Addr location, int addend) {
    add_relocation(index, location, addend, R_X86_64_64, frame_relocations);
}


void Elf_X64::code_relocation(unsigned index, Elf64_Addr location, int addend) {
    add_relocation(index, location, addend, R_X86_64_PC32, code_relocations);
}


void Elf_X64::data_relocation(unsigned index, Elf64_Addr location, int addend) {
    add_relocation(index, location, addend, R_X86_64_64, data_relocations);
}
