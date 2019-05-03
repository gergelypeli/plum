
class TypeSpec;
typedef std::array<TypeSpec,4> TypeMatch;
class X64;

enum AsWhat {
    AS_VALUE, AS_VARIABLE, AS_ARGUMENT, AS_LVALUE_ARGUMENT, AS_RETRO_ARGUMENT
};


class TypeSpec: public std::vector<Type *> {
public:
    TypeSpec();
    TypeSpec(iterator tsi);
    TypeSpec(std::initializer_list<Type *> il):std::vector<Type *>(il) {}
    TypeSpec(Type *t, TypeSpec &tm1, TypeSpec &tm2);

    std::string symbolize(std::string suffix = "");
    TypeMatch match();    
    Allocation measure();
    Allocation measure_identity();
    int measure_raw();
    int measure_elem();
    int measure_stack();
    int measure_where(StorageWhere where);
    Storage optimal_value_storage(Regs preferred);
    devector<VirtualEntry *> get_virtual_table();
    Label get_virtual_table_label(X64 *x64);
    Label get_interface_table_label(X64 *x64);
    Label get_finalizer_label(X64 *x64);
    Value *autoconv(Type *target, Value *orig, TypeSpec &ifts);
    StorageWhere where(AsWhat as_what);
    TypeSpec prefix(Type *t);
    TypeSpec unprefix(Type *t = NULL);
    TypeSpec reprefix(Type *s, Type *t);
    TypeSpec rvalue();
    TypeSpec lvalue();
    bool has_meta(MetaType *mt);
    bool is_meta();
    bool is_hyper();
    Storage store(Storage s, Storage t, X64 *x64);
    Storage create(Storage s, Storage t, X64 *x64);
    void destroy(Storage s, X64 *x64);
    void equal(Storage s, Storage t, X64 *x64);
    void compare(Storage s, Storage t, X64 *x64);
    void streamify(X64 *x64);
    Value *lookup_initializer(std::string name, Scope *scope);
    Value *lookup_matcher(std::string name, Value *pivot, Scope *scope);
    Value *lookup_inner(std::string name, Value *pivot, Scope *scope);
    void init_vt(Address self_addr, X64 *x64);
    void incref(Register r, X64 *x64);
    void decref(Register r, X64 *x64);
    void type_info(X64 *x64);
};

typedef TypeSpec::iterator TypeSpecIter;
typedef std::vector<TypeSpec> TSs;
typedef std::vector<std::string> Ss;
std::ostream &operator<<(std::ostream &os, const TypeSpec &ts);
std::ostream &operator<<(std::ostream &os, const TSs &tss);
std::ostream &operator<<(std::ostream &os, const TypeMatch &tm);


template <class T, class S> T *ptr_cast(S *s) {
    return dynamic_cast<T *>(s);
}


// Functions

// Value wrappers
Declaration *declaration_get_decl(DeclarationValue *dv);
PartialInfo *partial_variable_get_info(Value *v);
bool unpack_value(Value *v, std::vector<TypeSpec> &tss);
bool is_initializer_function_call(Value *value);
TypeSpec type_value_represented_ts(Value *v);
void function_call_be_static(Value *v, Associable *sr);
Value *value_lookup_inner(Value *value, std::string name, Scope *scope);
bool value_check(Value *v, Args &a, Kwargs &k, Scope *s);
void value_need_lvalue(Value *v);
const char *typeidname(Value *v);

// Declaration wrappers
Declaration *make_record_compare();
void associable_override_virtual_entry(Associable *a, int vi, VirtualEntry *ve);
bool associable_is_or_is_in_requiring(Associable *a);
std::string treenumeration_get_name(TreenumerationType *t);
std::string function_get_name(Function *f);
Label function_get_label(Function *f, X64 *x64);

// TypeSpec operations
TypeSpec get_typespec(Value *value);
void set_typespec(Value *value, TypeSpec ts);
std::string print_exception_type(TreenumerationType *t);
TreenumerationType *make_treenum(const char *name, const char *kw1);
TreenumerationType *make_treenum(const char *name, TreenumInput *x);


// Streamification
void stream_preappend2(Address alias_addr, X64 *x64);
void streamify_ascii(std::string s, Address alias_addr, X64 *x64);

// Nosy stuff
void compile_nosytree_callback(Label label, TypeSpec elem_ts, X64 *x64);
//void compile_weakref_nosy_callback(Label label, X64 *x64);
void rbtree_fcb_action(Label action_label, TypeSpec elem_ts, X64 *x64);
void compile_rbtree_clone(Label label, TypeSpec elem_ts, X64 *x64);

