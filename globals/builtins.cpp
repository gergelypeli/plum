
void builtin_types(Scope *root_scope) {

    // Phase 1: declare the hypertype

    metatype_hypertype = new MetaType("<Meta>", {}, NULL);
    root_scope->add(metatype_hypertype);


    // Phase 2: declare the metatypes

    // Abstract metatypes can't be looked up, so their scope doesn't matter
    type_metatype = new MetaType("<Type>", {}, NULL);
    root_scope->add(type_metatype);

    value_metatype = new MetaType("<Value>", { type_metatype }, NULL);
    root_scope->add(value_metatype);

    identity_metatype = new MetaType("<Identity>", { type_metatype }, NULL);
    root_scope->add(identity_metatype);

    module_metatype = new MetaType("<Module>", { type_metatype }, NULL);
    root_scope->add(module_metatype);

    attribute_metatype = new MetaType("<Attribute>", { type_metatype }, NULL);
    root_scope->add(attribute_metatype);
    
    // Here comes a bit of a twist (UnitType needed value_metatype)
    colon_type = new UnitType("Colon");  // NOTE: Unit is a value type
    root_scope->add(colon_type);
    colon_scope = colon_type->make_inner_scope({ colon_type });

    // User accessible metatypes go into the colon scope
    integer_metatype = new MetaType("Integer", { value_metatype }, make<IntegerDefinitionValue>);
    colon_scope->add(integer_metatype);

    enumeration_metatype = new MetaType("Enumeration", { value_metatype }, make<EnumerationDefinitionValue>);
    colon_scope->add(enumeration_metatype);

    treenumeration_metatype = new MetaType("Treenumeration", { value_metatype }, make<TreenumerationDefinitionValue>);
    colon_scope->add(treenumeration_metatype);

    record_metatype = new MetaType("Record", { value_metatype }, make<RecordDefinitionValue>);
    colon_scope->add(record_metatype);

    class_metatype = new MetaType("Class", { identity_metatype }, make<ClassDefinitionValue>);
    colon_scope->add(class_metatype);

    interface_metatype = new MetaType("Interface", { value_metatype, identity_metatype }, make<InterfaceDefinitionValue>);
    colon_scope->add(interface_metatype);

    import_metatype = new MetaType("Import", { type_metatype }, make<ImportDefinitionValue>);
    colon_scope->add(import_metatype);


    // Phase 3: declare wildcard types, so subsequent types can have an inner scope

    any_type = new AnyType("<Any>", {}, value_metatype);
    root_scope->add(any_type);

    any2_type = new AnyType("<Any2>", {}, value_metatype);
    root_scope->add(any2_type);

    any3_type = new AnyType("<Any3>", {}, value_metatype);
    root_scope->add(any3_type);

    anyid_type = new AnyType("<Anyid>", {}, identity_metatype);
    root_scope->add(anyid_type);

    anyid2_type = new AnyType("<Anyid2>", {}, identity_metatype);
    root_scope->add(anyid2_type);

    anyid3_type = new AnyType("<Anyid3>", {}, identity_metatype);
    root_scope->add(anyid3_type);

    same_type = new SameType("<Same>", {}, value_metatype);
    root_scope->add(same_type);

    same2_type = new SameType("<Same2>", {}, value_metatype);
    root_scope->add(same2_type);

    same3_type = new SameType("<Same3>", {}, value_metatype);
    root_scope->add(same3_type);

    sameid_type = new SameType("<Sameid>", {}, identity_metatype);
    root_scope->add(sameid_type);

    sameid2_type = new SameType("<Sameid2>", {}, identity_metatype);
    root_scope->add(sameid2_type);

    sameid3_type = new SameType("<Sameid3>", {}, identity_metatype);
    root_scope->add(sameid3_type);


    // Phase 4: declare special types

    unit_type = new UnitType("<Unit>");
    root_scope->add(unit_type);

    void_type = new VoidType("<Void>");
    root_scope->add(void_type);

    multi_type = new MultiType("<Multi>");
    root_scope->add(multi_type);

    multilvalue_type = new MultiType("<Multilvalue>");
    root_scope->add(multilvalue_type);

    multitype_type = new MultiType("<Multitype>");
    root_scope->add(multitype_type);

    uninitialized_type = new UninitializedType("<Uninitialized>");
    root_scope->add(uninitialized_type);

    initializable_type = new InitializableType("<Initializable>");
    root_scope->add(initializable_type);

    stringtemplate_type = new StringtemplateType("<Stringtemplate>");
    root_scope->add(stringtemplate_type);

    lvalue_type = new AttributeType("Lvalue");
    root_scope->add(lvalue_type);
    
    ovalue_type = new AttributeType("Ovalue");
    root_scope->add(ovalue_type);

    dvalue_type = new AttributeType("Dvalue");
    root_scope->add(dvalue_type);

    code_type = new AttributeType("Code");
    root_scope->add(code_type);

    rvalue_type = new AttributeType("Rvalue", interface_metatype);
    root_scope->add(rvalue_type);

    whatever_type = new WhateverType("<Whatever>");
    root_scope->add(whatever_type);


    // Phase 5: declare basic types

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
    
    unsigned_integer8_type = new IntegerType("Byte", 1, true);
    root_scope->add(unsigned_integer8_type);

    float_type = new FloatType("Float");
    root_scope->add(float_type);

    ref_type = new ReferenceType("Ref");
    root_scope->add(ref_type);

    ptr_type = new PointerType("Ptr");
    root_scope->add(ptr_type);

    nosyvalue_type = new NosyValueType("<NosyValue>");
    root_scope->add(nosyvalue_type);

    nosytree_type = new NosytreeType("<Nosytree>");
    root_scope->add(nosytree_type);

    nosyref_type = new NosyrefType("<Nosyref>");
    root_scope->add(nosyref_type);

    weakref_type = new WeakrefType("Weakref");
    root_scope->add(weakref_type);

    partial_type = new PartialType("<Partial>");
    root_scope->add(partial_type);

    string_type = new StringType("String");
    root_scope->add(string_type);

    option_type = new OptionType("Option");
    root_scope->add(option_type);

    item_type = new ItemType("Item");
    root_scope->add(item_type);

    equalitymatcher_type = new EqualitymatcherType("<Equalitymatcher>");
    root_scope->add(equalitymatcher_type);


    // Phase 6: define interface and exception types
    
    streamifiable_type = new InterfaceType("Streamifiable", {});
    root_scope->add(streamifiable_type);

    iterator_type = new InterfaceType("Iterator", { value_metatype });
    root_scope->add(iterator_type);

    iterable_type = new InterfaceType("Iterable", { value_metatype });
    root_scope->add(iterable_type);

    application_type = new InterfaceType("Application", {});
    root_scope->add(application_type);

    iterator_done_exception_type = make_treenum("Iterator_done_exception", "ITERATOR_DONE");
    root_scope->add(iterator_done_exception_type);

    container_full_exception_type = make_treenum("Container_full_exception", "CONTAINER_FULL");
    root_scope->add(container_full_exception_type);

    container_empty_exception_type = make_treenum("Container_empty_exception", "CONTAINER_EMPTY");
    root_scope->add(container_empty_exception_type);

    container_lent_exception_type = make_treenum("Container_lent_exception", "CONTAINER_LENT");
    root_scope->add(container_lent_exception_type);

    match_unmatched_exception_type = make_treenum("Match_unmatched_exception", "UNMATCHED");
    root_scope->add(match_unmatched_exception_type);

    lookup_exception_type = make_treenum("Lookup_exception", "NOT_FOUND");
    root_scope->add(lookup_exception_type);

    code_break_exception_type = make_treenum("<Code_break_exception>", "CODE_BREAK");
    root_scope->add(code_break_exception_type);
    
    errno_exception_type = make_treenum("Errno_exception", errno_treenum_input);
    root_scope->add(errno_exception_type);
    

    // Phase 7: define container and iterator types

    linearray_type = new LinearrayType("Linearray");
    root_scope->add(linearray_type);

    circularray_type = new CircularrayType("Circularray");
    root_scope->add(circularray_type);

    rbtree_type = new RbtreeType("Rbtree");
    root_scope->add(rbtree_type);

    slice_type = new SliceType("Slice");
    root_scope->add(slice_type);

    array_type = new ArrayType("Array");
    root_scope->add(array_type);

    queue_type = new QueueType("Queue");
    root_scope->add(queue_type);

    countup_type = new RecordType("Countup", {});
    root_scope->add(countup_type);

    countdown_type = new RecordType("Countdown", {});
    root_scope->add(countdown_type);

    arrayelemiter_type = new RecordType("Arrayelem_iter", { value_metatype });
    root_scope->add(arrayelemiter_type);
    
    arrayindexiter_type = new RecordType("Arrayindex_iter", { value_metatype });
    root_scope->add(arrayindexiter_type);
    
    arrayitemiter_type = new RecordType("Arrayitem_iter", { value_metatype });
    root_scope->add(arrayitemiter_type);

    queueelemiter_type = new RecordType("Queueelem_iter", { value_metatype });
    root_scope->add(queueelemiter_type);
    
    queueindexiter_type = new RecordType("Queueindex_iter", { value_metatype });
    root_scope->add(queueindexiter_type);
    
    queueitemiter_type = new RecordType("Queueitem_iter", { value_metatype });
    root_scope->add(queueitemiter_type);

    setelembyageiter_type = new RecordType("Setelembyage_iter", { value_metatype });
    root_scope->add(setelembyageiter_type);

    setelembyorderiter_type = new RecordType("Setelembyorder_iter", { value_metatype });
    root_scope->add(setelembyorderiter_type);

    sliceelemiter_type = new RecordType("Sliceelem_iter", { value_metatype });
    root_scope->add(sliceelemiter_type);
    
    sliceindexiter_type = new RecordType("Sliceindex_iter", { value_metatype });
    root_scope->add(sliceindexiter_type);
    
    sliceitemiter_type = new RecordType("Sliceitem_iter", { value_metatype });
    root_scope->add(sliceitemiter_type);


    // Define TS
    
    // NO_TS will contain no Type pointers
    HYPERTYPE_TS = { metatype_hypertype };
    ANY_TS = { any_type };
    ANY_LVALUE_TS = { lvalue_type, any_type };
    ANY_OVALUE_TS = { ovalue_type, any_type };
    SAME_TS = { same_type };
    SAME_LVALUE_TS = { lvalue_type, same_type };
    SAME2_LVALUE_TS = { lvalue_type, same2_type };
    VOID_TS = { void_type };
    MULTI_TS = { multi_type };
    MULTILVALUE_TS = { multilvalue_type };
    MULTITYPE_TS = { multitype_type };
    UNIT_TS = { unit_type };
    WHATEVER_TS = { whatever_type };
    WHATEVER_CODE_TS = { code_type, whatever_type };
    BOOLEAN_TS = { boolean_type };
    INTEGER_TS = { integer_type };
    INTEGER_LVALUE_TS = { lvalue_type, integer_type };
    INTEGER_OVALUE_TS = { ovalue_type, integer_type };
    BOOLEAN_LVALUE_TS = { lvalue_type, boolean_type };
    UNSIGNED_INTEGER8_TS = { unsigned_integer8_type };
    UNSIGNED_INTEGER8_ARRAY_TS = { array_type, unsigned_integer8_type };
    UNTEGER_TS = { unsigned_integer_type };
    CHARACTER_TS = { character_type };
    CHARACTER_LVALUE_TS = { lvalue_type, character_type };
    CHARACTER_ARRAY_TS = { array_type, character_type };
    CHARACTER_ARRAY_LVALUE_TS = { lvalue_type, array_type, character_type };
    FLOAT_TS = { float_type };
    FLOAT_LVALUE_TS = { lvalue_type, float_type };
    ANYID_REF_TS = { ref_type, anyid_type };
    ANYID_REF_LVALUE_TS = { lvalue_type, ref_type, anyid_type };
    ANYID_PTR_TS = { ptr_type, anyid_type };
    ANYID_PTR_LVALUE_TS = { lvalue_type, ptr_type, anyid_type };
    ANYID_WEAKREF_TS = { weakref_type, anyid_type };
    ANYID_WEAKREF_LVALUE_TS = { lvalue_type, weakref_type, anyid_type };
    //SAMEID_NOSYVALUE_NOSYCONTAINER_REF_LVALUE_TS = { lvalue_type, ref_type, nosycontainer_type, nosyvalue_type, sameid_type };
    ANY_UNINITIALIZED_TS = { uninitialized_type, any_type };
    WHATEVER_UNINITIALIZED_TS = { uninitialized_type, whatever_type };
    STRINGTEMPLATE_TS = { stringtemplate_type };
    ANY_ARRAY_TS = { array_type, any_type };
    ANY_ARRAY_LVALUE_TS = { lvalue_type, array_type, any_type };
    SAME_LINEARRAY_REF_LVALUE_TS = { lvalue_type, ref_type, linearray_type, same_type };
    ANY_QUEUE_TS = { queue_type, any_type };
    ANY_QUEUE_LVALUE_TS = { lvalue_type, queue_type, any_type };
    SAME_CIRCULARRAY_REF_LVALUE_TS = { lvalue_type, ref_type, circularray_type, same_type };
    ANY_RBTREE_REF_TS = { ref_type, rbtree_type, any_type };
    ANY_RBTREE_REF_LVALUE_TS = { lvalue_type, ref_type, rbtree_type, any_type };
    SAME_RBTREE_REF_LVALUE_TS = { lvalue_type, ref_type, rbtree_type, same_type };
    SAME_SAME2_ITEM_RBTREE_REF_LVALUE_TS = { lvalue_type, ref_type, rbtree_type, item_type, same_type, same2_type };
    ANY_RBTREE_PTR_TS = { ptr_type, rbtree_type, any_type };
    VOID_CODE_TS = { code_type, void_type };
    BOOLEAN_CODE_TS = { code_type, boolean_type };
    STREAMIFIABLE_TS = { streamifiable_type };
    ANY_ITERATOR_TS = { iterator_type, any_type };
    SAME_ITERATOR_TS = { iterator_type, same_type };
    SAME_ITERATOR_RVALUE_TS = { rvalue_type, iterator_type, same_type };
    INTEGER_ITERATOR_TS = { iterator_type, integer_type };
    INTEGER_ITERABLE_TS = { iterable_type, integer_type };
    ANY_ITERABLE_TS = { iterable_type, any_type };
    SAME_ITERABLE_TS = { iterable_type, same_type };
    STRING_TS = { string_type };
    STRING_LVALUE_TS = { lvalue_type, string_type };
    STRING_ARRAY_TS = { array_type, string_type };
    ANY_SLICE_TS = { slice_type, any_type };
    BYTE_SLICE_TS = { slice_type, unsigned_integer8_type };
    ANY_OPTION_TS = { option_type, any_type };
    ANY_OPTION_LVALUE_TS = { lvalue_type, option_type, any_type };

    SAMEID_NOSYVALUE_LVALUE_TS = { lvalue_type, nosyvalue_type, sameid_type };

    SAME_SAMEID2_NOSYVALUE_ITEM_RBTREE_REF_LVALUE_TS = { lvalue_type, ref_type, rbtree_type, item_type, same_type, nosyvalue_type, sameid2_type };
    SAMEID_NOSYVALUE_SAME2_ITEM_RBTREE_REF_LVALUE_TS = { lvalue_type, ref_type, rbtree_type, item_type, nosyvalue_type, sameid_type, same2_type };
    SAMEID_NOSYVALUE_RBTREE_REF_LVALUE_TS = { lvalue_type, ref_type, rbtree_type, nosyvalue_type, sameid_type };

    SAMEID_NOSYVALUE_TS = { nosyvalue_type, sameid_type };
    SAMEID_NOSYVALUE_SAME2_ITEM_TS = { item_type, nosyvalue_type, sameid_type, same2_type };
    SAME_SAMEID2_NOSYVALUE_ITEM_TS = { item_type, same_type, nosyvalue_type, sameid2_type };

    COUNTUP_TS = { countup_type };
    COUNTDOWN_TS = { countdown_type };
    ANY_ANY2_ITEM_TS = { item_type, any_type, any2_type };
    SAME_SAME2_ITEM_TS = { item_type, same_type, same2_type };
    INTEGER_SAME_ITEM_TS = { item_type, integer_type, same_type };
    SAME_ARRAYELEMITER_TS = { arrayelemiter_type, same_type };
    SAME_ARRAYINDEXITER_TS = { arrayindexiter_type, same_type };
    SAME_ARRAYITEMITER_TS = { arrayitemiter_type, same_type };
    SAME_QUEUEELEMITER_TS = { queueelemiter_type, same_type };
    SAME_QUEUEINDEXITER_TS = { queueindexiter_type, same_type };
    SAME_QUEUEITEMITER_TS = { queueitemiter_type, same_type };
    SAME_SETELEMBYAGEITER_TS = { setelembyageiter_type, same_type };
    SAME_SETELEMBYORDERITER_TS = { setelembyorderiter_type, same_type };
    SAME_SLICEELEMITER_TS = { sliceelemiter_type, same_type };
    SAME_SLICEINDEXITER_TS = { sliceindexiter_type, same_type };
    SAME_SLICEITEMITER_TS = { sliceitemiter_type, same_type };
    COLON_TS = { colon_type };

    SAME_SAMEID2_NOSYVALUE_ITEM_RBTREE_REF_TS = { ref_type, rbtree_type, item_type, same_type, nosyvalue_type, sameid2_type };
    SAMEID_NOSYVALUE_SAME2_ITEM_RBTREE_REF_TS = { ref_type, rbtree_type, item_type, nosyvalue_type, sameid_type, same2_type };
    SAMEID_NOSYVALUE_RBTREE_REF_TS = { ref_type, rbtree_type,  nosyvalue_type, sameid_type };

}


