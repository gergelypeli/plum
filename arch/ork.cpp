#include <stdio.h>
#include <string.h>

#include "ork.h"

const char *elf_magic = "\177ELF\001\001\001";


#define ET_NONE         0  // No file type
#define ET_REL          1  // Relocatable file
#define ET_EXEC         2  // Executable file
#define ET_DYN          3  // Shared object file
#define ET_CORE         4  // Core file
#define ET_LOPROC  0xff00  // Processor-specific
#define ET_HIPROC  0xffff  // Processor-specific

#define EM_NONE       0  // No machine
#define EM_M32        1  // AT&T WE 32100
#define EM_SPARC      2  // SPARC
#define EM_386        3  // Intel 80386
#define EM_68K        4  // Motorola 68000
#define EM_88K        5  // Motorola 88000
#define EM_860        7  // Intel 80860
#define EM_MIPS       8  // MIPS RS3000

#define EV_NONE       0  // Invalid version
#define EV_CURRENT    1  // Current version


#define SHT_NULL        0
#define SHT_PROGBITS    1
#define SHT_SYMTAB      2
#define SHT_STRTAB	3
#define SHT_RELA	4
#define SHT_HASH	5
#define SHT_DYNAMIC     6
#define SHT_NOTE	7
#define SHT_NOBITS	8
#define SHT_REL	        9
#define SHT_SHLIB       10
#define SHT_DYNSYM      11
#define SHT_LOPROC      0x70000000
#define SHT_HIPROC      0x7fffffff
#define SHT_LOUSER      0x80000000
#define SHT_HIUSER      0xffffffff

#define SHF_WRITE             0x1
#define SHF_ALLOC             0x2
#define SHF_EXECINSTR         0x4
#define SHF_MASKPROC   0xf0000000

#define SHN_ABS 0xfff1
#define SHN_COMMON 0xfff2

#define ELF32_ST_BIND(i)	((i) >> 4)
#define ELF32_ST_TYPE(i)	((i) & 0xf)
#define ELF32_ST_INFO(b, t)	(((b) << 4) + ((t) & 0xf))

#define STB_LOCAL       0
#define STB_GLOBAL      1
#define STB_WEAK        2
#define STB_LOPROC     13
#define STB_HIPROC     15

#define STT_NOTYPE       0
#define STT_OBJECT       1
#define STT_FUNC         2
#define STT_SECTION      3
#define STT_FILE         4
#define STT_LOPROC      13
#define STT_HIPROC      15


#define ELF32_R_SYM(i)	((i) >> 8)
#define ELF32_R_TYPE(i)	((unsigned char)(i))
#define ELF32_R_INFO(s, t)	(((s) << 8) + (unsigned char)(t))

#define R_386_NONE        0  //  none    none
#define R_386_32	  1  //  word32  S + A
#define R_386_PC32	  2  //  word32  S + A - P
#define R_386_GOT32	  3  //  word32  G + A - P
#define R_386_PLT32	  4  //  word32  L + A - P
#define R_386_COPY	  5  //  none    none
#define R_386_GLOB_DAT    6  //  word32  S
#define R_386_JMP_SLOT    7  //  word32  S
#define R_386_RELATIVE    8  //  word32  B + A
#define R_386_GOTOFF	  9  //  word32  S + A - GOT
#define R_386_GOTPC	  10 //   word32  GOT + A - P




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
    strcpy((char *)ehdr.e_ident, elf_magic);
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
