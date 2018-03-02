
void builtin_types(Scope *root_scope) {
    type_type = new SpecialType("<Type>", { GENERIC_TYPE }, GENERIC_TYPE);
    root_scope->add(type_type);

    any_type = new SpecialType("<Any>", {}, VALUE_TYPE);
    root_scope->add(any_type);

    any2_type = new SpecialType("<Any2>", {}, VALUE_TYPE);
    root_scope->add(any2_type);

    any3_type = new SpecialType("<Any3>", {}, VALUE_TYPE);
    root_scope->add(any3_type);

    anyid_type = new SpecialType("<Anyid>", {}, IDENTITY_TYPE);
    root_scope->add(anyid_type);

    same_type = new SameType("<Same>", {}, VALUE_TYPE);
    root_scope->add(same_type);

    same2_type = new SameType("<Same2>", {}, VALUE_TYPE);
    root_scope->add(same2_type);

    same3_type = new SameType("<Same3>", {}, VALUE_TYPE);
    root_scope->add(same3_type);

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
    
    void_type = new SpecialType("Void", {}, VALUE_TYPE);
    root_scope->add(void_type);

    metatype_type = new SpecialType("<Metatype>", {}, GENERIC_TYPE);
    root_scope->add(metatype_type);

    multi_type = new SpecialType("<Multi>", {}, GENERIC_TYPE);
    root_scope->add(multi_type);

    uninitialized_type = new SpecialType("<Uninitialized>", { GENERIC_TYPE }, GENERIC_TYPE);
    root_scope->add(uninitialized_type);

    lvalue_type = new AttributeType("Lvalue");
    root_scope->add(lvalue_type);
    
    ovalue_type = new AttributeType("Ovalue");
    root_scope->add(ovalue_type);

    dvalue_type = new AttributeType("Dvalue");
    root_scope->add(dvalue_type);

    code_type = new AttributeType("Code");
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

    ref_type = new ReferenceType("Ref");
    root_scope->add(ref_type);

    weakref_type = new WeakReferenceType("Weakref");
    root_scope->add(weakref_type);

    partial_type = new PartialType("<Partial>");
    root_scope->add(partial_type);

    //borrowed_type = new BorrowedReferenceType("Borrowed");
    //root_scope->add(borrowed_type);
    
    array_type = new ArrayType("Array");
    root_scope->add(array_type);

    circularray_type = new CircularrayType("Circularray");
    root_scope->add(circularray_type);

    rbtree_type = new RbtreeType("Rbtree");
    root_scope->add(rbtree_type);

    streamifiable_type = new InterfaceType("Streamifiable", {});
    root_scope->add(streamifiable_type);

    iterator_type = new InterfaceType("Iterator", { VALUE_TYPE });
    root_scope->add(iterator_type);

    iterable_type = new InterfaceType("Iterable", { VALUE_TYPE });
    root_scope->add(iterable_type);

    string_type = new StringType("String");
    root_scope->add(string_type);

    option_type = new OptionType("Option");
    root_scope->add(option_type);

    stack_type = new StackType("Stack");
    root_scope->add(stack_type);

    queue_type = new QueueType("Queue");
    root_scope->add(queue_type);

    set_type = new SetType("Set");
    root_scope->add(set_type);

    map_type = new MapType("Map");
    root_scope->add(map_type);

    countup_type = new RecordType("Countup", {});
    root_scope->add(countup_type);

    countdown_type = new RecordType("Countdown", {});
    root_scope->add(countdown_type);

    item_type = new ItemType("Item");
    root_scope->add(item_type);

    arrayelemiter_type = new RecordType("Arrayelem_iter", { VALUE_TYPE });
    root_scope->add(arrayelemiter_type);
    
    arrayindexiter_type = new RecordType("Arrayindex_iter", { VALUE_TYPE });
    root_scope->add(arrayindexiter_type);
    
    arrayitemiter_type = new RecordType("Arrayitem_iter", { VALUE_TYPE });
    root_scope->add(arrayitemiter_type);

    circularrayelemiter_type = new RecordType("Circularrayelem_iter", { VALUE_TYPE });
    root_scope->add(circularrayelemiter_type);
    
    circularrayindexiter_type = new RecordType("Circularrayindex_iter", { VALUE_TYPE });
    root_scope->add(circularrayindexiter_type);
    
    circularrayitemiter_type = new RecordType("Circularrayitem_iter", { VALUE_TYPE });
    root_scope->add(circularrayitemiter_type);

    rbtreeelembyageiter_type = new RecordType("Rbtreeelembyage_iter", { VALUE_TYPE });
    root_scope->add(rbtreeelembyageiter_type);

    rbtreeelembyorderiter_type = new RecordType("Rbtreeelembyorder_iter", { VALUE_TYPE });
    root_scope->add(rbtreeelembyorderiter_type);

    iterator_done_exception_type = new TreenumerationType("<Iterator_done>", { "", "ITERATOR_DONE" }, { 0, 1 });
    root_scope->add(iterator_done_exception_type);

    container_full_exception_type = new TreenumerationType("<Container_full>", { "", "CONTAINER_FULL" }, { 0, 1 });
    root_scope->add(container_full_exception_type);

    container_empty_exception_type = new TreenumerationType("<Container_empty>", { "", "CONTAINER_EMPTY" }, { 0, 1 });
    root_scope->add(container_empty_exception_type);

    container_lent_exception_type = new TreenumerationType("<Container_lent>", { "", "CONTAINER_LENT" }, { 0, 1 });
    root_scope->add(container_lent_exception_type);

    match_unmatched_exception_type = new TreenumerationType("<Match_unmatched>", { "", "UNMATCHED" }, { 0, 1 });
    root_scope->add(match_unmatched_exception_type);

    code_break_exception_type = new TreenumerationType("<Code_break>", { "", "CODE_BREAK" }, { 0, 1 });
    root_scope->add(code_break_exception_type);
    
    // NO_TS will contain no Type pointers
    ANY_TS = { any_type };
    ANY_LVALUE_TS = { lvalue_type, any_type };
    ANY_OVALUE_TS = { ovalue_type, any_type };
    SAME_TS = { same_type };
    SAME_LVALUE_TS = { lvalue_type, same_type };
    SAME2_LVALUE_TS = { lvalue_type, same2_type };
    METATYPE_TS = { metatype_type };
    TREENUMMETA_TS = { treenumeration_metatype };
    VOID_TS = { void_type };
    MULTI_TS = { multi_type };
    MULTI_LVALUE_TS = { lvalue_type, multi_type };
    MULTI_TYPE_TS = { type_type, multi_type };
    BOOLEAN_TS = { boolean_type };
    INTEGER_TS = { integer_type };
    INTEGER_LVALUE_TS = { lvalue_type, integer_type };
    INTEGER_OVALUE_TS = { ovalue_type, integer_type };
    BOOLEAN_LVALUE_TS = { lvalue_type, boolean_type };
    UNSIGNED_INTEGER8_TS = { unsigned_integer8_type };
    UNSIGNED_INTEGER8_ARRAY_REF_TS = { ref_type, array_type, unsigned_integer8_type };
    CHARACTER_TS = { character_type };
    CHARACTER_LVALUE_TS = { lvalue_type, character_type };
    CHARACTER_ARRAY_REF_TS = { ref_type, array_type, character_type };
    CHARACTER_ARRAY_REF_LVALUE_TS = { lvalue_type, ref_type, array_type, character_type };
    ANYID_REF_TS = { ref_type, anyid_type };
    ANYID_REF_LVALUE_TS = { lvalue_type, ref_type, anyid_type };
    ANYID_WEAKREF_TS = { weakref_type, anyid_type };
    ANYID_WEAKREF_LVALUE_TS = { lvalue_type, weakref_type, anyid_type };
    ANY_UNINITIALIZED_TS = { uninitialized_type, any_type };
    ANY_ARRAY_REF_TS = { ref_type, array_type, any_type };
    ANY_ARRAY_REF_LVALUE_TS = { lvalue_type, ref_type, array_type, any_type };
    SAME_ARRAY_REF_LVALUE_TS = { lvalue_type, ref_type, array_type, same_type };
    ANY_CIRCULARRAY_REF_TS = { ref_type, circularray_type, any_type };
    ANY_CIRCULARRAY_REF_LVALUE_TS = { lvalue_type, ref_type, circularray_type, any_type };
    SAME_CIRCULARRAY_REF_LVALUE_TS = { lvalue_type, ref_type, circularray_type, same_type };
    ANY_RBTREE_REF_TS = { ref_type, rbtree_type, any_type };
    ANY_RBTREE_REF_LVALUE_TS = { lvalue_type, ref_type, rbtree_type, any_type };
    SAME_RBTREE_REF_LVALUE_TS = { lvalue_type, ref_type, rbtree_type, same_type };
    SAME_SAME2_ITEM_RBTREE_REF_LVALUE_TS = { lvalue_type, ref_type, rbtree_type, item_type, same_type, same2_type };
    VOID_CODE_TS = { code_type, void_type };
    BOOLEAN_CODE_TS = { code_type, boolean_type };
    STREAMIFIABLE_TS = { streamifiable_type };
    ANY_ITERATOR_TS = { iterator_type, any_type };
    SAME_ITERATOR_TS = { iterator_type, same_type };
    INTEGER_ITERATOR_TS = { iterator_type, integer_type };
    ANY_ITERABLE_TS = { iterable_type, any_type };
    SAME_ITERABLE_TS = { iterable_type, same_type };
    STRING_TS = { string_type };
    STRING_LVALUE_TS = { lvalue_type, string_type };
    ANY_OPTION_TS = { option_type, any_type };
    ANY_OPTION_LVALUE_TS = { lvalue_type, option_type, any_type };
    ANY_STACK_REF_TS = { ref_type, stack_type, any_type };
    ANY_QUEUE_REF_TS = { ref_type, queue_type, any_type };
    ANY_SET_REF_TS = { ref_type, set_type, any_type };
    ANY_ANY2_MAP_REF_TS = { ref_type, map_type, any_type, any2_type };
    ANY_STACK_WEAKREF_TS = { weakref_type, stack_type, any_type };
    ANY_QUEUE_WEAKREF_TS = { weakref_type, queue_type, any_type };
    ANY_SET_WEAKREF_TS = { weakref_type, set_type, any_type };
    ANY_ANY2_MAP_WEAKREF_TS = { weakref_type, map_type, any_type, any2_type };
    COUNTUP_TS = { countup_type };
    COUNTDOWN_TS = { countdown_type };
    ANY_ANY2_ITEM_TS = { item_type, any_type, any2_type };
    SAME_SAME2_ITEM_TS = { item_type, same_type };
    INTEGER_SAME_ITEM_TS = { item_type, integer_type, same_type };
    SAME_ARRAYELEMITER_TS = { arrayelemiter_type, same_type };
    SAME_ARRAYINDEXITER_TS = { arrayindexiter_type, same_type };
    SAME_ARRAYITEMITER_TS = { arrayitemiter_type, same_type };
    SAME_CIRCULARRAYELEMITER_TS = { circularrayelemiter_type, same_type };
    SAME_CIRCULARRAYINDEXITER_TS = { circularrayindexiter_type, same_type };
    SAME_CIRCULARRAYITEMITER_TS = { circularrayitemiter_type, same_type };
    SAME_RBTREEELEMBYAGEITER_TS = { rbtreeelembyageiter_type, same_type };
    SAME_RBTREEELEMBYORDERITER_TS = { rbtreeelembyorderiter_type, same_type };
}


