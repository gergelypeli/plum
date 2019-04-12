
// Typize
bool is_typedefinition(Expr *expr);
Value *typize(Expr *expr, Scope *scope, TypeSpec *context = NULL);
Value *lookup(std::string name, Value *pivot, Scope *scope, Expr *expr, TypeSpec *context = NULL);
Value *lookup_fake(std::string name, Value *pivot, Scope *scope, Token token, TypeSpec *context, Variable *arg_var = NULL);
Value *lookup_switch_variable(Scope *scope, Token token);


