

Value *make_variable_value(Variable *decl, Value *pivot, TypeMatch &match) {
    return new VariableValue(decl, pivot, match);
}


Value *make_partial_variable_value(PartialVariable *decl, Value *pivot, TypeMatch &match) {
    return new PartialVariableValue(decl, pivot, match);
}


Value *make_role_value(Role *role, Value *pivot, TypeMatch &tm) {
    return new RoleValue(role, pivot, tm);
}


Value *make_function_call_value(Function *decl, Value *pivot, TypeMatch &match) {
    return new FunctionCallValue(decl, pivot, match);
}


Value *make_type_value(Type *mt, TypeSpec ts) {
    return new TypeValue(mt, ts);
}


Value *make_code_block_value(TypeSpec *context) {
    return new CodeBlockValue(context);
}


Value *make_multi_value() {
    return new MultiValue();
}


Value *make_eval_value(std::string en) {
    return new EvalValue(en);
}


Value *make_yield_value(EvalScope *es) {
    return new YieldValue(es);
}


Value *make_declaration_value(std::string name, TypeSpec *context) {
    return new DeclarationValue(name, context);
}


Value *make_basic_value(TypeSpec ts, long number) {
    return new BasicValue(ts, number);
}


Value *make_float_value(TypeSpec ts, double number) {
    return new FloatValue(ts, number);
}


Value *make_float_function_value(ImportedFloatFunction *f, Value *cpivot, TypeMatch &match) {
    return new FloatFunctionValue(f, cpivot, match);
}


Value *make_string_literal_value(std::string text) {
    return new StringLiteralValue(text);
}


Value *make_code_scope_value(Value *value, CodeScope *code_scope) {
    return new CodeScopeValue(value, code_scope);
}


Value *make_scalar_conversion_value(Value *p) {
    return new ScalarConversionValue(p);
}


Value *make_void_conversion_value(Value *p) {
    return new VoidConversionValue(p);
}


Value *make_implementation_conversion_value(ImplementationType *imt, Value *p, TypeMatch &match) {
    return new ImplementationConversionValue(imt, p, match);
}


Value *make_boolean_not_value(Value *p) {
    TypeMatch match;
    
    if (!typematch(BOOLEAN_TS, p, match))
        throw INTERNAL_ERROR;
        
    return new BooleanOperationValue(COMPLEMENT, p, match);
}


Value *make_array_empty_value(TypeSpec ts) {
    return new ArrayEmptyValue(ts);
}


Value *make_array_initializer_value(TypeSpec ts) {
    return new ArrayInitializerValue(ts);
}


Value *make_circularray_empty_value(TypeSpec ts) {
    return new CircularrayEmptyValue(ts);
}


Value *make_circularray_initializer_value(TypeSpec ts) {
    return new CircularrayInitializerValue(ts);
}


Value *make_rbtree_empty_value(TypeSpec ts) {
    return new RbtreeEmptyValue(ts);
}


Value *make_rbtree_reserved_value(TypeSpec ts) {
    return new RbtreeReservedValue(ts);
}


Value *make_rbtree_initializer_value(TypeSpec ts) {
    return new RbtreeInitializerValue(ts);
}


Value *make_unicode_character_value() {
    return new UnicodeCharacterValue();
}


Value *make_integer_definition_value() {
    return new IntegerDefinitionValue();
}


Value *make_enumeration_definition_value() {
    return new EnumerationDefinitionValue();
}


Value *make_treenumeration_definition_value() {
    return new TreenumerationDefinitionValue();
}


Value *make_treenumeration_matcher_value(TypeSpec ts, int i, Value *p) {
    return new TreenumerationMatcherValue(i, p);
}


Value *make_record_definition_value() {
    return new RecordDefinitionValue();
}


Value *make_record_initializer_value(TypeMatch &match) {
    return new RecordInitializerValue(match);
}