void implement(Scope *implementor_scope, TypeSpec interface_ts, std::string implementation_name, std::vector<Declaration *> contents) {
    TypeSpec implementor_ts = implementor_scope->pivot_type_hint();
    ImplementationType *implementation = new ImplementationType(implementation_name, implementor_ts, interface_ts);
    DataScope *inner_scope = implementation->make_inner_scope(implementor_ts);
    
    for (Declaration *d : contents)
        inner_scope->add(d);
    
    implementation->complete_type();
    implementor_scope->add(implementation);
}


void define_integers() {
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

    Scope *integer_scope = integer_metatype->get_inner_scope(TypeMatch());
    
    for (auto &item : integer_rvalue_operations)
        integer_scope->add(new TemplateOperation<IntegerOperationValue>(item.name, ANY_TS, item.operation));

    for (auto &item : integer_lvalue_operations)
        integer_scope->add(new TemplateOperation<IntegerOperationValue>(item.name, ANY_LVALUE_TS, item.operation));

    implement(integer_scope, STREAMIFIABLE_TS, "sable", {
        new ImportedFunction("streamify_integer", "streamify", INTEGER_TS, TSs { STRING_LVALUE_TS }, Ss { "stream" }, TSs {}, NULL)
    });
    
    integer_scope->add(new TemplateIdentifier<CountupValue>("countup", INTEGER_TS));
    integer_scope->add(new TemplateIdentifier<CountdownValue>("countdown", INTEGER_TS));
}


