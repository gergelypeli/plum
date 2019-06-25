
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

    void uleb128(unsigned x);
    void sleb128(int x);
    void data1(int x);
    void data2(int value);
    void data4(int value);
    void data8(int value);
    void string(std::string s);
};


class Dwarf {
public:
    struct DwarfAttrDef {
        int at;
        int form;
    };

    DwarfBuffer lineno;
    DwarfBuffer abbrev;
    DwarfBuffer info;

    std::map<unsigned, unsigned> info_def_offsets_by_index;
    std::map<unsigned, unsigned> info_ref_indexes_by_offset;

    Elf *elf;
    int cuh_offset, dlh_offset;
    unsigned abbrev_count;

    unsigned compile_unit_abbrev_number;
    unsigned base_type_abbrev_number, enumeration_type_abbrev_number, enumerator_abbrev_number;
    unsigned unspecified_type_abbrev_number, typedef_abbrev_number;
    unsigned pointer_type_abbrev_number, reference_type_abbrev_number;
    unsigned array_type_abbrev_number, subrange_type_abbrev_number;
    unsigned structure_type_abbrev_number, class_type_abbrev_number, interface_type_abbrev_number;
    unsigned variant_part_abbrev_number, variant_abbrev_number;
    unsigned subprogram_abbrev_number, abstract_subprogram_abbrev_number;
    unsigned lexical_block_abbrev_number, try_block_abbrev_number, catch_block_abbrev_number;
    unsigned variable_abbrev_number, formal_parameter_abbrev_number, member_abbrev_number;

    int lineno_sm_address, lineno_sm_file_index, lineno_sm_line_number;
    
    Dwarf(Elf *o, std::vector<std::string> &source_names);
    void finish();
    
    unsigned add_abbrev(int tag, bool has_children, std::vector<DwarfAttrDef> attrdefs);

    void info_def(unsigned index);
    void info_ref(unsigned index);

    void init_lineno(std::vector<std::string> &source_names);
    void add_lineno(int pc, int file_index, int line_number);
    void finish_lineno();
    
    void init_abbrev();
    void finish_abbrev();
    
    void init_info();
    void finish_info();

    void info_code_address(int value);
    
    void begin_compile_unit_info(std::string name, std::string producer, int low_pc, int high_pc);
    void end_info();

    void base_type_info(std::string name, unsigned size, int encoding);
    void begin_enumeration_type_info(std::string name, unsigned size);
    void enumerator_info(std::string name, int value);
    void unspecified_type_info(std::string name);
    void typedef_info(std::string name, unsigned ts_index);
    void pointer_type_info(std::string name, unsigned ts_index);
    void reference_type_info(std::string name, unsigned ts_index);
    void begin_array_type_info(std::string name, unsigned ts_index, unsigned elem_size);
    void subrange_type_info(int length_offset);
    void begin_structure_type_info(std::string name, unsigned size);
    void begin_class_type_info(std::string name, unsigned size);
    void begin_interface_type_info(std::string name);
    void begin_variant_part_info(unsigned discr_index);
    void begin_variant_info(unsigned discr_value);

    void begin_subprogram_info(std::string name, int low_pc, int high_pc, int fbregnum, bool virtuality, unsigned self_index);
    void begin_abstract_subprogram_info(std::string name, bool virtuality);
    void begin_lexical_block_info(int low_pc, int high_pc);
    void begin_try_block_info(int low_pc, int high_pc);
    void begin_catch_block_info(int low_pc, int high_pc);

    void local_variable_info(std::string name, int rbp_offset, unsigned ts_index, bool is_artificial);
    void formal_parameter_info(std::string name, int rbp_offset, unsigned ts_index, bool is_artificial);
    void member_info(std::string name, int offset, unsigned ts_index, bool is_artificial);
};
