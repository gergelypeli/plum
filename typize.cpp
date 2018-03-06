
// Stage 4

const char NO_EXCEPTION = 0;
const char RETURN_EXCEPTION = -1;

class Declaration;
class Allocable;
class Variable;
class Role;
class PartialVariable;
class Evaluable;
class Function;
class ClassType;

class Type;
class TreenumerationType;
class ArrayType;
class InterfaceType;
class ImplementationType;
class HeapType;

class Scope;
class CodeScope;
class DataScope;
class SwitchScope;
class TryScope;
class EvalScope;
class FunctionScope;

class Value;
class DeclarationValue;
class GenericValue;
class PartialVariableValue;


#include "global_types.h"
#include "global_functions.h"
#include "builtins.h"

Value *typize(Expr *expr, Scope *scope, TypeSpec *context = NULL);
Value *lookup(std::string name, Value *pivot, Expr *expr, Scope *scope, TypeSpec *context = NULL);
Value *lookup_fake(std::string name, Value *pivot, Token token, Scope *scope, TypeSpec *context, Variable *arg_var = NULL);
bool check_argument(unsigned i, Expr *e, const std::vector<ArgInfo> &arg_infos);
bool check_arguments(Args &args, Kwargs &kwargs, const ArgInfos &arg_infos);

#include "declarations/declaration.cpp"
#include "values/value.cpp"

#include "builtins.cpp"
#include "global_functions.cpp"
#include "global_types.cpp"


Value *lookup_unchecked(std::string name, Value *pivot, Scope *scope) {
    if (pivot && pivot->ts[0] == multi_type) {
        // Conversions from Multi to a scalar can only be attempted once (see typematch),
        // but it's not a problem, since we don't want Multi pivots anyway. But for correct
        // operation we should catch this case here.
        return NULL;
    }
    
    //std::cerr << "Looking up  " << pts << " " << name << " definition.\n";
    Value *value = NULL;
    
    for (Scope *s = scope; s; s = s->outer_scope) {
        value = s->lookup(name, pivot);
        
        if (value)
            break;
    }

    if (!value) {
        if (pivot)
            value = pivot->lookup_inner(name);
    }

    if (!value) {
        if (name == "is_equal" || name == "not_equal") {
            std::cerr << "Trying equal fallback for missing " << name << ".\n";
            Value *fallback = lookup_unchecked("equal", pivot, scope);
            
            if (fallback) {
                bool no = (name == "not_equal");
                value = make_equality_value(no, fallback);
            }
        }
    }
    
    if (!value) {
        if (name == "is_equal" || name == "not_equal" ||
            name == "is_less" || name == "is_greater" ||
            name == "not_less" || name == "not_greater"
        ) {
            std::cerr << "Trying compare fallback for missing " << name << ".\n";
            Value *fallback = lookup_unchecked("compare", pivot, scope);
            
            if (fallback) {
                BitSetOp bs = (
                    name == "is_equal" ? SETE :
                    name == "not_equal" ? SETNE :
                    name == "is_less" ? SETL :
                    name == "is_greater" ? SETG :
                    name == "not_less" ? SETGE :
                    name == "not_greater" ? SETLE :
                    throw INTERNAL_ERROR
                );
                value = make_comparison_value(bs, fallback);
            }
        }
    }
    
    // TODO: we should print the definition pivot type, not the value type
    if (value) {
        std::cerr << "Found       " << get_typespec(pivot) << " " << name << " returning " << value->ts << ".\n";
        //std::cerr << "... type " << typeid(*value).name() << ".\n";
    }
        
    return value;
}


Value *lookup(std::string name, Value *pivot, Expr *expr, Scope *scope, TypeSpec *context) {
    //std::cerr << "Looking up  " << pts << " " << name << " definition.\n";
    Value *value = lookup_unchecked(name, pivot, scope);
    
    if (!value) {
        std::cerr << "No match for " << get_typespec(pivot) << " " << name << " at " << expr->token << "!\n";
        throw TYPE_ERROR;
    }

    value->set_token(expr->token);
    value->set_context_ts(context);
    
    bool ok = value->check(expr->args, expr->kwargs, scope);

    if (!ok) {
        std::cerr << "Argument problem for " << expr->token << "!\n";
        throw TYPE_ERROR;
    }
    
    return value;
}


Value *lookup_fake(std::string name, Value *pivot, Token token, Scope *scope, TypeSpec *context, Variable *arg_var) {
    Expr fake_expr(Expr::IDENTIFIER, token, name);
    
    if (arg_var) {
        Expr *fake_arg_expr = new Expr(Expr::IDENTIFIER, token, arg_var->name);
        fake_expr.args.push_back(std::unique_ptr<Expr>(fake_arg_expr));
    }
    
    return lookup(name, pivot, &fake_expr, scope, context);
}