void define_interfaces() {
    // Streamifiable interface
    DataScope *sis = streamifiable_type->make_inner_scope(STREAMIFIABLE_TS);
    Function *sf = new Function("streamify",
        STREAMIFIABLE_TS,
        TSs { STRING_LVALUE_TS },
        Ss { "stream" },
        TSs {},
        NULL
    );
    sf->be_interface_function();
    sis->add(sf);
    streamifiable_type->complete_type();
    
    // Iterable interface
    DataScope *jis = iterable_type->make_inner_scope(ANY_ITERABLE_TS);
    Function *xf = new Function("iter",
        ANY_ITERABLE_TS,
        TSs {},
        Ss {},
        TSs { SAME_ITERATOR_TS },
        NULL
    );
    xf->be_interface_function();
    jis->add(xf);
    iterable_type->complete_type();

    // Iterator interface
    DataScope *iis = iterator_type->make_inner_scope(ANY_ITERATOR_TS);
    Function *nf = new Function("next",
        ANY_ITERATOR_TS,
        TSs {},
        Ss {},
        TSs { SAME_TS },
        NULL
    );
    nf->be_interface_function();
    iis->add(nf);
    implement(iis, SAME_ITERABLE_TS, "ible", {
        // This must return the concrete type, so the pivot type must be Any so that no
        // conversion to an interface happens, which would hide the concrete type.
        // TODO: why would an ANY_ITERATOR_TS pivot break anything?
        new Identity("iter", ANY_TS)
    });
    iterator_type->complete_type();
}

