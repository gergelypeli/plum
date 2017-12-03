
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
    { "compare", COMPARE },
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


void init_interfaces(Scope *root_scope) {
    // Streamifiable interface
    DataScope *sis = new DataScope;
    Function *sf = new Function("streamify",
        STREAMIFIABLE_TS,
        TSs { STRING_LVALUE_TS },
        Ss { "stream" },
        TSs {},
        NULL
    );
    sf->be_interface_function();
    sis->add(sf);
    streamifiable_type->set_inner_scope(sis);
    
    // Iterable interface
    DataScope *jis = new DataScope;
    Function *xf = new Function("iter",
        ANY_ITERABLE_TS,
        TSs {},
        Ss {},
        TSs { SAME_ITERATOR_TS },
        NULL
    );
    xf->be_interface_function();
    jis->add(xf);
    iterable_type->set_inner_scope(jis);

    // Iterator interface
    DataScope *iis = new DataScope;
    Function *nf = new Function("next",
        ANY_ITERATOR_TS,
        TSs {},
        Ss {},
        TSs { SAME_TS },
        NULL
    );
    nf->be_interface_function();
    iis->add(nf);
    iis->set_pivot_type_hint(ANY_ITERATOR_TS);
    Scope *ii_scope = implement(iis, SAME_ITERABLE_TS, "ible");
    // This must return the concrete type, so the pivot type must be Any so that no
    // conversion to an interface happens, which would hide the concrete type.
    ii_scope->add(new TemplateIdentifier<IteratorIterableIterValue>("iter", ANY_TS));
    iterator_type->set_inner_scope(iis);
}


void init_iterators(Scope *root_scope) {
    // Counter operations
    for (auto is_down : { false, true }) {
        RecordType *counter_type = new RecordType(is_down ? "Countdown" : "Countup", 0);
        TypeSpec COUNTER_TS = { counter_type };
    
        DataScope *cis = new DataScope;
        root_scope->add(cis);
        cis->set_pivot_type_hint(COUNTER_TS);
    
        cis->add(new Variable("limit", COUNTER_TS, INTEGER_LVALUE_TS));  // Order matters!
        cis->add(new Variable("value", COUNTER_TS, INTEGER_LVALUE_TS));
        Scope *citer_scope = implement(cis, INTEGER_ITERATOR_TS, "iter");
        
        if (!is_down) {
            citer_scope->add(new TemplateIdentifier<CountupNextValue>("next", COUNTER_TS));
            COUNTUP_TS = { counter_type };
        }
        else {
            citer_scope->add(new TemplateIdentifier<CountdownNextValue>("next", COUNTER_TS));
            COUNTDOWN_TS = { counter_type };
        }
        
        counter_type->set_inner_scope(cis);
        root_scope->add(counter_type);

    }

    // Item type for itemized iteration
    RecordType *item_type = new RecordType("Item", 1);
    ANY_ITEM_TS = { item_type, any_type };
    SAME_ITEM_TS = { item_type, same_type };
    
    DataScope *itis = new DataScope;
    root_scope->add(itis);
    itis->set_pivot_type_hint(ANY_ITEM_TS);

    itis->add(new Variable("index", ANY_ITEM_TS, INTEGER_LVALUE_TS));  // Order matters!
    itis->add(new Variable("value", ANY_ITEM_TS, SAME_LVALUE_TS));
    
    item_type->set_inner_scope(itis);
    root_scope->add(item_type);
    
    TypeSpec SAME_ITEM_ITERATOR_TS = { iterator_type, item_type, same_type };

    // Array Iterator operations
    enum AIT { ELEM, INDEX, ITEM };
    for (auto type : { ELEM, INDEX, ITEM }) {
        RecordType *aiter_type = new RecordType(type == INDEX ? "Arrayindex_iter" : type == ELEM ? "Arrayelem_iter" : "Arrayitem_iter", 1);
        TypeSpec ANY_ARRAYITER_TS = { aiter_type, any_type };
        TypeSpec SAME_ARRAY_REFERENCE_LVALUE_TS = { lvalue_type, reference_type, array_type, same_type };
    
        DataScope *aiis = new DataScope;
        root_scope->add(aiis);
        aiis->set_pivot_type_hint(ANY_ARRAYITER_TS);
    
        aiis->add(new Variable("array", ANY_ARRAYITER_TS, SAME_ARRAY_REFERENCE_LVALUE_TS));  // Order matters!
        aiis->add(new Variable("value", ANY_ARRAYITER_TS, INTEGER_LVALUE_TS));

        if (type == ELEM) {
            Scope *aiter_scope = implement(aiis, SAME_ITERATOR_TS, "elemiter");
            aiter_scope->add(new TemplateIdentifier<ArrayNextElemValue>("next", ANY_ARRAYITER_TS));
            SAME_ARRAYELEMITER_TS = { aiter_type, same_type };
        }
        else if (type == INDEX) {
            Scope *aiter_scope = implement(aiis, INTEGER_ITERATOR_TS, "indexiter");
            aiter_scope->add(new TemplateIdentifier<ArrayNextIndexValue>("next", ANY_ARRAYITER_TS));
            SAME_ARRAYINDEXITER_TS = { aiter_type, same_type };
        }
        else {
            Scope *aiter_scope = implement(aiis, SAME_ITEM_ITERATOR_TS, "itemiter");
            aiter_scope->add(new TemplateIdentifier<ArrayNextItemValue>("next", ANY_ARRAYITER_TS));
            SAME_ARRAYITEMITER_TS = { aiter_type, same_type };
        }
        
        aiter_type->set_inner_scope(aiis);
        root_scope->add(aiter_type);
    }
}


