

struct ArgInfo {
    const char *name;
    TypeSpec *context;
    Scope *scope;
    std::unique_ptr<Value> *target;  // Yes, a pointer to an unique_ptr
};

struct ExprInfo {
    std::string name;
    std::unique_ptr<Expr> *target;  // Yes, too
};

// Check
bool typematch(TypeSpec tt, Value *&v, TypeMatch &match);
TypeSpec typesubst(TypeSpec &ts, TypeMatch &match);
Allocation allocsubst(Allocation a, TypeMatch &match);
bool converts(TypeSpec sts, TypeSpec tts);

bool check_argument(unsigned i, Expr *e, const std::vector<ArgInfo> &arg_infos, bool is_function_call = false);
bool check_arguments(Args &args, Kwargs &kwargs, const ArgInfos &arg_infos, bool is_function_call = false);
bool check_exprs(Args &args, Kwargs &kwargs, const ExprInfos &expr_infos);
