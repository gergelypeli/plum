
// Structs

struct ArgInfo;
typedef std::vector<ArgInfo> ArgInfos;
struct PartialInfo;
struct TreenumInput;
struct Allocation;


#include "declarations/all.h"
#include "values/all.h"
#include "globals/all.h"


// Typize
Value *typize(Expr *expr, Scope *scope, TypeSpec *context = NULL);
Value *lookup(std::string name, Value *pivot, Scope *in_scope, Expr *expr, Scope *scope, TypeSpec *context = NULL);
Value *lookup_fake(std::string name, Value *pivot, Token token, Scope *scope, TypeSpec *context, Variable *arg_var = NULL);
Value *lookup_switch(Scope *scope, Token token);


// Top
ModuleScope *lookup_module(std::string module_name, ModuleScope *module_scope);
