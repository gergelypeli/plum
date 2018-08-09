
class Once {
public:
    typedef void (*FunctionCompiler)(Label, X64 *);
    typedef void (*TypedFunctionCompiler)(Label, TypeSpec, X64 *);
    typedef std::pair<TypedFunctionCompiler, TypeSpec> FunctionCompilerTuple;

    std::map<FunctionCompiler, Label> function_compiler_labels;
    std::map<FunctionCompilerTuple, Label> typed_function_compiler_labels;
    
    std::set<FunctionCompiler> function_compiler_todo;
    std::set<FunctionCompilerTuple> typed_function_compiler_todo;

    std::map<std::string, Label> import_labels;
    std::map<std::string, Label> import_got_labels;
    
    Label compile(FunctionCompiler fc);
    Label compile(TypedFunctionCompiler tfc, TypeSpec ts);
    
    Label import(std::string name);
    Label import_got(std::string name);

    void for_all(X64 *x64);
};




class Unwind {
public:
    std::vector<Value *> stack;
    
    void push(Value *v);
    void pop(Value *v);
    void initiate(Declaration *last, X64 *x64);
};




class Runtime {
public:
    X64 *x64;
    
    Label application_label;
    Label zero_label, float_minus_zero_label;
    Label alloc_RAX_RBX_label, realloc_RAX_RBX_label;
    Label empty_function_label, empty_array_label;
    Label alloc_fcb_label, free_fcb_label;
    Label finalize_label, finalize_reference_array_label, weak_finalized_die_label;
    std::vector<Label> incref_labels, decref_labels;

    Label sysv_memalloc_label, sysv_memfree_label, sysv_memrealloc_label;
    Label sysv_logfunc_label, sysv_dump_label, sysv_die_label, sysv_dies_label;
    Label sysv_sort_label, sysv_string_regexp_match_label;
    
    Runtime(X64 *x, unsigned application_size);

    void data_heap_header();
    Label data_heap_string(std::vector<unsigned16> characters);
    
    void call_sysv(Label l);
    void call_sysv_got(Label got_l);
    
    int pusha(bool except_rax = false);
    void popa(bool except_rax = false);
    
    void init_memory_management();
    
    void incref(Register reg);
    void decref(Register reg);
    void incweakref(Register reg);
    void decweakref(Register reg);
    
    void memfree(Register reg);
    void alloc_RAX_RBX();
    void realloc_RAX_RBX();
    void lock(Register r, Label ok);
    
    void log(std::string message);
    void dump(std::string message);
    void die(std::string message);
    void dies(Register r);
};
