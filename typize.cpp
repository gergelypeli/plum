
const long NO_EXCEPTION = 0;
const long RETURN_EXCEPTION = -1;
const long BREAK_EXCEPTION = -2;
const long CONTINUE_EXCEPTION = -3;

// Stage 4

class Value;

class Declaration;
class Type;
class HeapType;
class Scope;
class CodeScope;
class SwitchScope;
class TryScope;
class EvalScope;
class FunctionScope;
class Variable;
class Function;

Variable *variable_cast(Declaration *decl);
HeapType *heap_type_cast(Type *t);

Type *any_type = NULL;
Type *same_type = NULL;
Type *type_type = NULL;
Type *ovalue_type = NULL;
Type *lvalue_type = NULL;
Type *code_type = NULL;
Type *metatype_type = NULL;
Type *void_type = NULL;
Type *multi_type = NULL;
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
Type *enumeration_metatype = NULL;
Type *treenumeration_metatype = NULL;
Type *integer_metatype = NULL;
Type *record_metatype = NULL;


class TypeSpec: public std::vector<Type *> {
public:
    TypeSpec();
    TypeSpec(iterator tsi);
    TypeSpec(std::initializer_list<Type *> il):std::vector<Type *>(il) {}
    
    unsigned measure(StorageWhere where);
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
    Value *lookup_initializer(std::string name, Scope *scope);
};

typedef TypeSpec::iterator TypeSpecIter;
typedef std::vector<TypeSpec> TypeMatch;
std::ostream &operator<<(std::ostream &os, const TypeSpec &ts);

TypeSpec BOGUS_TS;
TypeSpec VOID_TS;
TypeSpec MULTI_TS;
TypeSpec MULTI_LVALUE_TS;
TypeSpec ANY_TS;
TypeSpec ANY_TYPE_TS;
TypeSpec ANY_OVALUE_TS;
TypeSpec ANY_LVALUE_TS;
TypeSpec METATYPE_TS;
TypeSpec TREENUMMETA_TS;
TypeSpec BOOLEAN_TS;
TypeSpec INTEGER_TS;
TypeSpec INTEGER_LVALUE_TS;
TypeSpec INTEGER_OVALUE_TS;
TypeSpec BOOLEAN_LVALUE_TS;
TypeSpec UNSIGNED_INTEGER8_TS;
TypeSpec UNSIGNED_INTEGER8_ARRAY_REFERENCE_TS;
TypeSpec CHARACTER_TS;
TypeSpec CHARACTER_LVALUE_TS;
TypeSpec CHARACTER_ARRAY_REFERENCE_TS;
TypeSpec CHARACTER_ARRAY_REFERENCE_LVALUE_TS;
TypeSpec ANY_REFERENCE_TS;
TypeSpec ANY_REFERENCE_LVALUE_TS;
TypeSpec ANY_ARRAY_REFERENCE_TS;
TypeSpec VOID_CODE_TS;
TypeSpec BOOLEAN_CODE_TS;


class DeclarationValue;

Value *typize(Expr *expr, Scope *scope, TypeSpec *context = NULL);
Value *lookup(std::string name, Value *pivot, Expr *expr, Scope *scope, TypeSpec *context = NULL);

TypeSpec get_typespec(Value *value);
DeclarationValue *declaration_value_cast(Value *value);
std::string declaration_get_name(DeclarationValue *dv);
Declaration *declaration_get_decl(DeclarationValue *dv);
Variable *declaration_get_var(DeclarationValue *dv);
bool typematch(TypeSpec tt, Value *&v, TypeMatch &match);
bool unpack_value(Value *v, std::vector<TypeSpec> &tss);

Value *make_variable_value(Variable *decl, Value *pivot);
Value *make_function_call_value(Function *decl, Value *pivot);
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
Value *make_boolean_not_value(Value *value);
Value *make_null_reference_value(TypeSpec ts);
Value *make_unicode_character_value();
Value *make_integer_definition_value();
Value *make_enumeration_definition_value();
Value *make_treenumeration_definition_value();
Value *make_record_definition_value();
Value *make_record_initializer_value(Variable *var);