void wrapped_types(Scope *root_scope) {
    set_type = new SetType("Set");
    root_scope->add(set_type);

    map_type = new MapType("Map");
    root_scope->add(map_type);

    weakvaluemap_type = new WeakValueMapType("WeakValueMap");
    root_scope->add(weakvaluemap_type);

    weakindexmap_type = new WeakIndexMapType("WeakIndexMap");
    root_scope->add(weakindexmap_type);
    
    weakset_type = new WeakSetType("WeakSet");
    root_scope->add(weakset_type);

    //ANY_ARRAY_TS = { array_type, any_type };
    //ANY_QUEUE_TS = { queue_type, any_type };
    ANY_SET_TS = { set_type, any_type };
    ANY_SET_LVALUE_TS = { lvalue_type, set_type, any_type };
    ANY_ANY2_MAP_TS = { map_type, any_type, any2_type };
    ANY_ANY2_MAP_LVALUE_TS = { lvalue_type, map_type, any_type, any2_type };
    //ANY_STACK_PTR_TS = { ptr_type, stack_type, any_type };
    //ANY_QUEUE_PTR_TS = { ptr_type, queue_type, any_type };
    ANY_ANYID2_WEAKVALUEMAP_TS = { weakvaluemap_type, any_type, anyid2_type };
    ANYID_WEAKSET_TS = { weakset_type, anyid_type };

    //SAME_SAMEID2_NOSYVALUE_MAP_PTR_TS = { ptr_type, map_type, same_type, nosyvalue_type, sameid2_type };

    ANYID_ANY2_WEAKINDEXMAP_TS = { weakindexmap_type, anyid_type, any2_type };
    //SAMEID_NOSYVALUE_SAME2_MAP_PTR_TS = { ptr_type, map_type, nosyvalue_type, sameid_type, same2_type };

    //SAMEID_NOSYVALUE_UNIT_MAP_PTR_TS = { ptr_type, map_type, nosyvalue_type, sameid_type, unit_type };
}