Value *typize(Expr *expr, Scope *scope, TypeSpec *context) {
    Value *value = NULL;

    if (!expr)
        throw INTERNAL_ERROR;
    else if (expr->type == Expr::TUPLE) {
        if (expr->pivot) {
            std::cerr << "A TUPLE had a pivot argument!\n";
            throw INTERNAL_ERROR;
        }

        if (context) {
            value = make_code_block_value(context);
            value->set_token(expr->token);
        
            bool ok = value->check(expr->args, expr->kwargs, scope);
            if (!ok) {
                std::cerr << "Code block error!\n";
                throw TYPE_ERROR;
            }
        }
        else {
            value = make_multi_value();
            value->set_token(expr->token);
        
            bool ok = value->check(expr->args, expr->kwargs, scope);
            if (!ok) {
                std::cerr << "Multi value error!\n";
                throw TYPE_ERROR;
            }
        }
    }
    else if (expr->type == Expr::DECLARATION) {
        std::string name = expr->text;
        if (name == "")
            name = "<anonymous>";
        Value *p = expr->pivot ? typize(expr->pivot.get(), scope) : NULL;
        
        if (p) {
            std::cerr << "Invalid declaration!\n";
            throw TYPE_ERROR;
        }
        else {
            value = make_declaration_value(name, context);
            value->set_token(expr->token);
            bool ok = value->check(expr->args, expr->kwargs, scope);
        
            if (!ok) {
                std::cerr << "Couldn't declare " << name << "!\n";
                throw TYPE_ERROR;
            }

            std::cerr << "Declared " << name << " as " << value->ts << ".\n";
        }
    }
    else if (expr->type == Expr::IDENTIFIER) {
        std::string name = expr->text;
        Value *p = expr->pivot ? typize(expr->pivot.get(), scope) : NULL;
        //if (p)
        //    p->set_marker(marker);
        
        value = lookup(name, p, expr, scope);
        
        if (!value)
            throw TYPE_ERROR;
    }
    else if (expr->type == Expr::CONTROL) {
        std::string name = ":" + expr->text;
        Value *p = expr->pivot ? typize(expr->pivot.get(), scope) : NULL;
        //if (p)
        //    p->set_marker(marker);

        TypeSpec ts;
        
        if (p) {
            if (p->ts[0] == type_type)
                ts = p->ts.unprefix(type_type);
            else {
                std::cerr << "Control with nontype context!\n";
                throw TYPE_ERROR;
            }
        }
        else if (context)
            ts = *context;
        
        value = lookup(name, NULL, expr, scope, &ts);

        if (!value)
            throw TYPE_ERROR;
    }
    else if (expr->type == Expr::EVAL) {
        value = make_eval_value(expr->text);
        value->set_token(expr->token);
        value->set_context_ts(context);
        
        if (!value->check(expr->args, expr->kwargs, scope))
            throw TYPE_ERROR;
    }
    else if (expr->type == Expr::INITIALIZER) {
        std::string name = expr->text;
        Value *p = expr->pivot ? typize(expr->pivot.get(), scope) : NULL;
        //if (p)
        //    p->set_marker(marker);  // just in case

        StringLiteralValue *s = dynamic_cast<StringLiteralValue *>(p);
        
        if (s) {
            if (name.size()) {
                std::cerr << "No named initializers for string literals!\n";
                throw TYPE_ERROR;
            }
            
            value = interpolate(s->text, expr, scope);
        }
        else {
            TypeSpec ts;
            Value *iv = NULL;
            
            if (p) {
                if (p->ts[0] == type_type)
                    ts = p->ts.unprefix(type_type);
                else if (p->ts[0] == initializable_type) {
                    ts = p->ts.unprefix(initializable_type);
                    iv = p;
                }
                else {
                    std::cerr << "Initializer with nontype context!\n";
                    throw TYPE_ERROR;
                }
            }
            else if (context)
                ts = *context;
            else {
                std::cerr << "Initializer without type context!\n";
                throw TYPE_ERROR;
            }
            
            // We must have checked this.
            Type *t = ts[0];
            if (t->type != VALUE_TYPE && !dynamic_cast<MetaType *>(t)) {
                std::cerr << "Initializer with nonvalue type context!\n";
                throw TYPE_ERROR;
            }
            
            if (name.size() == 0)
                name = "{}";
            
            value = ts.lookup_initializer(name, iv);
            
            if (!value) {
                std::cerr << "No initializer " << ts << " `" << name << "!\n";
                throw TYPE_ERROR;
            }
            
            value->set_token(expr->token);
            bool ok = value->check(expr->args, expr->kwargs, scope);
        
            if (!ok) {
                std::cerr << "Initializer argument problem for " << expr->token << "!\n";
                throw TYPE_ERROR;
            }
            
            std::cerr << "Using initializer " << ts << " `" << name << ".\n";
            //std::cerr << "... with type " << value->ts << "\n";
        }
    }
    else if (expr->type == Expr::MATCHER) {
        std::string name = expr->text;
        Value *p = expr->pivot ? typize(expr->pivot.get(), scope) : NULL;

        if (!p) {
            // TODO: put this into IsValue instead!
            SwitchScope *ss = scope->get_switch_scope();
            
            if (!ss) {
                std::cerr << "Pivotless matcher outside of :switch at " << expr->token << "!\n";
                throw TYPE_ERROR;
            }
            
            Variable *v = ss->get_variable();
            p = v->match(v->name, NULL);
            
            if (!p)
                throw INTERNAL_ERROR;
        }

        if (name == "=") {
            Value *equality = lookup_unchecked("is_equal", p, scope);
            value = make_equality_matcher_value(equality);
        }
        else
            value = p->ts.lookup_matcher(name, p);
    
        if (!value) {
            std::cerr << "No matcher " << p->ts << " ~" << name << "!\n";
            throw TYPE_ERROR;
        }

        value->set_token(expr->token);
        bool ok = value->check(expr->args, expr->kwargs, scope);
    
        if (!ok) {
            std::cerr << "Matcher argument problem for " << expr->token << "!\n";
            throw TYPE_ERROR;
        }
        
        std::cerr << "Using matcher " << p->ts << " `" << name << ".\n";
    }
    else if (expr->type == Expr::NUMBER) {
        TypeSpec ts = {
            ends_with(expr->text, "s32") ? integer32_type :
            ends_with(expr->text, "s16") ? integer16_type :
            ends_with(expr->text, "s8") ? integer8_type :
            ends_with(expr->text, "u32") ? unsigned_integer32_type :
            ends_with(expr->text, "u16") ? unsigned_integer16_type :
            ends_with(expr->text, "u8") ? unsigned_integer8_type :
            ends_with(expr->text, "u") ? unsigned_integer_type :
            integer_type
        };

        value = make_basic_value(ts, std::stoi(expr->text));
        value->set_token(expr->token);
    }
    else if (expr->type == Expr::STRING) {
        value = make_string_literal_value(expr->text);
        value->set_token(expr->token);
    }
    else {
        std::cerr << "Can't typize this now: " << expr->token << "!\n";
        throw INTERNAL_ERROR;
    }
    
    //if (context && (*context)[0] == code_type)
    //    value->set_marker(marker);
    
    return value;
}