DeclarationValue *make_declaration_by_value(std::string name, Value *v, Scope *scope);
Value *make_declaration_by_type(std::string name, TypeSpec ts, Scope *scope);


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
    { "assign other", ASSIGN },
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

    same_type = new SpecialType("<Same>", 0);
    root_scope->add(same_type);

    integer_metatype = new IntegerMetaType(":Integer");
    root_scope->add(integer_metatype);

    enumeration_metatype = new EnumerationMetaType(":Enumeration");
    root_scope->add(enumeration_metatype);

    treenumeration_metatype = new TreenumerationMetaType(":Treenumeration");
    root_scope->add(treenumeration_metatype);

    record_metatype = new RecordMetaType(":Record");
    root_scope->add(record_metatype);
    
    type_type = new SpecialType("<Type>", 1);
    root_scope->add(type_type);

    void_type = new SpecialType("<Void>", 0);
    root_scope->add(void_type);

    metatype_type = new SpecialType("<Metatype>", 0);
    root_scope->add(metatype_type);

    multi_type = new SpecialType("<Multi>", 0);
    root_scope->add(multi_type);

    lvalue_type = new AttributeType("<Lvalue>");
    root_scope->add(lvalue_type);
    
    ovalue_type = new AttributeType("Ovalue");
    root_scope->add(ovalue_type);

    code_type = new AttributeType("<Code>");
    root_scope->add(code_type);
    
    boolean_type = new BooleanType("Boolean", 1);
    root_scope->add(boolean_type);

    character_type = new CharacterType("Character", 2);
    root_scope->add(character_type);

    integer_type = new IntegerType("Integer", 8, false);
    root_scope->add(integer_type);
    
    integer32_type = new IntegerType("Integer32", 4, false);
    root_scope->add(integer32_type);
    
    integer16_type = new IntegerType("Integer16", 2, false);
    root_scope->add(integer16_type);
    
    integer8_type = new IntegerType("Integer8", 1, false);
    root_scope->add(integer8_type);

    unsigned_integer_type = new IntegerType("Unteger", 8, true);
    root_scope->add(unsigned_integer_type);
    
    unsigned_integer32_type = new IntegerType("Unteger32", 4, true);
    root_scope->add(unsigned_integer32_type);
    
    unsigned_integer16_type = new IntegerType("Unteger16", 2, true);
    root_scope->add(unsigned_integer16_type);
    
    unsigned_integer8_type = new IntegerType("Unteger8", 1, true);
    root_scope->add(unsigned_integer8_type);

    reference_type = new ReferenceType("Reference");
    root_scope->add(reference_type);
    
    array_type = new HeapType("Array", 1);
    root_scope->add(array_type);

    // BOGUS_TS will contain no Type pointers
    ANY_TS = { any_type };
    ANY_TYPE_TS = { type_type, any_type };
    ANY_LVALUE_TS = { lvalue_type, any_type };
    ANY_OVALUE_TS = { ovalue_type, any_type };
    METATYPE_TS = { metatype_type };
    TREENUMMETA_TS = { treenumeration_metatype };
    VOID_TS = { void_type };
    MULTI_TS = { multi_type };
    MULTI_LVALUE_TS = { lvalue_type, multi_type };
    BOOLEAN_TS = { boolean_type };
    INTEGER_TS = { integer_type };
    INTEGER_LVALUE_TS = { lvalue_type, integer_type };
    INTEGER_OVALUE_TS = { ovalue_type, integer_type };
    BOOLEAN_LVALUE_TS = { lvalue_type, boolean_type };
    UNSIGNED_INTEGER8_TS = { unsigned_integer8_type };
    UNSIGNED_INTEGER8_ARRAY_REFERENCE_TS = { reference_type, array_type, unsigned_integer8_type };
    CHARACTER_TS = { character_type };
    CHARACTER_LVALUE_TS = { lvalue_type, character_type };
    CHARACTER_ARRAY_REFERENCE_TS = { reference_type, array_type, character_type };
    CHARACTER_ARRAY_REFERENCE_LVALUE_TS = { lvalue_type, reference_type, array_type, character_type };
    ANY_REFERENCE_TS = { reference_type, any_type };
    ANY_REFERENCE_LVALUE_TS = { lvalue_type, reference_type, any_type };
    ANY_ARRAY_REFERENCE_TS = { reference_type, array_type, any_type };
    VOID_CODE_TS = { code_type, void_type };
    BOOLEAN_CODE_TS = { code_type, boolean_type };

    typedef std::vector<TypeSpec> TSs;
    TSs NO_TSS = { };
    TSs INTEGER_TSS = { INTEGER_TS };
    TSs BOOLEAN_TSS = { BOOLEAN_TS };
    TSs UNSIGNED_INTEGER8_TSS = { UNSIGNED_INTEGER8_TS };
    TSs UNSIGNED_INTEGER8_ARRAY_REFERENCE_TSS = { UNSIGNED_INTEGER8_ARRAY_REFERENCE_TS };
    TSs CHARACTER_ARRAY_REFERENCE_TSS = { CHARACTER_ARRAY_REFERENCE_TS };

    typedef std::vector<std::string> Ss;
    Ss no_names = { };
    Ss value_names = { "value" };

    // Integer operations
    Scope *integer_scope = integer_metatype->get_inner_scope();
    
    for (auto &item : integer_rvalue_operations)
        integer_scope->add(new TemplateOperation<IntegerOperationValue>(item.name, ANY_TS, item.operation));

    for (auto &item : integer_lvalue_operations)
        integer_scope->add(new TemplateOperation<IntegerOperationValue>(item.name, ANY_LVALUE_TS, item.operation));

    integer_scope->add(new TemplateOperation<IntegerOperationValue>("cover", ANY_TS, EQUAL));
    
    // Character operations
    Scope *char_scope = character_type->get_inner_scope();
    char_scope->add(new TemplateOperation<IntegerOperationValue>("assign other", CHARACTER_LVALUE_TS, ASSIGN));
    char_scope->add(new TemplateIdentifier<CharacterStreamificationValue>("streamify", CHARACTER_TS));
    
    // Boolean operations
    Scope *bool_scope = boolean_type->get_inner_scope();
    bool_scope->add(new TemplateOperation<BooleanOperationValue>("assign other", BOOLEAN_LVALUE_TS, ASSIGN));

    // Logical operations, unscoped
    root_scope->add(new TemplateIdentifier<BooleanNotValue>("logical not", ANY_TS));
    root_scope->add(new TemplateIdentifier<BooleanAndValue>("logical and", BOOLEAN_TS));
    root_scope->add(new TemplateIdentifier<BooleanOrValue>("logical or", ANY_TS));

    // Enum operations
    Scope *enum_scope = enumeration_metatype->get_inner_scope();
    enum_scope->add(new TemplateOperation<IntegerOperationValue>("assign other", ANY_LVALUE_TS, ASSIGN));
    enum_scope->add(new TemplateOperation<IntegerOperationValue>("is_equal", ANY_TS, EQUAL));
    enum_scope->add(new TemplateIdentifier<EnumStreamificationValue>("streamify", ANY_TS));
    enum_scope->add(new TemplateOperation<IntegerOperationValue>("cover", ANY_TS, EQUAL));

    // Treenum operations
    Scope *treenum_scope = treenumeration_metatype->get_inner_scope();
    treenum_scope->add(new TemplateOperation<IntegerOperationValue>("assign other", ANY_LVALUE_TS, ASSIGN));
    treenum_scope->add(new TemplateOperation<IntegerOperationValue>("is_equal", ANY_TS, EQUAL));
    treenum_scope->add(new TemplateIdentifier<EnumStreamificationValue>("streamify", ANY_TS));
    treenum_scope->add(new TemplateOperation<TreenumCoveringValue>("cover", ANY_TS, TWEAK));

    // Record operations
    Scope *record_scope = record_metatype->get_inner_scope();
    record_scope->add(new TemplateOperation<RecordOperationValue>("assign other", ANY_LVALUE_TS, ASSIGN));

    // Reference operations, unscoped
    typedef TemplateOperation<ReferenceOperationValue> ReferenceOperation;
    root_scope->add(new ReferenceOperation("assign other", ANY_REFERENCE_LVALUE_TS, ASSIGN));
    root_scope->add(new ReferenceOperation("is_equal", ANY_REFERENCE_TS, EQUAL));
    root_scope->add(new ReferenceOperation("not_equal", ANY_REFERENCE_TS, NOT_EQUAL));

    // Array operations
    Scope *array_scope = array_type->get_inner_scope();
    array_scope->add(new TemplateIdentifier<ArrayLengthValue>("length", ANY_ARRAY_REFERENCE_TS));
    array_scope->add(new TemplateOperation<ArrayReallocValue>("realloc", ANY_ARRAY_REFERENCE_TS, TWEAK));
    array_scope->add(new TemplateIdentifier<ArrayConcatenationValue>("binary_plus", ANY_ARRAY_REFERENCE_TS));
    array_scope->add(new TemplateOperation<ArrayItemValue>("index", ANY_ARRAY_REFERENCE_TS, TWEAK));
    array_scope->add(new TemplateIdentifier<StringStreamificationValue>("streamify", CHARACTER_ARRAY_REFERENCE_TS));
    
    // Unpacking
    root_scope->add(new TemplateIdentifier<UnpackingValue>("assign other", MULTI_LVALUE_TS));
    
    // Builtin controls, unscoped
    root_scope->add(new TemplateOperation<BooleanIfValue>(":if", VOID_TS, TWEAK));
    root_scope->add(new TemplateIdentifier<RepeatValue>(":repeat", VOID_TS));
    root_scope->add(new TemplateIdentifier<ForEachValue>(":for", VOID_TS));
    root_scope->add(new TemplateIdentifier<SwitchValue>(":switch", VOID_TS));
    root_scope->add(new TemplateIdentifier<WhenValue>(":when", VOID_TS));
    root_scope->add(new TemplateIdentifier<RaiseValue>(":raise", VOID_TS));
    root_scope->add(new TemplateIdentifier<TryValue>(":try", VOID_TS));
    //root_scope->add(new TemplateIdentifier<EvalValue>(":eval", VOID_TS));
    //root_scope->add(new TemplateIdentifier<YieldValue>(":yield", VOID_TS));
    root_scope->add(new TemplateOperation<FunctionReturnValue>(":return", VOID_TS, TWEAK));
    root_scope->add(new TemplateOperation<FunctionDefinitionValue>(":Function", VOID_TS, TWEAK));
    
    // Library functions, unscoped
    root_scope->add(new ImportedFunction("print", "print", VOID_TS, INTEGER_TSS, value_names, NO_TSS, NULL));
    root_scope->add(new ImportedFunction("printu8", "printu8", VOID_TS, UNSIGNED_INTEGER8_TSS, value_names, NO_TSS, NULL));
    root_scope->add(new ImportedFunction("printb", "printb", VOID_TS, UNSIGNED_INTEGER8_ARRAY_REFERENCE_TSS, value_names, NO_TSS, NULL));
    root_scope->add(new ImportedFunction("prints", "prints", VOID_TS, CHARACTER_ARRAY_REFERENCE_TSS, value_names, NO_TSS, NULL));
    root_scope->add(new ImportedFunction("decode_utf8", "decode_utf8", UNSIGNED_INTEGER8_ARRAY_REFERENCE_TS, NO_TSS, no_names, TSs { CHARACTER_ARRAY_REFERENCE_TS }, NULL));
    root_scope->add(new ImportedFunction("encode_utf8", "encode_utf8", CHARACTER_ARRAY_REFERENCE_TS, NO_TSS, no_names, TSs { UNSIGNED_INTEGER8_ARRAY_REFERENCE_TS }, NULL));

    root_scope->add(new ImportedFunction("stringify_integer", "stringify", INTEGER_TS, NO_TSS, no_names, TSs { CHARACTER_ARRAY_REFERENCE_TS }, NULL));
    root_scope->add(new ImportedFunction("streamify_integer", "streamify", INTEGER_TS, TSs { CHARACTER_ARRAY_REFERENCE_LVALUE_TS }, Ss { "stream" }, NO_TSS, NULL));
    //root_scope->add(new ImportedFunction("streamify_string", "streamify", CHARACTER_ARRAY_REFERENCE_TS, TSs { CHARACTER_ARRAY_REFERENCE_LVALUE_TS }, Ss { "stream" }, VOID_TS));

    return root_scope;
}


