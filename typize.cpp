
// Stage 4

class Declaration;
class Type;
class Scope;
class FunctionReturnScope;
class FunctionHeadScope;
class FunctionBodyScope;
class FunctionScope;
class Variable;
class Function;

typedef std::vector<Type *> TypeSpec;
typedef TypeSpec::iterator TypeSpecIter;

Type *type_type = NULL;
Type *lvalue_type = NULL;
Type *void_type = NULL;
Type *function_type = NULL;
Type *boolean_type = NULL;
Type *integer_type = NULL;

TypeSpec TS_VOID;

typedef std::vector<std::unique_ptr<Expr>> Args;
typedef std::map<std::string, std::unique_ptr<Expr>> Kwargs;

bool operator>>(TypeSpec &this_ts, TypeSpec &that_ts);
bool are_equal(TypeSpecIter &this_tsi, TypeSpecIter &that_tsi);
bool are_convertible(TypeSpecIter &this_tsi, TypeSpecIter &that_tsi);
unsigned measure(TypeSpec &ts);
unsigned measure(TypeSpecIter &tsi);

class Value;
Value *typize(Expr *expr, Scope *scope);
TypeSpec get_typespec(Value *v);
Value *make_function_head_value(FunctionHeadScope *s);
Value *make_function_body_value(FunctionBodyScope *s);
Value *make_function_return_value(FunctionReturnScope *s, Value *v);
Value *make_variable_value(Variable *decl, Value *pivot);
Value *make_function_value(Function *decl, Value *pivot);
Value *make_type_value(TypeSpec ts);
Value *make_block_value();
Value *make_function_definition_value(Value *ret, Value *head, Value *body, FunctionScope *fn_scope);
Value *make_declaration_value(std::string name);
Value *make_number_value(std::string text);


unsigned round_up(unsigned size) {
    return (size + 7) & ~7;
}


enum StorageWhere {
    // No result
    NOWHERE,
    // the value is on the top of the stack (fo=0, so=0)
    STACK,
    // the value is in RAX (fo=0, so=0)
    REGISTER,
    // the value is in a local variable at [RBP + fo] (so=0)
    FRAME

    // Not needed yet.
    // the value is at [[RBP + fo] + so], where [RBP + fo] is an outer frame pointer
    //FRAME_FRAME,

    // TODO: there will be multiple reference classes, this needs to be rethought.
    // TODO: and direct member access will only be allowed from methods, and
    // that will use a borrowed reference only.
    // the value is at [STACK + so], STACK is refcounted container (fo=0)
    //STACK_HEAP,
    // the value is at [RAX + so], RAX is refcounted container (fo=0)
    //REGISTER_HEAP,
    // the value is at [[RBP + fo] + so], where [RBP + fo] is refcounted container
    //FRAME_HEAP
};


struct Storage {
    StorageWhere where;
    int frame_offset;
    //int secondary_offset;
    
    Storage(StorageWhere w = NOWHERE, int fo = 0) {
        where = w;
        frame_offset = fo;
    }
};


std::ostream &operator<<(std::ostream &os, Storage &s) {
    if (s.where == NOWHERE)
        os << "NOWHERE";
    else if (s.where == STACK)
        os << "STACK";
    else if (s.where == REGISTER)
        os << "REGISTER";
    else if (s.where == FRAME)
        os << "FRAME[" << s.frame_offset << "]";
    else
        os << "???";
        
    return os;
}


void store(TypeSpecIter &tsi, Storage s, Storage t, X64 *x64);
void store(TypeSpec &ts, Storage s, Storage t, X64 *x64);


#include "declarations.cpp"

#include "values.cpp"


Scope *init_types() {
    Scope *root_scope = new Scope();
    
    type_type = new Type("<Type>", 1);
    root_scope->add(type_type);

    lvalue_type = new LvalueType();
    root_scope->add(lvalue_type);

    function_type = new Type("<Function>", 1);
    root_scope->add(function_type);
    
    void_type = new BasicType("<Void>", 0);
    root_scope->add(void_type);
    TS_VOID.push_back(void_type);

    boolean_type = new BasicType("Boolean", 1);
    root_scope->add(boolean_type);

    integer_type = new BasicType("Integer", 8);
    root_scope->add(integer_type);
    
    TypeSpec void_ts = { void_type };
    TypeSpec bool_ts = { boolean_type };
    TypeSpec int_ts = { integer_type };
    TypeSpec lint_ts = { lvalue_type, integer_type };
    
    std::vector<TypeSpec> int_tss = { int_ts };
    std::vector<TypeSpec> bool_tss = { bool_ts };
    
    std::vector<std::string> value_names = { "value" };

    for (auto name : { "minus", "negate" })
        root_scope->add(new Function(name, void_ts, int_tss, value_names, int_ts));

    for (auto name : { "plus", "minus", "star", "slash", "percent", "or", "xor", "and", "exponent" })
        root_scope->add(new Function(name, int_ts, int_tss, value_names, int_ts));

    for (auto name : { "equal", "not_equal", "less", "greater", "less_equal", "greater_equal", "incomparable", "compare" })
        root_scope->add(new Function(name, int_ts, int_tss, value_names, bool_ts));

    for (auto name : { "logical not", "logical and", "logical or", "logical xor" })
        root_scope->add(new Function(name, bool_ts, bool_tss, value_names, bool_ts));

    for (auto name : { "plus_assign", "minus_assign", "star_assign", "slash_assign", "percent_assign", "or_assign", "xor_assign", "and_assign" })
        root_scope->add(new Function(name, lint_ts, int_tss, value_names, int_ts));

    root_scope->add(new Function("print", void_ts, int_tss, value_names, void_ts));

    return root_scope;
}