void init_string(Scope *root_scope) {
    RecordType *record_type = new StringType("String");
    string_type = record_type;

    STRING_TS = { string_type };
    STRING_LVALUE_TS = { lvalue_type, string_type };

    DataScope *is = new DataScope;
    root_scope->add(is);
    is->set_pivot_type_hint(STRING_TS);

    is->add(new Variable("chars", STRING_TS, CHARACTER_ARRAY_REFERENCE_LVALUE_TS));  // Order matters!

    is->add(new TemplateIdentifier<StringLengthValue>("length", STRING_TS));
    is->add(new TemplateIdentifier<StringConcatenationValue>("binary_plus", STRING_TS));
    is->add(new TemplateIdentifier<StringItemValue>("index", STRING_TS));

    is->add(new TemplateOperation<RecordOperationValue>("assign other", STRING_LVALUE_TS, ASSIGN));
    is->add(new TemplateIdentifier<StringEqualityValue>("is_equal", STRING_TS));

    Scope *ible_scope = implement(is, TypeSpec { iterable_type, character_type }, "ible");
    ible_scope->add(new TemplateIdentifier<StringElemIterValue>("iter", STRING_TS));

    is->add(new TemplateIdentifier<StringIndexIterValue>("indexes", STRING_TS));
    is->add(new TemplateIdentifier<StringItemIterValue>("items", STRING_TS));

    is->add(new Identity("null", STRING_TS));  // a null initializer that does nothing

    is->add(new TemplateOperation<RecordOperationValue>("compare", ANY_TS, COMPARE));

    record_type->set_inner_scope(is);
    root_scope->add(string_type);

    // String operations
    Scope *sable_scope = implement(is, STREAMIFIABLE_TS, "sable");
    sable_scope->add(new TemplateIdentifier<StringStreamificationValue>("streamify", STRING_TS));
}


