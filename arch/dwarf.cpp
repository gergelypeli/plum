
class DwarfBuffer: public std::vector<char> {
public:
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

    void data1(int x) {
        push_back(x);
    }

    void data2(int value) {
        auto offset = append<unsigned16>();
        *pointer<unsigned16>(offset) = value;
    }
    
    void data4(int value) {
        auto offset = append<unsigned32>();
        *pointer<unsigned32>(offset) = value;
    }

    void data8(int value) {
        auto offset = append<unsigned64>();
        *pointer<unsigned64>(offset) = value;
    }

    void string(std::string s) {
        for (auto c : s)
            push_back(c);
            
        push_back(0);
    }
};


class Dwarf {
public:
    struct CompilationUnitHeader {
        uint32_t length;
        uint16_t version;
        uint32_t abbrev_offset;
        uint8_t address_size;
    } __attribute__((packed));

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
    

    struct DwarfAttrDef {
        int at;
        int form;
    };

    DwarfBuffer lineno;
    DwarfBuffer abbrev;
    DwarfBuffer info;

    Elf *elf;
    int cuh_offset, dlh_offset;
    unsigned abbrev_count;
    unsigned compile_unit_abbrev_number, base_type_abbrev_number;
    unsigned subprogram_abbrev_number, lexical_block_abbrev_number, try_block_abbrev_number, catch_block_abbrev_number;
    unsigned variable_abbrev_number, formal_parameter_abbrev_number;
    unsigned integer_die_offset;
    int lineno_sm_address, lineno_sm_file_index, lineno_sm_line_number;
    
    Dwarf(Elf *o, std::vector<std::string> &source_names);
    void finish();
    
    unsigned add_abbrev(int tag, bool has_children, std::vector<DwarfAttrDef> attrdefs);

    void init_lineno(std::vector<std::string> &source_names);
    void add_lineno(int pc, int file_index, int line_number);
    void finish_lineno();
    
    void init_abbrev();
    void finish_abbrev();
    
    void init_info();
    void info_code_address(int value);
    void begin_compile_unit_info(std::string name, std::string producer, int low_pc, int high_pc);
    void begin_subprogram_info(std::string name, int low_pc, int high_pc);
    void begin_lexical_block_info(int low_pc, int high_pc);
    void begin_try_block_info(int low_pc, int high_pc);
    void begin_catch_block_info(int low_pc, int high_pc);
    void local_variable_info(std::string name, int rbp_offset);
    void formal_parameter_info(std::string name, int rbp_offset);
    void end_info();
    void finish_info();
};


Dwarf::Dwarf(Elf *o, std::vector<std::string> &source_names) {
    elf = o;
    
    init_abbrev();
    init_info();
    init_lineno(source_names);
}


void Dwarf::finish() {
    finish_abbrev();
    finish_lineno();
    finish_info();

    elf->set_lineno(lineno);
    elf->set_abbrev(abbrev);
    elf->set_info(info);
}


unsigned Dwarf::add_abbrev(int tag, bool has_children, std::vector<DwarfAttrDef> attrdefs) {
    unsigned number = ++abbrev_count;
    
    abbrev.uleb128(number);
    abbrev.uleb128(tag);
    abbrev.uleb128(has_children ? DW_children_yes : DW_children_no);
    
    for (auto attrdef : attrdefs) {
        abbrev.uleb128(attrdef.at);
        abbrev.uleb128(attrdef.form);
    }

    abbrev.uleb128(0);
    abbrev.uleb128(0);
    
    return number;
}


