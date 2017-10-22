
const long NO_EXCEPTION = 0;
const long RETURN_EXCEPTION = -1;
const long DONE_EXCEPTION = 1;

// Used to simulate size polynoms
const unsigned SAME_SIZE = 65536;


// Stage 4

class Declaration;
class Type;
class ArrayType;
class InterfaceType;
class ImplementationType;
class Scope;
class CodeScope;
class SwitchScope;
class TryScope;
class EvalScope;
class FunctionScope;
class Variable;
class Function;

class Value;
class DeclarationValue;

class TypeSpec: public std::vector<Type *> {
public:
    TypeSpec();
    TypeSpec(iterator tsi);
    TypeSpec(std::initializer_list<Type *> il):std::vector<Type *>(il) {}
    
    unsigned measure(StorageWhere where);
    std::vector<Function *> get_virtual_table();
    Label get_virtual_table_label();
    StorageWhere where(bool is_arg);
    Storage boolval(Storage s, X64 *x64, bool probe);
    TypeSpec prefix(Type *t);
    TypeSpec unprefix(Type *t);
    TypeSpec rvalue();
    TypeSpec lvalue();
    TypeSpec nonlvalue();
    TypeSpec nonrvalue();
    void store(Storage s, Storage t, X64 *x64);
    void create(Storage s, Storage t, X64 *x64);
    void destroy(Storage s, X64 *x64);
    bool compare(Storage s, Storage t, X64 *x64);
    Value *lookup_initializer(std::string name, Scope *scope);
    Scope *get_inner_scope();
};

typedef TypeSpec::iterator TypeSpecIter;
typedef std::vector<TypeSpec> TSs;
typedef std::vector<TypeSpec> TypeMatch;
typedef std::vector<std::string> Ss;
std::ostream &operator<<(std::ostream &os, const TypeSpec &ts);

Value *typize(Expr *expr, Scope *scope, TypeSpec *context = NULL);
Value *lookup(std::string name, Value *pivot, Expr *expr, Scope *scope, TypeSpec *context = NULL);
Value *lookup_fake(std::string name, Value *pivot, Token token, Scope *scope, TypeSpec *context, Variable *arg_var = NULL);

TypeSpec get_typespec(Value *value);
DeclarationValue *declaration_value_cast(Value *value);
Variable *variable_cast(Declaration *decl);
bool is_heap_type(Type *t);
Value *implemented(Declaration *d, TypeSpec ts, TypeSpecIter target, Value *orig);
std::string declaration_get_name(DeclarationValue *dv);
Declaration *declaration_get_decl(DeclarationValue *dv);
Variable *declaration_get_var(DeclarationValue *dv);
Declaration *make_record_compare();

bool typematch(TypeSpec tt, Value *&v, TypeMatch &match);
TypeSpec typesubst(TypeSpec &ts, TypeMatch &match);
TypeMatch type_parameters_to_match(TypeSpec ts);
bool unpack_value(Value *v, std::vector<TypeSpec> &tss);

Value *make_variable_value(Variable *decl, Value *pivot, TypeMatch &match);
Value *make_role_value(Variable *decl, Value *pivot);
Value *make_function_call_value(Function *decl, Value *pivot, TypeMatch &match);
Value *make_type_value(TypeSpec ts);
Value *make_code_block_value(TypeSpec *context);
Value *make_multi_value();
Value *make_eval_value(std::string en);
Value *make_yield_value(EvalScope *es);
Value *make_scalar_conversion_value(Value *p);
Value *make_function_definition_value(TypeSpec fn_ts, Value *ret, Value *head, Value *body, FunctionScope *fn_scope);
Value *make_declaration_value(std::string name, TypeSpec *context);
Value *make_basic_value(TypeSpec ts, int number);
Value *make_string_literal_value(std::string text);
Value *make_code_value(Value *value, Declaration *escape = NULL);
Value *make_void_conversion_value(Value *orig);
Value *make_boolean_conversion_value(Value *orig);
Value *make_implementation_conversion_value(ImplementationType *imt, Value *orig, TypeMatch &match);
Value *make_boolean_not_value(Value *value);
Value *make_null_reference_value(TypeSpec ts);
Value *make_unicode_character_value();
Value *make_integer_definition_value();
Value *make_enumeration_definition_value();
Value *make_treenumeration_definition_value();
Value *make_record_definition_value();
Value *make_record_initializer_value(TypeMatch &match);
Value *make_record_preinitializer_value(TypeSpec ts);
Value *make_record_postinitializer_value(Value *v);
Value *make_class_definition_value();
Value *make_interface_definition_value();
Value *make_implementation_definition_value();
Value *make_identity_value(Value *v);
Value *make_equality_value(bool no, Value *v);
Value *make_comparison_value(BitSetOp bs, Value *v);

DeclarationValue *make_declaration_by_value(std::string name, Value *v, Scope *scope);
Value *make_declaration_by_type(std::string name, TypeSpec ts, Scope *scope);


#include "builtin.h"
#include "declarations/declaration.cpp"
#include "values/value.cpp"
#include "builtin.cpp"


Value *lookup_unchecked(std::string name, Value *pivot, Scope *scope) {
    //std::cerr << "Looking up  " << pts << " " << name << " definition.\n";
    Value *value = NULL;
    TypeMatch match;
    
    for (Scope *s = scope; s; s = s->outer_scope) {
        value = s->lookup(name, pivot, match);
        
        if (value)
            break;
    }

    if (!value) {
        if (pivot)
            value = pivot->lookup_inner(name, match);
    }

    if (!value) {
        if (name == "is_equal" || name == "not_equal") {
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
    if (value)
        std::cerr << "Found       " << get_typespec(pivot) << " " << name << " returning " << value->ts << ".\n";
        
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
    Marker marker = scope->mark();

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
        
        value = make_declaration_value(name, context);
        value->set_token(expr->token);
        bool ok = value->check(expr->args, expr->kwargs, scope);
        
        if (!ok) {
            std::cerr << "Couldn't declare " << name << "!\n";
            throw TYPE_ERROR;
        }

        std::cerr << "Declared " << name << ".\n";
    }
    else if (expr->type == Expr::IDENTIFIER) {
        std::string name = expr->text;
        Value *p = expr->pivot ? typize(expr->pivot.get(), scope) : NULL;
        if (p)
            p->set_marker(marker);
        
        value = lookup(name, p, expr, scope);
        
        if (!value)
            throw TYPE_ERROR;
    }
    else if (expr->type == Expr::CONTROL) {
        std::string name = ":" + expr->text;
        Value *p = expr->pivot ? typize(expr->pivot.get(), scope) : NULL;
        if (p)
            p->set_marker(marker);
        
        value = lookup(name, p, expr, scope, context);

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
        if (p)
            p->set_marker(marker);  // just in case
        
        if (p) {
            StringLiteralValue *s = dynamic_cast<StringLiteralValue *>(p);
        
            if (!s) {
                std::cerr << "Invalid literal initializer!\n";
                throw TYPE_ERROR;
            }
            
            if (name.size()) {
                std::cerr << "No named initializers for string literals!\n";
                throw TYPE_ERROR;
            }
            
            value = interpolate(s->text, expr, scope);
        }
        else {
            if (!context) {
                std::cerr << "Initializer without type context!\n";
                throw TYPE_ERROR;
            }
            
            if (name.size() == 0)
                name = "{}";
            
            TypeSpec ts = *context;
            value = ts.lookup_initializer(name, scope);
            
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
        }
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
    
    if (context && (*context)[0] == code_type)
        value->set_marker(marker);
    
    return value;
}