// Check
bool typematch(TypeSpec tt, Value *&v, TypeMatch &match);
TypeSpec typesubst(TypeSpec &ts, TypeMatch &match);
Allocation allocsubst(Allocation a, TypeMatch &match);
bool converts(TypeSpec sts, TypeSpec tts);
bool check_argument(unsigned i, Expr *e, const std::vector<ArgInfo> &arg_infos, bool is_function_call = false);
bool check_arguments(Args &args, Kwargs &kwargs, const ArgInfos &arg_infos, bool is_function_call = false);
bool check_exprs(Args &args, Kwargs &kwargs, const ExprInfos &expr_infos);

// TODO
Storage preinitialize_class(TypeSpec class_ts, X64 *x64);

// Makers
template <typename T>
Value *make() {
    return new T;
}

template <typename T, typename A>
Value *make(A a) {
    return new T(a);
}

template <typename T, typename A, typename B>
Value *make(A a, B b) {
    return new T(a, b);
}

template <typename T, typename A, typename B, typename C>
Value *make(A a, B b, C c) {
    return new T(a, b, c);
}

template <typename T, typename A, typename B, typename C, typename D>
Value *make(A a, B b, C c, D d) {
    return new T(a, b, c, d);
}

template <typename T, typename A, typename B, typename C, typename D, typename E>
Value *make(A a, B b, C c, D d, E e) {
    return new T(a, b, c, d, e);
}

template <typename T, typename A, typename B, typename C, typename D, typename E, typename F>
Value *make(A a, B b, C c, D d, E e, F f) {
    return new T(a, b, c, d, e, f);
}


// Builtins

// Vizipok-csodapok!
MetaType *metatype_hypertype = NULL;

MetaType *type_metatype = NULL;

Type *any_type = NULL;
Type *any2_type = NULL;
Type *any3_type = NULL;
Type *anyid_type = NULL;
Type *anyid2_type = NULL;
Type *anyid3_type = NULL;
Type *same_type = NULL;
Type *same2_type = NULL;
Type *same3_type = NULL;
Type *sameid_type = NULL;
Type *sameid2_type = NULL;
Type *sameid3_type = NULL;

MetaType *value_metatype = NULL;
MetaType *identity_metatype = NULL;
MetaType *module_metatype = NULL;
MetaType *attribute_metatype = NULL;
MetaType *enumeration_metatype = NULL;
MetaType *treenumeration_metatype = NULL;
MetaType *integer_metatype = NULL;
MetaType *record_metatype = NULL;
MetaType *union_metatype = NULL;
MetaType *abstract_metatype = NULL;
MetaType *class_metatype = NULL;
MetaType *singleton_metatype = NULL;
MetaType *interface_metatype = NULL;
MetaType *import_metatype = NULL;

Type *ovalue_type = NULL;
Type *lvalue_type = NULL;
Type *dvalue_type = NULL;
Type *code_type = NULL;
Type *void_type = NULL;
Type *unit_type = NULL;
Type *partial_type = NULL;
Type *uninitialized_type = NULL;
Type *initializable_type = NULL;
Type *stringtemplate_type = NULL;
Type *multi_type = NULL;
Type *multilvalue_type = NULL;
Type *multitype_type = NULL;
Type *multicode_type = NULL;
Type *whatever_type = NULL;

Type *boolean_type = NULL;
Type *integer_type = NULL;
Type *integer32_type = NULL;
Type *integer16_type = NULL;
Type *integer8_type = NULL;
Type *unsigned_integer_type = NULL;
Type *unsigned_integer32_type = NULL;
Type *unsigned_integer16_type = NULL;
Type *unsigned_integer8_type = NULL;
Type *character_type = NULL;
Type *float_type = NULL;
Type *ref_type = NULL;
Type *ptr_type = NULL;
Type *nosyvalue_type = NULL;
Type *nosytree_type = NULL;
Type *nosyref_type = NULL;
Type *weakref_type = NULL;
Type *linearray_type = NULL;
Type *circularray_type = NULL;
Type *rbtree_type = NULL;
Type *string_type = NULL;
Type *slice_type = NULL;
Type *option_type = NULL;
Type *optionis_type = NULL;
Type *optionas_type = NULL;
Type *array_type = NULL;
Type *queue_type = NULL;
Type *set_type = NULL;
Type *map_type = NULL;
Type *weakvaluemap_type = NULL;
Type *weakindexmap_type = NULL;
Type *weakset_type = NULL;
Type *countup_type = NULL;
Type *countdown_type = NULL;
Type *item_type = NULL;
Type *arrayelemiter_type = NULL;
Type *arrayindexiter_type = NULL;
Type *arrayitemiter_type = NULL;
Type *queueelemiter_type = NULL;
Type *queueindexiter_type = NULL;
Type *queueitemiter_type = NULL;
Type *setelembyageiter_type = NULL;
Type *setelembyorderiter_type = NULL;
Type *mapitembyageiter_type = NULL;
Type *mapitembyorderiter_type = NULL;
Type *mapindexbyageiter_type = NULL;
Type *mapindexbyorderiter_type = NULL;
Type *sliceelemiter_type = NULL;
Type *sliceindexiter_type = NULL;
Type *sliceitemiter_type = NULL;
//Type *equalitymatcher_type = NULL;

