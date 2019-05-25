#include "../plum.h"


Elf::Elf(std::string module_name) {
    strings.push_back('\0');

    symbols.push_back(Elf64_Sym());
    Elf64_Sym &s = symbols.back();
    
    s.st_name = 0;
    s.st_value = 0;
    s.st_size = 0;
    s.st_info = 0;
    s.st_other = 0;
    s.st_shndx = 0;
    
    code_start_sym = add_symbol(module_name + ".code_start", 0, 0, false, STT_SECTION, 6);
    data_start_sym = add_symbol(module_name + ".data_start", 0, 0, false, STT_SECTION, 7);
    line_start_sym = add_symbol(module_name + ".line_start", 0, 0, false, STT_SECTION, 8);
    abbr_start_sym = add_symbol(module_name + ".abbr_start", 0, 0, false, STT_SECTION, 9);
}


Elf::~Elf() {
}


unsigned Elf::add_string(std::string s) {
    unsigned pos = strings.size();
    
    for (unsigned i = 0; i < s.size(); i++)
        strings.push_back(s[i]);
        
    strings.push_back('\0');
    return pos;
}


unsigned Elf::add_symbol(std::string name, Elf64_Addr value, unsigned size, bool is_global, int type, int section) {
    symbols.push_back(Elf64_Sym());
    Elf64_Sym &s = symbols.back();

    s.st_name = add_string(name);
    s.st_value = value;
    s.st_size = size;
    s.st_info = ELF64_ST_INFO((is_global ? STB_GLOBAL : STB_WEAK), type);
    s.st_other = 0;
    s.st_shndx = section;

    return symbols.size() - 1;
}


unsigned Elf::export_absolute(std::string name, Elf64_Addr value, unsigned size, bool is_global) {
    return add_symbol(name, value, size, is_global, STT_NOTYPE, SHN_ABS);
}


unsigned Elf::export_data(std::string name, Elf64_Addr location, unsigned size, bool is_global) {
    return add_symbol(name, location, size, is_global, STT_OBJECT, 7);  // data section
}


unsigned Elf::export_code(std::string name, Elf64_Addr location, unsigned size, bool is_global) {
    return add_symbol(name, location, size, is_global, STT_FUNC, 6);  // code section
}


unsigned Elf::import(std::string name) {
    return add_symbol(name, 0, 0, true, STT_FUNC, 0);
}


void Elf::add_relocation(unsigned index, Elf64_Addr location, int addend, int type, std::vector<Elf64_Rela> &relas) {
    if (!index) {
        std::cerr << "Invalid symbol index for relocation!\n";
        throw ASM_ERROR;
    }
        
    relas.push_back(Elf64_Rela());
    Elf64_Rela &r = relas.back();

    r.r_offset = location;
    r.r_info = ELF64_R_INFO(index, type);
    r.r_addend = addend;
}


void Elf::code_relocation(unsigned index, Elf64_Addr location, int addend) {
    add_relocation(index, location, addend, R_X86_64_PC32, code_relocations);
}


void Elf::data_relocation(unsigned index, Elf64_Addr location, int addend) {
    add_relocation(index, location, addend, R_X86_64_64, data_relocations);
}


void Elf::info_relocation32(unsigned index, Elf64_Addr location, int addend) {
    add_relocation(index, location, addend, R_X86_64_32, info_relocations);
}


void Elf::info_relocation64(unsigned index, Elf64_Addr location, int addend) {
    add_relocation(index, location, addend, R_X86_64_64, info_relocations);
}


void Elf::line_relocation64(unsigned index, Elf64_Addr location, int addend) {
    add_relocation(index, location, addend, R_X86_64_64, line_relocations);
}


void Elf::set_code(std::vector<char> &c) {
    code = c;
}


void Elf::set_data(std::vector<char> &d) {
    data = d;
}


void Elf::set_lineno(std::vector<char> &l) {
    lineno = l;
}


void Elf::set_abbrev(std::vector<char> &a) {
    abbrev = a;
}


void Elf::set_info(std::vector<char> &i) {
    info = i;
}