Scope *init_builtins() {
    Scope *root_scope = new Scope();

    any_type = new SpecialType("<Any>", 0);
    root_scope->add(any_type);

    same_type = new SameType("<Same>");
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

    lvalue_type = new AttributeType("Lvalue");
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

    streamifiable_type = new InterfaceType("Streamifiable", 0);
    root_scope->add(streamifiable_type);

    iterator_type = new InterfaceType("Iterator", 1);
    root_scope->add(iterator_type);

    iterable_type = new InterfaceType("Iterable", 1);
    root_scope->add(iterable_type);

    //string_type = new RecordType("String");
    //root_scope->add(string_type);

    // NO_TS will contain no Type pointers
    ANY_TS = { any_type };
    ANY_TYPE_TS = { type_type, any_type };
    ANY_LVALUE_TS = { lvalue_type, any_type };
    ANY_OVALUE_TS = { ovalue_type, any_type };
    SAME_TS = { same_type };
    SAME_LVALUE_TS = { lvalue_type, same_type };
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
    ANY_ARRAY_REFERENCE_LVALUE_TS = { lvalue_type, reference_type, array_type, any_type };
    VOID_CODE_TS = { code_type, void_type };
    BOOLEAN_CODE_TS = { code_type, boolean_type };
    STREAMIFIABLE_TS = { streamifiable_type };
    ANY_ITERATOR_TS = { iterator_type, any_type };
    SAME_ITERATOR_TS = { iterator_type, same_type };
    INTEGER_ITERATOR_TS = { iterator_type, integer_type };
    ANY_ITERABLE_TS = { iterable_type, any_type };
    SAME_ITERABLE_TS = { iterable_type, same_type };
    //STRING_TS = { string_type };
    //STRING_LVALUE_TS = { lvalue_type, string_type };

    TSs NO_TSS = { };
    TSs INTEGER_TSS = { INTEGER_TS };
    TSs BOOLEAN_TSS = { BOOLEAN_TS };
    TSs UNSIGNED_INTEGER8_TSS = { UNSIGNED_INTEGER8_TS };
    TSs UNSIGNED_INTEGER8_ARRAY_REFERENCE_TSS = { UNSIGNED_INTEGER8_ARRAY_REFERENCE_TS };
    TSs CHARACTER_ARRAY_REFERENCE_TSS = { CHARACTER_ARRAY_REFERENCE_TS };

    Ss no_names = { };
    Ss value_names = { "value" };

    init_string(root_scope);

    init_interfaces(root_scope);

    init_iterators(root_scope);

    // Integer operations
    Scope *integer_scope = integer_metatype->get_inner_scope(NO_TS.begin());
    
    for (auto &item : integer_rvalue_operations)
        integer_scope->add(new TemplateOperation<IntegerOperationValue>(item.name, ANY_TS, item.operation));

    for (auto &item : integer_lvalue_operations)
        integer_scope->add(new TemplateOperation<IntegerOperationValue>(item.name, ANY_LVALUE_TS, item.operation));

    integer_scope->add(new TemplateOperation<IntegerOperationValue>("cover", ANY_TS, EQUAL));
    Scope *isable_scope = implement(integer_scope, STREAMIFIABLE_TS, "sable");
    isable_scope->add(new ImportedFunction("streamify_integer", "streamify", INTEGER_TS, TSs { STRING_LVALUE_TS }, Ss { "stream" }, NO_TSS, NULL));
    
    integer_scope->add(new TemplateIdentifier<CountupValue>("countup", INTEGER_TS));
    integer_scope->add(new TemplateIdentifier<CountdownValue>("countdown", INTEGER_TS));
        
    // Character operations
    Scope *char_scope = character_type->get_inner_scope(NO_TS.begin());
    char_scope->add(new TemplateOperation<IntegerOperationValue>("assign other", CHARACTER_LVALUE_TS, ASSIGN));
    char_scope->add(new TemplateOperation<IntegerOperationValue>("compare", CHARACTER_TS, COMPARE));
    Scope *csable_scope = implement(char_scope, STREAMIFIABLE_TS, "sable");
    csable_scope->add(new TemplateIdentifier<CharacterStreamificationValue>("streamify", CHARACTER_TS));
    
    // Boolean operations
    Scope *bool_scope = boolean_type->get_inner_scope(NO_TS.begin());
    bool_scope->add(new TemplateOperation<BooleanOperationValue>("assign other", BOOLEAN_LVALUE_TS, ASSIGN));
    bool_scope->add(new TemplateOperation<BooleanOperationValue>("compare", BOOLEAN_TS, COMPARE));
    Scope *bsable_scope = implement(bool_scope, STREAMIFIABLE_TS, "sable");
    bsable_scope->add(new ImportedFunction("streamify_boolean", "streamify", BOOLEAN_TS, TSs { STRING_LVALUE_TS }, Ss { "stream" }, NO_TSS, NULL));

    // Logical operations, unscoped
    root_scope->add(new TemplateIdentifier<BooleanNotValue>("logical not", BOOLEAN_TS));
    root_scope->add(new TemplateIdentifier<BooleanAndValue>("logical and", BOOLEAN_TS));
    root_scope->add(new TemplateIdentifier<BooleanOrValue>("logical or", ANY_TS));

    // Enum operations
    Scope *enum_scope = enumeration_metatype->get_inner_scope(NO_TS.begin());
    enum_scope->add(new TemplateOperation<IntegerOperationValue>("assign other", ANY_LVALUE_TS, ASSIGN));
    enum_scope->add(new TemplateOperation<IntegerOperationValue>("is_equal", ANY_TS, EQUAL));
    enum_scope->add(new TemplateOperation<IntegerOperationValue>("cover", ANY_TS, EQUAL));
    Scope *esable_scope = implement(enum_scope, STREAMIFIABLE_TS, "sable");
    esable_scope->add(new TemplateIdentifier<EnumStreamificationValue>("streamify", ANY_TS));

    // Treenum operations
    Scope *treenum_scope = treenumeration_metatype->get_inner_scope(NO_TS.begin());
    treenum_scope->add(new TemplateOperation<IntegerOperationValue>("assign other", ANY_LVALUE_TS, ASSIGN));
    treenum_scope->add(new TemplateOperation<IntegerOperationValue>("is_equal", ANY_TS, EQUAL));
    treenum_scope->add(new TemplateOperation<TreenumCoveringValue>("cover", ANY_TS, TWEAK));
    Scope *tsable_scope = implement(treenum_scope, STREAMIFIABLE_TS, "sable");
    tsable_scope->add(new TemplateIdentifier<EnumStreamificationValue>("streamify", ANY_TS));

    // Record operations
    Scope *record_scope = record_metatype->get_inner_scope(NO_TS.begin());
    record_scope->add(new TemplateOperation<RecordOperationValue>("assign other", ANY_LVALUE_TS, ASSIGN));

    // Reference operations, unscoped
    typedef TemplateOperation<ReferenceOperationValue> ReferenceOperation;
    root_scope->add(new ReferenceOperation("assign other", ANY_REFERENCE_LVALUE_TS, ASSIGN));
    root_scope->add(new ReferenceOperation("is_equal", ANY_REFERENCE_TS, EQUAL));
    root_scope->add(new ReferenceOperation("not_equal", ANY_REFERENCE_TS, NOT_EQUAL));

    // Array operations
    Scope *array_scope = array_type->get_inner_scope(NO_TS.begin());
    array_scope->add(new TemplateIdentifier<ArrayLengthValue>("length", ANY_ARRAY_REFERENCE_TS));
    array_scope->add(new TemplateOperation<ArrayReallocValue>("realloc", ANY_ARRAY_REFERENCE_TS, TWEAK));
    array_scope->add(new TemplateIdentifier<ArrayConcatenationValue>("binary_plus", ANY_ARRAY_REFERENCE_TS));
    array_scope->add(new TemplateOperation<ArrayItemValue>("index", ANY_ARRAY_REFERENCE_TS, TWEAK));
    array_scope->add(new TemplateIdentifier<ArraySortValue>("sort", ANY_ARRAY_REFERENCE_TS));
    array_scope->add(new TemplateIdentifier<ArrayPushValue>("push", ANY_ARRAY_REFERENCE_LVALUE_TS));
    
    // Array iterable operations
    Scope *ible_scope = implement(array_scope, SAME_ITERABLE_TS, "ible");
    ible_scope->add(new TemplateIdentifier<ArrayElemIterValue>("iter", ANY_ARRAY_REFERENCE_TS));

    array_scope->add(new TemplateIdentifier<ArrayIndexIterValue>("indexes", ANY_ARRAY_REFERENCE_TS));
    array_scope->add(new TemplateIdentifier<ArrayItemIterValue>("items", ANY_ARRAY_REFERENCE_TS));

    // String operations
    Scope *sable_scope = implement(array_scope, STREAMIFIABLE_TS, "sable");
    sable_scope->add(new TemplateIdentifier<StringStreamificationValue>("streamify", STRING_TS));
    
    // Unpacking
    root_scope->add(new TemplateIdentifier<UnpackingValue>("assign other", MULTI_LVALUE_TS));
    
    // Builtin controls, unscoped
    root_scope->add(new TemplateOperation<IfValue>(":if", NO_TS, TWEAK));
    root_scope->add(new TemplateIdentifier<RepeatValue>(":repeat", NO_TS));
    root_scope->add(new TemplateIdentifier<ForEachValue>(":for", NO_TS));
    root_scope->add(new TemplateIdentifier<SwitchValue>(":switch", NO_TS));
    root_scope->add(new TemplateIdentifier<WhenValue>(":when", NO_TS));
    root_scope->add(new TemplateIdentifier<RaiseValue>(":raise", NO_TS));
    root_scope->add(new TemplateIdentifier<TryValue>(":try", NO_TS));
    root_scope->add(new TemplateOperation<FunctionReturnValue>(":return", NO_TS, TWEAK));
    root_scope->add(new TemplateOperation<FunctionDefinitionValue>(":Function", NO_TS, TWEAK));
    
    // Library functions, unscoped
    root_scope->add(new ImportedFunction("print", "print", NO_TS, INTEGER_TSS, value_names, NO_TSS, NULL));
    root_scope->add(new ImportedFunction("printu8", "printu8", NO_TS, UNSIGNED_INTEGER8_TSS, value_names, NO_TSS, NULL));
    root_scope->add(new ImportedFunction("printb", "printb", NO_TS, UNSIGNED_INTEGER8_ARRAY_REFERENCE_TSS, value_names, NO_TSS, NULL));
    root_scope->add(new ImportedFunction("prints", "prints", NO_TS, TSs { STRING_TS }, value_names, NO_TSS, NULL));
    root_scope->add(new ImportedFunction("decode_utf8", "decode_utf8", UNSIGNED_INTEGER8_ARRAY_REFERENCE_TS, NO_TSS, no_names, TSs { STRING_TS }, NULL));
    root_scope->add(new ImportedFunction("encode_utf8", "encode_utf8", STRING_TS, NO_TSS, no_names, TSs { UNSIGNED_INTEGER8_ARRAY_REFERENCE_TS }, NULL));

    root_scope->add(new ImportedFunction("stringify_integer", "stringify", INTEGER_TS, NO_TSS, no_names, TSs { STRING_TS }, NULL));

    return root_scope;
}

