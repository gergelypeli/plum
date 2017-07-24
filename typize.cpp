
// Stage 4


class Declaration;
class Type;
class Scope;
class FunctionResultScope;
class FunctionHeadScope;
class FunctionBodyScope;
class FunctionScope;
class Variable;
class Function;

class Value;
class DeclarationValue;

class TypeSpec: public std::vector<Type *> {
public:
    TypeSpec();
    TypeSpec(iterator tsi);
    unsigned measure();
    bool is_unsigned();
    StorageWhere where();
    Storage boolval(Storage s, X64 *x64);
    Value *convertible(TypeSpec &other, Value *orig);
    Storage convert(TypeSpec &other, Storage s, X64 *x64);
    TypeSpec prefix(Type *t);
    TypeSpec unprefix(Type *t);
    TypeSpec rvalue();
    TypeSpec lvalue();
    void store(Storage s, Storage t, X64 *x64);
    void create(Storage s, X64 *x64);
    void destroy(Storage s, X64 *x64);
};

typedef TypeSpec::iterator TypeSpecIter;
std::ostream &operator<<(std::ostream &os, const TypeSpec &ts);


Type *type_type = NULL;
Type *lvalue_type = NULL;
Type *void_type = NULL;
Type *function_type = NULL;
Type *code_type = NULL;
Type *boolean_type = NULL;
Type *integer_type = NULL;
Type *integer32_type = NULL;
Type *integer16_type = NULL;
Type *integer8_type = NULL;
Type *unsigned_integer_type = NULL;
Type *unsigned_integer32_type = NULL;
Type *unsigned_integer16_type = NULL;
Type *unsigned_integer8_type = NULL;
Type *character_type = NULL;
Type *reference_type = NULL;
Type *array_type = NULL;

TypeSpec BOGUS_TS;
TypeSpec VOID_TS;
TypeSpec BOOLEAN_TS;
TypeSpec INTEGER_TS;
TypeSpec INTEGER_LVALUE_TS;
TypeSpec BOOLEAN_LVALUE_TS;
TypeSpec UNSIGNED_INTEGER8_TS;
TypeSpec UNSIGNED_INTEGER8_ARRAY_REFERENCE_TS;
TypeSpec CHARACTER_TS;
TypeSpec CHARACTER_LVALUE_TS;
TypeSpec CHARACTER_ARRAY_REFERENCE_TS;

typedef std::vector<std::unique_ptr<Expr>> Args;
typedef std::map<std::string, std::unique_ptr<Expr>> Kwargs;

enum NumericOperation {
    TWEAK,
    COMPLEMENT, NEGATE,
    ADD, SUBTRACT, MULTIPLY, DIVIDE, MODULO, EXPONENT,
    OR, XOR, AND, SHIFT_LEFT, SHIFT_RIGHT, 
    EQUAL, NOT_EQUAL, LESS, GREATER, LESS_EQUAL, GREATER_EQUAL, INCOMPARABLE,
    ASSIGN, ASSIGN_ADD, ASSIGN_SUBTRACT, ASSIGN_MULTIPLY, ASSIGN_DIVIDE, ASSIGN_MODULO, ASSIGN_EXPONENT,
    ASSIGN_OR, ASSIGN_XOR, ASSIGN_AND, ASSIGN_SHIFT_LEFT, ASSIGN_SHIFT_RIGHT
};

bool is_unary(NumericOperation o) {
    return o == COMPLEMENT || o == NEGATE;
}

bool is_comparison(NumericOperation o) {
    return o >= EQUAL && o <= INCOMPARABLE;
}

bool is_assignment(NumericOperation o) {
    return o >= ASSIGN;
}

Value *typize(Expr *expr, Scope *scope);
Value *convertible(TypeSpec to, Value *orig);
TypeSpec get_typespec(Value *value);
Variable *variable_cast(Declaration *decl);
DeclarationValue *declaration_value_cast(Value *value);