void implement(Scope *implementor_scope, TypeSpec interface_ts, std::string implementation_name, std::vector<Identifier *> contents, InheritAs ia = AS_AUTO) {
    TypeSpec implementor_ts = implementor_scope->pivot_type_hint();
    Implementation *implementation = new Implementation(implementation_name, implementor_ts, interface_ts, ia);
    implementor_scope->add(implementation);
    
    for (Identifier *i : contents) {
        // Necessary to handle nested implementations
        Associable *a = implementation->lookup_associable(implementation_name);
        if (!a)
            throw INTERNAL_ERROR;

        i->name = implementation_name + QUALIFIER_NAME + i->name;  // TODO: ugly!
        
        if (!a->check_associated(i))
            throw INTERNAL_ERROR;
            
        if (i->pivot_ts != implementor_ts)
            throw INTERNAL_ERROR;  // sanity check
        
        implementor_scope->add(i);
    }
}


void lself_implement(Scope *implementor_scope, TypeSpec interface_ts, std::string implementation_name, std::vector<Identifier *> contents) {
    Lself *lself = new Lself("lself", NO_TS);
    implementor_scope->add(lself);
    
    implement(implementor_scope, interface_ts, "lself." + implementation_name, contents);
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

    Scope *integer_metascope = integer_metatype->make_inner_scope(ANY_TS);
    
    for (auto &item : integer_rvalue_operations)
        integer_metascope->add(new TemplateOperation<IntegerOperationValue>(item.name, ANY_TS, item.operation));

    for (auto &item : integer_lvalue_operations)
        integer_metascope->add(new TemplateOperation<IntegerOperationValue>(item.name, ANY_LVALUE_TS, item.operation));

    integer_metascope->add(new TemplateIdentifier<CountupValue>("countup", INTEGER_TS));
    integer_metascope->add(new TemplateIdentifier<CountdownValue>("countdown", INTEGER_TS));
    
    integer_metascope->leave();
    
    for (auto t : {
        integer_type, integer32_type, integer16_type, integer8_type,
        unsigned_integer_type, unsigned_integer32_type, unsigned_integer16_type, unsigned_integer8_type
    }) {
        t->make_inner_scope({ t })->leave();
    }
}