void Dwarf::init_abbrev() {
    // Fill .debug_abbrev
    abbrev_count = 0;
    
    compile_unit_abbrev_number = add_abbrev(DW_TAG_compile_unit, true, {
        { DW_AT_stmt_list, DW_FORM_sec_offset },
        { DW_AT_low_pc, DW_FORM_addr },
        { DW_AT_high_pc, DW_FORM_data8 },
        { DW_AT_name, DW_FORM_string },
        { DW_AT_producer, DW_FORM_string }
    });

    base_type_abbrev_number = add_abbrev(DW_TAG_base_type, false, {
        { DW_AT_name, DW_FORM_string },
        { DW_AT_byte_size, DW_FORM_data1 },
        { DW_AT_encoding, DW_FORM_data1 }
    });

    subprogram_abbrev_number = add_abbrev(DW_TAG_subprogram, true, {
        { DW_AT_name, DW_FORM_string },
        { DW_AT_low_pc, DW_FORM_addr },
        { DW_AT_high_pc, DW_FORM_data8 },
        { DW_AT_frame_base, DW_FORM_exprloc }
    });

    lexical_block_abbrev_number = add_abbrev(DW_TAG_lexical_block, true, {
        { DW_AT_low_pc, DW_FORM_addr },
        { DW_AT_high_pc, DW_FORM_data8 }
    });

    try_block_abbrev_number = add_abbrev(DW_TAG_try_block, true, {
        { DW_AT_low_pc, DW_FORM_addr },
        { DW_AT_high_pc, DW_FORM_data8 }
    });
    
    catch_block_abbrev_number = add_abbrev(DW_TAG_catch_block, true, {
        { DW_AT_low_pc, DW_FORM_addr },
        { DW_AT_high_pc, DW_FORM_data8 }
    });

    variable_abbrev_number = add_abbrev(DW_TAG_variable, false, {
        { DW_AT_name, DW_FORM_string },
        { DW_AT_location, DW_FORM_exprloc },
        { DW_AT_type, DW_FORM_ref4 }
    });

    formal_parameter_abbrev_number = add_abbrev(DW_TAG_formal_parameter, false, {
        { DW_AT_name, DW_FORM_string },
        { DW_AT_location, DW_FORM_exprloc },
        { DW_AT_type, DW_FORM_ref4 }
    });
}


void Dwarf::finish_abbrev() {
    // end of abbrevs
    abbrev.uleb128(0);
}


void Dwarf::init_info() {
    // Fill .debug_info

    cuh_offset = info.append<CompilationUnitHeader>();
    auto cuh = info.pointer<CompilationUnitHeader>(cuh_offset);
    
    cuh->length = 0;  // to be filled
    cuh->version = 4;
    cuh->abbrev_offset = 0;  // needs 32-bits relocation to .debug_abbrev
    cuh->address_size = 8;

    Elf64_Addr abbr_relocation = cuh_offset + offsetof(CompilationUnitHeader, abbrev_offset);
    elf->info_relocation32(elf->abbr_start_sym, abbr_relocation, 0);
}


void Dwarf::finish_info() {
    // info vector may have been reallocated
    int info_start = cuh_offset + offsetof(CompilationUnitHeader, version);
    int info_end = info.size();
    auto cuh = info.pointer<CompilationUnitHeader>(cuh_offset);
    cuh->length = info_end - info_start;
}


void Dwarf::info_code_address(int value) {
    Elf64_Addr offset = info.append<unsigned64>();
    elf->info_relocation64(elf->code_start_sym, offset, value);
}


void Dwarf::begin_compile_unit_info(std::string name, std::string producer, int low_pc, int high_pc) {
    // DIE - Compile Unit
    info.uleb128(compile_unit_abbrev_number);

    // stmt list
    Elf64_Addr line_relocation = info.append<unsigned32>();  // needs 32-bits relocation to .debug_line
    elf->info_relocation32(elf->line_start_sym, line_relocation, 0);
        
    // low pc
    info_code_address(low_pc);

    // high pc
    info.data8(high_pc - low_pc);
    
    // name
    info.string(name);
    
    // producer
    info.string(producer);

    // Compile Unit children
    /*
    // DIE - Boolean
    info.uleb128(base_type_abbrev_number);
    info.string("Boolean");
    info.data1(1);
    info.data1(DW_ATE_boolean);

    // DIE - Character
    info.uleb128(base_type_abbrev_number);
    info.string("Character");
    info.data1(2);
    info.data1(DW_ATE_UCS);
    */
    // DIE - Integer
    integer_die_offset = info.size();
    info.uleb128(base_type_abbrev_number);
    info.string("Integer");
    info.data1(8);
    info.data1(DW_ATE_signed);
}


