#include <stdio.h>
#include <string.h>

#include "ork.h"


Ork::Ork() {
    strings.push_back('\0');

    symbols.push_back(Elf32_Sym());
    Elf32_Sym &s = symbols.back();
    s.st_name = 0;
    s.st_value = 0;
    s.st_size = 0;
    s.st_info = 0;
    s.st_other = 0;
    s.st_shndx = 0;
}


Ork::~Ork() {
}


void Ork::done(std::string filename) {
    const int SECTION_COUNT = 8;
    
    Elf32_Ehdr ehdr;
    ehdr.e_ident[0] = ELFMAG0;
    ehdr.e_ident[1] = ELFMAG1;
    ehdr.e_ident[2] = ELFMAG2;
    ehdr.e_ident[3] = ELFMAG3;
    ehdr.e_ident[4] = ELFCLASS32;
    ehdr.e_ident[5] = ELFDATA2LSB;
    ehdr.e_ident[6] = EV_CURRENT;
    ehdr.e_ident[7] = ELFOSABI_LINUX;

    ehdr.e_type = ET_REL;
    ehdr.e_machine = EM_386;
    ehdr.e_version = EV_CURRENT;
    ehdr.e_entry = 0;
    ehdr.e_phoff = 0;
    ehdr.e_shoff = sizeof(Elf32_Ehdr);
    ehdr.e_flags = 0;
    ehdr.e_ehsize = sizeof(Elf32_Ehdr);
    ehdr.e_phentsize = 0;
    ehdr.e_phnum = 0;
    ehdr.e_shentsize = sizeof(Elf32_Shdr);
    ehdr.e_shnum = SECTION_COUNT;
    ehdr.e_shstrndx = 1;

    // Sections: empty, sectionnames, strings, symbols, code and data relocations, code, data
    Elf32_Shdr shdr[SECTION_COUNT];
    const char *section_names = "\0sections\0strings\0symbols\0coderels\0datarels\0code\0data\0";
    //                           0 1         10       18       26        35        44    49
    int section_names_length = 53;
    int offset = sizeof(Elf32_Ehdr) + SECTION_COUNT * sizeof(Elf32_Shdr);

    // null
    shdr[0].sh_name = 0;
    shdr[0].sh_type = SHT_NULL;
    shdr[0].sh_flags = 0;
    shdr[0].sh_addr = 0;
    shdr[0].sh_offset = 0;
    shdr[0].sh_size = 0;
    shdr[0].sh_link = 0;
    shdr[0].sh_info = 0;
    shdr[0].sh_addralign = 0;
    shdr[0].sh_entsize = 0;
    offset += shdr[0].sh_size;
    
    // section names
    shdr[1].sh_name = 1;
    shdr[1].sh_type = SHT_STRTAB;
    shdr[1].sh_flags = 0;
    shdr[1].sh_addr = 0;
    shdr[1].sh_offset = offset;
    shdr[1].sh_size = section_names_length;
    shdr[1].sh_link = 0;
    shdr[1].sh_info = 0;
    shdr[1].sh_addralign = 0;
    shdr[1].sh_entsize = 0;
    offset += shdr[1].sh_size;
    
    // strings
    shdr[2].sh_name = 10;
    shdr[2].sh_type = SHT_STRTAB;
    shdr[2].sh_flags = 0;
    shdr[2].sh_addr = 0;
    shdr[2].sh_offset = offset;
    shdr[2].sh_size = strings.size();
    shdr[2].sh_link = 0;
    shdr[2].sh_info = 0;
    shdr[2].sh_addralign = 0;
    shdr[2].sh_entsize = 0;
    offset += shdr[2].sh_size;
    
    // symbols
    shdr[3].sh_name = 18;
    shdr[3].sh_type = SHT_SYMTAB;
    shdr[3].sh_flags = 0;
    shdr[3].sh_addr = 0;
    shdr[3].sh_offset = offset;
    shdr[3].sh_size = symbols.size() * sizeof(Elf32_Sym);
    shdr[3].sh_link = 2;    // Take strings from here
    shdr[3].sh_info = 0;
    shdr[3].sh_addralign = 0;
    shdr[3].sh_entsize = sizeof(Elf32_Sym);
    offset += shdr[3].sh_size;
    
    // code relocations
    shdr[4].sh_name = 26;
    shdr[4].sh_type = SHT_REL;
    shdr[4].sh_flags = 0;
    shdr[4].sh_addr = 0;
    shdr[4].sh_offset = offset;
    shdr[4].sh_size = code_relocations.size() * sizeof(Elf32_Rel);
    shdr[4].sh_link = 3;    // Take symbols from here
    shdr[4].sh_info = 6;    // Put relocations here
    shdr[4].sh_addralign = 0;
    shdr[4].sh_entsize = sizeof(Elf32_Rel);
    offset += shdr[4].sh_size;
    
    // data relocations
    shdr[5].sh_name = 35;
    shdr[5].sh_type = SHT_REL;
    shdr[5].sh_flags = 0;
    shdr[5].sh_addr = 0;
    shdr[5].sh_offset = offset;
    shdr[5].sh_size = data_relocations.size() * sizeof(Elf32_Rel);
    shdr[5].sh_link = 3;    // Take symbols from here
    shdr[5].sh_info = 7;    // Put relocations here
    shdr[5].sh_addralign = 0;
    shdr[5].sh_entsize = sizeof(Elf32_Rel);
    offset += shdr[5].sh_size;
    
    // code
    shdr[6].sh_name = 44;
    shdr[6].sh_type = SHT_PROGBITS;
    shdr[6].sh_flags = (SHF_ALLOC | SHF_EXECINSTR);
    shdr[6].sh_addr = 0;
    shdr[6].sh_offset = offset;
    shdr[6].sh_size = code.size();
    shdr[6].sh_link = 0;
    shdr[6].sh_info = 0;
    shdr[6].sh_addralign = 4;
    shdr[6].sh_entsize = 0;
    offset += shdr[6].sh_size;
    
    // data
    shdr[7].sh_name = 49;
    shdr[7].sh_type = SHT_PROGBITS;
    shdr[7].sh_flags = (SHF_ALLOC | SHF_WRITE);
    shdr[7].sh_addr = 0;
    shdr[7].sh_offset = offset;
    shdr[7].sh_size = data.size();
    shdr[7].sh_link = 0;
    shdr[7].sh_info = 0;
    shdr[7].sh_addralign = 4;
    shdr[7].sh_entsize = 0;
    offset += shdr[7].sh_size;
    
    FILE *out = fopen(filename.c_str(), "wb");
    
    if (code.size() > 2) {
        fwrite(&ehdr, sizeof(Elf32_Ehdr), 1, out);
        fwrite(&shdr, sizeof(Elf32_Shdr), SECTION_COUNT, out);
        fwrite(section_names, 1, section_names_length, out);
        fwrite(strings.data(), 1, strings.size(), out);
        fwrite(symbols.data(), sizeof(Elf32_Sym), symbols.size(), out);
        fwrite(code_relocations.data(), sizeof(Elf32_Rel), code_relocations.size(), out);
        fwrite(data_relocations.data(), sizeof(Elf32_Rel), data_relocations.size(), out);
        fwrite(code.data(), 1, code.size(), out);
        fwrite(data.data(), 1, data.size(), out);
    }
    
    fclose(out);
}