Value *make_function_return_value(Variable *result_var, Value *v);
Value *make_variable_value(Variable *decl, Value *pivot);
Value *make_function_value(Function *decl, Value *pivot);
Value *make_type_value(TypeSpec ts);
Value *make_block_value();
Value *make_function_definition_value(TypeSpec fn_ts, Value *ret, Value *head, Value *body, FunctionScope *fn_scope);
Value *make_declaration_value(std::string name);
Value *make_number_value(std::string text);
Value *make_string_value(std::string text);
Value *make_integer_operation_value(NumericOperation operation, TypeSpec ts, Value *pivot);
Value *make_boolean_operation_value(NumericOperation operation, Value *pivot);
Value *make_boolean_and_value(Value *pivot);
Value *make_boolean_or_value(Value *pivot);
Value *make_boolean_if_value(Value *pivot);
Value *make_converted_value(TypeSpec to, Value *orig);
Value *make_code_value(Value *orig);
Value *make_array_item_value(TypeSpec t, Value *array);
Value *make_array_concatenation_value(TypeSpec t, Value *array, Value *other = NULL);
Value *make_array_realloc_value(TypeSpec t, Value *array);


#include "declarations/declaration.cpp"
#include "values/value.cpp"


struct {
    const char *name;
    NumericOperation operation;
} integer_rvalue_operations[] = {
    { "unary_minus", NEGATE },
    { "unary_tilde", COMPLEMENT },
    { "exponent", EXPONENT },
    { "shift_left", SHIFT_LEFT },
    { "shift_right", SHIFT_RIGHT },
    { "star", MULTIPLY },
    { "slash", DIVIDE },
    { "percent", MODULO },
    { "and", AND },
    { "plus", ADD },
    { "minus", SUBTRACT },
    { "or", OR },
    { "xor", XOR },
    { "equal", EQUAL },
    { "not_equal", NOT_EQUAL },
    { "less", LESS },
    { "greater", GREATER },
    { "less_equal", LESS_EQUAL },
    { "greater_equal", GREATER_EQUAL },
    { "incomparable", INCOMPARABLE },
    //{ "compare",  },
}, integer_lvalue_operations[] = {
    { "assign", ASSIGN },
    { "plus_assign", ASSIGN_ADD },
    { "minus_assign", ASSIGN_SUBTRACT },
    { "star_assign", ASSIGN_MULTIPLY },
    { "slash-assign", ASSIGN_DIVIDE },
    { "percent_assign", ASSIGN_MODULO },
    { "and_assign", ASSIGN_AND },
    { "or_assign", ASSIGN_OR },
    { "xor_assign", ASSIGN_XOR },
    { "shift_left_assign", ASSIGN_SHIFT_LEFT },
    { "shift_right_assign", ASSIGN_SHIFT_RIGHT }
};


