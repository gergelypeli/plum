
struct LineInfo {
    int address;
    int file_index;
    int line_number;
};


class Ork {
public:
    std::vector<char> code;
    std::vector<char> data;
    std::vector<char> lineno;
    std::vector<char> abbrev;
    std::vector<char> info;
    std::vector<char> strings;
    
    std::vector<Elf64_Sym> symbols;
    std::vector<Elf64_Rela> code_relocations;
    std::vector<Elf64_Rela> data_relocations;
    std::vector<Elf64_Rela> line_relocations;
    std::vector<Elf64_Rela> info_relocations;
    
    Ork();
    ~Ork();

    void uleb128(unsigned x);
    void sleb128(int x);
    
    unsigned add_string(std::string s);
    unsigned add_symbol(std::string name, Elf64_Addr value, unsigned size, bool is_global, int type, int section);
    void add_relocation(unsigned index, Elf64_Addr location, int addend, int type, std::vector<Elf64_Rela> &relas);

    unsigned export_absolute(std::string name, Elf64_Addr value, unsigned size, bool is_global);
    unsigned export_data(std::string name, Elf64_Addr location, unsigned size, bool is_global);
    unsigned export_code(std::string name, Elf64_Addr location, unsigned size, bool is_global);
    unsigned import(std::string name);
    
    void code_relocation(unsigned index, Elf64_Addr location, int addend);
    void data_relocation(unsigned index, Elf64_Addr location, int addend);

    void set_code(std::vector<char> &c);
    void set_data(std::vector<char> &d);
    void set_lineno(std::vector<std::string> &source_names, std::vector<LineInfo> &lis);
    void done(std::string filename);
};


Ork::Ork() {
    strings.push_back('\0');

    symbols.push_back(Elf64_Sym());
    Elf64_Sym &s = symbols.back();
    
    s.st_name = 0;
    s.st_value = 0;
    s.st_size = 0;
    s.st_info = 0;
    s.st_other = 0;
    s.st_shndx = 0;
}


Ork::~Ork() {
}


void Ork::uleb128(unsigned x) {
    while (true) {
        if ((x & ~0x7f) == 0) {
            lineno.push_back(x & 0x7f);
            break;
        }
        
        lineno.push_back((x & 0x7f) | 0x80);
        x = (x & ~0x7f) / 128;  // arithmetic shift right
    }
}


void Ork::sleb128(int x) {
    while (true) {
        if ((x & ~0x7f) == (x & 0x40 ? ~0x7f : 0)) {
            lineno.push_back(x & 0x7f);
            break;
        }
        
        lineno.push_back((x & 0x7f) | 0x80);
        x = (x & ~0x7f) / 128;  // arithmetic shift right
    }
}


unsigned Ork::add_string(std::string s) {
    unsigned pos = strings.size();
    
    for (unsigned i = 0; i < s.size(); i++)
        strings.push_back(s[i]);
        
    strings.push_back('\0');
    return pos;
}