void define_float() {
    struct {
        const char *name;
        OperationType operation;
    } float_rvalue_operations[] = {
        { "unary_minus", NEGATE },
        { "binary_star", MULTIPLY },
        { "binary_slash", DIVIDE },
        { "binary_plus", ADD },
        { "binary_minus", SUBTRACT },
        { "is_equal", EQUAL },
        { "not_equal", NOT_EQUAL },
        { "is_less", LESS },
        { "is_greater", GREATER },
        { "not_greater", LESS_EQUAL },
        { "not_less", GREATER_EQUAL },
        { "compare", COMPARE },
    }, float_lvalue_operations[] = {
        { "assign other", ASSIGN },
        { "assign_plus", ASSIGN_ADD },
        { "assign_minus", ASSIGN_SUBTRACT },
        { "assign_star", ASSIGN_MULTIPLY },
        { "assign_slash", ASSIGN_DIVIDE },
    };

    Scope *float_scope = float_type->make_inner_scope(FLOAT_TS);

    for (auto &item : float_rvalue_operations)
        float_scope->add(new TemplateOperation<FloatOperationValue>(item.name, ANY_TS, item.operation));

    for (auto &item : float_lvalue_operations)
        float_scope->add(new TemplateOperation<FloatOperationValue>(item.name, ANY_LVALUE_TS, item.operation));
    
    float_scope->add(new ImportedFloatFunction("log", "log", FLOAT_TS, NO_TS, FLOAT_TS));
    float_scope->add(new ImportedFloatFunction("exp", "exp", FLOAT_TS, NO_TS, FLOAT_TS));
    float_scope->add(new ImportedFloatFunction("pow", "binary_exponent", FLOAT_TS, FLOAT_TS, FLOAT_TS));

    float_scope->leave();
}


void define_interfaces() {
    // Streamifiable interface
    TypeSpec spts = STREAMIFIABLE_TS.prefix(ptr_type);
    DataScope *sis = streamifiable_type->make_inner_scope(spts);
    Function *sf = new Function("streamify",
        spts,
        ABSTRACT_FUNCTION,
        TSs { STRING_LVALUE_TS },
        Ss { "stream" },
        TSs {},
        NULL,
        NULL
    );
    sis->add(sf);
    streamifiable_type->complete_type();
    sis->leave();
    
    // Iterable interface
    TypeSpec jpts = ANY_ITERABLE_TS.prefix(ptr_type);
    DataScope *jis = iterable_type->make_inner_scope(jpts);
    Function *xf = new Function("iter",
        jpts,
        ABSTRACT_FUNCTION,
        TSs {},
        Ss {},
        TSs { SAME_ITERATOR_RVALUE_TS },
        NULL,
        NULL
    );
    jis->add(xf);
    iterable_type->complete_type();
    jis->leave();

    // Iterator interface
    TypeSpec ipts = ANY_ITERATOR_TS.prefix(ptr_type);
    DataScope *iis = iterator_type->make_inner_scope(ipts);
    Function *nf = new Function("next",
        ipts,
        ABSTRACT_FUNCTION,
        TSs {},
        Ss {},
        TSs { SAME_TS },
        iterator_done_exception_type,
        NULL
    );
    iis->add(nf);
    //implement(iis, SAME_ITERABLE_TS, "iterable", {});
    iterator_type->complete_type();
    iis->leave();
    
    // Application interface
    TypeSpec apts = { ptr_type, application_type };
    DataScope *ais = application_type->make_inner_scope(apts);
    Function *tf = new Function("start",
        apts,
        ABSTRACT_FUNCTION,
        TSs {},
        Ss {},
        TSs {},
        NULL,
        NULL
    );
    ais->add(tf);
    application_type->complete_type();
    ais->leave();
}


template <typename NextValue>
void define_container_iterator(Type *iter_type, Type *container_type, TypeSpec interface_ts) {
    TypeSpec PIVOT_TS = { iter_type, any_type };
    TypeSpec SAME_CONTAINER_LVALUE_TS = { lvalue_type, container_type, same_type };
    
    DataScope *aiis = iter_type->make_inner_scope(PIVOT_TS);

    // Order matters!
    aiis->add(new Variable("container", PIVOT_TS, SAME_CONTAINER_LVALUE_TS));
    aiis->add(new Variable("value", PIVOT_TS, INTEGER_LVALUE_TS));

    lself_implement(aiis, interface_ts, "iterator", {
        new TemplateIdentifier<NextValue>("next", PIVOT_TS),
    });
    
    implement(aiis, SAME_ITERABLE_TS, "iterable", {
        new Identity("iter", PIVOT_TS)
    });
    
    iter_type->complete_type();
    aiis->leave();
}


template <typename NextValue>
void define_slice_iterator(Type *iter_type, TypeSpec interface_ts) {
    TypeSpec PIVOT_TS = { iter_type, any_type };
    TypeSpec SAME_ARRAY_LVALUE_TS = { lvalue_type, array_type, same_type };
    
    DataScope *aiis = iter_type->make_inner_scope(PIVOT_TS);

    // Order matters!
    aiis->add(new Variable("container", PIVOT_TS, SAME_ARRAY_LVALUE_TS));
    aiis->add(new Variable("front", PIVOT_TS, INTEGER_LVALUE_TS));
    aiis->add(new Variable("length", PIVOT_TS, INTEGER_LVALUE_TS));
    aiis->add(new Variable("value", PIVOT_TS, INTEGER_LVALUE_TS));

    lself_implement(aiis, interface_ts, "iterator", {
        new TemplateIdentifier<NextValue>("next", PIVOT_TS)
    });

    implement(aiis, SAME_ITERABLE_TS, "iterable", {
        new Identity("iter", PIVOT_TS)
    });
    
    iter_type->complete_type();
    aiis->leave();
}