Type *colon_type = NULL;
DataScope *colon_scope = NULL;
Value *colon_value = NULL;

TreenumerationType *iterator_done_exception_type = NULL;
TreenumerationType *container_full_exception_type = NULL;
TreenumerationType *container_empty_exception_type = NULL;
TreenumerationType *container_lent_exception_type = NULL;
TreenumerationType *match_unmatched_exception_type = NULL;
TreenumerationType *lookup_exception_type = NULL;
TreenumerationType *code_break_exception_type = NULL;
TreenumerationType *errno_exception_type = NULL;
TreenumerationType *parse_exception_type = NULL;

InterfaceType *streamifiable_type = NULL;
InterfaceType *iterator_type = NULL;
InterfaceType *iterable_type = NULL;
AbstractType *application_type = NULL;

TypeSpec HYPERTYPE_TS;
TypeSpec NO_TS;
TypeSpec VOID_TS;
TypeSpec MULTI_TS;
TypeSpec MULTILVALUE_TS;
TypeSpec MULTITYPE_TS;
TypeSpec MULTICODE_TS;
TypeSpec ANY_TS;
TypeSpec ANY_OVALUE_TS;
TypeSpec ANY_LVALUE_TS;
TypeSpec SAME_TS;
TypeSpec SAME_LVALUE_TS;
TypeSpec SAME2_LVALUE_TS;
TypeSpec UNIT_TS;
TypeSpec WHATEVER_TS;
TypeSpec WHATEVER_CODE_TS;
TypeSpec BOOLEAN_TS;
TypeSpec INTEGER_TS;
TypeSpec INTEGER_LVALUE_TS;
TypeSpec INTEGER_OVALUE_TS;
TypeSpec BOOLEAN_LVALUE_TS;
TypeSpec UNSIGNED_INTEGER8_TS;
TypeSpec UNSIGNED_INTEGER8_ARRAY_TS;
TypeSpec UNTEGER_TS;
TypeSpec CHARACTER_TS;
TypeSpec CHARACTER_LVALUE_TS;
TypeSpec CHARACTER_ARRAY_TS;
TypeSpec CHARACTER_ARRAY_LVALUE_TS;
TypeSpec FLOAT_TS;
TypeSpec FLOAT_LVALUE_TS;
TypeSpec ANYID_REF_TS;
TypeSpec ANYID_REF_LVALUE_TS;
TypeSpec ANYID_PTR_TS;
TypeSpec ANYID_PTR_LVALUE_TS;
TypeSpec ANYID_WEAKREF_TS;
TypeSpec ANYID_WEAKREF_LVALUE_TS;
TypeSpec SAMEID_NOSYVALUE_NOSYCONTAINER_REF_LVALUE_TS;
TypeSpec ANY_UNINITIALIZED_TS;
TypeSpec WHATEVER_UNINITIALIZED_TS;
TypeSpec STRINGTEMPLATE_TS;
TypeSpec ANY_ARRAY_TS;
TypeSpec ANY_ARRAY_LVALUE_TS;
TypeSpec SAME_ARRAY_TS;
TypeSpec SAME_LINEARRAY_REF_LVALUE_TS;
TypeSpec ANY_QUEUE_TS;
TypeSpec ANY_QUEUE_LVALUE_TS;
TypeSpec SAME_QUEUE_TS;
TypeSpec SAME_CIRCULARRAY_REF_LVALUE_TS;
TypeSpec ANY_RBTREE_REF_TS;
TypeSpec ANY_RBTREE_REF_LVALUE_TS;
TypeSpec SAME_RBTREE_REF_LVALUE_TS;
TypeSpec ANY_RBTREE_PTR_TS;
TypeSpec SAME_SAME2_ITEM_RBTREE_REF_LVALUE_TS;
TypeSpec VOID_CODE_TS;
TypeSpec BOOLEAN_CODE_TS;
TypeSpec STRING_TS;
TypeSpec STRING_LVALUE_TS;
TypeSpec STRING_ARRAY_TS;
TypeSpec ANY_SLICE_TS;
TypeSpec BYTE_SLICE_TS;
TypeSpec ANY_OPTION_TS;
TypeSpec ANY_OPTION_LVALUE_TS;
TypeSpec OPTIONSELECTOR_TS;
TypeSpec ANY_ANY2_MAP_TS;
TypeSpec ANY_ANY2_MAP_LVALUE_TS;
TypeSpec ANY_SET_TS;
TypeSpec ANY_SET_LVALUE_TS;
TypeSpec SAME_SET_TS;
TypeSpec SAME_SAME2_MAP_TS;
TypeSpec ANY_ANYID2_WEAKVALUEMAP_TS;
TypeSpec SAME_SAMEID2_NOSYVALUE_ITEM_RBTREE_REF_TS;
TypeSpec SAME_SAMEID2_NOSYVALUE_ITEM_RBTREE_REF_LVALUE_TS;
TypeSpec ANYID_ANY2_WEAKINDEXMAP_TS;
TypeSpec SAMEID_NOSYVALUE_SAME2_ITEM_RBTREE_REF_TS;
TypeSpec SAMEID_NOSYVALUE_SAME2_ITEM_RBTREE_REF_LVALUE_TS;
TypeSpec ANYID_WEAKSET_TS;

