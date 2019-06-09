
class Accounter {
public:
    virtual bool is_on() =0;
    virtual void adjust_stack_usage(int mod) =0;
};


class Emitter {
public:
    virtual void code_byte(char x) =0;
    virtual void code_word(int16 x) =0;
    virtual void code_dword(int x) =0;
    virtual void code_qword(int64 x) =0;
};


class Asm {
public:
    Accounter *accounter;
    Emitter *emitter;
    
    Asm();
    virtual ~Asm();

    virtual void setup(Accounter *a, Emitter *e);
    
    virtual void code_byte(char x);
    virtual void code_word(int16 x);
    virtual void code_dword(int x);
    virtual void code_qword(int64 x);

    virtual bool is_accounting();
    virtual void adjust_stack_usage(int mod);
};
