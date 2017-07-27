
// Stage 4


class Declaration;
class Type;
class Scope;
class FunctionScope;
class Variable;
class Function;

Variable *variable_cast(Declaration *decl);

Type *any_type = NULL;
Type *type_type = NULL;
Type *lvalue_type = NULL;
Type *function_type = NULL;
Type *code_type = NULL;
Type *void_type = NULL;
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


class TypeSpec: public std::vector<Type *> {
public:
    TypeSpec();
    TypeSpec(std::initializer_list<Type *> il):std::vector<Type *>(il) {}
    
    unsigned measure();
    bool is_unsigned();
    StorageWhere where();
    Storage boolval(Storage s, X64 *x64, bool probe);
    TypeSpec prefix(Type *t);
    TypeSpec unprefix(Type *t);
    TypeSpec rvalue();
    TypeSpec lvalue();
    void store(Storage s, Storage t, X64 *x64);
    void create(Storage s, X64 *x64);
    void destroy(Storage s, X64 *x64);
};

typedef TypeSpec::iterator TypeSpecIter;
typedef std::vector<TypeSpec> TypeMatch;
std::ostream &operator<<(std::ostream &os, const TypeSpec &ts);

TypeSpec BOGUS_TS;
TypeSpec VOID_TS;
TypeSpec ANY_TS;
TypeSpec ANY_TYPE_TS;
TypeSpec BOOLEAN_TS;
TypeSpec INTEGER_TS;
TypeSpec INTEGER_LVALUE_TS;
TypeSpec BOOLEAN_LVALUE_TS;
TypeSpec UNSIGNED_INTEGER8_TS;
TypeSpec UNSIGNED_INTEGER8_ARRAY_REFERENCE_TS;
TypeSpec CHARACTER_TS;
TypeSpec CHARACTER_LVALUE_TS;
TypeSpec CHARACTER_ARRAY_REFERENCE_TS;
TypeSpec ANY_REFERENCE_TS;
TypeSpec ANY_REFERENCE_LVALUE_TS;
TypeSpec ANY_ARRAY_REFERENCE_TS;
TypeSpec VOID_CODE_TS;
TypeSpec VOID_FUNCTION_TS;


class Value;
class DeclarationValue;

Value *typize(Expr *expr, Scope *scope);
TypeSpec get_typespec(Value *value);
DeclarationValue *declaration_value_cast(Value *value);
bool typematch(TypeSpec tt, Value *&v, TypeMatch &match);

Value *make_variable_value(Variable *decl, Value *pivot);
Value *make_function_value(Function *decl, Value *pivot);
Value *make_type_value(TypeSpec ts);
Value *make_function_definition_value(TypeSpec fn_ts, Value *ret, Value *head, Value *body, FunctionScope *fn_scope);
Value *make_declaration_value(std::string name);
Value *make_number_value(std::string text);
Value *make_string_value(std::string text);
Value *make_code_value(Value *orig);
Value *make_void_conversion_value(Value *orig);
Value *make_boolean_conversion_value(Value *orig);
Value *make_boolean_not_value(Value *value);


#include "declarations/declaration.cpp"
#include "values/value.cpp"


struct {
    const char *name;
    OperationType operation;
} integer_rvalue_operations[] = {
    { "unary_minus", NEGATE },
    { "unary_tilde", COMPLEMENT },
    { "binary_exponent", EXPONENT },
    { "binary_shift_left", SHIFT_LEFT },
    { "binary_shift_right", SHIFT_RIGHT },
    { "binary_star", MULTIPLY },
    { "binary_slash", DIVIDE },
    { "binary_percent", MODULO },
    { "binary_and", AND },
    { "binary_plus", ADD },
    { "binary_minus", SUBTRACT },
    { "binary_or", OR },
    { "binary_xor", XOR },
    { "is_equal", EQUAL },
    { "not_equal", NOT_EQUAL },
    { "is_less", LESS },
    { "is_greater", GREATER },
    { "not_greater", LESS_EQUAL },
    { "not_less", GREATER_EQUAL },
    //{ "incomparable", INCOMPARABLE },
    //{ "compare",  },
}, integer_lvalue_operations[] = {
    { "assign", ASSIGN },
    { "assign_plus", ASSIGN_ADD },
    { "assign_minus", ASSIGN_SUBTRACT },
    { "assign_star", ASSIGN_MULTIPLY },
    { "assign_slash", ASSIGN_DIVIDE },
    { "assign_percent", ASSIGN_MODULO },
    { "assign_and", ASSIGN_AND },
    { "assign_or", ASSIGN_OR },
    { "assign_xor", ASSIGN_XOR },
    { "assign_shift_left", ASSIGN_SHIFT_LEFT },
    { "assign_shift_right", ASSIGN_SHIFT_RIGHT }
};