TypeSpec SAMEID_NOSYVALUE_LVALUE_TS;

TypeSpec SAMEID_NOSYVALUE_RBTREE_REF_TS;
TypeSpec SAMEID_NOSYVALUE_RBTREE_REF_LVALUE_TS;

TypeSpec SAMEID_NOSYVALUE_TS;
TypeSpec SAMEID_NOSYVALUE_SAME2_ITEM_TS;
TypeSpec SAME_SAMEID2_NOSYVALUE_ITEM_TS;

TypeSpec STREAMIFIABLE_TS;
TypeSpec ANY_ITERATOR_TS;
TypeSpec SAME_ITERATOR_TS;
TypeSpec SAME_SAME2_ITEM_ITERATOR_TS;
TypeSpec ANY_ITERABLE_TS;
TypeSpec SAME_ITERABLE_TS;
TypeSpec INTEGER_ITERATOR_TS;
TypeSpec INTEGER_ITERABLE_TS;
TypeSpec COUNTUP_TS;
TypeSpec COUNTDOWN_TS;
TypeSpec SAME_ARRAYELEMITER_TS;
TypeSpec SAME_ARRAYINDEXITER_TS;
TypeSpec SAME_ARRAYITEMITER_TS;
TypeSpec SAME_QUEUEELEMITER_TS;
TypeSpec SAME_QUEUEINDEXITER_TS;
TypeSpec SAME_QUEUEITEMITER_TS;
TypeSpec SAME_SETELEMBYAGEITER_TS;
TypeSpec SAME_SETELEMBYORDERITER_TS;
TypeSpec SAME_SAME2_MAPITEMBYAGEITER_TS;
TypeSpec SAME_SAME2_MAPITEMBYORDERITER_TS;
TypeSpec SAME_SAME2_MAPINDEXBYAGEITER_TS;
TypeSpec SAME_SAME2_MAPINDEXBYORDERITER_TS;
TypeSpec SAME_SLICEELEMITER_TS;
TypeSpec SAME_SLICEINDEXITER_TS;
TypeSpec SAME_SLICEITEMITER_TS;
TypeSpec ANY_ANY2_ITEM_TS;
TypeSpec SAME_SAME2_ITEM_TS;
TypeSpec INTEGER_SAME_ITEM_TS;
TypeSpec COLON_TS;

extern TreenumInput errno_treenum_input[];


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
    LabelStore<TypeSpec> type_die_offsets;
    
    Label compile(FunctionCompiler fc);
    Label compile(TypedFunctionCompiler tfc, TypeSpec ts);
    Label compile(Deferrable *d);
    
    Label import_got(std::string name);
    unsigned type_info(TypeSpec ts);

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
    void fix_address(Address address);

    void load_lvalue(Register reg, Register tmp, Storage ref_storage);
    void store_lvalue(Register reg, Register tmp, Storage ref_storage);
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


