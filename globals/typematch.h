
// Check
bool typematch(TypeSpec tt, Value *&v, TypeMatch &match);
TypeSpec typesubst(TypeSpec &ts, TypeMatch &match);
Allocation allocsubst(Allocation a, TypeMatch &match);
bool converts(TypeSpec sts, TypeSpec tts);
bool check_argument(unsigned i, Expr *e, const std::vector<ArgInfo> &arg_infos, bool is_function_call = false);
bool check_arguments(Args &args, Kwargs &kwargs, const ArgInfos &arg_infos, bool is_function_call = false);
bool check_exprs(Args &args, Kwargs &kwargs, const ExprInfos &expr_infos);