template <typename NextValue>
void define_container_iterator(Type *iter_type, Type *container_type, TypeSpec interface_ts) {
    TypeSpec PIVOT_TS = { iter_type, any_type };
    TypeSpec SAME_CONTAINER_REF_LVALUE_TS = { lvalue_type, ref_type, container_type, same_type };
    
    DataScope *aiis = iter_type->make_inner_scope(PIVOT_TS);

    // Order matters!
    aiis->add(new Variable("container", PIVOT_TS, SAME_CONTAINER_REF_LVALUE_TS));
    aiis->add(new Variable("value", PIVOT_TS, INTEGER_LVALUE_TS));

    implement(aiis, interface_ts, "iterator", {
        new TemplateIdentifier<NextValue>("next", PIVOT_TS)
    });
    
    iter_type->complete_type();
}


void define_iterators() {
    // Counter operations
    for (auto is_down : { false, true }) {
        RecordType *counter_type = dynamic_cast<RecordType *>(is_down ? countdown_type : countup_type);
        TypeSpec COUNTER_TS = { counter_type };
    
        DataScope *cis = counter_type->make_inner_scope(COUNTER_TS);
    
        cis->add(new Variable("limit", COUNTER_TS, INTEGER_LVALUE_TS));  // Order matters!
        cis->add(new Variable("value", COUNTER_TS, INTEGER_LVALUE_TS));
        Declaration *next_fn;
        
        if (!is_down) {
            next_fn = new TemplateIdentifier<CountupNextValue>("next", COUNTER_TS);
        }
        else {
            next_fn = new TemplateIdentifier<CountdownNextValue>("next", COUNTER_TS);
        }

        implement(cis, INTEGER_ITERATOR_TS, "iter", { next_fn });
        
        counter_type->complete_type();
    }

    // Item type for itemized iteration
    RecordType *item_type = dynamic_cast<RecordType *>(::item_type);
    DataScope *itis = item_type->make_inner_scope(ANY_ANY2_ITEM_TS);

    itis->add(new Variable("index", ANY_ANY2_ITEM_TS, SAME_LVALUE_TS));  // Order matters!
    itis->add(new Variable("value", ANY_ANY2_ITEM_TS, SAME2_LVALUE_TS));
    
    item_type->complete_type();
    
    TypeSpec INTEGER_SAME_ITEM_ITERATOR_TS = { iterator_type, item_type, integer_type, same_type };

    // Array Iterator operations
    define_container_iterator<ArrayNextElemValue>(arrayelemiter_type, array_type, SAME_ITERATOR_TS);
    define_container_iterator<ArrayNextIndexValue>(arrayindexiter_type, array_type, INTEGER_ITERATOR_TS);
    define_container_iterator<ArrayNextItemValue>(arrayitemiter_type, array_type, INTEGER_SAME_ITEM_ITERATOR_TS);

    // Circularray Iterator operations
    define_container_iterator<CircularrayNextElemValue>(circularrayelemiter_type, circularray_type, SAME_ITERATOR_TS);
    define_container_iterator<CircularrayNextIndexValue>(circularrayindexiter_type, circularray_type, INTEGER_ITERATOR_TS);
    define_container_iterator<CircularrayNextItemValue>(circularrayitemiter_type, circularray_type, INTEGER_SAME_ITEM_ITERATOR_TS);

    // Rbtree Iterator operations
    define_container_iterator<RbtreeNextElemByAgeValue>(rbtreeelembyageiter_type, rbtree_type, SAME_ITERATOR_TS);
    define_container_iterator<RbtreeNextElemByOrderValue>(rbtreeelembyorderiter_type, rbtree_type, SAME_ITERATOR_TS);
}


void define_string() {
    RecordType *record_type = dynamic_cast<RecordType *>(string_type);
    DataScope *is = record_type->make_inner_scope(STRING_TS);

    is->add(new Variable("chars", STRING_TS, CHARACTER_ARRAY_REF_LVALUE_TS));  // Order matters!

    is->add(new RecordWrapperIdentifier("length", STRING_TS, CHARACTER_ARRAY_REF_TS, INTEGER_TS, "length"));
    is->add(new RecordWrapperIdentifier("binary_plus", STRING_TS, CHARACTER_ARRAY_REF_TS, STRING_TS, "binary_plus", "chars"));
    is->add(new RecordWrapperIdentifier("index", STRING_TS, CHARACTER_ARRAY_REF_TS, CHARACTER_TS, "index"));
    is->add(new RecordWrapperIdentifier("realloc", STRING_LVALUE_TS, CHARACTER_ARRAY_REF_LVALUE_TS, STRING_LVALUE_TS, "realloc"));

    is->add(new TemplateOperation<RecordOperationValue>("assign other", STRING_LVALUE_TS, ASSIGN));
    is->add(new TemplateIdentifier<StringEqualityValue>("is_equal", STRING_TS));

    implement(is, TypeSpec { iterable_type, character_type }, "ible", {
        new RecordWrapperIdentifier("iter", STRING_TS, CHARACTER_ARRAY_REF_TS, TypeSpec { arrayelemiter_type, character_type }, "elements")
    });

    is->add(new RecordWrapperIdentifier("elements", STRING_TS, CHARACTER_ARRAY_REF_TS, TypeSpec { arrayelemiter_type, character_type }, "elements"));
    is->add(new RecordWrapperIdentifier("indexes", STRING_TS, CHARACTER_ARRAY_REF_TS, TypeSpec { arrayindexiter_type, character_type }, "indexes"));
    is->add(new RecordWrapperIdentifier("items", STRING_TS, CHARACTER_ARRAY_REF_TS, TypeSpec { arrayitemiter_type, character_type }, "items"));

    is->add(new TemplateOperation<RecordOperationValue>("compare", ANY_TS, COMPARE));

    // String operations
    implement(is, STREAMIFIABLE_TS, "sable", {
        new TemplateIdentifier<StringStreamificationValue>("streamify", STRING_TS)
    });

    record_type->complete_type();
}


