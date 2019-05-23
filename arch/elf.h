
class Elf {
public:
    std::vector<char> code;
    std::vector<char> data;
    std::vector<char> strings;
    std::vector<char> lineno;
    std::vector<char> abbrev;
    std::vector<char> info;

    std::vector<Elf64_Sym> symbols;
    std::vector<Elf64_Rela> code_relocations;
    std::vector<Elf64_Rela> data_relocations;
    std::vector<Elf64_Rela> line_relocations;
    std::vector<Elf64_Rela> info_relocations;

    unsigned code_start_sym, data_start_sym, line_start_sym, abbr_start_sym;
    
    Elf(std::string module_name);
    ~Elf();
    
    unsigned add_string(std::string s);
    unsigned add_symbol(std::string name, Elf64_Addr value, unsigned size, bool is_global, int type, int section);
    void add_relocation(unsigned index, Elf64_Addr location, int addend, int type, std::vector<Elf64_Rela> &relas);

    unsigned export_absolute(std::string name, Elf64_Addr value, unsigned size, bool is_global);
    unsigned export_data(std::string name, Elf64_Addr location, unsigned size, bool is_global);
    unsigned export_code(std::string name, Elf64_Addr location, unsigned size, bool is_global);
    unsigned import(std::string name);
    
    void code_relocation(unsigned index, Elf64_Addr location, int addend);
    void data_relocation(unsigned index, Elf64_Addr location, int addend);
    void info_relocation32(unsigned index, Elf64_Addr location, int addend);
    void info_relocation64(unsigned index, Elf64_Addr location, int addend);
    void line_relocation64(unsigned index, Elf64_Addr location, int addend);

    void set_code(std::vector<char> &c);
    void set_data(std::vector<char> &d);
    void set_lineno(std::vector<char> &l);
    void set_abbrev(std::vector<char> &a);
    void set_info(std::vector<char> &i);

    void done(std::string filename);
};
