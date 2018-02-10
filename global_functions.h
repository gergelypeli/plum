
// Value wrappers

bool partial_variable_is_initialized(std::string name, Value *pivot);
DeclarationValue *declaration_value_cast(Value *value);
Declaration *declaration_get_decl(DeclarationValue *dv);
bool unpack_value(Value *v, std::vector<TypeSpec> &tss);


// Declaration wrappers

Variable *variable_cast(Declaration *decl);
HeapType *heap_type_cast(Type *t);
Declaration *make_record_compare();


// TypeSpec operations

TypeSpec get_typespec(Value *value);
bool is_implementation(Type *t, TypeMatch &match, TypeSpecIter target, TypeSpec &ifts);
Value *find_implementation(Scope *inner_scope, TypeMatch &match, TypeSpecIter target, Value *orig, TypeSpec &ifts);
bool typematch(TypeSpec tt, Value *&v, TypeMatch &match, CodeScope *code_scope = NULL);
TypeSpec typesubst(TypeSpec &ts, TypeMatch &match);
TypeMatch type_parameters_to_match(TypeSpec ts);


// Streamification
void compile_array_preappend(Label label, TypeSpec elem_ts, X64 *x64);


// Value makers

Value *make_variable_value(Variable *decl, Value *pivot, TypeMatch &match);
Value *make_partial_variable_value(PartialVariable *decl, Value *pivot, TypeMatch &match);
Value *make_role_value(Variable *decl, Value *pivot, TypeMatch &tm);
Value *make_function_call_value(Function *decl, Value *pivot, TypeMatch &match);
Value *make_type_value(TypeSpec ts);
Value *make_code_block_value(TypeSpec *context);
Value *make_multi_value();
Value *make_eval_value(std::string en);
Value *make_yield_value(EvalScope *es);
Value *make_scalar_conversion_value(Value *p);
Value *make_function_definition_value(TypeSpec fn_ts, Value *ret, Value *head, Value *body, FunctionScope *fn_scope);
Value *make_declaration_value(std::string name, TypeSpec *context);
Value *make_partial_declaration_value(std::string name, PartialVariableValue *pivot);
Value *make_basic_value(TypeSpec ts, int number);
Value *make_string_literal_value(std::string text);
Value *make_code_scope_value(Value *value, CodeScope *code_scope);
Value *make_void_conversion_value(Value *orig);
Value *peek_void_conversion_value(Value *v);
Value *make_boolean_conversion_value(Value *orig);
Value *make_implementation_conversion_value(ImplementationType *imt, Value *orig, TypeMatch &match);
Value *make_boolean_not_value(Value *value);
Value *make_null_reference_value(TypeSpec ts);
Value *make_null_string_value();  // TODO: rethink!
Value *make_array_empty_value(TypeSpec ts);
Value *make_array_initializer_value(TypeSpec ts);
Value *make_circularray_empty_value(TypeSpec ts);
Value *make_circularray_initializer_value(TypeSpec ts);
Value *make_rbtree_empty_value(TypeSpec ts);
Value *make_rbtree_reserved_value(TypeSpec ts);
Value *make_rbtree_initializer_value(TypeSpec ts);
Value *make_unicode_character_value();
Value *make_integer_definition_value();
Value *make_enumeration_definition_value();
Value *make_treenumeration_definition_value();
Value *make_record_definition_value();
Value *make_record_initializer_value(TypeMatch &match);
Value *make_record_preinitializer_value(TypeSpec ts);
Value *make_record_postinitializer_value(Value *v);
Value *make_class_definition_value();
Value *make_class_preinitializer_value(TypeSpec ts);
Value *make_interface_definition_value();
Value *make_implementation_definition_value();
Value *make_cast_value(Value *v, TypeSpec ts);
Value *make_equality_value(bool no, Value *v);
Value *make_comparison_value(BitSetOp bs, Value *v);
Value *make_record_unwrap_value(TypeSpec cast_ts, Value *v);
Value *make_record_wrapper_value(Value *pivot, TypeSpec pivot_cast_ts, TypeSpec res_ts, std::string operation_name, std::string arg_operation_name);
Value *make_class_wrapper_initializer_value(Value *object, Value *value);
Value *make_option_none_value(TypeSpec ts);
Value *make_option_some_value(TypeSpec ts);

DeclarationValue *make_declaration_by_value(std::string name, Value *v, Scope *scope);