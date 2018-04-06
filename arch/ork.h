#include <string>
#include <vector>
#include "elf.h"


class Ork {
public:
    std::vector<char> code;
    std::vector<char> data;
    std::vector<char> strings;
    std::vector<Elf64_Sym> symbols;
    std::vector<Elf64_Rela> code_relocations;
    std::vector<Elf64_Rela> data_relocations;
    
    Ork();
    ~Ork();
    
    unsigned add_string(std::string s);
    void set_code(std::vector<char> &c);
    void set_data(std::vector<char> &d);
    void done(std::string filename);

    unsigned export_absolute(std::string name, Elf64_Addr value, unsigned size, bool is_global);
    unsigned export_data(std::string name, Elf64_Addr location, unsigned size, bool is_global);
    unsigned export_code(std::string name, Elf64_Addr location, unsigned size, bool is_global);
    unsigned import(std::string name);
    
    void code_relocation(unsigned index, Elf64_Addr location, int addend);
    void data_relocation(unsigned index, Elf64_Addr location, int addend);
};