Value *lookup_scope(Scope *s, std::string name, Value *pivot, Expr *expr, Scope *scope, TypeSpec *context) {
    //std::cerr << "Trying a scope...\n";
    TypeMatch match;
    Value *value = s->lookup(name, pivot, match);
    
    if (!value)
        return NULL;
    
    // TODO: we should print the definition pivot type, not the value type
    std::cerr << "Found       " << match[0] << " " << name << " returning " << value->ts << ".\n";
    value->set_token(expr->token);
    value->set_context_ts(context);
    
    bool ok = value->check(expr->args, expr->kwargs, scope);

    if (!ok) {
        std::cerr << "Argument problem for " << expr->token << "!\n";
        throw TYPE_ERROR;
    }

    //std::cerr << "Accepted    " << match[0] << " " << name << " arguments.\n";
    return value;
}


Value *lookup(std::string name, Value *pivot, Expr *expr, Scope *scope, TypeSpec *context) {
    TypeSpec pts = pivot ? pivot->ts : VOID_TS;
    //std::cerr << "Looking up  " << pts << " " << name << " definition.\n";
    
    for (Scope *s = scope; s; s = s->outer_scope) {
        Value *value = lookup_scope(s, name, pivot, expr, scope, context);
        
        if (value)
            return value;
    }

    unsigned i = 0;
    if (pts[i] == lvalue_type || pts[i] == ovalue_type || pts[i] == code_type)
        i++;
    if (pts[i] == reference_type)
        i++;
        
    Scope *inner_scope = pts[i]->get_inner_scope();
    
    if (inner_scope) {
        Value *value = lookup_scope(inner_scope, name, pivot, expr, scope, context);
        
        if (value)
            return value;
    }
    
    if (name == "not_equal") {
        Value *value = lookup("is_equal", pivot, expr, scope, context);
        return value ? make_boolean_not_value(value) : NULL;
    }
    
    std::cerr << "No match for " << pts << " " << name << " at " << expr->token << "!\n";
    return NULL;
}