void Dwarf::begin_subprogram_info(std::string name, int low_pc, int high_pc) {
    info.uleb128(subprogram_abbrev_number);
    
    info.string(name);
    info_code_address(low_pc);
    info.data8(high_pc - low_pc);
    
    info.uleb128(1);
    info.data1(DW_OP_reg6);  // frame base is RBP
}


void Dwarf::begin_lexical_block_info(int low_pc, int high_pc) {
    info.uleb128(lexical_block_abbrev_number);
    
    info_code_address(low_pc);
    info.data8(high_pc - low_pc);
}


void Dwarf::begin_try_block_info(int low_pc, int high_pc) {
    info.uleb128(try_block_abbrev_number);
    
    info_code_address(low_pc);
    info.data8(high_pc - low_pc);
}


void Dwarf::begin_catch_block_info(int low_pc, int high_pc) {
    info.uleb128(catch_block_abbrev_number);
    
    info_code_address(low_pc);
    info.data8(high_pc - low_pc);
}


void Dwarf::local_variable_info(std::string name, int rbp_offset) {
    info.uleb128(variable_abbrev_number);
    
    info.string(name);
    
    DwarfBuffer offbuff;
    offbuff.sleb128(rbp_offset);
    int exprlen = 1 + offbuff.size();
        
    info.uleb128(exprlen);
    info.data1(DW_OP_fbreg);
    info.sleb128(rbp_offset);
    
    info.data4(integer_die_offset);  // hardcoded Integer type for now
}


void Dwarf::formal_parameter_info(std::string name, int rbp_offset) {
    info.uleb128(formal_parameter_abbrev_number);
    
    info.string(name);
    
    DwarfBuffer offbuff;
    offbuff.sleb128(rbp_offset);
    int exprlen = 1 + offbuff.size();
        
    info.uleb128(exprlen);
    info.data1(DW_OP_fbreg);
    info.sleb128(rbp_offset);
    
    info.data4(integer_die_offset);  // hardcoded Integer type for now
}

    
void Dwarf::end_info() {
    // End of children
    info.uleb128(0x00);
}


void Dwarf::init_lineno(std::vector<std::string> &source_names) {
    // Fill .debug_line

    dlh_offset = lineno.append<DebugLineHeader>();
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
    elf->line_relocation64(elf->code_start_sym, code_relocation, 0);
    
    // Initialize state machine
    lineno_sm_address = 0;
    lineno_sm_file_index = 0;
    lineno_sm_line_number = 1;
}


void Dwarf::finish_lineno() {
    // size after length
    int debug_line_start = dlh_offset + offsetof(DebugLineHeader, version);
    int debug_line_end = lineno.size();
    auto dlh = lineno.pointer<DebugLineHeader>(dlh_offset);
    dlh->length = debug_line_end - debug_line_start;
}


void Dwarf::add_lineno(int pc, int file_index, int line_number) {
    lineno.push_back(2);  // advance pc op
    lineno.uleb128(pc - lineno_sm_address);
    lineno_sm_address = pc;

    lineno.push_back(3);  // advance line op
    lineno.sleb128(line_number - lineno_sm_line_number);
    lineno_sm_line_number = line_number;
    
    if (file_index != lineno_sm_file_index) {
        lineno.push_back(4);  // set file op
        lineno.uleb128(file_index + 1);  // DWARF file indexing from 1
        lineno_sm_file_index = file_index;
    }
    
    lineno.push_back(1);  // copy op
}