Value *make_record_preinitializer_value(TypeSpec ts) {
    return new RecordPreinitializerValue(ts);
}


Value *make_record_postinitializer_value(Value *v) {
    return new RecordPostinitializerValue(v);
}


Value *make_class_definition_value() {
    return new ClassDefinitionValue();
}


Value *make_class_preinitializer_value(TypeSpec ts) {
    return new ClassPreinitializerValue(ts);
}


Value *make_reference_weaken_value(Value *v) {
    TypeMatch tm;
    return new ReferenceWeakenValue(v, tm);
}


Value *make_interface_definition_value() {
    return new InterfaceDefinitionValue();
}


Value *make_implementation_definition_value() {
    return new ImplementationDefinitionValue();
}


Value *make_cast_value(Value *v, TypeSpec ts) {
    return new CastValue(v, ts);
}


Value *make_equality_value(bool no, Value *v) {
    return new EqualityValue(no, v);
}


Value *make_comparison_value(ConditionCode cc, Value *v) {
    return new ComparisonValue(cc, v);
}


CreateValue *make_initialization_by_value(std::string name, Value *v, Scope *scope) {
    Args fake_args;
    Kwargs fake_kwargs;
    
    DeclarationValue *dv = new DeclarationValue(name);
    dv->check(fake_args, fake_kwargs, scope);
    
    TypeMatch tm = { VOID_UNINITIALIZED_TS, VOID_TS };
    CreateValue *cv = new CreateValue(dv, tm);
    if (!cv->use(v, scope))
        throw INTERNAL_ERROR;
    
    return cv;
}


Value *make_record_unwrap_value(TypeSpec cast_ts, Value *v) {
    return new RecordUnwrapValue(cast_ts, v);
}


Value *make_record_wrapper_value(Value *pivot, TypeSpec pivot_cast_ts, TypeSpec result_ts, std::string operation_name, std::string arg_operation_name) {
    return new RecordWrapperValue(pivot, pivot_cast_ts, result_ts, operation_name, arg_operation_name);
}


Value *make_class_wrapper_initializer_value(Value *object, Value *value) {
    return new ClassWrapperInitializerValue(object, value);
}


Value *make_option_none_value(TypeSpec ts) {
    return new OptionNoneValue(ts);
}


Value *make_option_some_value(TypeSpec ts) {
    return new OptionSomeValue(ts);
}


Value *make_option_none_matcher_value(Value *p, TypeMatch &match) {
    return new OptionNoneMatcherValue(p, match);
}


Value *make_option_some_matcher_value(Value *p, TypeMatch &match) {
    return new OptionSomeMatcherValue(p, match);
}


Value *make_evaluable_value(Evaluable *e, Value *cpivot, TypeMatch &match) {
    return new EvaluableValue(e, cpivot, match);
}


Value *make_implicit_equality_matcher_value(Value *p) {
    return new ImplicitEqualityMatcherValue(p);
}


Value *make_initializer_equality_matcher_value(Value *p) {
    return new InitializerEqualityMatcherValue(p);
}


Value *make_bulk_equality_matcher_value() {
    return new BulkEqualityMatcherValue();
}


Value *make_create_value(Value *p, TypeMatch &match) {
    return new CreateValue(p, match);
}


Value *make_weakanchorage_value(TypeSpec rts) {
    return new WeakAnchorageValue(rts);
}


Value *make_weakanchorage_dead_matcher_value(Value *p, TypeMatch &match) {
    return new WeakAnchorageDeadMatcherValue(p, match);
}


Value *make_weakanchorage_live_matcher_value(Value *p, TypeMatch &match) {
    return new WeakAnchorageLiveMatcherValue(p, match);
}


Value *make_string_regexp_matcher_value(Value *p, TypeMatch &match) {
    return new StringRegexpMatcherValue(p, match);
}


Value *make_class_matcher_value(std::string name, Value *pivot) {
    return new ClassMatcherValue(name, pivot);
}