Scope *init_types() {
    Scope *root_scope = new Scope();
    
    type_type = new SpecialType("<Type>", 1);
    root_scope->add(type_type);

    void_type = new SpecialType("<Void>", 0);
    root_scope->add(void_type);

    function_type = new AttributeType("<Function>");
    root_scope->add(function_type);

    lvalue_type = new AttributeType("<Lvalue>");
    root_scope->add(lvalue_type);

    code_type = new AttributeType("<Code>");
    root_scope->add(code_type);
    
    boolean_type = new BasicType("Boolean", 1);
    root_scope->add(boolean_type);

    character_type = new BasicType("Character", 2);
    root_scope->add(character_type);

    integer_type = new SignedIntegerType("Integer", 8);
    root_scope->add(integer_type);
    
    integer32_type = new SignedIntegerType("Integer32", 4);
    root_scope->add(integer32_type);
    
    integer16_type = new SignedIntegerType("Integer16", 2);
    root_scope->add(integer16_type);
    
    integer8_type = new SignedIntegerType("Integer8", 1);
    root_scope->add(integer8_type);

    unsigned_integer_type = new UnsignedIntegerType("Unsigned_Integer", 8);
    root_scope->add(unsigned_integer_type);
    
    unsigned_integer32_type = new UnsignedIntegerType("Unsigned_Integer32", 4);
    root_scope->add(unsigned_integer32_type);
    
    unsigned_integer16_type = new UnsignedIntegerType("Unsigned_Integer16", 2);
    root_scope->add(unsigned_integer16_type);
    
    unsigned_integer8_type = new UnsignedIntegerType("Unsigned_Integer8", 1);
    root_scope->add(unsigned_integer8_type);

    reference_type = new ReferenceType();
    root_scope->add(reference_type);
    
    array_type = new HeapType("Array", 1);
    root_scope->add(array_type);
    
    // BOGUS_TS will contain no Type pointers
    VOID_TS.push_back(void_type);
    BOOLEAN_TS.push_back(boolean_type);
    INTEGER_TS.push_back(integer_type);
    INTEGER_LVALUE_TS.push_back(lvalue_type);
    INTEGER_LVALUE_TS.push_back(integer_type);
    BOOLEAN_LVALUE_TS.push_back(lvalue_type);
    BOOLEAN_LVALUE_TS.push_back(boolean_type);
    UNSIGNED_INTEGER8_TS.push_back(unsigned_integer8_type);
    UNSIGNED_INTEGER8_ARRAY_REFERENCE_TS.push_back(reference_type);
    UNSIGNED_INTEGER8_ARRAY_REFERENCE_TS.push_back(array_type);
    UNSIGNED_INTEGER8_ARRAY_REFERENCE_TS.push_back(unsigned_integer8_type);
    CHARACTER_TS.push_back(character_type);
    CHARACTER_LVALUE_TS.push_back(lvalue_type);
    CHARACTER_LVALUE_TS.push_back(character_type);
    CHARACTER_ARRAY_REFERENCE_TS.push_back(reference_type);
    CHARACTER_ARRAY_REFERENCE_TS.push_back(array_type);
    CHARACTER_ARRAY_REFERENCE_TS.push_back(character_type);
    
    std::vector<TypeSpec> NO_TSS = { };
    std::vector<TypeSpec> INTEGER_TSS = { INTEGER_TS };
    std::vector<TypeSpec> BOOLEAN_TSS = { BOOLEAN_TS };
    std::vector<TypeSpec> UNSIGNED_INTEGER8_TSS = { UNSIGNED_INTEGER8_TS };
    std::vector<TypeSpec> UNSIGNED_INTEGER8_ARRAY_REFERENCE_TSS = { UNSIGNED_INTEGER8_ARRAY_REFERENCE_TS };
    std::vector<TypeSpec> CHARACTER_ARRAY_REFERENCE_TSS = { CHARACTER_ARRAY_REFERENCE_TS };
    
    std::vector<std::string> no_names = { };
    std::vector<std::string> value_names = { "value" };

    for (Type *t : {
        integer_type, integer32_type, integer16_type, integer8_type,
        unsigned_integer_type, unsigned_integer32_type, unsigned_integer16_type, unsigned_integer8_type,
    }) {
        TypeSpec ts;
        ts.push_back(t);
        
        for (auto &item : integer_rvalue_operations)
            root_scope->add(new IntegerOperation(item.name, ts, item.operation));

        ts.insert(ts.begin(), lvalue_type);
        
        for (auto &item : integer_lvalue_operations)
            root_scope->add(new IntegerOperation(item.name, ts, item.operation));
    }
    
    root_scope->add(new IntegerOperation("assign", CHARACTER_LVALUE_TS, ASSIGN));
    
    root_scope->add(new BooleanOperation("logical not", BOOLEAN_TS, COMPLEMENT));
    root_scope->add(new BooleanOperation("equal", BOOLEAN_TS, EQUAL));
    root_scope->add(new BooleanOperation("not_equal", BOOLEAN_TS, NOT_EQUAL));
    root_scope->add(new BooleanOperation("assign", BOOLEAN_LVALUE_TS, ASSIGN));
    root_scope->add(new BooleanOperation("logical and", VOID_TS, AND));
    root_scope->add(new BooleanOperation("logical or", VOID_TS, OR));
    
    //root_scope->add(new BooleanIf());
    
    root_scope->add(new ArrayIndexing(CHARACTER_ARRAY_REFERENCE_TS));
    root_scope->add(new ArrayConcatenation(CHARACTER_ARRAY_REFERENCE_TS));

    root_scope->add(new ImportedFunction("print", "print", VOID_TS, INTEGER_TSS, value_names, VOID_TS));
    root_scope->add(new ImportedFunction("printu8", "printu8", VOID_TS, UNSIGNED_INTEGER8_TSS, value_names, VOID_TS));
    root_scope->add(new ImportedFunction("printb", "printb", VOID_TS, UNSIGNED_INTEGER8_ARRAY_REFERENCE_TSS, value_names, VOID_TS));
    root_scope->add(new ImportedFunction("prints", "prints", VOID_TS, CHARACTER_ARRAY_REFERENCE_TSS, value_names, VOID_TS));
    root_scope->add(new ImportedFunction("decode_utf8", "decode_utf8", UNSIGNED_INTEGER8_ARRAY_REFERENCE_TS, NO_TSS, no_names, CHARACTER_ARRAY_REFERENCE_TS));
    root_scope->add(new ImportedFunction("encode_utf8", "encode_utf8", CHARACTER_ARRAY_REFERENCE_TS, NO_TSS, no_names, UNSIGNED_INTEGER8_ARRAY_REFERENCE_TS));

    root_scope->add(new ImportedFunction("stringify_integer", "stringify", INTEGER_TS, NO_TSS, no_names, CHARACTER_ARRAY_REFERENCE_TS));

    return root_scope;
}