void define_option() {
    //OptionType *option_type = dynamic_cast<OptionType *>(::option_type);
    DataScope *is = option_type->make_inner_scope(ANY_OPTION_TS);

    is->add(new TemplateOperation<OptionOperationValue>("assign other", ANY_OPTION_LVALUE_TS, ASSIGN));
    is->add(new TemplateOperation<OptionOperationValue>("compare", ANY_OPTION_TS, COMPARE));

    implement(is, STREAMIFIABLE_TS, "sable", {
        new TemplateIdentifier<OptionStreamificationValue>("streamify", ANY_OPTION_TS)
    });

    option_type->complete_type();
}


void define_array() {
    Scope *array_scope = array_type->get_inner_scope(TypeMatch());

    array_scope->add(new TemplateIdentifier<ArrayLengthValue>("length", ANY_ARRAY_REF_TS));
    array_scope->add(new TemplateOperation<ArrayReallocValue>("realloc", ANY_ARRAY_REF_TS, TWEAK));
    array_scope->add(new TemplateIdentifier<ArrayConcatenationValue>("binary_plus", ANY_ARRAY_REF_TS));
    array_scope->add(new TemplateOperation<ArrayIndexValue>("index", ANY_ARRAY_REF_TS, TWEAK));
    array_scope->add(new TemplateIdentifier<ArraySortValue>("sort", ANY_ARRAY_REF_TS));
    array_scope->add(new TemplateIdentifier<ArrayPushValue>("push", ANY_ARRAY_REF_TS));
    array_scope->add(new TemplateIdentifier<ArrayPopValue>("pop", ANY_ARRAY_REF_TS));
    array_scope->add(new TemplateIdentifier<ArrayAutogrowValue>("autogrow", ANY_ARRAY_REF_LVALUE_TS));
    
    // Array iterable operations
    implement(array_scope, SAME_ITERABLE_TS, "ible", {
        new TemplateIdentifier<ArrayElemIterValue>("iter", ANY_ARRAY_REF_TS)
    });

    array_scope->add(new TemplateIdentifier<ArrayElemIterValue>("elements", ANY_ARRAY_REF_TS));
    array_scope->add(new TemplateIdentifier<ArrayIndexIterValue>("indexes", ANY_ARRAY_REF_TS));
    array_scope->add(new TemplateIdentifier<ArrayItemIterValue>("items", ANY_ARRAY_REF_TS));
}


void define_circularray() {
    Scope *circularray_scope = circularray_type->get_inner_scope(TypeMatch());

    circularray_scope->add(new TemplateIdentifier<CircularrayLengthValue>("length", ANY_CIRCULARRAY_REF_TS));
    //circularray_scope->add(new TemplateOperation<ArrayReallocValue>("realloc", ANY_ARRAY_REF_TS, TWEAK));
    //circularray_scope->add(new TemplateIdentifier<ArrayConcatenationValue>("binary_plus", ANY_ARRAY_REF_TS));
    circularray_scope->add(new TemplateOperation<CircularrayIndexValue>("index", ANY_CIRCULARRAY_REF_TS, TWEAK));
    //circularray_scope->add(new TemplateIdentifier<ArraySortValue>("sort", ANY_ARRAY_REF_TS));
    circularray_scope->add(new TemplateIdentifier<CircularrayPushValue>("push", ANY_CIRCULARRAY_REF_TS));
    circularray_scope->add(new TemplateIdentifier<CircularrayPopValue>("pop", ANY_CIRCULARRAY_REF_TS));
    circularray_scope->add(new TemplateIdentifier<CircularrayUnshiftValue>("unshift", ANY_CIRCULARRAY_REF_TS));
    circularray_scope->add(new TemplateIdentifier<CircularrayShiftValue>("shift", ANY_CIRCULARRAY_REF_TS));
    circularray_scope->add(new TemplateIdentifier<CircularrayAutogrowValue>("autogrow", ANY_CIRCULARRAY_REF_LVALUE_TS));
    
    // Circularray iterable operations
    implement(circularray_scope, SAME_ITERABLE_TS, "ible", {
        new TemplateIdentifier<CircularrayElemIterValue>("iter", ANY_CIRCULARRAY_REF_TS)
    });

    circularray_scope->add(new TemplateIdentifier<CircularrayElemIterValue>("elements", ANY_CIRCULARRAY_REF_TS));
    circularray_scope->add(new TemplateIdentifier<CircularrayIndexIterValue>("indexes", ANY_CIRCULARRAY_REF_TS));
    circularray_scope->add(new TemplateIdentifier<CircularrayItemIterValue>("items", ANY_CIRCULARRAY_REF_TS));
}