unsigned Ork::add_symbol(std::string name, Elf64_Addr value, unsigned size, bool is_global, int type, int section) {
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


unsigned Ork::export_absolute(std::string name, Elf64_Addr value, unsigned size, bool is_global) {
    return add_symbol(name, value, size, is_global, STT_NOTYPE, SHN_ABS);
}


unsigned Ork::export_data(std::string name, Elf64_Addr location, unsigned size, bool is_global) {
    return add_symbol(name, location, size, is_global, STT_OBJECT, 7);  // data section
}


unsigned Ork::export_code(std::string name, Elf64_Addr location, unsigned size, bool is_global) {
    return add_symbol(name, location, size, is_global, STT_FUNC, 6);  // code section
}


unsigned Ork::import(std::string name) {
    return add_symbol(name, 0, 0, true, STT_FUNC, 0);
}


void Ork::add_relocation(unsigned index, Elf64_Addr location, int addend, int type, std::vector<Elf64_Rela> &relas) {
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


void Ork::code_relocation(unsigned index, Elf64_Addr location, int addend) {
    add_relocation(index, location, addend, R_X86_64_PC32, code_relocations);
}


void Ork::data_relocation(unsigned index, Elf64_Addr location, int addend) {
    add_relocation(index, location, addend, R_X86_64_64, data_relocations);
}


void Ork::set_code(std::vector<char> &c) {
    code = c;
}


void Ork::set_data(std::vector<char> &d) {
    data = d;
}


struct DebugLineHeader {
    uint32_t length;
    uint16_t version;
    uint32_t header_length;
    uint8_t min_instruction_length;
    uint8_t maximum_operations_per_instruction;
    uint8_t default_is_stmt;
    int8_t line_base;
    uint8_t line_range;
    uint8_t opcode_base;
    uint8_t std_opcode_lengths[12];
} __attribute__((packed));


struct CompilationUnitHeader {
    uint32_t length;
    uint16_t version;
    uint32_t abbrev_offset;
    uint8_t address_size;
} __attribute__((packed));


void Ork::set_lineno(std::vector<std::string> &source_names, std::vector<LineInfo> &lis) {
    // Fill .debug_line
    
    lineno.resize(sizeof(DebugLineHeader));
    DebugLineHeader *dlh = (DebugLineHeader *)&lineno[0];
    
    dlh->length = 0;  // To be filled later
    dlh->version = 4;  // Not the DWARF version, but the line numbering version
    dlh->header_length = 0;  // To be filled later
    dlh->min_instruction_length = 1;
    dlh->maximum_operations_per_instruction = 1;
    dlh->default_is_stmt = 1;
    dlh->line_base = 0;
    dlh->line_range = 1;
    dlh->opcode_base = 13;
    dlh->std_opcode_lengths[0] = 0;
    dlh->std_opcode_lengths[1] = 1;
    dlh->std_opcode_lengths[2] = 1;
    dlh->std_opcode_lengths[3] = 1;
    dlh->std_opcode_lengths[4] = 1;
    dlh->std_opcode_lengths[5] = 0;
    dlh->std_opcode_lengths[6] = 0;
    dlh->std_opcode_lengths[7] = 0;
    dlh->std_opcode_lengths[8] = 1;
    dlh->std_opcode_lengths[9] = 0;
    dlh->std_opcode_lengths[10] = 0;
    dlh->std_opcode_lengths[11] = 1;
    
    lineno.push_back(0);  // no include directories
    
    for (auto &sn : source_names) {
        for (auto &c : sn)
            lineno.push_back(c);
            
        lineno.push_back(0);  // terminate file name
        lineno.push_back(0);  // current dir
        lineno.push_back(0);  // no mtime
        lineno.push_back(0);  // no size
    }
    
    lineno.push_back(0);  // end of file names

    int header_length = lineno.size() - 10;  // size after header_length
    
    // Line numbers program opcodes
    
    // Start with extended opcode 2 to set the relocatable 64-bits address
    lineno.push_back(0);
    lineno.push_back(9);
    lineno.push_back(2);
    
    Elf64_Addr line_to_code_rel_location = lineno.size();
    
    for (int i=0; i<8; i++)
        lineno.push_back(0);  // needs 64-bits relocation to .text

    // Initialize state machine
    int sm_address = 0;
    int sm_file_index = 0;
    int sm_line_number = 1;
        
    for (auto &li : lis) {
        lineno.push_back(2);  // advance pc op
        uleb128(li.address - sm_address);
        sm_address = li.address;

        lineno.push_back(3);  // advance line op
        sleb128(li.line_number - sm_line_number);
        sm_line_number = li.line_number;
        
        if (li.file_index != sm_file_index) {
            lineno.push_back(4);  // set file op
            uleb128(li.file_index + 1);  // DWARF file indexing from 1
            sm_file_index = li.file_index;
        }
        
        lineno.push_back(1);  // copy op
    }

    int length = lineno.size() - 4;  // size after length
    
    // lineno vector may have been reallocated
    dlh = (DebugLineHeader *)&lineno[0];
    dlh->length = length;
    dlh->header_length = header_length;
    
    // Fill .debug_abbrev
    // These 3 attributes are necessary to make gdb work
    abbrev.push_back(0x01);  // type 1
    abbrev.push_back(0x11);  // compile unit
    abbrev.push_back(0x01);  // has children (well, it doesn't really)

    abbrev.push_back(0x10);  // stmt list
    abbrev.push_back(0x17);  // sec offset

    abbrev.push_back(0x11);  // low pc
    abbrev.push_back(0x01);  // address

    abbrev.push_back(0x12);  // high pc
    abbrev.push_back(0x07);  // data8
    
    abbrev.push_back(0x00);  // end of attributes
    abbrev.push_back(0x00);  // end of attributes

    abbrev.push_back(0x00);  // end of abbreviations
    
    // Fill .debug_info
    info.resize(sizeof(CompilationUnitHeader));
    CompilationUnitHeader *cuh = (CompilationUnitHeader *)&info[0];
    
    cuh->length = 0;  // to be filled
    cuh->version = 4;
    cuh->abbrev_offset = 0;  // needs 32-bits relocation to .debug_abbrev
    cuh->address_size = 8;

    Elf64_Addr info_to_abbr_rel_location = 6;
    
    info.push_back(0x01);  // abbrev 1

    // stmt list
    Elf64_Addr info_to_line_rel_location = info.size();
    
    for (int i=0; i<4; i++)
        info.push_back(0x00);  // needs 32-bits relocation to .debug_line
        
    // low pc
    Elf64_Addr info_to_code_rel_location = info.size();

    for (int i=0; i<8; i++)
        info.push_back(0x00);  // needs 64-bits relocation to .text
    
    // high pc
    for (int i=0; i<8; i++)
        info.push_back(0x00);  // constant length filled below

    *(unsigned64 *)&info[info.size() - 8] = code.size();
    
    // info vector may have been reallocated
    cuh = (CompilationUnitHeader *)&info[0];
    cuh->length = info.size() - 4;
    
    // Add symbols necessary for our relocations
    unsigned code_start_sym = add_symbol(".line_to_code", 0, 0, false, STT_SECTION, 6);
    unsigned line_start_sym = add_symbol(".info_to_line", 0, 0, false, STT_SECTION, 8);
    unsigned abbr_start_sym = add_symbol(".info_to_abbr", 0, 0, false, STT_SECTION, 9);
    
    // Add relocations to .rela.debug_line and .rela.debug_info
    add_relocation(code_start_sym, line_to_code_rel_location, 0, R_X86_64_64, line_relocations);
    add_relocation(line_start_sym, info_to_line_rel_location, 0, R_X86_64_32, info_relocations);
    add_relocation(abbr_start_sym, info_to_abbr_rel_location, 0, R_X86_64_32, info_relocations);
    add_relocation(code_start_sym, info_to_code_rel_location, 0, R_X86_64_64, info_relocations);
}


void Ork::done(std::string filename) {
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

    // Spaces will be replaced with null bytes before writing.
    // It begins with one and ends with two spaces.
    //                      01         11      19      27         38         49    55    61          73            87          99               116
    char section_names[] = " .shstrtab .strtab .symtab .rela.text .rela.data .text .data .debug_line .debug_abbrev .debug_info .rela.debug_line .rela.debug_info  ";
    for (unsigned i = 0; i < sizeof(section_names); i++)
        if (section_names[i] == ' ')
            section_names[i] = '\0';
    
    Elf64_Shdr shdr[SECTION_COUNT];
    int offset = sizeof(Elf64_Ehdr) + SECTION_COUNT * sizeof(Elf64_Shdr);

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
    
    // .shstrtab
    shdr[1].sh_name = 1;
    shdr[1].sh_type = SHT_STRTAB;
    shdr[1].sh_flags = 0;
    shdr[1].sh_addr = 0;
    shdr[1].sh_offset = offset;
    shdr[1].sh_size = sizeof(section_names);
    shdr[1].sh_link = 0;
    shdr[1].sh_info = 0;
    shdr[1].sh_addralign = 0;
    shdr[1].sh_entsize = 0;
    offset += shdr[1].sh_size;
    
    // .strtab
    shdr[2].sh_name = 11;
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
    shdr[3].sh_name = 19;
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
    shdr[4].sh_name = 27;
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
    shdr[5].sh_name = 38;
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
    shdr[6].sh_name = 49;
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
    shdr[7].sh_name = 55;
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
    shdr[8].sh_name = 61;
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
    shdr[9].sh_name = 73;
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
    shdr[10].sh_name = 87;
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
    shdr[11].sh_name = 99;
    shdr[11].sh_type = SHT_RELA;
    shdr[11].sh_flags = 0;
    shdr[11].sh_addr = 0;
    shdr[11].sh_offset = offset;
    shdr[11].sh_size = 1 * sizeof(Elf64_Rela);  // just one
    shdr[11].sh_link = 3;    // Take symbols from here
    shdr[11].sh_info = 8;    // Put relocations here
    shdr[11].sh_addralign = 0;
    shdr[11].sh_entsize = sizeof(Elf64_Rela);
    offset += shdr[11].sh_size;
    
    // .rela.debug_info
    shdr[12].sh_name = 116;
    shdr[12].sh_type = SHT_RELA;
    shdr[12].sh_flags = 0;
    shdr[12].sh_addr = 0;
    shdr[12].sh_offset = offset;
    shdr[12].sh_size = 3 * sizeof(Elf64_Rela);  // just three
    shdr[12].sh_link = 3;    // Take symbols from here
    shdr[12].sh_info = 10;   // Put relocations here
    shdr[12].sh_addralign = 0;
    shdr[12].sh_entsize = sizeof(Elf64_Rela);
    offset += shdr[12].sh_size;
    
    FILE *out = fopen(filename.c_str(), "wb");
    
    if (code.size() > 2) {
        fwrite(&ehdr, sizeof(Elf64_Ehdr), 1, out);
        fwrite(&shdr, sizeof(Elf64_Shdr), SECTION_COUNT, out);
        fwrite(section_names, 1, sizeof(section_names), out);
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