Value *typize(Expr *expr, Scope *scope) {
    if (expr->type == OPEN) {
        if (expr->pivot) {
            std::cerr << "An OPEN had a pivot argument!\n";
            throw INTERNAL_ERROR;
        }

        Value *v = make_block_value()->set_token(expr->token);
        v->check(expr->args, expr->kwargs, scope);
            
        return v;
    }
    else if (expr->type == STATEMENT) {
        if (expr->text == "function") {
            FunctionScope *fn_scope = new FunctionScope();
            scope->add(fn_scope);
            
            Expr *r = expr->pivot.get();
            Scope *rs = fn_scope->add_return_scope();
            Value *ret = r ? typize(r, rs) : NULL;
        
            Expr *h = expr->kwargs["from"].get();
            Scope *hs = fn_scope->add_head_scope();
            Value *head = h ? typize(h, hs) : NULL;
            
            Expr *b = expr->kwargs["as"].get();
            Scope *bs = fn_scope->add_body_scope();
            Value *body = b ? typize(b, bs) : NULL;
            
            TypeSpec fn_ts;
                
            if (ret) {
                if (ret->ts[0] != type_type) {
                    std::cerr << "Function return expression is not a type!\n";
                    throw TYPE_ERROR;
                }

                // Add anon return value variable, so funretscp can compute its size!
                TypeSpec var_ts = ret->ts;
                var_ts[0] = lvalue_type;
                Variable *decl = new Variable("<ret>", TS_VOID, var_ts);
                rs->add(decl);
                    
                fn_ts = ret->ts;
                fn_ts[0] = function_type;
            }
            else {
                fn_ts.push_back(function_type);
                fn_ts.push_back(void_type);
            }

            std::cerr << "Function ts " << fn_ts << "\n";
            
            Value *v = make_function_definition_value(ret, head, body, fn_scope)->set_token(expr->token);
            v->set_ts(fn_ts);
            
            return v;
        }
        else if (expr->text == "return") {
            Expr *r = expr->pivot.get();
            Value *ret = r ? typize(r, scope) : NULL;  // TODO: statement scope? Or already have?

            Value *v = make_function_return_value(scope, ret)->set_token(expr->token);

            return v;
        }
        else {
            std::cerr << "Unknown statement " << expr->token << "!\n";
            throw TYPE_ERROR;
        }
    }
    else if (expr->type == DECLARATION) {
        std::string name = expr->text;
        std::cerr << "Declaring " << name << ".\n";
        
        Value *v = make_declaration_value(name)->set_token(expr->token);
        bool ok = v->check(expr->args, expr->kwargs, scope);
        
        if (!ok) {
            std::cerr << "Couldn't declare " << name << "!\n";
            throw TYPE_ERROR;
        }

        std::cerr << "Declared " << name << ".\n";
        return v;
    }
    else if (expr->type == IDENTIFIER) {
        std::string name = expr->text;
        Value *p = expr->pivot ? typize(expr->pivot.get(), scope) : NULL;
        TypeSpec pts = p ? p->ts : TS_VOID;
        
        std::cerr << "Looking up " << pts << " " << name << "\n";

        for (Scope *s = scope; s; s = s->outer) {
            //std::cerr << "Trying a scope...\n";
            
            Value *v = s->lookup(expr->text, p);
        
            if (v) {
                v->set_token(expr->token);
                bool ok = v->check(expr->args, expr->kwargs, scope);
                
                if (!ok) {
                    std::cerr << "Argument problem for " << expr->token << "!\n";
                    throw TYPE_ERROR;
                }

                std::cerr << "Found " << pts << " " << name << " as a " << v->ts << ".\n";
                return v;
            }
        }
        
        std::cerr << "No match for " << pts << " " << name << " at " << expr->token << "!\n";
        throw TYPE_ERROR;
    }
    else if (expr->type == NUMBER) {
        return make_number_value(expr->text)->set_token(expr->token);  // TODO
    }
    else {
        std::cerr << "Can't typize this now: " << expr->token << "!\n";
        throw INTERNAL_ERROR;
    }
}