void define_rbtree() {
    Scope *rbtree_scope = rbtree_type->get_inner_scope(TypeMatch());

    rbtree_scope->add(new TemplateIdentifier<RbtreeLengthValue>("length", ANY_RBTREE_REF_TS));
    //array_scope->add(new TemplateOperation<ArrayItemValue>("index", ANY_ARRAY_REF_TS, TWEAK));
    rbtree_scope->add(new TemplateIdentifier<RbtreeHasValue>("has", ANY_RBTREE_REF_TS));
    rbtree_scope->add(new TemplateIdentifier<RbtreeAddValue>("add", ANY_RBTREE_REF_TS));
    rbtree_scope->add(new TemplateIdentifier<RbtreeRemoveValue>("remove", ANY_RBTREE_REF_TS));
    rbtree_scope->add(new TemplateIdentifier<RbtreeAutogrowValue>("autogrow", ANY_RBTREE_REF_LVALUE_TS));

    // Rbtree iterable operations
    rbtree_scope->add(new TemplateIdentifier<RbtreeElemByAgeIterValue>("elements_by_age", ANY_RBTREE_REF_TS));
    rbtree_scope->add(new TemplateIdentifier<RbtreeElemByOrderIterValue>("elements_by_order", ANY_RBTREE_REF_TS));
}


void define_stack() {
    TypeSpec PIVOT = ANY_STACK_WEAKREF_TS;
    TypeSpec CAST = SAME_ARRAY_REF_LVALUE_TS;
    
    ClassType *class_type = dynamic_cast<ClassType *>(stack_type);
    DataScope *is = class_type->make_inner_scope(PIVOT);

    is->add(new Variable("array", PIVOT, CAST));

    is->add(new ClassWrapperIdentifier("length", PIVOT, CAST, "length"));
    is->add(new ClassWrapperIdentifier("index", PIVOT, CAST, "index"));
    is->add(new ClassWrapperIdentifier("realloc", PIVOT, CAST, "realloc"));

    implement(is, TypeSpec { iterable_type, same_type }, "ible", {
        new ClassWrapperIdentifier("iter", PIVOT, CAST, "elements")
    });

    is->add(new ClassWrapperIdentifier("elements", PIVOT, CAST, "elements"));
    is->add(new ClassWrapperIdentifier("indexes", PIVOT, CAST, "indexes"));
    is->add(new ClassWrapperIdentifier("items", PIVOT, CAST, "items"));

    is->add(new ClassWrapperIdentifier("push", PIVOT, CAST, "push", true));
    is->add(new ClassWrapperIdentifier("pop", PIVOT, CAST, "pop"));

    class_type->complete_type();
}


void define_queue() {
    TypeSpec PIVOT = ANY_QUEUE_WEAKREF_TS;
    TypeSpec CAST = SAME_CIRCULARRAY_REF_LVALUE_TS;
    
    ClassType *class_type = dynamic_cast<ClassType *>(queue_type);
    DataScope *is = class_type->make_inner_scope(PIVOT);

    is->add(new Variable("carray", PIVOT, CAST));

    is->add(new ClassWrapperIdentifier("length", PIVOT, CAST, "length"));
    is->add(new ClassWrapperIdentifier("index", PIVOT, CAST, "index"));
    is->add(new ClassWrapperIdentifier("realloc", PIVOT, CAST, "realloc"));

    implement(is, TypeSpec { iterable_type, same_type }, "ible", {
        new ClassWrapperIdentifier("iter", PIVOT, CAST, "elements")
    });

    is->add(new ClassWrapperIdentifier("elements", PIVOT, CAST, "elements"));
    is->add(new ClassWrapperIdentifier("indexes", PIVOT, CAST, "indexes"));
    is->add(new ClassWrapperIdentifier("items", PIVOT, CAST, "items"));

    is->add(new ClassWrapperIdentifier("push", PIVOT, CAST, "push", true));
    is->add(new ClassWrapperIdentifier("unshift", PIVOT, CAST, "unshift", true));
    is->add(new ClassWrapperIdentifier("pop", PIVOT, CAST, "pop"));
    is->add(new ClassWrapperIdentifier("shift", PIVOT, CAST, "shift"));

    class_type->complete_type();
}