void define_iterators() {
    // Counter operations
    for (auto is_down : { false, true }) {
        RecordType *counter_type = ptr_cast<RecordType>(is_down ? countdown_type : countup_type);
        TypeSpec COUNTER_TS = { counter_type };
    
        DataScope *cis = counter_type->make_inner_scope(COUNTER_TS);
    
        cis->add(new Variable("limit", COUNTER_TS, INTEGER_LVALUE_TS));  // Order matters!
        cis->add(new Variable("value", COUNTER_TS, INTEGER_LVALUE_TS));
        Identifier *next_fn;
        
        if (!is_down) {
            next_fn = new TemplateIdentifier<CountupNextValue>("next", COUNTER_TS);
        }
        else {
            next_fn = new TemplateIdentifier<CountdownNextValue>("next", COUNTER_TS);
        }

        lself_implement(cis, INTEGER_ITERATOR_TS, "iter", {
            next_fn,
        });

        implement(cis, INTEGER_ITERABLE_TS, "iterable", {
            new Identity("iter", COUNTER_TS)
        });
        
        counter_type->complete_type();
        cis->leave();
    }

    // Item type for itemized iteration
    RecordType *item_type = ptr_cast<RecordType>(::item_type);
    DataScope *itis = item_type->make_inner_scope(ANY_ANY2_ITEM_TS);

    itis->add(new Variable("index", ANY_ANY2_ITEM_TS, SAME_LVALUE_TS));  // Order matters!
    itis->add(new Variable("value", ANY_ANY2_ITEM_TS, SAME2_LVALUE_TS));
    
    item_type->complete_type();
    itis->leave();
    
    TypeSpec INTEGER_SAME_ITEM_ITERATOR_TS = { iterator_type, item_type, integer_type, same_type };

    // Array Iterator operations
    define_container_iterator<ArrayNextElemValue>(arrayelemiter_type, array_type, SAME_ITERATOR_TS);
    define_container_iterator<ArrayNextIndexValue>(arrayindexiter_type, array_type, INTEGER_ITERATOR_TS);
    define_container_iterator<ArrayNextItemValue>(arrayitemiter_type, array_type, INTEGER_SAME_ITEM_ITERATOR_TS);

    // Circularray Iterator operations
    define_container_iterator<QueueNextElemValue>(queueelemiter_type, queue_type, SAME_ITERATOR_TS);
    define_container_iterator<QueueNextIndexValue>(queueindexiter_type, queue_type, INTEGER_ITERATOR_TS);
    define_container_iterator<QueueNextItemValue>(queueitemiter_type, queue_type, INTEGER_SAME_ITEM_ITERATOR_TS);

    // Set Iterator operations
    define_container_iterator<RbtreeNextElemByAgeValue>(setelembyageiter_type, set_type, SAME_ITERATOR_TS);
    define_container_iterator<RbtreeNextElemByOrderValue>(setelembyorderiter_type, set_type, SAME_ITERATOR_TS);

    // Rbtree Iterator operations
    //define_container_iterator<RbtreeNextElemByAgeValue>(rbtreeelembyageiter_type, rbtree_type, SAME_ITERATOR_TS);
    //define_container_iterator<RbtreeNextElemByOrderValue>(rbtreeelembyorderiter_type, rbtree_type, SAME_ITERATOR_TS);

    // Slice Iterator operations
    define_slice_iterator<SliceNextElemValue>(sliceelemiter_type, SAME_ITERATOR_TS);
    define_slice_iterator<SliceNextIndexValue>(sliceindexiter_type, INTEGER_ITERATOR_TS);
    define_slice_iterator<SliceNextItemValue>(sliceitemiter_type, INTEGER_SAME_ITEM_ITERATOR_TS);
}


void define_string() {
    RecordType *record_type = ptr_cast<RecordType>(string_type);
    DataScope *is = record_type->make_inner_scope(STRING_TS);

    is->add(new Variable("chars", STRING_TS, CHARACTER_ARRAY_LVALUE_TS));

    is->add(new RecordWrapperIdentifier("length", STRING_TS, CHARACTER_ARRAY_TS, INTEGER_TS, "length"));
    is->add(new RecordWrapperIdentifier("binary_plus", STRING_TS, CHARACTER_ARRAY_TS, STRING_TS, "binary_plus", "chars"));
    is->add(new RecordWrapperIdentifier("assign_plus", STRING_LVALUE_TS, CHARACTER_ARRAY_LVALUE_TS, STRING_TS, "assign_plus", "chars"));
    is->add(new RecordWrapperIdentifier("index", STRING_TS, CHARACTER_ARRAY_TS, CHARACTER_TS, "index"));
    is->add(new RecordWrapperIdentifier("realloc", STRING_LVALUE_TS, CHARACTER_ARRAY_LVALUE_TS, STRING_LVALUE_TS, "realloc"));

    is->add(new TemplateOperation<RecordOperationValue>("assign other", STRING_LVALUE_TS, ASSIGN));

    is->add(new TemplateOperation<StringOperationValue>("is_equal", STRING_TS, EQUAL));
    is->add(new TemplateOperation<StringOperationValue>("not_equal", STRING_TS, NOT_EQUAL));
    is->add(new TemplateOperation<StringOperationValue>("compare", STRING_TS, COMPARE));

    implement(is, TypeSpec { iterable_type, character_type }, "iterable", {
        new RecordWrapperIdentifier("iter", STRING_TS, CHARACTER_ARRAY_TS, TypeSpec { arrayelemiter_type, character_type }, "elements")
    });

    is->add(new RecordWrapperIdentifier("elements", STRING_TS, CHARACTER_ARRAY_TS, TypeSpec { arrayelemiter_type, character_type }, "elements"));
    is->add(new RecordWrapperIdentifier("indexes", STRING_TS, CHARACTER_ARRAY_TS, TypeSpec { arrayindexiter_type, character_type }, "indexes"));
    is->add(new RecordWrapperIdentifier("items", STRING_TS, CHARACTER_ARRAY_TS, TypeSpec { arrayitemiter_type, character_type }, "items"));

    is->add(new AltStreamifiableImplementation("raw", STRING_TS));

    // String operations
    is->add(new SysvFunction("encode_utf8", "encode_utf8", STRING_TS, GENERIC_FUNCTION, TSs {}, {}, TSs { UNSIGNED_INTEGER8_ARRAY_TS }, NULL, NULL));

    record_type->complete_type();
    is->leave();
}


void define_slice() {
    TypeSpec SAME_ARRAY_LVALUE_TS = { lvalue_type, array_type, same_type };
    
    RecordType *record_type = ptr_cast<RecordType>(slice_type);
    DataScope *is = record_type->make_inner_scope(ANY_SLICE_TS);

    is->add(new Variable("array", ANY_SLICE_TS, SAME_ARRAY_LVALUE_TS));
    is->add(new Variable("front", ANY_SLICE_TS, INTEGER_LVALUE_TS));
    is->add(new Variable("length", ANY_SLICE_TS, INTEGER_LVALUE_TS));

    is->add(new TemplateIdentifier<SliceIndexValue>("index", ANY_SLICE_TS));
    is->add(new TemplateIdentifier<SliceFindValue>("find", ANY_SLICE_TS));
    is->add(new TemplateIdentifier<SliceSliceValue>("slice", ANY_SLICE_TS));

    implement(is, SAME_ITERABLE_TS, "iterable", {
        new TemplateIdentifier<SliceElemIterValue>("iter", ANY_SLICE_TS)
    });

    is->add(new TemplateIdentifier<SliceElemIterValue>("elements", ANY_SLICE_TS));
    is->add(new TemplateIdentifier<SliceIndexIterValue>("indexes", ANY_SLICE_TS));
    is->add(new TemplateIdentifier<SliceItemIterValue>("items", ANY_SLICE_TS));

    is->add(new SysvFunction("decode_utf8_slice", "decode_utf8", BYTE_SLICE_TS, GENERIC_FUNCTION, TSs {}, {}, TSs { STRING_TS }, NULL, NULL));
    
    record_type->complete_type();
    is->leave();
}