void Elf::done(std::string filename) {
    const int SECTION_COUNT = 13;
    
    Elf64_Ehdr ehdr;
    ehdr.e_ident[0] = ELFMAG0;
    ehdr.e_ident[1] = ELFMAG1;
    ehdr.e_ident[2] = ELFMAG2;
    ehdr.e_ident[3] = ELFMAG3;
    ehdr.e_ident[4] = ELFCLASS64;
    ehdr.e_ident[5] = ELFDATA2LSB;
    ehdr.e_ident[6] = EV_CURRENT;
    ehdr.e_ident[7] = ELFOSABI_LINUX;

    ehdr.e_type = ET_REL;
    ehdr.e_machine = EM_X86_64;
    ehdr.e_version = EV_CURRENT;
    ehdr.e_entry = 0;
    ehdr.e_phoff = 0;
    ehdr.e_shoff = sizeof(Elf64_Ehdr);
    ehdr.e_flags = 0;
    ehdr.e_ehsize = sizeof(Elf64_Ehdr);
    ehdr.e_phentsize = 0;
    ehdr.e_phnum = 0;
    ehdr.e_shentsize = sizeof(Elf64_Shdr);
    ehdr.e_shnum = SECTION_COUNT;
    ehdr.e_shstrndx = 1;

    const char *SECTION_NAMES[] = {
        "", ".shstrtab", ".strtab", ".symtab", ".rela.text", ".rela.data", ".text", ".data", ".debug_line", ".debug_abbrev", ".debug_info", ".rela.debug_line", ".rela.debug_info"
    };
    
    std::vector<char> section_names;
    std::vector<int> section_name_offsets;
    
    for (int i = 0; i < SECTION_COUNT; i++) {
        section_name_offsets.push_back(section_names.size());
        
        int len = strlen(SECTION_NAMES[i]);
        
        for (int j = 0; j < len; j++)
            section_names.push_back(SECTION_NAMES[i][j]);
            
        section_names.push_back(0);
    }

    section_names.push_back(0);
    
    Elf64_Shdr shdr[SECTION_COUNT];
    int offset = sizeof(Elf64_Ehdr) + SECTION_COUNT * sizeof(Elf64_Shdr);

    // null
    shdr[0].sh_name = section_name_offsets[0];
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
    
    // .shstrtab
    shdr[1].sh_name = section_name_offsets[1];
    shdr[1].sh_type = SHT_STRTAB;
    shdr[1].sh_flags = 0;
    shdr[1].sh_addr = 0;
    shdr[1].sh_offset = offset;
    shdr[1].sh_size = section_names.size();
    shdr[1].sh_link = 0;
    shdr[1].sh_info = 0;
    shdr[1].sh_addralign = 0;
    shdr[1].sh_entsize = 0;
    offset += shdr[1].sh_size;
    
    // .strtab
    shdr[2].sh_name = section_name_offsets[2];
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
    
    // .symtab
    shdr[3].sh_name = section_name_offsets[3];
    shdr[3].sh_type = SHT_SYMTAB;
    shdr[3].sh_flags = 0;
    shdr[3].sh_addr = 0;
    shdr[3].sh_offset = offset;
    shdr[3].sh_size = symbols.size() * sizeof(Elf64_Sym);
    shdr[3].sh_link = 2;    // Take strings from here
    shdr[3].sh_info = 0;
    shdr[3].sh_addralign = 0;
    shdr[3].sh_entsize = sizeof(Elf64_Sym);
    offset += shdr[3].sh_size;
    
    // .rela.text
    shdr[4].sh_name = section_name_offsets[4];
    shdr[4].sh_type = SHT_RELA;
    shdr[4].sh_flags = 0;
    shdr[4].sh_addr = 0;
    shdr[4].sh_offset = offset;
    shdr[4].sh_size = code_relocations.size() * sizeof(Elf64_Rela);
    shdr[4].sh_link = 3;    // Take symbols from here
    shdr[4].sh_info = 6;    // Put relocations here
    shdr[4].sh_addralign = 0;
    shdr[4].sh_entsize = sizeof(Elf64_Rela);
    offset += shdr[4].sh_size;
    
    // .rela.data
    shdr[5].sh_name = section_name_offsets[5];
    shdr[5].sh_type = SHT_RELA;
    shdr[5].sh_flags = 0;
    shdr[5].sh_addr = 0;
    shdr[5].sh_offset = offset;
    shdr[5].sh_size = data_relocations.size() * sizeof(Elf64_Rela);
    shdr[5].sh_link = 3;    // Take symbols from here
    shdr[5].sh_info = 7;    // Put relocations here
    shdr[5].sh_addralign = 0;
    shdr[5].sh_entsize = sizeof(Elf64_Rela);
    offset += shdr[5].sh_size;
    
    // .text
    shdr[6].sh_name = section_name_offsets[6];
    shdr[6].sh_type = SHT_PROGBITS;
    shdr[6].sh_flags = (SHF_ALLOC | SHF_EXECINSTR);
    shdr[6].sh_addr = 0;
    shdr[6].sh_offset = offset;
    shdr[6].sh_size = code.size();
    shdr[6].sh_link = 0;
    shdr[6].sh_info = 0;
    shdr[6].sh_addralign = 8;  // align code
    shdr[6].sh_entsize = 0;
    offset += shdr[6].sh_size;
    
    // .data
    shdr[7].sh_name = section_name_offsets[7];
    shdr[7].sh_type = SHT_PROGBITS;
    shdr[7].sh_flags = (SHF_ALLOC | SHF_WRITE);
    shdr[7].sh_addr = 0;
    shdr[7].sh_offset = offset;
    shdr[7].sh_size = data.size();
    shdr[7].sh_link = 0;
    shdr[7].sh_info = 0;
    shdr[7].sh_addralign = 8;  // align data
    shdr[7].sh_entsize = 0;
    offset += shdr[7].sh_size;

    // .debug_line
    shdr[8].sh_name = section_name_offsets[8];
    shdr[8].sh_type = SHT_PROGBITS;
    shdr[8].sh_flags = 0;
    shdr[8].sh_addr = 0;
    shdr[8].sh_offset = offset;
    shdr[8].sh_size = lineno.size();
    shdr[8].sh_link = 0;
    shdr[8].sh_info = 0;
    shdr[8].sh_addralign = 0;  // must not align debug
    shdr[8].sh_entsize = 0;
    offset += shdr[8].sh_size;

    // .debug_abbrev
    shdr[9].sh_name = section_name_offsets[9];
    shdr[9].sh_type = SHT_PROGBITS;
    shdr[9].sh_flags = 0;
    shdr[9].sh_addr = 0;
    shdr[9].sh_offset = offset;
    shdr[9].sh_size = abbrev.size();
    shdr[9].sh_link = 0;
    shdr[9].sh_info = 0;
    shdr[9].sh_addralign = 0;  // must not align debug
    shdr[9].sh_entsize = 0;
    offset += shdr[9].sh_size;

    // .debug_info
    shdr[10].sh_name = section_name_offsets[10];
    shdr[10].sh_type = SHT_PROGBITS;
    shdr[10].sh_flags = 0;
    shdr[10].sh_addr = 0;
    shdr[10].sh_offset = offset;
    shdr[10].sh_size = info.size();
    shdr[10].sh_link = 0;
    shdr[10].sh_info = 0;
    shdr[10].sh_addralign = 0;  // must not align debug
    shdr[10].sh_entsize = 0;
    offset += shdr[10].sh_size;
    
    // .rela.debug_line
    shdr[11].sh_name = section_name_offsets[11];
    shdr[11].sh_type = SHT_RELA;
    shdr[11].sh_flags = 0;
    shdr[11].sh_addr = 0;
    shdr[11].sh_offset = offset;
    shdr[11].sh_size = line_relocations.size() * sizeof(Elf64_Rela);
    shdr[11].sh_link = 3;    // Take symbols from here
    shdr[11].sh_info = 8;    // Put relocations here
    shdr[11].sh_addralign = 0;
    shdr[11].sh_entsize = sizeof(Elf64_Rela);
    offset += shdr[11].sh_size;
    
    // .rela.debug_info
    shdr[12].sh_name = section_name_offsets[12];
    shdr[12].sh_type = SHT_RELA;
    shdr[12].sh_flags = 0;
    shdr[12].sh_addr = 0;
    shdr[12].sh_offset = offset;
    shdr[12].sh_size = info_relocations.size() * sizeof(Elf64_Rela);
    shdr[12].sh_link = 3;    // Take symbols from here
    shdr[12].sh_info = 10;   // Put relocations here
    shdr[12].sh_addralign = 0;
    shdr[12].sh_entsize = sizeof(Elf64_Rela);
    offset += shdr[12].sh_size;
    
    FILE *out = fopen(filename.c_str(), "wb");
    
    if (code.size() > 2) {
        fwrite(&ehdr, sizeof(Elf64_Ehdr), 1, out);
        fwrite(&shdr, sizeof(Elf64_Shdr), SECTION_COUNT, out);
        fwrite(section_names.data(), 1, section_names.size(), out);
        fwrite(strings.data(), 1, strings.size(), out);
        fwrite(symbols.data(), sizeof(Elf64_Sym), symbols.size(), out);
        fwrite(code_relocations.data(), sizeof(Elf64_Rela), code_relocations.size(), out);
        fwrite(data_relocations.data(), sizeof(Elf64_Rela), data_relocations.size(), out);
        fwrite(code.data(), 1, code.size(), out);
        fwrite(data.data(), 1, data.size(), out);
        fwrite(lineno.data(), 1, lineno.size(), out);
        fwrite(abbrev.data(), 1, abbrev.size(), out);
        fwrite(info.data(), 1, info.size(), out);
        fwrite(line_relocations.data(), sizeof(Elf64_Rela), line_relocations.size(), out);
        fwrite(info_relocations.data(), sizeof(Elf64_Rela), info_relocations.size(), out);
    }
    
    fclose(out);
}