Value *interpolate(std::string text, Expr *expr, Scope *scope) {
    std::vector<std::string> fragments = brace_split(text);
    
    if (expr->args.size() > 0) {
        std::cerr << "String interpolation must use keyword arguments only!\n";
        throw TYPE_ERROR;
    }

    // We must scope ourselves
    //CodeScope *s = new CodeScope;
    //scope->add(s);
    //scope = s;
    
    Marker marker = scope->mark();
    CodeBlockValue *block = new CodeBlockValue(NULL);
    block->set_marker(marker);
    
    DeclarationValue *dv = make_declaration_by_value("<interpolated>", new StringBufferValue(100), scope);
    //DeclarationValue *dv = new DeclarationValue("<result>");
    //Value *initial_value = new StringBufferValue(100);
    //dv->use(initial_value, scope);
    Variable *v = dv->get_var();
    block->add_statement(dv);

    for (auto &kv : expr->kwargs) {
        std::string keyword = kv.first;
        Expr *expr = kv.second.get();
        Value *keyword_value = typize(expr, scope);
        //DeclarationValue *decl_value = new DeclarationValue(keyword);
        //decl_value->use(keyword_value, scope);
        DeclarationValue *decl_value = make_declaration_by_value(keyword, keyword_value, scope);
        block->add_statement(decl_value);
    }

    bool identifier = false;
    Expr streamify_expr(Expr::IDENTIFIER, expr->token, "streamify");
    streamify_expr.add_arg(new Expr(Expr::IDENTIFIER, expr->token, "<interpolated>"));
    
    for (auto &fragment : fragments) {
        Value *pivot;
        TypeMatch match;
        
        if (identifier) {
            // For explicit keywords, we only look up in the innermost scope.
            // For identifiers, we look up outer scopes, but we don't need to look
            // in inner scopes, because that would need a pivot value, which we don't have.
            
            for (Scope *s = scope; s; s = s->outer_scope) {
                pivot = s->lookup(fragment, NULL, match);
        
                if (pivot)
                    break;
                else if (expr->kwargs.size() > 0)
                    break;  // Look up only pseudo variables in this scope
            }
            
            if (!pivot) {
                std::cerr << "Cannot interpolate undefined {" << fragment << "}!\n";
                throw TYPE_ERROR;
            }
        }
        else {
            pivot = make_string_literal_value(fragment);
        }

        Value *streamify = lookup("streamify", pivot, &streamify_expr, scope);
        if (!streamify) {
            std::cerr << "Cannot interpolate unstreamifiable " << pivot->ts << "!\n";
            throw TYPE_ERROR;
        }

        block->add_statement(streamify);
        identifier = !identifier;
    }

    Value *ret = make_variable_value(v, NULL);
    TypeMatch match;  // kinda unnecessary
    ret = new ArrayReallocValue(TWEAK, ret, match);
    block->add_statement(ret, true);
    
    return make_code_value(block);
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
        
        StringLiteralValue *s = dynamic_cast<StringLiteralValue *>(p);
        
        if (s) {
            if (name.size()) {
                std::cerr << "No named initializers for string literals!\n";
                throw TYPE_ERROR;
            }
            
            value = interpolate(s->text, expr, scope);
        }
        else {
            if (name.size() == 0)
                name = "{}";
            
            TypeSpec ts;
            TypeMatch match;
            
            if (typematch(ANY_TYPE_TS, p, match))
                ts = match[1];
            else if (context) {
                ts = *context;
            }
            else {
                std::cerr << "Initializer with neither explicit nor implicit type!\n";
                throw TYPE_ERROR;
            }
            
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