void define_set() {
    TypeSpec PIVOT = ANY_SET_WEAKREF_TS;
    TypeSpec CAST = SAME_RBTREE_REF_LVALUE_TS;
    
    ClassType *class_type = dynamic_cast<ClassType *>(set_type);
    DataScope *is = class_type->make_inner_scope(PIVOT);

    is->add(new Variable("tree", PIVOT, CAST));

    is->add(new ClassWrapperIdentifier("length", PIVOT, CAST, "length"));
    is->add(new ClassWrapperIdentifier("has", PIVOT, CAST, "has"));
    is->add(new ClassWrapperIdentifier("add", PIVOT, CAST, "add", true));
    is->add(new ClassWrapperIdentifier("remove", PIVOT, CAST, "remove"));

    //implement(is, TypeSpec { iterable_type, same_type }, "ible", {
    //    new ClassWrapperIdentifier("iter", PIVOT, CAST, "elements")
    //});

    is->add(new ClassWrapperIdentifier("elements_by_age", PIVOT, CAST, "elements_by_age"));
    is->add(new ClassWrapperIdentifier("elements_by_order", PIVOT, CAST, "elements_by_order"));

    class_type->complete_type();
}


void define_map() {
    TypeSpec PIVOT = ANY_ANY2_MAP_WEAKREF_TS;
    TypeSpec CAST = SAME_SAME2_ITEM_RBTREE_REF_LVALUE_TS;
    
    ClassType *class_type = dynamic_cast<ClassType *>(map_type);
    DataScope *is = class_type->make_inner_scope(PIVOT);

    is->add(new Variable("tree", PIVOT, CAST));
    
    is->add(new ClassWrapperIdentifier("length", PIVOT, CAST, "length"));
    is->add(new TemplateIdentifier<MapAddValue>("add", PIVOT));
    is->add(new TemplateIdentifier<MapRemoveValue>("remove", PIVOT));
    is->add(new TemplateIdentifier<MapIndexValue>("index", PIVOT));
    
    class_type->complete_type();
}


void builtin_runtime(Scope *root_scope) {
    TSs NO_TSS = { };
    TSs INTEGER_TSS = { INTEGER_TS };
    TSs BOOLEAN_TSS = { BOOLEAN_TS };
    TSs UNSIGNED_INTEGER8_TSS = { UNSIGNED_INTEGER8_TS };
    TSs UNSIGNED_INTEGER8_ARRAY_REF_TSS = { UNSIGNED_INTEGER8_ARRAY_REF_TS };
    TSs CHARACTER_ARRAY_REF_TSS = { CHARACTER_ARRAY_REF_TS };

    Ss no_names = { };
    Ss value_names = { "value" };

    root_scope->add(new ImportedFunction("printi", "printi", NO_TS, INTEGER_TSS, value_names, NO_TSS, NULL));
    root_scope->add(new ImportedFunction("printc", "printc", NO_TS, UNSIGNED_INTEGER8_TSS, value_names, NO_TSS, NULL));
    root_scope->add(new ImportedFunction("printb", "printb", NO_TS, UNSIGNED_INTEGER8_ARRAY_REF_TSS, value_names, NO_TSS, NULL));
    root_scope->add(new ImportedFunction("prints", "prints", NO_TS, TSs { STRING_TS }, value_names, NO_TSS, NULL));
    root_scope->add(new ImportedFunction("decode_utf8", "decode_utf8", UNSIGNED_INTEGER8_ARRAY_REF_TS, NO_TSS, no_names, TSs { STRING_TS }, NULL));
    root_scope->add(new ImportedFunction("encode_utf8", "encode_utf8", STRING_TS, NO_TSS, no_names, TSs { UNSIGNED_INTEGER8_ARRAY_REF_TS }, NULL));

    root_scope->add(new ImportedFunction("stringify_integer", "stringify", INTEGER_TS, NO_TSS, no_names, TSs { STRING_TS }, NULL));
}


