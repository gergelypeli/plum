
class Deferrable {
public:
    virtual void deferred_compile(Label label, X64 *x64) {
        throw INTERNAL_ERROR;
    }
};


class Once {
public:
    typedef void (*FunctionCompiler)(Label, X64 *);
    
    typedef void (*TypedFunctionCompiler)(Label, TypeSpec, X64 *);
    typedef std::pair<TypedFunctionCompiler, TypeSpec> FunctionCompilerTuple;

    typedef std::pair<TypeSpec, bool> TypeSpecTuple;

    template<typename KeyType>
    struct LabelStore {
        std::map<KeyType, Label> labels;
        std::set<KeyType> todos;
    
        Label add(KeyType key) {
            int before = labels.size();
            Label label = labels[key];
            int after = labels.size();
    
            if (after != before)
                todos.insert(key);
            
            return label;
        }
    
        bool is_dirty() {
            return !todos.empty();
        }
    
        typename decltype(labels)::value_type take() {
            KeyType key = *todos.begin();
            todos.erase(todos.begin());
        
            return *labels.find(key);
        }
    };

    LabelStore<FunctionCompiler> function_compilers;
    LabelStore<FunctionCompilerTuple> typed_function_compilers;
    LabelStore<Deferrable *> deferrables;
    LabelStore<std::string> import_gots;
    LabelStore<TypeSpecTuple> type_die_offsets;
    
    Label compile(FunctionCompiler fc);
    Label compile(TypedFunctionCompiler tfc, TypeSpec ts);
    Label compile(Deferrable *d);
    
    Label import_got(std::string name);
    unsigned type_info(TypeSpec ts, bool as_alias = false);

    void for_all(X64 *x64);
    void for_debug(X64 *x64);
};


class Unwind {
public:
    std::vector<Value *> stack;
    
    void push(Value *v);
    void pop(Value *v);
    void initiate(Declaration *last, X64 *x64);
};


class Accounting {
public:
    bool am_on;
    int current_stack_usage, highest_stack_usage;

    Accounting();
    
    void start();
    int stop();
    
    bool pause();
    void unpause(bool old_on);
    
    int mark();
    void rewind(int old_stack_usage);

    bool is_on();
    void adjust_stack_usage(int mod);
};


class Runtime {
public:
    const int ARGS_1 = 16;
    const int ARGS_2 = 24;
    const int ARGS_3 = 32;
    const int ARGS_4 = 40;
    
    const int ARG_1 = -8;
    const int ARG_2 = -16;
    const int ARG_3 = -24;
    const int ARG_4 = -32;

    const int PAGE_SIZE = 4096;
    const int INITIAL_STACK_SIZE = PAGE_SIZE * 2;
    const int PROT_NONE = 0;
    const int PROT_RW = 3;

    struct LineInfo {
        int address;
        int file_index;
        int line_number;
    };

    struct FuncInfo {
        std::string name;
        Label start_label;
        Label end_label;
        Label name_label;
    };

    X64 *x64;
    
    Label code_start_label, data_start_label, application_label;
    Label zero_label, float_zero_label, float_minus_zero_label;
    Label allocation_count_label, refcount_balance_label;
    Label die_unmatched_message_label, start_frame_label, task_stack_address_label, task_stack_size_label;

    Label call_sysv_label;
    Label heap_alloc_label, heap_realloc_label;
    Label fcb_alloc_label, fcb_free_label;
    Label empty_function_label, empty_array_label;
    Label finalize_label, finalize_reference_array_label;
    Label lookup_source_info_label, lookup_call_info_label, lookup_frame_info_label, caller_frame_info_label;
    Label fix_stack_label, double_stack_label;
    Label log_label, logref_label, dump_label;
    std::vector<Label> incref_labels, decref_labels;