Scope *init_builtins() {
    Scope *root_scope = new Scope();

    any_type = new SpecialType("<Any>", 0);
    root_scope->add(any_type);
    
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
    ANY_TS = { any_type };
    ANY_TYPE_TS = { type_type, any_type };
    VOID_TS = { void_type };
    BOOLEAN_TS = { boolean_type };
    INTEGER_TS = { integer_type };
    INTEGER_LVALUE_TS = { lvalue_type, integer_type };
    BOOLEAN_LVALUE_TS = { lvalue_type, boolean_type };
    UNSIGNED_INTEGER8_TS = { unsigned_integer8_type };
    UNSIGNED_INTEGER8_ARRAY_REFERENCE_TS = { reference_type, array_type, unsigned_integer8_type };
    CHARACTER_TS = { character_type };
    CHARACTER_LVALUE_TS = { lvalue_type, character_type };
    CHARACTER_ARRAY_REFERENCE_TS = { reference_type, array_type, character_type };
    ANY_REFERENCE_TS = { reference_type, any_type };
    ANY_REFERENCE_LVALUE_TS = { lvalue_type, reference_type, any_type };
    ANY_ARRAY_REFERENCE_TS = { reference_type, array_type, any_type };
    VOID_CODE_TS = { code_type, void_type };
    VOID_FUNCTION_TS = { function_type, void_type };
    
    std::vector<TypeSpec> NO_TSS = { };
    std::vector<TypeSpec> INTEGER_TSS = { INTEGER_TS };
    std::vector<TypeSpec> BOOLEAN_TSS = { BOOLEAN_TS };
    std::vector<TypeSpec> UNSIGNED_INTEGER8_TSS = { UNSIGNED_INTEGER8_TS };
    std::vector<TypeSpec> UNSIGNED_INTEGER8_ARRAY_REFERENCE_TSS = { UNSIGNED_INTEGER8_ARRAY_REFERENCE_TS };
    std::vector<TypeSpec> CHARACTER_ARRAY_REFERENCE_TSS = { CHARACTER_ARRAY_REFERENCE_TS };
    
    std::vector<std::string> no_names = { };
    std::vector<std::string> value_names = { "value" };

    // Integer operations
    for (Type *t : {
        integer_type, integer32_type, integer16_type, integer8_type,
        unsigned_integer_type, unsigned_integer32_type, unsigned_integer16_type, unsigned_integer8_type,
    }) {
        TypeSpec ts = { t };
        
        for (auto &item : integer_rvalue_operations)
            root_scope->add(new TemplateOperation<IntegerOperationValue>(item.name, ts, item.operation));

        TypeSpec lts = { lvalue_type, t };
        
        for (auto &item : integer_lvalue_operations)
            root_scope->add(new TemplateOperation<IntegerOperationValue>(item.name, lts, item.operation));
    }
    
    // Character operations
    root_scope->add(new TemplateOperation<IntegerOperationValue>("assign", CHARACTER_LVALUE_TS, ASSIGN));
    
    // Boolean operations
    typedef TemplateOperation<BooleanOperationValue> BooleanOperation;
    root_scope->add(new BooleanOperation("logical not", BOOLEAN_TS, COMPLEMENT));
    root_scope->add(new BooleanOperation("is_equal", BOOLEAN_TS, EQUAL));
    root_scope->add(new BooleanOperation("not_equal", BOOLEAN_TS, NOT_EQUAL));
    root_scope->add(new BooleanOperation("assign", BOOLEAN_LVALUE_TS, ASSIGN));
    root_scope->add(new TemplateOperation<BooleanAndValue>("logical and", BOOLEAN_TS, AND));
    root_scope->add(new TemplateOperation<BooleanOrValue>("logical or", ANY_TS, OR));

    // Reference operations
    typedef TemplateOperation<ReferenceOperationValue> ReferenceOperation;
    root_scope->add(new ReferenceOperation("assign", ANY_REFERENCE_LVALUE_TS, ASSIGN));
    root_scope->add(new ReferenceOperation("is_equal", ANY_REFERENCE_TS, EQUAL));
    root_scope->add(new ReferenceOperation("not_equal", ANY_REFERENCE_TS, NOT_EQUAL));

    // Array operations
    root_scope->add(new TemplateOperation<ArrayReallocValue>("realloc", ANY_ARRAY_REFERENCE_TS, TWEAK));
    root_scope->add(new TemplateOperation<ArrayConcatenationValue>("binary_plus", ANY_ARRAY_REFERENCE_TS, TWEAK));
    root_scope->add(new TemplateOperation<ArrayItemValue>("index", ANY_ARRAY_REFERENCE_TS, TWEAK));
    
    // Builtin controls
    root_scope->add(new TemplateOperation<BooleanIfValue>(":if", BOOLEAN_TS, TWEAK));
    root_scope->add(new TemplateOperation<FunctionReturnValue>(":return", VOID_TS, TWEAK));
    root_scope->add(new TemplateOperation<FunctionReturnValue>(":return", ANY_TS, TWEAK));
    root_scope->add(new TemplateOperation<FunctionDefinitionValue>(":function", VOID_TS, TWEAK));
    root_scope->add(new TemplateOperation<FunctionDefinitionValue>(":function", ANY_TYPE_TS, TWEAK));
    
    // Library functions
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
            // TODO: we should print the definition pivot type, not the value type
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
    
    if (name == "is equal") {
        Value *value = lookup("equality", pivot, args, kwargs, token, scope);
        return value;
    }
    else if (name == "not equal") {
        Value *value = lookup("equality", pivot, args, kwargs, token, scope);
        return value ? make_boolean_not_value(value) : NULL;
    }
    
    std::cerr << "No match for " << pts << " " << name << " at " << token << "!\n";
    return NULL;
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
        std::string name = ":" + expr->text;
        Value *p = expr->pivot ? typize(expr->pivot.get(), scope) : NULL;
        value = lookup(name, p, expr->args, expr->kwargs, expr->token, scope);

        if (!value)
            throw TYPE_ERROR;
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
        
        if (!value)
            throw TYPE_ERROR;
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
                
                if (root) {
                    // Ugly hacks follow
                    TypeMatch match;
                    match.push_back(CHARACTER_ARRAY_REFERENCE_TS);
                    ArrayConcatenationValue *acv = new ArrayConcatenationValue(TWEAK, root, match);
                    acv->right.reset(next);
                    root = acv;
                }
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