Value *lookup(std::string name, Value *pivot, Args &args, Kwargs &kwargs, Token &token, Scope *scope) {
    TypeSpec pts = pivot ? pivot->ts : VOID_TS;
    std::cerr << "Looking up " << pts << " " << name << " definition.\n";
    
    for (Scope *s = scope; s; s = s->outer_scope) {
        //std::cerr << "Trying a scope...\n";
        Value *value = s->lookup(name, pivot);
        
        if (value) {
            std::cerr << "Found      " << pts << " " << name << " returning " << value->ts << ".\n";
            bool ok = value->check(args, kwargs, scope);
        
            if (!ok) {
                std::cerr << "Argument problem for " << token << "!\n";
                throw TYPE_ERROR;
            }

            std::cerr << "Accepted   " << pts << " " << name << " arguments.\n";
            return value;
        }
    }
    
    std::cerr << "No match for " << pts << " " << name << " at " << token << "!\n";
    throw TYPE_ERROR;
}


Value *typize(Expr *expr, Scope *scope) {
    Value *value = NULL;
    Marker marker = scope->mark();
    
    if (expr->type == Expr::TUPLE) {
        if (expr->pivot) {
            std::cerr << "A TUPLE had a pivot argument!\n";
            throw INTERNAL_ERROR;
        }

        value = make_block_value();
        value->check(expr->args, expr->kwargs, scope);
    }
    else if (expr->type == Expr::CONTROL) {
        if (expr->text == "function") {
            FunctionScope *fn_scope = new FunctionScope();
            scope->add(fn_scope);
            
            Expr *r = expr->pivot.get();
            Scope *rs = fn_scope->add_result_scope();
            Value *ret = r ? typize(r, rs) : NULL;

            TypeSpec fn_ts;

            if (ret) {
                if (ret->ts[0] != type_type) {
                    std::cerr << "Function return expression is not a type!\n";
                    throw TYPE_ERROR;
                }

                // Add internal result variable
                TypeSpec var_ts = ret->ts;
                var_ts[0] = lvalue_type;
                Variable *decl = new Variable("<result>", VOID_TS, var_ts);
                rs->add(decl);
                    
                fn_ts = ret->ts.unprefix(type_type).prefix(function_type);
            }
            else {
                fn_ts.push_back(function_type);
                fn_ts.push_back(void_type);
            }

            std::cerr << "Function ts " << fn_ts << "\n";
        
            Expr *h = expr->kwargs["from"].get();
            Scope *hs = fn_scope->add_head_scope();
            Value *head = h ? typize(h, hs) : NULL;
            
            Expr *b = expr->kwargs["as"].get();
            Scope *bs = fn_scope->add_body_scope();
            Value *body = b ? typize(b, bs) : NULL;
            
            value = make_function_definition_value(fn_ts, ret, head, body, fn_scope);
        }
        else if (expr->text == "return") {
            Expr *r = expr->pivot.get();
            Value *result = r ? typize(r, scope) : NULL;  // TODO: statement scope? Or already have?

            FunctionScope *fn_scope = scope->get_function_scope();
            if (!fn_scope) {
                std::cerr << "A :return control outside of a function!\n";
                throw TYPE_ERROR;
            }
            
            Variable *result_var = fn_scope->get_result_variable();
            if (!result_var) {
                std::cerr << "A :return control with value in a void function!\n";
                throw TYPE_ERROR;
            }
            
            TypeSpec result_ts = result_var->var_ts.rvalue();
            Value *cr = convertible(result_ts, result);
            if (!cr) {
                std::cerr << "A :return control with incompatible value!\n";
                std::cerr << "Type " << get_typespec(result) << " is not " << result_ts << "!\n";
                throw TYPE_ERROR;
            }
            
            value = make_function_return_value(result_var, cr);
        }
        else if (expr->text == "if") {
            Expr *e = expr->pivot.get();
            Value *condition = e ? convertible(BOOLEAN_TS, typize(e, scope)) : NULL;
            
            if (!condition) {
                std::cerr << "Not a boolean condition!\n";
                throw TYPE_ERROR;
            }
            
            value = make_boolean_if_value(condition);

            bool ok = value->check(expr->args, expr->kwargs, scope);
                
            if (!ok) {
                std::cerr << "Argument problem for " << expr->token << "!\n";
                throw TYPE_ERROR;
            }
        }
        else {
            std::cerr << "Unknown control " << expr->token << "!\n";
            throw TYPE_ERROR;
        }
    }
    else if (expr->type == Expr::DECLARATION) {
        std::string name = expr->text;
        std::cerr << "Declaring " << name << ".\n";
        
        value = make_declaration_value(name);
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
        value = lookup(name, p, expr->args, expr->kwargs, expr->token, scope);
    }
    else if (expr->type == Expr::INITIALIZER) {
        Value *p = expr->pivot ? typize(expr->pivot.get(), scope) : NULL;
        StringValue *s = dynamic_cast<StringValue *>(p);
        
        if (s) {
            std::vector<std::string> fragments = brace_split(s->text);
            Value *root = NULL;
            bool identifier = false;
            Args no_args;
            Kwargs no_kwargs;
            Token no_token = s->token;
            
            for (auto &fragment : fragments) {
                Value *next;
                
                if (identifier) {
                    Value *pivot = lookup(fragment, NULL, no_args, no_kwargs, no_token, scope);
                    next = lookup("stringify", pivot, no_args, no_kwargs, no_token, scope);
                    //next = make_string_value("<" + fragment + ">");
                }
                else {
                    next = make_string_value(fragment);
                }
                
                if (root)
                    root = make_array_concatenation_value(CHARACTER_ARRAY_REFERENCE_TS, root, next);
                else
                    root = next;
                
                identifier = !identifier;
            }
            
            value = root;
        }
        else {
            std::cerr << "Can't process this initialization yet!\n";
            throw TYPE_ERROR;
        }
    }
    else if (expr->type == Expr::NUMBER) {
        value = make_number_value(expr->text);
    }
    else if (expr->type == Expr::STRING) {
        value = make_string_value(expr->text);
    }
    else {
        std::cerr << "Can't typize this now: " << expr->token << "!\n";
        throw INTERNAL_ERROR;
    }
    
    value->set_token(expr->token);
    value->set_marker(marker);
    
    return value;
}
