#include <string>
#include <vector>

typedef unsigned Elf32_Addr;
typedef unsigned short Elf32_Half;
typedef unsigned Elf32_Off;
typedef int Elf32_Sword;
typedef unsigned Elf32_Word;

typedef struct {
    unsigned char             e_ident[16];
    Elf32_Half                    e_type;
    Elf32_Half                    e_machine;
    Elf32_Word                    e_version;
    Elf32_Addr                    e_entry;
    Elf32_Off                     e_phoff;
    Elf32_Off                     e_shoff;
    Elf32_Word                    e_flags;
    Elf32_Half                    e_ehsize;
    Elf32_Half                    e_phentsize;
    Elf32_Half                    e_phnum;
    Elf32_Half                    e_shentsize;
    Elf32_Half                    e_shnum;
    Elf32_Half                    e_shstrndx;
} Elf32_Ehdr;

typedef struct {
    Elf32_Word	sh_name;
    Elf32_Word	sh_type;
    Elf32_Word	sh_flags;
    Elf32_Addr	sh_addr;
    Elf32_Off	sh_offset;
    Elf32_Word	sh_size;
    Elf32_Word	sh_link;
    Elf32_Word	sh_info;
    Elf32_Word	sh_addralign;
    Elf32_Word	sh_entsize;
} Elf32_Shdr;

typedef struct {
    Elf32_Word	st_name;
    Elf32_Addr	st_value;
    Elf32_Word	st_size;
    unsigned char	st_info;
    unsigned char	st_other;
    Elf32_Half	st_shndx;
} Elf32_Sym;

typedef struct {
    Elf32_Addr	r_offset;
    Elf32_Word	r_info;
} Elf32_Rel;


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