void define_weakref() {
    RecordType *record_type = ptr_cast<RecordType>(weakref_type);
    DataScope *is = record_type->make_inner_scope(ANYID_WEAKREF_TS);

    TypeSpec MEMBER_TS = SAMEID_NOSYVALUE_LVALUE_TS;
    TypeSpec NOSY_TS = MEMBER_TS.rvalue().prefix(nosyref_type).prefix(ref_type);

    is->add(new Variable("nosyref", ANYID_WEAKREF_TS, NOSY_TS));

    is->add(new TemplateOperation<RecordOperationValue>("assign other", ANYID_WEAKREF_LVALUE_TS, ASSIGN));
    is->add(new TemplateOperation<RecordOperationValue>("compare", ANYID_WEAKREF_TS, COMPARE));

    record_type->complete_type();
    is->leave();
}


void define_option() {
    DataScope *is = option_type->make_inner_scope(ANY_OPTION_TS);

    is->add(new TemplateOperation<OptionOperationValue>("assign other", ANY_OPTION_LVALUE_TS, ASSIGN));
    is->add(new TemplateOperation<OptionOperationValue>("compare", ANY_OPTION_TS, COMPARE));

    option_type->complete_type();
    is->leave();
}


void define_array() {
    TypeSpec PIVOT_TS = ANY_ARRAY_TS;
    TypeSpec PIVOT_LVALUE_TS = ANY_ARRAY_LVALUE_TS;
    
    Scope *array_scope = array_type->make_inner_scope(ANY_ARRAY_TS);

    array_scope->add(new Variable("linearray", PIVOT_TS, SAME_LINEARRAY_REF_LVALUE_TS));

    array_scope->add(new TemplateIdentifier<ArrayLengthValue>("length", PIVOT_TS));
    array_scope->add(new TemplateOperation<ArrayReallocValue>("realloc", PIVOT_LVALUE_TS, TWEAK));
    array_scope->add(new TemplateIdentifier<ArrayRemoveValue>("remove", PIVOT_TS));  // needs Ref
    array_scope->add(new TemplateIdentifier<ArrayRefillValue>("refill", PIVOT_LVALUE_TS));
    array_scope->add(new TemplateIdentifier<ArrayConcatenationValue>("binary_plus", PIVOT_TS));
    array_scope->add(new TemplateIdentifier<ArrayExtendValue>("assign_plus", PIVOT_LVALUE_TS));
    array_scope->add(new TemplateOperation<ArrayIndexValue>("index", PIVOT_TS, TWEAK));
    array_scope->add(new TemplateIdentifier<ArraySortValue>("sort", PIVOT_TS));
    array_scope->add(new TemplateIdentifier<ArrayPushValue>("push", PIVOT_LVALUE_TS));
    array_scope->add(new TemplateIdentifier<ArrayPopValue>("pop", PIVOT_LVALUE_TS));
    //array_scope->add(new TemplateIdentifier<ArrayAutogrowValue>("autogrow", PIVOT_LVALUE_TS));
    array_scope->add(new TemplateIdentifier<ArraySliceValue>("slice", PIVOT_TS));
    
    // Array iterable operations
    implement(array_scope, SAME_ITERABLE_TS, "iterable", {
        new TemplateIdentifier<ArrayElemIterValue>("iter", PIVOT_TS)
    });

    array_scope->add(new TemplateIdentifier<ArrayElemIterValue>("elements", PIVOT_TS));
    array_scope->add(new TemplateIdentifier<ArrayIndexIterValue>("indexes", PIVOT_TS));
    array_scope->add(new TemplateIdentifier<ArrayItemIterValue>("items", PIVOT_TS));

    //array_scope->add(new AltStreamifiableImplementation("contents", PIVOT_TS));
    
    array_scope->add(new SysvFunction("decode_utf8", "decode_utf8", UNSIGNED_INTEGER8_ARRAY_TS, GENERIC_FUNCTION, TSs {}, {}, TSs { STRING_TS }, NULL, NULL));

    array_type->complete_type();
    array_scope->leave();
}


void define_queue() {
    TypeSpec PIVOT_TS = ANY_QUEUE_TS;
    TypeSpec PIVOT_LVALUE_TS = ANY_QUEUE_LVALUE_TS;
    Scope *queue_scope = queue_type->make_inner_scope(ANY_QUEUE_TS);

    queue_scope->add(new Variable("circularray", PIVOT_TS, SAME_CIRCULARRAY_REF_LVALUE_TS));

    queue_scope->add(new TemplateIdentifier<QueueLengthValue>("length", PIVOT_TS));
    queue_scope->add(new TemplateOperation<QueueIndexValue>("index", PIVOT_TS, TWEAK));
    queue_scope->add(new TemplateIdentifier<QueuePushValue>("push", PIVOT_LVALUE_TS));
    queue_scope->add(new TemplateIdentifier<QueuePopValue>("pop", PIVOT_LVALUE_TS));
    queue_scope->add(new TemplateIdentifier<QueueUnshiftValue>("unshift", PIVOT_LVALUE_TS));
    queue_scope->add(new TemplateIdentifier<QueueShiftValue>("shift", PIVOT_LVALUE_TS));
    //queue_scope->add(new TemplateIdentifier<QueueAutogrowValue>("autogrow", PIVOT_LVALUE_TS));
    
    // Queue iterable operations
    implement(queue_scope, SAME_ITERABLE_TS, "iterable", {
        new TemplateIdentifier<QueueElemIterValue>("iter", PIVOT_TS)
    });

    queue_scope->add(new TemplateIdentifier<QueueElemIterValue>("elements", PIVOT_TS));
    queue_scope->add(new TemplateIdentifier<QueueIndexIterValue>("indexes", PIVOT_TS));
    queue_scope->add(new TemplateIdentifier<QueueItemIterValue>("items", PIVOT_TS));

    queue_type->complete_type();
    queue_scope->leave();
}


void define_set() {
    TypeSpec PIVOT_TS = ANY_SET_TS;
    TypeSpec PIVOT_LVALUE_TS = ANY_SET_LVALUE_TS;
    TypeSpec MEMBER_TS = SAME_RBTREE_REF_LVALUE_TS;
    
    DataScope *is = set_type->make_inner_scope(PIVOT_TS);

    is->add(new Variable("rbtree", PIVOT_TS, MEMBER_TS));

    is->add(new TemplateIdentifier<SetLengthValue>("length", PIVOT_TS));
    is->add(new TemplateIdentifier<SetAddValue>("add", PIVOT_LVALUE_TS));
    is->add(new TemplateIdentifier<SetRemoveValue>("remove", PIVOT_LVALUE_TS));
    is->add(new TemplateIdentifier<SetHasValue>("has", PIVOT_TS));
    
    is->add(new TemplateIdentifier<SetElemByAgeIterValue>("elements_by_age", PIVOT_TS));
    is->add(new TemplateIdentifier<SetElemByOrderIterValue>("elements_by_order", PIVOT_TS));
    
    /*
    is->add(new CastWrapperIdentifier("length", PIVOT, CAST));
    is->add(new CastWrapperIdentifier("has", PIVOT, CAST));
    is->add(new CastWrapperIdentifier("add", PIVOT, CAST));
    is->add(new CastWrapperIdentifier("remove", PIVOT, CAST));

    is->add(new CastWrapperIdentifier("elements_by_age", PIVOT, CAST));
    is->add(new CastWrapperIdentifier("elements_by_order", PIVOT, CAST));
    */
    
    set_type->complete_type();
    is->leave();
}


