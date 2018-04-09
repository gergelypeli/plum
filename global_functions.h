
// Value wrappers

Declaration *declaration_get_decl(DeclarationValue *dv);
bool declaration_use(DeclarationValue *dv, Value *v, Scope *s);
PartialVariable *partial_variable_get_pv(Value *v);
bool unpack_value(Value *v, std::vector<TypeSpec> &tss);
bool is_initializer_function_call(Value *value);
void function_call_be_static(Value *fcv);
TypeSpec type_value_represented_ts(Value *v);
Value *peek_void_conversion_value(Value *v);


// Declaration wrappers
Declaration *make_record_compare();
bool descend_into_explicit_scope(std::string &name, Scope *&scope);


// TypeSpec operations
TypeSpec get_typespec(Value *value);
void set_typespec(Value *value, TypeSpec ts);
//bool is_implementation(Type *t, TypeMatch &match, TypeSpecIter target, TypeSpec &ifts);
std::string print_exception_type(TreenumerationType *t);
TreenumerationType *make_treenum(const char *name, const char *kw1);


// Streamification
void compile_array_preappend(Label label, TypeSpec elem_ts, X64 *x64);


// Check
Value *find_implementation(TypeMatch &match, TypeSpecIter target, Value *orig, TypeSpec &ifts);
bool typematch(TypeSpec tt, Value *&v, TypeMatch &match, CodeScope *code_scope = NULL);
TypeSpec typesubst(TypeSpec &ts, TypeMatch &match);
bool check_argument(unsigned i, Expr *e, const std::vector<ArgInfo> &arg_infos);
bool check_arguments(Args &args, Kwargs &kwargs, const ArgInfos &arg_infos);


// Typize
Value *typize(Expr *expr, Scope *scope, TypeSpec *context = NULL);
Value *lookup(std::string name, Value *pivot, Expr *expr, Scope *scope, TypeSpec *context = NULL);
Value *lookup_fake(std::string name, Value *pivot, Token token, Scope *scope, TypeSpec *context, Variable *arg_var = NULL);
Value *lookup_switch(Scope *scope, Token token);


// Top
ModuleScope *lookup_module(std::string module_name, ModuleScope *module_scope);
