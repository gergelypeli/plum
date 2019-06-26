
class Elf_X64: public Elf {
public:
    Elf_X64(std::string module_name);

    virtual Elf64_Half get_machine();
    virtual void info_relocation32(unsigned index, Elf64_Addr location, int addend);
    virtual void info_relocation64(unsigned index, Elf64_Addr location, int addend);
    virtual void line_relocation64(unsigned index, Elf64_Addr location, int addend);
    virtual void frame_relocation64(unsigned index, Elf64_Addr location, int addend);
    
    virtual void code_relocation(unsigned index, Elf64_Addr location, int addend);
    virtual void data_relocation(unsigned index, Elf64_Addr location, int addend);
};