void define_map() {
    TypeSpec PIVOT_TS = ANY_ANY2_MAP_TS;
    TypeSpec PIVOT_LVALUE_TS = ANY_ANY2_MAP_LVALUE_TS;
    TypeSpec MEMBER_TS = SAME_SAME2_ITEM_RBTREE_REF_LVALUE_TS;
    
    DataScope *is = map_type->make_inner_scope(PIVOT_TS);

    is->add(new Variable("rbtree", PIVOT_TS, MEMBER_TS));
    
    is->add(new TemplateIdentifier<MapLengthValue>("length", PIVOT_TS));
    is->add(new TemplateIdentifier<MapAddValue>("add", PIVOT_LVALUE_TS));
    is->add(new TemplateIdentifier<MapRemoveValue>("remove", PIVOT_LVALUE_TS));
    is->add(new TemplateIdentifier<MapHasValue>("has", PIVOT_TS));
    is->add(new TemplateIdentifier<MapIndexValue>("index", PIVOT_TS));

    //is->add(new AltStreamifiableImplementation("contents", PIVOT));
    
    map_type->complete_type();
    is->leave();
}


void define_weakvaluemap() {
    TypeSpec PIVOT_TS = ANY_ANYID2_WEAKVALUEMAP_TS;
    TypeSpec PIVOT_LVALUE_TS = PIVOT_TS.lvalue();
    TypeSpec MEMBER_TS = SAME_SAMEID2_NOSYVALUE_ITEM_RBTREE_REF_LVALUE_TS;
    TypeSpec NOSY_TS = MEMBER_TS.rvalue().prefix(nosytree_type).prefix(ref_type);

    DataScope *is = weakvaluemap_type->make_inner_scope(PIVOT_TS);

    is->add(new Variable("nosytree", PIVOT_TS, NOSY_TS));
    
    is->add(new NosytreeTemplateIdentifier<WeakValueMapLengthValue>("length", PIVOT_TS, MEMBER_TS));
    is->add(new NosytreeTemplateIdentifier<WeakValueMapAddValue>("add", PIVOT_LVALUE_TS, MEMBER_TS));
    is->add(new NosytreeTemplateIdentifier<WeakValueMapRemoveValue>("remove", PIVOT_LVALUE_TS, MEMBER_TS));
    is->add(new NosytreeTemplateIdentifier<WeakValueMapHasValue>("has", PIVOT_TS, MEMBER_TS));
    is->add(new NosytreeTemplateIdentifier<WeakValueMapIndexValue>("index", PIVOT_TS, MEMBER_TS));

    // Must not define iteration due to the volatility of this container
    
    weakvaluemap_type->complete_type();
    is->leave();
}


void define_weakindexmap() {
    TypeSpec PIVOT_TS = ANYID_ANY2_WEAKINDEXMAP_TS;
    TypeSpec PIVOT_LVALUE_TS = PIVOT_TS.lvalue();
    TypeSpec MEMBER_TS = SAMEID_NOSYVALUE_SAME2_ITEM_RBTREE_REF_LVALUE_TS;
    TypeSpec NOSY_TS = MEMBER_TS.rvalue().prefix(nosytree_type).prefix(ref_type);
    
    DataScope *is = weakindexmap_type->make_inner_scope(PIVOT_TS);

    is->add(new Variable("nosytree", PIVOT_TS, NOSY_TS));

    is->add(new NosytreeTemplateIdentifier<WeakIndexMapLengthValue>("length", PIVOT_TS, MEMBER_TS));
    is->add(new NosytreeTemplateIdentifier<WeakIndexMapAddValue>("add", PIVOT_LVALUE_TS, MEMBER_TS));
    is->add(new NosytreeTemplateIdentifier<WeakIndexMapRemoveValue>("remove", PIVOT_LVALUE_TS, MEMBER_TS));
    is->add(new NosytreeTemplateIdentifier<WeakIndexMapHasValue>("has", PIVOT_TS, MEMBER_TS));
    is->add(new NosytreeTemplateIdentifier<WeakIndexMapIndexValue>("index", PIVOT_TS, MEMBER_TS));
    
    // Must not define iteration due to the volatility of this container
    
    weakindexmap_type->complete_type();
    is->leave();
}


void define_weakset() {
    TypeSpec PIVOT_TS = ANYID_WEAKSET_TS;
    TypeSpec PIVOT_LVALUE_TS = PIVOT_TS.lvalue();
    TypeSpec MEMBER_TS = SAMEID_NOSYVALUE_RBTREE_REF_LVALUE_TS;
    TypeSpec NOSY_TS = MEMBER_TS.rvalue().prefix(nosytree_type).prefix(ref_type);
    
    DataScope *is = weakset_type->make_inner_scope(PIVOT_TS);

    is->add(new Variable("nosytree", PIVOT_TS, NOSY_TS));
    
    is->add(new NosytreeTemplateIdentifier<WeakSetLengthValue>("length", PIVOT_TS, MEMBER_TS));
    is->add(new NosytreeTemplateIdentifier<WeakSetAddValue>("add", PIVOT_LVALUE_TS, MEMBER_TS));
    is->add(new NosytreeTemplateIdentifier<WeakSetRemoveValue>("remove", PIVOT_LVALUE_TS, MEMBER_TS));
    is->add(new NosytreeTemplateIdentifier<WeakSetHasValue>("has", PIVOT_TS, MEMBER_TS));

    // Must not define iteration due to the volatility of this container
    
    weakset_type->complete_type();
    is->leave();
}


void builtin_colon(Scope *root_scope) {
    colon_scope->add(new TemplateOperation<IfValue>("if", NO_TS, TWEAK));
    colon_scope->add(new TemplateIdentifier<RepeatValue>("repeat", NO_TS));
    colon_scope->add(new TemplateIdentifier<ForEachValue>("for", NO_TS));
    colon_scope->add(new TemplateIdentifier<SwitchValue>("switch", NO_TS));
    colon_scope->add(new TemplateIdentifier<RaiseValue>("raise", NO_TS));
    colon_scope->add(new TemplateIdentifier<TryValue>("try", NO_TS));
    colon_scope->add(new TemplateIdentifier<IsValue>("is", NO_TS));
    colon_scope->add(new TemplateOperation<FunctionReturnValue>("return", NO_TS, TWEAK));

    colon_scope->add(new TemplateIdentifier<FunctorDefinitionValue>("functor", NO_TS));

    colon_scope->add(new TemplateIdentifier<PtrCastValue>("ptr", NO_TS));

    colon_scope->add(new TemplateIdentifier<FunctionDefinitionValue>("Function", NO_TS));
    colon_scope->add(new TemplateIdentifier<InitializerDefinitionValue>("Initializer", NO_TS));
    colon_scope->add(new TemplateIdentifier<FinalizerDefinitionValue>("Finalizer", NO_TS));
    colon_scope->add(new TemplateIdentifier<RoleDefinitionValue>("Role", NO_TS));
    colon_scope->add(new TemplateIdentifier<BaseDefinitionValue>("Base", NO_TS));
    colon_scope->add(new TemplateIdentifier<AutoDefinitionValue>("Auto", NO_TS));
    colon_scope->add(new TemplateIdentifier<LselfDefinitionValue>("Lself", NO_TS));
    colon_scope->add(new TemplateIdentifier<GlobalDefinitionValue>("Global", NO_TS));

    colon_type->complete_type();
    colon_scope->leave();
    
    colon_value = new Value(VOID_TS);  // completely dummy, for lookup_unchecked
}


