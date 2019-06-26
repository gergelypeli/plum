
class Elf {
public:
    std::vector<char> code;
    std::vector<char> data;
    std::vector<char> strings;
    std::vector<char> lineno;
    std::vector<char> abbrev;
    std::vector<char> info;
    std::vector<char> frame;

    std::vector<Elf64_Sym> symbols;
    std::vector<Elf64_Rela> code_relocations;
    std::vector<Elf64_Rela> data_relocations;
    std::vector<Elf64_Rela> line_relocations;
    std::vector<Elf64_Rela> info_relocations;
    std::vector<Elf64_Rela> frame_relocations;

    unsigned code_start_sym, data_start_sym, line_start_sym, abbr_start_sym;
    
    Elf(std::string module_name);
    virtual ~Elf();

    virtual Elf64_Half get_machine() =0;
    virtual void info_relocation32(unsigned index, Elf64_Addr location, int addend) =0;
    virtual void info_relocation64(unsigned index, Elf64_Addr location, int addend) =0;
    virtual void line_relocation64(unsigned index, Elf64_Addr location, int addend) =0;
    virtual void frame_relocation64(unsigned index, Elf64_Addr location, int addend) =0;
    
    virtual unsigned add_string(std::string s);
    virtual unsigned add_symbol(std::string name, Elf64_Addr value, unsigned size, bool is_global, int type, int section);
    virtual void add_relocation(unsigned index, Elf64_Addr location, int addend, int type, std::vector<Elf64_Rela> &relas);

    virtual unsigned export_absolute(std::string name, Elf64_Addr value, unsigned size, bool is_global);
    virtual unsigned export_data(std::string name, Elf64_Addr location, unsigned size, bool is_global);
    virtual unsigned export_code(std::string name, Elf64_Addr location, unsigned size, bool is_global);
    virtual unsigned import(std::string name);

    virtual void set_code(std::vector<char> &c);
    virtual void set_data(std::vector<char> &d);
    virtual void set_lineno(std::vector<char> &l);
    virtual void set_abbrev(std::vector<char> &a);
    virtual void set_info(std::vector<char> &i);
    virtual void set_frame(std::vector<char> &f);

    virtual void done(std::string filename);
};

