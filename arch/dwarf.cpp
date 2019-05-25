#include "../plum.h"


void DwarfBuffer::uleb128(unsigned x) {
    while (true) {
        if ((x & ~0x7f) == 0) {
            push_back(x & 0x7f);
            break;
        }
    
        push_back((x & 0x7f) | 0x80);
        x = (x & ~0x7f) / 128;  // arithmetic shift right
    }
}


void DwarfBuffer::sleb128(int x) {
    while (true) {
        if ((x & ~0x7f) == (x & 0x40 ? ~0x7f : 0)) {
            push_back(x & 0x7f);
            break;
        }
    
        push_back((x & 0x7f) | 0x80);
        x = (x & ~0x7f) / 128;  // arithmetic shift right
    }
}

void DwarfBuffer::data1(int x) {
    push_back(x);
}

void DwarfBuffer::data2(int value) {
    auto offset = append<unsigned16>();
    *pointer<unsigned16>(offset) = value;
}

void DwarfBuffer::data4(int value) {
    auto offset = append<unsigned32>();
    *pointer<unsigned32>(offset) = value;
}

void DwarfBuffer::data8(int value) {
    auto offset = append<unsigned64>();
    *pointer<unsigned64>(offset) = value;
}

void DwarfBuffer::string(std::string s) {
    for (auto c : s)
        push_back(c);
        
    push_back(0);
}




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


void Dwarf::info_def(unsigned index) {
    unsigned def_offset = info.size();
    info_def_offsets_by_index[index] = def_offset;
}