void builtin_runtime(Scope *root_scope) {
    TSs NO_TSS = { };
    TSs INTEGER_TSS = { INTEGER_TS };
    TSs FLOAT_TSS = { FLOAT_TS };
    TSs BOOLEAN_TSS = { BOOLEAN_TS };
    TSs UNSIGNED_INTEGER8_TSS = { UNSIGNED_INTEGER8_TS };
    TSs UNSIGNED_INTEGER8_ARRAY_TSS = { UNSIGNED_INTEGER8_ARRAY_TS };
    TSs CHARACTER_ARRAY_TSS = { CHARACTER_ARRAY_TS };

    Ss no_names = { };
    Ss value_names = { "value" };

    Type *std_type = new UnitType("<Std>");
    root_scope->add(std_type);
    TypeSpec STD_TS = { std_type };
    Scope *is = std_type->make_inner_scope(STD_TS);
    
    Declaration *std = new GlobalNamespace("Std", STD_TS);
    root_scope->add(std);

    is->add(new SysvFunction("printi", "printi", STD_TS, GENERIC_FUNCTION, INTEGER_TSS, value_names, NO_TSS, NULL, NULL));
    is->add(new SysvFunction("printc", "printc", STD_TS, GENERIC_FUNCTION, UNSIGNED_INTEGER8_TSS, value_names, NO_TSS, NULL, NULL));
    is->add(new SysvFunction("printd", "printd", STD_TS, GENERIC_FUNCTION, FLOAT_TSS, value_names, NO_TSS, NULL, NULL));
    is->add(new SysvFunction("printb", "printb", STD_TS, GENERIC_FUNCTION, UNSIGNED_INTEGER8_ARRAY_TSS, value_names, NO_TSS, NULL, NULL));
    is->add(new SysvFunction("prints", "prints", STD_TS, GENERIC_FUNCTION, TSs { STRING_TS }, value_names, NO_TSS, NULL, NULL));
    is->add(new SysvFunction("printp", "printp", STD_TS, GENERIC_FUNCTION, TSs { ANYID_REF_LVALUE_TS }, value_names, NO_TSS, NULL, NULL));  // needs Lvalue to avoid ref copy

    std_type->complete_type();
    is->leave();
}


RootScope *init_builtins() {
    RootScope *root_scope = new RootScope;
    root_scope->set_name("<root>");
    root_scope->enter();

    builtin_types(root_scope);
    wrapped_types(root_scope);

    define_interfaces();

    define_string();
    define_slice();
    
    define_iterators();
    
    define_array();
    define_queue();
    define_set();
    define_map();
    define_weakvaluemap();
    define_weakindexmap();
    define_weakset();
    
    define_option();
    define_weakref();
    
    // Integer operations
    define_integers();
    define_float();
        
    // Character operations
    Scope *char_scope = character_type->make_inner_scope(CHARACTER_TS);
    char_scope->add(new TemplateOperation<IntegerOperationValue>("assign other", CHARACTER_LVALUE_TS, ASSIGN));
    char_scope->add(new TemplateOperation<IntegerOperationValue>("compare", CHARACTER_TS, COMPARE));
    char_scope->leave();
    
    // Boolean operations
    Scope *bool_scope = boolean_type->make_inner_scope(BOOLEAN_TS);
    bool_scope->add(new TemplateOperation<BooleanOperationValue>("assign other", BOOLEAN_LVALUE_TS, ASSIGN));
    bool_scope->add(new TemplateOperation<BooleanOperationValue>("compare", BOOLEAN_TS, COMPARE));
    bool_scope->add(new TemplateIdentifier<BooleanNotValue>("logical not", BOOLEAN_TS));
    bool_scope->add(new TemplateIdentifier<BooleanAndValue>("logical and", BOOLEAN_TS));
    bool_scope->add(new TemplateIdentifier<BooleanOrValue>("logical or", BOOLEAN_TS));
    bool_scope->leave();

    // Enum operations
    Scope *enum_metascope = enumeration_metatype->make_inner_scope(ANY_TS);
    enum_metascope->add(new TemplateOperation<IntegerOperationValue>("assign other", ANY_LVALUE_TS, ASSIGN));
    enum_metascope->add(new TemplateOperation<IntegerOperationValue>("is_equal", ANY_TS, EQUAL));
    enum_metascope->leave();

    // Treenum operations
    Scope *treenum_metascope = treenumeration_metatype->make_inner_scope(ANY_TS);
    treenum_metascope->add(new TemplateOperation<IntegerOperationValue>("assign other", ANY_LVALUE_TS, ASSIGN));
    treenum_metascope->add(new TemplateOperation<IntegerOperationValue>("is_equal", ANY_TS, EQUAL));
    treenum_metascope->leave();

    // Record operations
    Scope *record_metascope = record_metatype->make_inner_scope(ANY_TS);
    record_metascope->add(new TemplateOperation<RecordOperationValue>("assign other", ANY_LVALUE_TS, ASSIGN));
    record_metascope->leave();

    // Reference operations
    typedef TemplateOperation<ReferenceOperationValue> ReferenceOperation;
    Scope *ref_scope = ref_type->make_inner_scope(ANYID_REF_TS);
    ref_scope->add(new ReferenceOperation("assign other", ANYID_REF_LVALUE_TS, ASSIGN));
    ref_scope->add(new ReferenceOperation("is_equal", ANYID_REF_TS, EQUAL));
    ref_scope->add(new ReferenceOperation("not_equal", ANYID_REF_TS, NOT_EQUAL));
    ref_scope->leave();

    typedef TemplateOperation<PointerOperationValue> PointerOperation;
    Scope *ptr_scope = ptr_type->make_inner_scope(ANYID_PTR_TS);
    ptr_scope->add(new PointerOperation("assign other", ANYID_PTR_LVALUE_TS, ASSIGN));
    ptr_scope->add(new PointerOperation("is_equal", ANYID_PTR_TS, EQUAL));
    ptr_scope->add(new PointerOperation("not_equal", ANYID_PTR_TS, NOT_EQUAL));
    ptr_scope->leave();

    // Rbtree operations
    //define_rbtree();
    
    // Unpacking
    Scope *mls = multilvalue_type->make_inner_scope(MULTILVALUE_TS);
    mls->add(new TemplateIdentifier<UnpackingValue>("assign other", MULTILVALUE_TS));
    mls->leave();

    // Initializing
    Scope *uns = uninitialized_type->make_inner_scope(ANY_UNINITIALIZED_TS);
    uns->add(new TemplateIdentifier<CreateValue>("assign other", ANY_UNINITIALIZED_TS));
    uns->leave();
    
    // Builtin controls
    builtin_colon(root_scope);
    
    // Library functions
    builtin_runtime(root_scope);

    return root_scope;
}