    Label sysv_malloc_label, sysv_aligned_alloc_label, sysv_free_label, sysv_realloc_label, sysv_mprotect_label,  sysv_memcpy_label, sysv_memmove_label;
    Label sysv_log_label, sysv_logref_label, sysv_dump_label;
    Label sysv_die_label, sysv_dies_label, sysv_die_uncaught_label;
    Label sysv_sort_label, sysv_string_regexp_match_label;
    Label sysv_streamify_integer_label, sysv_streamify_unteger_label, sysv_streamify_boolean_label;
    Label sysv_streamify_pointer_label, sysv_streamify_float_label;

    Label source_infos_label, source_infos_length_label;

    Label func_infos_label, func_infos_length_label;
    std::vector<FuncInfo> func_infos;
    
    Label call_infos_label, call_infos_length_label;
    std::vector<LineInfo> call_infos;

    Runtime(X64 *x, unsigned application_size);

    void data_heap_header();
    Label data_heap_string(std::ustring characters);
    
    void call_sysv(Label l);
    void call_sysv_got(Label got_l);

    void compile_source_infos(std::vector<std::string> source_file_names);

    void add_func_info(std::string name, Label start, Label end);
    void compile_func_infos();

    void add_call_info(int source_file_index, int line_number);
    void compile_call_infos();
    
    int pusha(bool except_rax = false);
    void popa(bool except_rax = false);
    void compile_incref_decref();
    void compile_finalize();
    void compile_heap_alloc();
    void compile_heap_realloc();
    void compile_fcb_alloc();
    void compile_fcb_free();
    void compile_finalize_reference_array();
    void compile_lookup_source_info();
    void compile_lookup_call_info();
    void compile_lookup_frame_info();
    void compile_caller_frame_info();
    void compile_fix_stack();
    void compile_double_stack();
    void compile_call_sysv();
    void compile_logging();
    
    void compile_start(Storage main_storage, std::vector<Label> ils, std::vector<Label> fls);
    
    void incref(Register reg);
    void decref(Register reg);
    void oneref(Register reg);
    
    void heap_alloc();
    void heap_realloc();
    void check_unshared(Register r);

    void r10bcompar(bool is_unsigned);
    void copy(Address s, Address t, int size);
    void push(Address s, int size);
    void fix_address(Address address);

    void load_lvalue(Register reg, Register tmp, Storage ref_storage, int offset = 0);
    void store_lvalue(Register reg, Register tmp, Storage ref_storage, int offset = 0);
    void exchange_lvalue(Register reg, Register tmp, Storage ref_storage, int offset = 0);

    Address make_address(Register base, Register index, int scale, int offset);
    
    void log(std::string message);
    void logref(std::string message, Register r);
    void dump(std::string message);
    void die(std::string message);
    void dies(Register r);
};


class X64: public Asm64 {
public:
    Once *once;
    Unwind *unwind;
    Accounting *accounting;

    Runtime *runtime;
    Dwarf *dwarf;
    
    std::vector<std::string> source_file_names;
    
    X64(std::string module_name, int application_size, std::vector<std::string> sfns)
        :Asm64(module_name) {
        source_file_names = sfns;
        
        once = new Once;
        unwind = new Unwind;
        accounting = new Accounting;
        
        // Needs Accounting
        runtime = new Runtime(this, application_size);
        dwarf = new Dwarf(elf, sfns);
    }
    
    virtual void compile_rest() {
        // Don't associate the rest with the last line number compiled
        add_lineno(0, 0);

        once->for_all(this);
        
        runtime->compile_source_infos(source_file_names);
        runtime->compile_func_infos();
        runtime->compile_call_infos();
    }
    
    virtual void debug_rest() {
        once->for_debug(this);
    }
    
    virtual void add_lineno(int file_index, int line_number) {
        dwarf->add_lineno(get_pc(), file_index, line_number);
    }
    
    virtual bool is_accounting() {
        return accounting->is_on();
    }
    
    virtual void adjust_stack_usage(int mod) {
        accounting->adjust_stack_usage(mod);
    }
};


