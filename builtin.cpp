
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


Scope *implement(Scope *scope, TypeSpec interface_ts, std::string implementation_name) {
    DataScope *inner_scope = new DataScope;
    TypeSpec implementor_ts = scope->pivot_type_hint();
    ImplementationType *implementation = new ImplementationType(implementation_name, implementor_ts, interface_ts);
    implementation->set_inner_scope(inner_scope);
    scope->add(implementation);
    return inner_scope;
}


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

    class_metatype = new ClassMetaType(":Class");
    root_scope->add(class_metatype);

    interface_metatype = new InterfaceMetaType(":Interface");
    root_scope->add(interface_metatype);

    implementation_metatype = new ImplementationMetaType(":Implementation");
    root_scope->add(implementation_metatype);
    
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

    role_type = new AttributeType("Role");
    root_scope->add(role_type);
    
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

    borrowed_type = new BorrowedReferenceType("Borrowed");
    root_scope->add(borrowed_type);
    
    array_type = new ArrayType("Array");
    root_scope->add(array_type);

    InterfaceType *streamifiable_interface_type = new InterfaceType("Streamifiable", 0);
    streamifiable_type = streamifiable_interface_type;
    root_scope->add(streamifiable_type);

    InterfaceType *iterator_interface_type = new InterfaceType("Iterator", 1);
    iterator_type = iterator_interface_type;
    root_scope->add(iterator_type);

    // BOGUS_TS will contain no Type pointers
    ANY_TS = { any_type };
    ANY_TYPE_TS = { type_type, any_type };
    ANY_LVALUE_TS = { lvalue_type, any_type };
    ANY_OVALUE_TS = { ovalue_type, any_type };
    SAME_TS = { same_type };
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
    STREAMIFIABLE_TS = { streamifiable_type };
    ANY_ITERATOR_TS = { iterator_type, any_type };

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

    // Streamifiable interface
    DataScope *sis = new DataScope;
    Function *sf = new Function("streamify",
        STREAMIFIABLE_TS,
        TSs { CHARACTER_ARRAY_REFERENCE_LVALUE_TS },
        Ss { "stream" },
        NO_TSS,
        NULL
    );
    sis->add(sf);
    streamifiable_interface_type->set_inner_scope(sis);
    
    // Iterator interface
    DataScope *iis = new DataScope;
    Function *nf = new Function("next",
        ANY_ITERATOR_TS,
        NO_TSS,
        no_names,
        TSs { SAME_TS },
        NULL
    );
    iis->add(nf);
    iterator_interface_type->set_inner_scope(iis);

    // Integer operations
    Scope *integer_scope = integer_metatype->get_inner_scope(BOGUS_TS.begin());
    
    for (auto &item : integer_rvalue_operations)
        integer_scope->add(new TemplateOperation<IntegerOperationValue>(item.name, ANY_TS, item.operation));

    for (auto &item : integer_lvalue_operations)
        integer_scope->add(new TemplateOperation<IntegerOperationValue>(item.name, ANY_LVALUE_TS, item.operation));

    integer_scope->add(new TemplateOperation<IntegerOperationValue>("cover", ANY_TS, EQUAL));
    Scope *isable_scope = implement(integer_scope, STREAMIFIABLE_TS, "sable");
    isable_scope->add(new ImportedFunction("streamify_integer", "streamify", INTEGER_TS, TSs { CHARACTER_ARRAY_REFERENCE_LVALUE_TS }, Ss { "stream" }, NO_TSS, NULL));
    
    // Character operations
    Scope *char_scope = character_type->get_inner_scope(BOGUS_TS.begin());
    char_scope->add(new TemplateOperation<IntegerOperationValue>("assign other", CHARACTER_LVALUE_TS, ASSIGN));
    Scope *csable_scope = implement(char_scope, STREAMIFIABLE_TS, "sable");
    csable_scope->add(new TemplateIdentifier<CharacterStreamificationValue>("streamify", CHARACTER_TS));
    
    // Boolean operations
    Scope *bool_scope = boolean_type->get_inner_scope(BOGUS_TS.begin());
    bool_scope->add(new TemplateOperation<BooleanOperationValue>("assign other", BOOLEAN_LVALUE_TS, ASSIGN));

    // Logical operations, unscoped
    root_scope->add(new TemplateIdentifier<BooleanNotValue>("logical not", ANY_TS));
    root_scope->add(new TemplateIdentifier<BooleanAndValue>("logical and", BOOLEAN_TS));
    root_scope->add(new TemplateIdentifier<BooleanOrValue>("logical or", ANY_TS));

    // Enum operations
    Scope *enum_scope = enumeration_metatype->get_inner_scope(BOGUS_TS.begin());
    enum_scope->add(new TemplateOperation<IntegerOperationValue>("assign other", ANY_LVALUE_TS, ASSIGN));
    enum_scope->add(new TemplateOperation<IntegerOperationValue>("is_equal", ANY_TS, EQUAL));
    enum_scope->add(new TemplateOperation<IntegerOperationValue>("cover", ANY_TS, EQUAL));
    Scope *esable_scope = implement(enum_scope, STREAMIFIABLE_TS, "sable");
    esable_scope->add(new TemplateIdentifier<EnumStreamificationValue>("streamify", ANY_TS));

    // Treenum operations
    Scope *treenum_scope = treenumeration_metatype->get_inner_scope(BOGUS_TS.begin());
    treenum_scope->add(new TemplateOperation<IntegerOperationValue>("assign other", ANY_LVALUE_TS, ASSIGN));
    treenum_scope->add(new TemplateOperation<IntegerOperationValue>("is_equal", ANY_TS, EQUAL));
    treenum_scope->add(new TemplateOperation<TreenumCoveringValue>("cover", ANY_TS, TWEAK));
    Scope *tsable_scope = implement(treenum_scope, STREAMIFIABLE_TS, "sable");
    tsable_scope->add(new TemplateIdentifier<EnumStreamificationValue>("streamify", ANY_TS));

    // Record operations
    Scope *record_scope = record_metatype->get_inner_scope(BOGUS_TS.begin());
    record_scope->add(new TemplateOperation<RecordOperationValue>("assign other", ANY_LVALUE_TS, ASSIGN));

    // Reference operations, unscoped
    typedef TemplateOperation<ReferenceOperationValue> ReferenceOperation;
    root_scope->add(new ReferenceOperation("assign other", ANY_REFERENCE_LVALUE_TS, ASSIGN));
    root_scope->add(new ReferenceOperation("is_equal", ANY_REFERENCE_TS, EQUAL));
    root_scope->add(new ReferenceOperation("not_equal", ANY_REFERENCE_TS, NOT_EQUAL));

    // Array operations
    Scope *array_scope = array_type->get_inner_scope(BOGUS_TS.begin());
    array_scope->add(new TemplateIdentifier<ArrayLengthValue>("length", ANY_ARRAY_REFERENCE_TS));
    array_scope->add(new TemplateOperation<ArrayReallocValue>("realloc", ANY_ARRAY_REFERENCE_TS, TWEAK));
    array_scope->add(new TemplateIdentifier<ArrayConcatenationValue>("binary_plus", ANY_ARRAY_REFERENCE_TS));
    array_scope->add(new TemplateOperation<ArrayItemValue>("index", ANY_ARRAY_REFERENCE_TS, TWEAK));
    
    // String operations
    Scope *sable_scope = implement(array_scope, STREAMIFIABLE_TS, "sable");
    sable_scope->add(new TemplateIdentifier<StringStreamificationValue>("streamify", CHARACTER_ARRAY_REFERENCE_TS));
    
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
    //root_scope->add(new ImportedFunction("streamify_integer", "streamify", INTEGER_TS, TSs { CHARACTER_ARRAY_REFERENCE_LVALUE_TS }, Ss { "stream" }, NO_TSS, NULL));
    //root_scope->add(new ImportedFunction("streamify_string", "streamify", CHARACTER_ARRAY_REFERENCE_TS, TSs { CHARACTER_ARRAY_REFERENCE_LVALUE_TS }, Ss { "stream" }, VOID_TS));

    return root_scope;
}