void check_retros(unsigned i, CodeScope *code_scope, const std::vector<ArgInfo> &arg_infos) {
    // Grab all preceding Dvalue bar declarations, and put them in this scope.
    // Retro variables must only be accessible from the following Code argument's
    // scope, because their initialization is only guaranteed while that Code
    // is being evaluated.
    std::vector<Variable *> retros;
    
    for (unsigned j = i - 1; j < i; j--) {
        DeclarationValue *dv = declaration_value_cast(arg_infos[j].target->get());
        
        if (dv) {
            if (dv->ts[0] != dvalue_type)
                break;
                
            Declaration *decl = declaration_get_decl(dv);
            Variable *var = variable_cast(decl);
            if (!var)
                throw INTERNAL_ERROR;
                
            var->outer_scope->remove(var);
            retros.push_back(var);
        }
    }

    for (auto var : retros) {
        std::cerr << "Moving retro variable " << var->name << " to code scope.\n";
        code_scope->add(var);
    }
}


bool check_argument(unsigned i, Expr *e, const std::vector<ArgInfo> &arg_infos) {
    if (i >= arg_infos.size()) {
        std::cerr << "Too many arguments!\n";
        return false;
    }

    if (*arg_infos[i].target) {
        std::cerr << "Argument " << i << " already supplied!\n";
        return false;
    }

    CodeScope *code_scope = NULL;
    
    if (arg_infos[i].context && (*arg_infos[i].context)[0] == code_type) {
        code_scope = new CodeScope;
        
        check_retros(i, code_scope, arg_infos);
        
        arg_infos[i].scope->add(code_scope);
    }

    Value *v = typize(e, code_scope ? code_scope : arg_infos[i].scope, arg_infos[i].context);
    TypeMatch match;
    
    if (arg_infos[i].context && !typematch(*arg_infos[i].context, v, match, code_scope)) {
        std::cerr << "Argument type mismatch, " << get_typespec(v) << " is not a " << *arg_infos[i].context << "!\n";
        return false;
    }

    if (code_scope && !code_scope->is_taken)
        throw INTERNAL_ERROR;

    arg_infos[i].target->reset(v);
    return true;
}


bool check_arguments(Args &args, Kwargs &kwargs, const ArgInfos &arg_infos) {
    for (unsigned i = 0; i < args.size(); i++) {
        Expr *e = args[i].get();
        
        if (!check_argument(i, e, arg_infos))
            return false;
    }
            
    for (auto &kv : kwargs) {
        unsigned i = (unsigned)-1;
        
        for (unsigned j = 0; j < arg_infos.size(); j++) {
            if (arg_infos[j].name == kv.first) {
                i = j;
                break;
            }
        }
            
        if (i == (unsigned)-1) {
            std::cerr << "No argument named " << kv.first << "!\n";
            return false;
        }
        
        Expr *e = kv.second.get();
        
        if (!check_argument(i, e, arg_infos))
            return false;
    }
    
    return true;
}
