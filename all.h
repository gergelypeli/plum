
// Structs

struct ArgInfo;
typedef std::vector<ArgInfo> ArgInfos;
struct ExprInfo;
typedef std::vector<ExprInfo> ExprInfos;
struct PartialInfo;
struct TreenumInput;
struct Allocation;


#include "declarations/all.h"
#include "values/all.h"
#include "globals/all.h"


// Typize
bool is_typedefinition(Expr *expr);
Value *typize(Expr *expr, Scope *scope, TypeSpec *context = NULL);
Value *lookup(std::string name, Value *pivot, Scope *scope, Expr *expr, TypeSpec *context = NULL);
Value *lookup_fake(std::string name, Value *pivot, Scope *scope, Token token, TypeSpec *context, Variable *arg_var = NULL);
Value *lookup_switch_variable(Scope *scope, Token token);


// Top
ModuleScope *lookup_module(std::string required_name, Scope *scope);
ModuleScope *import_module(std::string required_name, Scope *scope);
