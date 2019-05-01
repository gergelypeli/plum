
struct LineInfo {
    int address;
    int file_index;
    int line_number;
};


class DwarfBuffer: public std::vector<char> {
public:
    void uleb128(unsigned x) {
        while (true) {
            if ((x & ~0x7f) == 0) {
                push_back(x & 0x7f);
                break;
            }
        
            push_back((x & 0x7f) | 0x80);
            x = (x & ~0x7f) / 128;  // arithmetic shift right
        }
    }


    void sleb128(int x) {
        while (true) {
            if ((x & ~0x7f) == (x & 0x40 ? ~0x7f : 0)) {
                push_back(x & 0x7f);
                break;
            }
        
            push_back((x & 0x7f) | 0x80);
            x = (x & ~0x7f) / 128;  // arithmetic shift right
        }
    }

    template <typename T>
    int append() {
        unsigned offset = size();
        resize(size() + sizeof(T), 0);
        return offset;
    }
    
    template <typename T>
    T *pointer(int offset) {
        return (T *)&((*this)[offset]);
    }
};


class Ork {
public:
    std::vector<char> code;
    std::vector<char> data;
    std::vector<char> strings;
    DwarfBuffer lineno;
    DwarfBuffer abbrev;
    DwarfBuffer info;
    
    std::vector<Elf64_Sym> symbols;
    std::vector<Elf64_Rela> code_relocations;
    std::vector<Elf64_Rela> data_relocations;
    std::vector<Elf64_Rela> line_relocations;
    std::vector<Elf64_Rela> info_relocations;

    unsigned code_start_sym, line_start_sym, abbr_start_sym;
    
    Ork();
    ~Ork();
    
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
    
    void fill_abbrev();
    void fill_info();
    
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
    
    code_start_sym = add_symbol(".code_start", 0, 0, false, STT_SECTION, 6);
    line_start_sym = add_symbol(".line_start", 0, 0, false, STT_SECTION, 8);
    abbr_start_sym = add_symbol(".abbr_start", 0, 0, false, STT_SECTION, 9);
}


Ork::~Ork() {
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


void Ork::fill_abbrev() {
    // Fill .debug_abbrev
    
    // These 3 attributes are necessary to make gdb work
    abbrev.uleb128(0x01);  // abbrev number 1
    abbrev.uleb128(0x11);  // compilation unit
    abbrev.push_back(0x01);  // has children

    abbrev.uleb128(0x10);  // stmt list
    abbrev.uleb128(0x17);  // sec offset

    abbrev.uleb128(0x11);  // low pc
    abbrev.uleb128(0x01);  // address

    abbrev.uleb128(0x12);  // high pc
    abbrev.uleb128(0x07);  // data8
    
    abbrev.uleb128(0x00);  // end of attributes
    abbrev.uleb128(0x00);  // end of attributes

    abbrev.uleb128(0x00);  // end of abbreviations
}


void Ork::fill_info() {
    // Fill .debug_info
    
    int cuh_offset = info.append<CompilationUnitHeader>();
    auto cuh = info.pointer<CompilationUnitHeader>(cuh_offset);
    
    cuh->length = 0;  // to be filled
    cuh->version = 4;
    cuh->abbrev_offset = 0;  // needs 32-bits relocation to .debug_abbrev
    cuh->address_size = 8;

    Elf64_Addr abbr_relocation = cuh_offset + offsetof(CompilationUnitHeader, abbrev_offset);
    add_relocation(abbr_start_sym, abbr_relocation, 0, R_X86_64_32, info_relocations);
    
    // DIE
    info.uleb128(0x01);  // abbrev 1 (compilation unit)

    // stmt list
    Elf64_Addr line_relocation = info.append<unsigned32>();  // needs 32-bits relocation to .debug_line
    add_relocation(line_start_sym, line_relocation, 0, R_X86_64_32, info_relocations);
        
    // low pc
    Elf64_Addr code_relocation = info.append<unsigned64>();  // needs 64-bits relocation to .text
    add_relocation(code_start_sym, code_relocation, 0, R_X86_64_64, info_relocations);

    // high pc
    auto hpc_offset = info.append<unsigned64>();
    *info.pointer<unsigned64>(hpc_offset) = code.size();
    
    // children
    info.uleb128(0x00);  // end of children
    
    // info vector may have been reallocated
    int info_start = cuh_offset + offsetof(CompilationUnitHeader, version);
    int info_end = info.size();
    cuh = info.pointer<CompilationUnitHeader>(cuh_offset);
    cuh->length = info_end - info_start;
}


void Ork::set_lineno(std::vector<std::string> &source_names, std::vector<LineInfo> &lis) {
    // Fill .debug_line
    
    int dlh_offset = lineno.append<DebugLineHeader>();
    auto dlh = lineno.pointer<DebugLineHeader>(dlh_offset);
    
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
    
    // Include directories
    
    lineno.push_back(0);  // end of include directories
    
    // Source file names
    
    for (auto &sn : source_names) {
        for (auto &c : sn)
            lineno.push_back(c);
            
        lineno.push_back(0);  // terminate file name
        lineno.push_back(0);  // current dir
        lineno.push_back(0);  // no mtime
        lineno.push_back(0);  // no size
    }
    
    lineno.push_back(0);  // end of file names
    
    // size after header_length
    int header_start = dlh_offset + offsetof(DebugLineHeader, min_instruction_length);
    int header_end = lineno.size();
    dlh = lineno.pointer<DebugLineHeader>(dlh_offset);
    dlh->header_length = header_end - header_start;
    
    // Line numbers program opcodes
    
    // Start with extended opcode 2 to set the relocatable 64-bits address
    lineno.push_back(0);
    lineno.push_back(9);
    lineno.push_back(2);
    Elf64_Addr code_relocation = lineno.append<unsigned64>();
    add_relocation(code_start_sym, code_relocation, 0, R_X86_64_64, line_relocations);
    
    // Initialize state machine
    int sm_address = 0;
    int sm_file_index = 0;
    int sm_line_number = 1;
        
    for (auto &li : lis) {
        lineno.push_back(2);  // advance pc op
        lineno.uleb128(li.address - sm_address);
        sm_address = li.address;

        lineno.push_back(3);  // advance line op
        lineno.sleb128(li.line_number - sm_line_number);
        sm_line_number = li.line_number;
        
        if (li.file_index != sm_file_index) {
            lineno.push_back(4);  // set file op
            lineno.uleb128(li.file_index + 1);  // DWARF file indexing from 1
            sm_file_index = li.file_index;
        }
        
        lineno.push_back(1);  // copy op
    }

    // size after length
    int debug_line_start = dlh_offset + offsetof(DebugLineHeader, version);
    int debug_line_end = lineno.size();
    dlh = lineno.pointer<DebugLineHeader>(dlh_offset);
    dlh->length = debug_line_end - debug_line_start;
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
    shdr[11].sh_size = 1 * sizeof(Elf64_Rela);  // just one
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