Scope *init_builtins() {
    Scope *root_scope = new Scope(ROOT_SCOPE);

    builtin_types(root_scope);

    define_string();
    define_interfaces();
    define_iterators();
    define_stack();
    define_queue();
    define_set();
    define_map();
    define_option();
    
    // Integer operations
    define_integers();
        
    // Character operations
    Scope *char_scope = character_type->get_inner_scope(TypeMatch());
    char_scope->add(new TemplateOperation<IntegerOperationValue>("assign other", CHARACTER_LVALUE_TS, ASSIGN));
    char_scope->add(new TemplateOperation<IntegerOperationValue>("compare", CHARACTER_TS, COMPARE));
    implement(char_scope, STREAMIFIABLE_TS, "sable", {
        new TemplateIdentifier<CharacterStreamificationValue>("streamify", CHARACTER_TS)
    });
    
    // Boolean operations
    Scope *bool_scope = boolean_type->get_inner_scope(TypeMatch());
    bool_scope->add(new TemplateOperation<BooleanOperationValue>("assign other", BOOLEAN_LVALUE_TS, ASSIGN));
    bool_scope->add(new TemplateOperation<BooleanOperationValue>("compare", BOOLEAN_TS, COMPARE));
    implement(bool_scope, STREAMIFIABLE_TS, "sable", {
        new ImportedFunction("streamify_boolean", "streamify", BOOLEAN_TS, TSs { STRING_LVALUE_TS }, Ss { "stream" }, TSs {}, NULL)
    });

    // Logical operations, unscoped
    root_scope->add(new TemplateIdentifier<BooleanNotValue>("logical not", BOOLEAN_TS));
    root_scope->add(new TemplateIdentifier<BooleanAndValue>("logical and", BOOLEAN_TS));
    root_scope->add(new TemplateIdentifier<BooleanOrValue>("logical or", ANY_TS));

    // Enum operations
    Scope *enum_scope = enumeration_metatype->get_inner_scope(TypeMatch());
    enum_scope->add(new TemplateOperation<IntegerOperationValue>("assign other", ANY_LVALUE_TS, ASSIGN));
    enum_scope->add(new TemplateOperation<IntegerOperationValue>("is_equal", ANY_TS, EQUAL));
    implement(enum_scope, STREAMIFIABLE_TS, "sable", {
        new TemplateIdentifier<EnumStreamificationValue>("streamify", ANY_TS)
    });

    // Treenum operations
    Scope *treenum_scope = treenumeration_metatype->get_inner_scope(TypeMatch());
    treenum_scope->add(new TemplateOperation<IntegerOperationValue>("assign other", ANY_LVALUE_TS, ASSIGN));
    treenum_scope->add(new TemplateOperation<IntegerOperationValue>("is_equal", ANY_TS, EQUAL));
    implement(treenum_scope, STREAMIFIABLE_TS, "sable", {
        new TemplateIdentifier<EnumStreamificationValue>("streamify", ANY_TS)
    });

    // Record operations
    Scope *record_scope = record_metatype->get_inner_scope(TypeMatch());
    record_scope->add(new TemplateOperation<RecordOperationValue>("assign other", ANY_LVALUE_TS, ASSIGN));

    // Reference operations, unscoped
    typedef TemplateOperation<ReferenceOperationValue> ReferenceOperation;
    root_scope->add(new ReferenceOperation("assign other", ANYID_REF_LVALUE_TS, ASSIGN));
    root_scope->add(new ReferenceOperation("is_equal", ANYID_REF_TS, EQUAL));
    root_scope->add(new ReferenceOperation("not_equal", ANYID_REF_TS, NOT_EQUAL));

    typedef TemplateOperation<WeakreferenceOperationValue> WeakreferenceOperation;
    root_scope->add(new WeakreferenceOperation("assign other", ANYID_WEAKREF_LVALUE_TS, ASSIGN));
    root_scope->add(new WeakreferenceOperation("is_equal", ANYID_WEAKREF_TS, EQUAL));
    root_scope->add(new WeakreferenceOperation("not_equal", ANYID_WEAKREF_TS, NOT_EQUAL));

    // Array operations
    define_array();
    
    // Circularray operations
    define_circularray();

    // Rbtree operations
    define_rbtree();
    
    // Unpacking
    root_scope->add(new TemplateIdentifier<UnpackingValue>("assign other", MULTI_LVALUE_TS));
    
    // Initialization of value types
    root_scope->add(new TemplateIdentifier<CreateValue>("create from", ANY_UNINITIALIZED_TS));
    
    // Builtin controls, unscoped
    root_scope->add(new TemplateOperation<IfValue>(":if", NO_TS, TWEAK));
    root_scope->add(new TemplateIdentifier<RepeatValue>(":repeat", NO_TS));
    root_scope->add(new TemplateIdentifier<ForEachValue>(":for", NO_TS));
    root_scope->add(new TemplateIdentifier<SwitchValue>(":switch", NO_TS));
    root_scope->add(new TemplateIdentifier<RaiseValue>(":raise", NO_TS));
    root_scope->add(new TemplateIdentifier<TryValue>(":try", NO_TS));
    root_scope->add(new TemplateIdentifier<IsValue>(":is", NO_TS));
    root_scope->add(new TemplateOperation<FunctionReturnValue>(":return", NO_TS, TWEAK));

    root_scope->add(new TemplateIdentifier<ReferenceWeakenValue>(":weak", NO_TS));

    root_scope->add(new TemplateIdentifier<FunctionDefinitionValue>(":Function", NO_TS));
    root_scope->add(new TemplateIdentifier<InitializerDefinitionValue>(":Initializer", NO_TS));
    
    // Library functions, unscoped
    builtin_runtime(root_scope);

    return root_scope;
}

