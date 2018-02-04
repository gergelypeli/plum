
class TypeSpec;
typedef std::vector<TypeSpec> TypeMatch;
struct Allocation;


class TypeSpec: public std::vector<Type *> {
public:
    TypeSpec();
    TypeSpec(iterator tsi);
    TypeSpec(std::initializer_list<Type *> il):std::vector<Type *>(il) {}

    TypeMatch match();    
    Allocation measure();
    int measure_raw();
    int measure_elem();
    int measure_stack();
    int measure_where(StorageWhere where);
    std::vector<Function *> get_virtual_table();
    Label get_virtual_table_label(X64 *x64);
    Label get_finalizer_label(X64 *x64);
    Value *autoconv(iterator target, Value *orig, TypeSpec &ifts);
    StorageWhere where(bool is_arg, bool is_lvalue = false);
    Storage boolval(Storage s, X64 *x64, bool probe);
    TypeSpec prefix(Type *t);
    TypeSpec unprefix(Type *t = NULL);
    TypeSpec reprefix(Type *s, Type *t);
    TypeSpec rvalue();
    TypeSpec lvalue();
    TypeSpec nonlvalue();
    TypeSpec nonrvalue();
    TypeSpec varvalue();
    void store(Storage s, Storage t, X64 *x64);
    void create(Storage s, Storage t, X64 *x64);
    void destroy(Storage s, X64 *x64);
    void compare(Storage s, Storage t, X64 *x64, Label less, Label greater);
    void compare(Storage s, Storage t, X64 *x64, Register reg);
    Value *lookup_initializer(std::string name, Scope *scope);
    Value *lookup_inner(std::string name, Value *pivot);
    DataScope *get_inner_scope();
};

typedef TypeSpec::iterator TypeSpecIter;
typedef std::vector<TypeSpec> TSs;
//typedef std::vector<TypeSpec> TypeMatch;
typedef std::vector<std::string> Ss;
std::ostream &operator<<(std::ostream &os, const TypeSpec &ts);
std::ostream &operator<<(std::ostream &os, const TypeMatch &tm);




class Once {
public:
    typedef void (*FunctionCompiler)(Label, X64 *);
    typedef void (*TypedFunctionCompiler)(Label, TypeSpec, X64 *);
    typedef std::pair<TypedFunctionCompiler, TypeSpec> FunctionCompilerTuple;
    
    std::map<FunctionCompiler, Label> function_compiler_labels;
    std::map<FunctionCompilerTuple, Label> typed_function_compiler_labels;
    
    std::set<FunctionCompiler> function_compiler_todo;
    std::set<FunctionCompilerTuple> typed_function_compiler_todo;
    
    Label compile(FunctionCompiler fc);
    Label compile(TypedFunctionCompiler tfc, TypeSpec ts);
    void for_all(X64 *x64);
};




class Unwind {
public:
    std::vector<Value *> stack;
    
    void push(Value *v);
    void pop(Value *v);
    void initiate(Declaration *last, X64 *x64);
};




struct ArgInfo {
    const char *name;
    TypeSpec *context;
    Scope *scope;
    std::unique_ptr<Value> *target;  // Yes, a pointer to an unique_ptr
};

typedef std::vector<ArgInfo> ArgInfos;



struct Allocation {
    int bytes;
    int count1;
    int count2;
    int count3;
    
    Allocation(int b = 0, int c1 = 0, int c2 = 0, int c3 = 0);
    int concretize(TypeMatch tm);
    int concretize();
};

Allocation stack_size(Allocation a);

std::ostream &operator<<(std::ostream &os, const Allocation &a);
