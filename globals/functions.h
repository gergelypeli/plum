
// Value operations

TypeSpec get_typespec(Value *value);
void set_typespec(Value *value, TypeSpec ts);
void function_call_be_static(Value *v, Associable *sr);
Declaration *declaration_get_decl(DeclarationValue *dv);
PartialInfo *partial_variable_get_info(Value *v);
bool is_initializer_function_call(Value *value);
TypeSpec type_value_represented_ts(Value *v);
TSs type_tuple_value_represented_tss(Value *v);
Value *value_lookup_inner(Value *value, std::string name, Scope *scope);
bool value_check(Value *v, Args &a, Kwargs &k, Scope *s);
const char *typeidname(Value *v);

// Declaration operations

Declaration *make_record_compare();
void associable_override_virtual_entry(Associable *a, int vi, VirtualEntry *ve);
bool associable_is_or_is_in_requiring(Associable *a);
std::string treenumeration_get_name(TreenumerationType *t);
std::string function_get_name(Function *f);
Label function_get_label(Function *f, X64 *x64);
std::string print_exception_type(TreenumerationType *t);
TreenumerationType *make_treenum(const char *name, const char *kw1);
TreenumerationType *make_treenum(const char *name, TreenumInput *x);

// Checking

void check_retros(unsigned i, Scope *scope, const std::vector<ArgInfo> &arg_infos, CodeScope *code_scope);
bool check_argument(unsigned i, Expr *e, const std::vector<ArgInfo> &arg_infos, bool is_function_call);
bool check_arguments(Args &args, Kwargs &kwargs, const ArgInfos &arg_infos, bool is_function_call);
bool check_exprs(Args &args, Kwargs &kwargs, const ExprInfos &expr_infos);