void Ork::set_code(std::vector<char> &c) {
    code = c;
}


void Ork::set_data(std::vector<char> &d) {
    data = d;
}


int Ork::add_string(std::string s) {
    int pos = strings.size();
    
    for (unsigned i = 0; i < s.size(); i++)
        strings.push_back(s[i]);
        
    strings.push_back('\0');
    return pos;
}


int Ork::export_common(std::string name, int align, int size, int glob) {
    symbols.push_back(Elf32_Sym());
    Elf32_Sym &s = symbols.back();
    
    s.st_name = add_string(name);
    s.st_value = align;
    s.st_size = size;
    s.st_info = ELF32_ST_INFO((glob ? STB_WEAK : STB_LOCAL), STT_NOTYPE);
    s.st_other = 0;
    s.st_shndx = SHN_COMMON;

    return symbols.size() - 1;
}


int Ork::export_absolute(std::string name, int value, int size, int glob) {
    symbols.push_back(Elf32_Sym());
    Elf32_Sym &s = symbols.back();

    s.st_name = add_string(name);
    s.st_value = value;
    s.st_size = size;
    s.st_info = ELF32_ST_INFO((glob ? STB_GLOBAL : STB_LOCAL), STT_NOTYPE);    // STB_WEAK
    s.st_other = 0;
    s.st_shndx = SHN_ABS;

    return symbols.size() - 1;
}


int Ork::export_data(std::string name, int location, int size, int glob) {
    symbols.push_back(Elf32_Sym());
    Elf32_Sym &s = symbols.back();

    s.st_name = add_string(name);
    s.st_value = location;
    s.st_size = size;
    s.st_info = ELF32_ST_INFO((glob ? STB_GLOBAL : STB_LOCAL), STT_OBJECT);    // STB_WEAK
    s.st_other = 0;
    s.st_shndx = 7;    // Refer to data section

    return symbols.size() - 1;
}


int Ork::export_code(std::string name, int location, int size, int glob) {
    symbols.push_back(Elf32_Sym());
    Elf32_Sym &s = symbols.back();

    s.st_name = add_string(name);
    s.st_value = location;
    s.st_size = size;
    s.st_info = ELF32_ST_INFO((glob ? STB_GLOBAL : STB_LOCAL), STT_FUNC);    // STB_WEAK
    s.st_other = 0;
    s.st_shndx = 6;    // Refer to code section

    return symbols.size() - 1;
}


int Ork::import(std::string name) {
    symbols.push_back(Elf32_Sym());
    Elf32_Sym &s = symbols.back();

    s.st_name = add_string(name);
    s.st_value = 0;
    s.st_size = 0;
    s.st_info = ELF32_ST_INFO(STB_GLOBAL, STT_FUNC);
    s.st_other = 0;
    s.st_shndx = 0;
    
    return symbols.size() - 1;
}


void Ork::code_relocation(int index, int location, int relative) {
    code_relocations.push_back(Elf32_Rel());
    Elf32_Rel &r = code_relocations.back();

    r.r_offset = location;    // Only 32-bit near
    r.r_info = ELF32_R_INFO(index, (relative ? R_386_PC32 : R_386_32));
}


void Ork::data_relocation(int index, int location) {
    data_relocations.push_back(Elf32_Rel());
    Elf32_Rel &r = data_relocations.back();

    r.r_offset = location;    // Only 32-bit near
    r.r_info = ELF32_R_INFO(index, R_386_32);
}
