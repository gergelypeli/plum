#include <string>
#include <vector>
#include "elf.h"


class Ork {
public:
    std::vector<char> code;
    std::vector<char> data;
    std::vector<char> strings;
    std::vector<Elf32_Sym> symbols;
    std::vector<Elf32_Rel> code_relocations;
    std::vector<Elf32_Rel> data_relocations;
    
    Ork();
    ~Ork();
    
    int add_string(std::string s);
    void set_code(std::vector<char> &c);
    void set_data(std::vector<char> &d);
    void done(std::string filename);

    int export_common(std::string name, int align, int size, int glob);
    int export_absolute(std::string name, int value, int size, int glob);
    int export_data(std::string name, int location, int size, int glob);
    int export_code(std::string name, int location, int size, int glob);
    int import(std::string name);
    void code_relocation(int index, int location, int relative);
    void data_relocation(int index, int location);
};