void Dwarf::info_ref(unsigned index) {
    Elf64_Addr ref_offset = info.append<unsigned32>();
    info_ref_indexes_by_offset[ref_offset] = index;
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
        { DW_AT_high_pc, DW_FORM_data4 },
        { DW_AT_name, DW_FORM_string },
        { DW_AT_producer, DW_FORM_string }
    });

    // Types
    base_type_abbrev_number = add_abbrev(DW_TAG_base_type, false, {
        { DW_AT_name, DW_FORM_string },
        { DW_AT_byte_size, DW_FORM_data4 },
        { DW_AT_encoding, DW_FORM_data1 }
    });

    enumeration_type_abbrev_number = add_abbrev(DW_TAG_enumeration_type, true, {
        { DW_AT_name, DW_FORM_string },
        { DW_AT_byte_size, DW_FORM_data4 }
    });

    enumerator_abbrev_number = add_abbrev(DW_TAG_enumerator, false, {
        { DW_AT_name, DW_FORM_string },
        { DW_AT_const_value, DW_FORM_data1 }
    });

    unspecified_type_abbrev_number = add_abbrev(DW_TAG_unspecified_type, false, {
        { DW_AT_name, DW_FORM_string }
    });

    typedef_abbrev_number = add_abbrev(DW_TAG_typedef, false, {
        { DW_AT_name, DW_FORM_string },
        { DW_AT_type, DW_FORM_ref4 }
    });

    pointer_type_abbrev_number = add_abbrev(DW_TAG_pointer_type, false, {
        { DW_AT_name, DW_FORM_string },
        { DW_AT_type, DW_FORM_ref4 }
    });

    reference_type_abbrev_number = add_abbrev(DW_TAG_reference_type, false, {
        { DW_AT_name, DW_FORM_string },
        { DW_AT_type, DW_FORM_ref4 }
    });

    array_type_abbrev_number = add_abbrev(DW_TAG_array_type, true, {
        { DW_AT_name, DW_FORM_string },
        { DW_AT_type, DW_FORM_ref4 },
        { DW_AT_byte_stride, DW_FORM_data4 }
    });

    subrange_type_abbrev_number = add_abbrev(DW_TAG_subrange_type, false, {
        { DW_AT_lower_bound, DW_FORM_data4 },
        { DW_AT_count, DW_FORM_exprloc }
    });

    structure_type_abbrev_number = add_abbrev(DW_TAG_structure_type, true, {
        { DW_AT_name, DW_FORM_string },
        { DW_AT_byte_size, DW_FORM_data4 }
    });

    class_type_abbrev_number = add_abbrev(DW_TAG_class_type, true, {
        { DW_AT_name, DW_FORM_string },
        { DW_AT_byte_size, DW_FORM_data4 }
    });

    interface_type_abbrev_number = add_abbrev(DW_TAG_interface_type, true, {
        { DW_AT_name, DW_FORM_string }
    });

    variant_part_abbrev_number = add_abbrev(DW_TAG_variant_part, true, {
        { DW_AT_discr, DW_FORM_ref4 }
    });

    variant_abbrev_number = add_abbrev(DW_TAG_variant, true, {
        { DW_AT_discr_value, DW_FORM_udata }
    });

    // Code
    subprogram_abbrev_number = add_abbrev(DW_TAG_subprogram, true, {
        { DW_AT_name, DW_FORM_string },
        { DW_AT_low_pc, DW_FORM_addr },
        { DW_AT_high_pc, DW_FORM_data4 },
        { DW_AT_frame_base, DW_FORM_exprloc },
        { DW_AT_virtuality, DW_FORM_data1 },
        { DW_AT_object_pointer, DW_FORM_ref4 }
    });

    abstract_subprogram_abbrev_number = add_abbrev(DW_TAG_subprogram, true, {
        { DW_AT_name, DW_FORM_string },
        { DW_AT_virtuality, DW_FORM_data1 }
    });

    lexical_block_abbrev_number = add_abbrev(DW_TAG_lexical_block, true, {
        { DW_AT_low_pc, DW_FORM_addr },
        { DW_AT_high_pc, DW_FORM_data4 }
    });

    try_block_abbrev_number = add_abbrev(DW_TAG_try_block, true, {
        { DW_AT_low_pc, DW_FORM_addr },
        { DW_AT_high_pc, DW_FORM_data4 }
    });
    
    catch_block_abbrev_number = add_abbrev(DW_TAG_catch_block, true, {
        { DW_AT_low_pc, DW_FORM_addr },
        { DW_AT_high_pc, DW_FORM_data4 }
    });

    // Data
    variable_abbrev_number = add_abbrev(DW_TAG_variable, false, {
        { DW_AT_name, DW_FORM_string },
        { DW_AT_location, DW_FORM_exprloc },
        { DW_AT_type, DW_FORM_ref4 },
        { DW_AT_artificial, DW_FORM_flag }
    });

    formal_parameter_abbrev_number = add_abbrev(DW_TAG_formal_parameter, false, {
        { DW_AT_name, DW_FORM_string },
        { DW_AT_location, DW_FORM_exprloc },
        { DW_AT_type, DW_FORM_ref4 },
        { DW_AT_artificial, DW_FORM_flag }
    });

    member_abbrev_number = add_abbrev(DW_TAG_member, false, {
        { DW_AT_name, DW_FORM_string },
        { DW_AT_data_member_location, DW_FORM_data4 },
        { DW_AT_type, DW_FORM_ref4 },
        { DW_AT_artificial, DW_FORM_flag }
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

    // Resolve DIE references
    for (auto &kv : info_ref_indexes_by_offset) {
        unsigned ref_offset = kv.first;
        unsigned index = kv.second;
        unsigned def_offset = info_def_offsets_by_index.at(index);
        
        // We're using DW_FORM_ref4, a CU-relative offset
        *info.pointer<unsigned32>(ref_offset) = def_offset;
    }
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
    info.data4(high_pc - low_pc);
    
    // name
    info.string(name);
    
    // producer
    info.string(producer);
}


void Dwarf::base_type_info(std::string name, unsigned size, int encoding) {
    info.uleb128(base_type_abbrev_number);
    
    info.string(name);
    info.data4(size);
    info.data1(encoding);
}


void Dwarf::begin_enumeration_type_info(std::string name, unsigned size) {
    info.uleb128(enumeration_type_abbrev_number);
    
    info.string(name);
    info.data4(size);
}


void Dwarf::enumerator_info(std::string name, int value) {
    info.uleb128(enumerator_abbrev_number);
    
    info.string(name);
    info.data1(value);
}


void Dwarf::unspecified_type_info(std::string name) {
    info.uleb128(unspecified_type_abbrev_number);
    
    info.string(name);
}


void Dwarf::typedef_info(std::string name, unsigned ts_index) {
    info.uleb128(typedef_abbrev_number);
    
    info.string(name);
    info_ref(ts_index);
}


void Dwarf::pointer_type_info(std::string name, unsigned ts_index) {
    info.uleb128(pointer_type_abbrev_number);
    
    info.string(name);
    info_ref(ts_index);
}


void Dwarf::reference_type_info(std::string name, unsigned ts_index) {
    info.uleb128(reference_type_abbrev_number);
    
    info.string(name);
    info_ref(ts_index);
}


void Dwarf::begin_array_type_info(std::string name, unsigned ts_index, unsigned elem_size) {
    info.uleb128(array_type_abbrev_number);
    
    info.string(name);
    info_ref(ts_index);
    info.data4(elem_size);
}

void Dwarf::subrange_type_info(int length_offset) {
    info.uleb128(subrange_type_abbrev_number);
    
    // We don't have a language specific default
    info.data4(0);
    
    // The program is 9 bytes long
    info.uleb128(9);
    info.data1(DW_OP_push_object_address);
    info.data1(DW_OP_const4s);
    info.data4(length_offset);
    info.data1(DW_OP_plus);
    info.data1(DW_OP_deref_size);
    info.data1(8);
}


void Dwarf::begin_structure_type_info(std::string name, unsigned size) {
    info.uleb128(structure_type_abbrev_number);
    
    info.string(name);
    info.data4(size);
}


void Dwarf::begin_class_type_info(std::string name, unsigned size) {
    info.uleb128(class_type_abbrev_number);
    
    info.string(name);
    info.data4(size);
}


void Dwarf::begin_interface_type_info(std::string name) {
    info.uleb128(interface_type_abbrev_number);
    
    info.string(name);
}


void Dwarf::begin_variant_part_info(unsigned discr_index) {
    info.uleb128(variant_part_abbrev_number);
    
    info_ref(discr_index);
}


void Dwarf::begin_variant_info(unsigned discr_value) {
    info.uleb128(variant_abbrev_number);
    
    info.uleb128(discr_value);
}


void Dwarf::begin_subprogram_info(std::string name, int low_pc, int high_pc, bool virtuality, unsigned self_index) {
    info.uleb128(subprogram_abbrev_number);
    
    info.string(name);
    info_code_address(low_pc);
    info.data4(high_pc - low_pc);
    
    info.uleb128(1);
    info.data1(DW_OP_reg6);  // frame base is RBP
    
    info.data1(virtuality ? DW_VIRTUALITY_virtual : DW_VIRTUALITY_none);
    info_ref(self_index);
}


void Dwarf::begin_abstract_subprogram_info(std::string name, bool virtuality) {
    info.uleb128(abstract_subprogram_abbrev_number);
    
    info.string(name);
    
    info.data1(virtuality ? DW_VIRTUALITY_virtual : DW_VIRTUALITY_none);
}


void Dwarf::begin_lexical_block_info(int low_pc, int high_pc) {
    info.uleb128(lexical_block_abbrev_number);
    
    info_code_address(low_pc);
    info.data4(high_pc - low_pc);
}


void Dwarf::begin_try_block_info(int low_pc, int high_pc) {
    info.uleb128(try_block_abbrev_number);
    
    info_code_address(low_pc);
    info.data4(high_pc - low_pc);
}


void Dwarf::begin_catch_block_info(int low_pc, int high_pc) {
    info.uleb128(catch_block_abbrev_number);
    
    info_code_address(low_pc);
    info.data4(high_pc - low_pc);
}


void Dwarf::local_variable_info(std::string name, int rbp_offset, unsigned ts_index, bool is_artificial) {
    info.uleb128(variable_abbrev_number);
    
    info.string(name);
    
    DwarfBuffer offbuff;
    offbuff.sleb128(rbp_offset);
    int exprlen = 1 + offbuff.size();
        
    info.uleb128(exprlen);
    info.data1(DW_OP_fbreg);
    info.sleb128(rbp_offset);
    
    info_ref(ts_index);
    info.data1(is_artificial ? 1 : 0);
}


void Dwarf::formal_parameter_info(std::string name, int rbp_offset, unsigned ts_index, bool is_artificial) {
    info.uleb128(formal_parameter_abbrev_number);
    
    info.string(name);
    
    DwarfBuffer offbuff;
    offbuff.sleb128(rbp_offset);
    int exprlen = 1 + offbuff.size();
        
    info.uleb128(exprlen);
    info.data1(DW_OP_fbreg);
    info.sleb128(rbp_offset);

    info_ref(ts_index);
    info.data1(is_artificial ? 1 : 0);
}


void Dwarf::member_info(std::string name, int offset, unsigned ts_index, bool is_artificial) {
    info.uleb128(member_abbrev_number);
    
    info.string(name);
    
    info.data4(offset);
    
    info_ref(ts_index);
    info.data1(is_artificial ? 1 : 0);
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
