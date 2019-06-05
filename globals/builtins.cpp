#include "../plum.h"

MetaType *metatype_hypertype = NULL;

MetaType *type_metatype = NULL;

Type *any_type = NULL;
Type *any2_type = NULL;
Type *any3_type = NULL;
Type *anyid_type = NULL;
Type *anyid2_type = NULL;
Type *anyid3_type = NULL;
Type *anytuple_type = NULL;
Type *anytuple2_type = NULL;
Type *anytuple3_type = NULL;
Type *same_type = NULL;
Type *same2_type = NULL;
Type *same3_type = NULL;
Type *sameid_type = NULL;
Type *sameid2_type = NULL;
Type *sameid3_type = NULL;
Type *sametuple_type = NULL;
Type *sametuple2_type = NULL;
Type *sametuple3_type = NULL;

MetaType *value_metatype = NULL;
MetaType *identity_metatype = NULL;
MetaType *module_metatype = NULL;
MetaType *tuple_metatype = NULL;
MetaType *argument_metatype = NULL;
MetaType *enumeration_metatype = NULL;
MetaType *treenumeration_metatype = NULL;
MetaType *integer_metatype = NULL;
MetaType *record_metatype = NULL;
MetaType *union_metatype = NULL;
MetaType *abstract_metatype = NULL;
MetaType *class_metatype = NULL;
MetaType *singleton_metatype = NULL;
MetaType *interface_metatype = NULL;
MetaType *import_metatype = NULL;

Type *ovalue_type = NULL;
Type *lvalue_type = NULL;
Type *dvalue_type = NULL;
Type *code_type = NULL;
Type *void_type = NULL;
Type *unit_type = NULL;
Type *partial_type = NULL;
Type *uninitialized_type = NULL;
Type *initializable_type = NULL;
Type *stringtemplate_type = NULL;
Type *bare_type = NULL;
Type *whatever_type = NULL;
Type *tuple0_type = NULL;
Type *tuple1_type = NULL;
Type *tuple2_type = NULL;
Type *tuple3_type = NULL;

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
Type *float_type = NULL;
Type *ref_type = NULL;
Type *ptr_type = NULL;
Type *nosyvalue_type = NULL;
Type *nosytree_type = NULL;
Type *nosyref_type = NULL;
Type *weakref_type = NULL;
Type *linearray_type = NULL;
Type *circularray_type = NULL;
Type *rbtree_type = NULL;
Type *string_type = NULL;
Type *slice_type = NULL;
Type *option_type = NULL;
Type *optionis_type = NULL;
Type *optionas_type = NULL;
Type *array_type = NULL;
Type *queue_type = NULL;
Type *set_type = NULL;
Type *map_type = NULL;
Type *weakvaluemap_type = NULL;
Type *weakindexmap_type = NULL;
Type *weakset_type = NULL;
Type *countup_type = NULL;
Type *countdown_type = NULL;
Type *item_type = NULL;
Type *arrayelemiter_type = NULL;
Type *arrayindexiter_type = NULL;
Type *arrayitemiter_type = NULL;
Type *queueelemiter_type = NULL;
Type *queueindexiter_type = NULL;
Type *queueitemiter_type = NULL;
Type *setelembyageiter_type = NULL;
Type *setelembyorderiter_type = NULL;
Type *mapitembyageiter_type = NULL;
Type *mapitembyorderiter_type = NULL;
Type *mapindexbyageiter_type = NULL;
Type *mapindexbyorderiter_type = NULL;
Type *sliceelemiter_type = NULL;
Type *sliceindexiter_type = NULL;
Type *sliceitemiter_type = NULL;

Type *colon_type = NULL;
DataScope *colon_scope = NULL;
Value *colon_value = NULL;

TreenumerationType *iterator_done_exception_type = NULL;
TreenumerationType *container_full_exception_type = NULL;
TreenumerationType *container_empty_exception_type = NULL;
TreenumerationType *container_lent_exception_type = NULL;
TreenumerationType *match_unmatched_exception_type = NULL;
TreenumerationType *lookup_exception_type = NULL;
TreenumerationType *code_break_exception_type = NULL;
TreenumerationType *errno_exception_type = NULL;
TreenumerationType *parse_exception_type = NULL;

InterfaceType *streamifiable_type = NULL;
InterfaceType *iterator_type = NULL;
InterfaceType *iterable_type = NULL;
AbstractType *application_type = NULL;

TypeSpec HYPERTYPE_TS;
TypeSpec NO_TS;
TypeSpec VOID_TS;
TypeSpec ANY_TS;
TypeSpec ANY_OVALUE_TS;
TypeSpec ANY_LVALUE_TS;
TypeSpec SAME_TS;
TypeSpec SAME_LVALUE_TS;
TypeSpec SAME2_LVALUE_TS;
TypeSpec UNIT_TS;
TypeSpec WHATEVER_TS;
TypeSpec WHATEVER_TUPLE1_CODE_TS;
TypeSpec BOOLEAN_TS;
TypeSpec INTEGER_TS;
TypeSpec INTEGER_LVALUE_TS;
TypeSpec INTEGER_OVALUE_TS;
TypeSpec BOOLEAN_LVALUE_TS;
TypeSpec UNSIGNED_INTEGER8_TS;
TypeSpec UNSIGNED_INTEGER8_ARRAY_TS;
TypeSpec UNTEGER_TS;
TypeSpec CHARACTER_TS;
TypeSpec CHARACTER_LVALUE_TS;
TypeSpec CHARACTER_ARRAY_TS;
TypeSpec CHARACTER_ARRAY_LVALUE_TS;
TypeSpec FLOAT_TS;
TypeSpec FLOAT_LVALUE_TS;
TypeSpec ANYID_REF_TS;
TypeSpec ANYID_REF_LVALUE_TS;
TypeSpec ANYID_PTR_TS;
TypeSpec ANYID_PTR_LVALUE_TS;
TypeSpec ANYID_WEAKREF_TS;
TypeSpec ANYID_WEAKREF_LVALUE_TS;
TypeSpec SAMEID_NOSYVALUE_NOSYCONTAINER_REF_LVALUE_TS;
TypeSpec ANY_UNINITIALIZED_TS;
TypeSpec BARE_UNINITIALIZED_TS;
TypeSpec STRINGTEMPLATE_TS;
TypeSpec ANY_ARRAY_TS;
TypeSpec ANY_ARRAY_LVALUE_TS;
TypeSpec SAME_ARRAY_TS;
TypeSpec SAME_LINEARRAY_REF_LVALUE_TS;
TypeSpec ANY_QUEUE_TS;
TypeSpec ANY_QUEUE_LVALUE_TS;
TypeSpec SAME_QUEUE_TS;
TypeSpec SAME_CIRCULARRAY_REF_LVALUE_TS;
TypeSpec ANY_RBTREE_REF_TS;
TypeSpec ANY_RBTREE_REF_LVALUE_TS;
TypeSpec SAME_RBTREE_REF_LVALUE_TS;
TypeSpec ANY_RBTREE_PTR_TS;
TypeSpec SAME_SAME2_ITEM_RBTREE_REF_LVALUE_TS;
TypeSpec TUPLE0_CODE_TS;
TypeSpec BOOLEAN_TUPLE1_CODE_TS;
TypeSpec STRING_TS;
TypeSpec STRING_LVALUE_TS;
TypeSpec STRING_ARRAY_TS;
TypeSpec ANY_SLICE_TS;
TypeSpec BYTE_SLICE_TS;
TypeSpec ANY_OPTION_TS;
TypeSpec ANY_OPTION_LVALUE_TS;
TypeSpec OPTIONSELECTOR_TS;
TypeSpec ANY_ANY2_MAP_TS;
TypeSpec ANY_ANY2_MAP_LVALUE_TS;
TypeSpec ANY_SET_TS;
TypeSpec ANY_SET_LVALUE_TS;
TypeSpec SAME_SET_TS;
TypeSpec SAME_SAME2_MAP_TS;
TypeSpec ANY_ANYID2_WEAKVALUEMAP_TS;
TypeSpec SAME_SAMEID2_NOSYVALUE_ITEM_RBTREE_REF_TS;
TypeSpec SAME_SAMEID2_NOSYVALUE_ITEM_RBTREE_REF_LVALUE_TS;
TypeSpec ANYID_ANY2_WEAKINDEXMAP_TS;
TypeSpec SAMEID_NOSYVALUE_SAME2_ITEM_RBTREE_REF_TS;
TypeSpec SAMEID_NOSYVALUE_SAME2_ITEM_RBTREE_REF_LVALUE_TS;
TypeSpec ANYID_WEAKSET_TS;

TypeSpec SAMEID_NOSYVALUE_LVALUE_TS;

TypeSpec SAMEID_NOSYVALUE_RBTREE_REF_TS;
TypeSpec SAMEID_NOSYVALUE_RBTREE_REF_LVALUE_TS;

TypeSpec SAMEID_NOSYVALUE_TS;
TypeSpec SAMEID_NOSYVALUE_SAME2_ITEM_TS;
TypeSpec SAME_SAMEID2_NOSYVALUE_ITEM_TS;

TypeSpec STREAMIFIABLE_TS;
TypeSpec ANYTUPLE_ITERATOR_TS;
TypeSpec SAMETUPLE_ITERATOR_TS;
TypeSpec SAME_SAME2_TUPLE2_ITERATOR_TS;
TypeSpec ANYTUPLE_ITERABLE_TS;
TypeSpec SAMETUPLE_ITERABLE_TS;
TypeSpec INTEGER_TUPLE1_ITERATOR_TS;
TypeSpec INTEGER_TUPLE1_ITERABLE_TS;
TypeSpec SAME_TUPLE1_TS;
TypeSpec INTEGER_TUPLE1_TS;
TypeSpec SAME_SAME2_TUPLE2_TS;
TypeSpec INTEGER_SAME_TUPLE2_TS;
TypeSpec SAME_TUPLE1_ITERABLE_TS;
TypeSpec SAME_LVALUE_TUPLE1_TS;
TypeSpec INTEGER_SAME_LVALUE_TUPLE2_TS;
TypeSpec SAME_SAME2_LVALUE_TUPLE2_TS;
TypeSpec SAMETUPLE_TS;
TypeSpec COUNTUP_TS;
TypeSpec COUNTDOWN_TS;
TypeSpec SAME_ARRAYELEMITER_TS;
TypeSpec SAME_ARRAYINDEXITER_TS;
TypeSpec SAME_ARRAYITEMITER_TS;
TypeSpec SAME_QUEUEELEMITER_TS;
TypeSpec SAME_QUEUEINDEXITER_TS;
TypeSpec SAME_QUEUEITEMITER_TS;
TypeSpec SAME_SETELEMBYAGEITER_TS;
TypeSpec SAME_SETELEMBYORDERITER_TS;
TypeSpec SAME_SAME2_MAPITEMBYAGEITER_TS;
TypeSpec SAME_SAME2_MAPITEMBYORDERITER_TS;
TypeSpec SAME_SAME2_MAPINDEXBYAGEITER_TS;
TypeSpec SAME_SAME2_MAPINDEXBYORDERITER_TS;
TypeSpec SAME_SLICEELEMITER_TS;
TypeSpec SAME_SLICEINDEXITER_TS;
TypeSpec SAME_SLICEITEMITER_TS;
TypeSpec ANY_ANY2_ITEM_TS;
TypeSpec SAME_SAME2_ITEM_TS;
TypeSpec INTEGER_SAME_ITEM_TS;
TypeSpec COLON_TS;


TreenumerationType *make_treenum(const char *name, const char *kw1) {
    TreenumerationType *t = new TreenumerationType(name, { "", kw1 }, { 0, 0 });
    t->make_inner_scope()->leave();
    return t;
}


TreenumerationType *make_treenum(const char *name, TreenumInput *x) {
    std::vector<std::string> keywords = { "" };
    std::vector<unsigned> parents = { 0 };

    for (unsigned i = 0; x[i].kw; i++) {
        keywords.push_back(x[i].kw);
        parents.push_back(x[i].p);
    }

    TreenumerationType *t = new TreenumerationType(name, keywords, parents);
    t->make_inner_scope()->leave();
    return t;
}


void builtin_types(Scope *root_scope) {

    // Phase 1: declare the hypertype

    metatype_hypertype = new MetaType("<Meta>", {}, NULL);
    root_scope->add(metatype_hypertype);


    // Phase 2: declare the abstract metatypes, their scope does not matter
    
    type_metatype = new MetaType("<Type>", {}, NULL);
    root_scope->add(type_metatype);

    argument_metatype = new MetaType("<Argument>", { type_metatype }, NULL);
    root_scope->add(argument_metatype);

    value_metatype = new MetaType("<Value>", { argument_metatype }, NULL);
    root_scope->add(value_metatype);

    identity_metatype = new MetaType("<Identity>", { type_metatype }, NULL);
    root_scope->add(identity_metatype);

    module_metatype = new MetaType("<Module>", { type_metatype }, NULL);
    root_scope->add(module_metatype);

    tuple_metatype = new TupleMetaType("<Tuple>", { type_metatype }, NULL);
    root_scope->add(tuple_metatype);
    tuple_metatype->make_inner_scope()->leave();  // some tuple types are created below

    // Phase 3: declare the wildcard types, needed for the regular metatypes
    
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

    anytuple_type = new AnyType("<Anytuple>", {}, tuple_metatype);
    root_scope->add(anytuple_type);

    anytuple2_type = new AnyType("<Anytuple2>", {}, tuple_metatype);
    root_scope->add(anytuple2_type);

    anytuple3_type = new AnyType("<Anytuple3>", {}, tuple_metatype);
    root_scope->add(anytuple3_type);

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

    sametuple_type = new SameType("<Sametuple>", {}, tuple_metatype);
    root_scope->add(sametuple_type);

    sametuple2_type = new SameType("<Sametuple2>", {}, tuple_metatype);
    root_scope->add(sametuple2_type);

    sametuple3_type = new SameType("<Sametuple3>", {}, tuple_metatype);
    root_scope->add(sametuple3_type);


    // Phase 4: declare Colon type, which needs value_metatype, and needed for the colon scope
    
    colon_type = new ColonType("Colon");
    root_scope->add(colon_type);
    colon_scope = colon_type->make_inner_scope();


    // Phase 5: declare regular metatypes, need the wildcard types and the colon scope
    
    integer_metatype = new MetaType("Integer", { value_metatype }, make<IntegerDefinitionValue>);
    colon_scope->add(integer_metatype);

    enumeration_metatype = new MetaType("Enumeration", { value_metatype }, make<EnumerationDefinitionValue>);
    colon_scope->add(enumeration_metatype);

    treenumeration_metatype = new MetaType("Treenumeration", { value_metatype }, make<TreenumerationDefinitionValue>);
    colon_scope->add(treenumeration_metatype);

    record_metatype = new MetaType("Record", { value_metatype }, make<RecordDefinitionValue>);
    colon_scope->add(record_metatype);

    union_metatype = new MetaType("Union", { }, make<UnionDefinitionValue>);
    colon_scope->add(union_metatype);

    abstract_metatype = new MetaType("Abstract", { identity_metatype }, make<AbstractDefinitionValue>);
    colon_scope->add(abstract_metatype);

    class_metatype = new MetaType("Class", { identity_metatype }, make<ClassDefinitionValue>);
    colon_scope->add(class_metatype);

    interface_metatype = new MetaType("Interface", { value_metatype }, make<InterfaceDefinitionValue>);
    colon_scope->add(interface_metatype);

    import_metatype = new MetaType("Import", { type_metatype }, make<ImportDefinitionValue>);
    colon_scope->add(import_metatype);


    // Phase 6: declare internally used types

    void_type = new VoidType("<Void>");
    root_scope->add(void_type);

    uninitialized_type = new UninitializedType("<Uninitialized>");
    root_scope->add(uninitialized_type);

    initializable_type = new InitializableType("<Initializable>");
    root_scope->add(initializable_type);

    stringtemplate_type = new StringtemplateType("<Stringtemplate>");
    root_scope->add(stringtemplate_type);

    nosyvalue_type = new NosyValueType("<NosyValue>");
    root_scope->add(nosyvalue_type);

    nosytree_type = new NosytreeType("<Nosytree>");
    root_scope->add(nosytree_type);

    nosyref_type = new NosyrefType("<Nosyref>");
    root_scope->add(nosyref_type);

    partial_type = new PartialType("<Partial>");
    root_scope->add(partial_type);

    bare_type = new BareType("<Bare>");
    root_scope->add(bare_type);


    // Phase 7: declare special types

    lvalue_type = new AttributeType("Lvalue");
    root_scope->add(lvalue_type);
    
    ovalue_type = new AttributeType("Ovalue");
    root_scope->add(ovalue_type);

    dvalue_type = new DvalueType("Dvalue");
    root_scope->add(dvalue_type);

    code_type = new CodeType("Code");
    root_scope->add(code_type);

    whatever_type = new WhateverType("Whatever");
    root_scope->add(whatever_type);

    unit_type = new UnitType("Unit");
    root_scope->add(unit_type);

    tuple0_type = TupleType::get({});
    root_scope->add(tuple0_type);

    tuple1_type = TupleType::get({ "" });
    root_scope->add(tuple1_type);

    tuple2_type = TupleType::get({ "", "" });
    root_scope->add(tuple2_type);

    tuple3_type = TupleType::get({ "", "", "" });
    root_scope->add(tuple3_type);
    

    // Phase 8: declare basic types

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

    weakref_type = new WeakrefType("Weakref");
    root_scope->add(weakref_type);

    string_type = new StringType("String");
    root_scope->add(string_type);

    option_type = new OptionType("Option");
    root_scope->add(option_type);

    item_type = new ItemType("Item");
    root_scope->add(item_type);


    // Phase 9: define interface and exception types
    
    streamifiable_type = new InterfaceType("Streamifiable", {});
    root_scope->add(streamifiable_type);

    iterator_type = new InterfaceType("Iterator", { tuple_metatype });
    root_scope->add(iterator_type);

    iterable_type = new InterfaceType("Iterable", { tuple_metatype });
    root_scope->add(iterable_type);

    application_type = new AbstractType("Application", {});
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

    parse_exception_type = make_treenum("Parse_exception", "PARSE_ERROR");
    root_scope->add(parse_exception_type);
    

    // Phase 10: define container and iterator types

    linearray_type = new LinearrayType("<Linearray>");
    root_scope->add(linearray_type);

    circularray_type = new CircularrayType("<Circularray>");
    root_scope->add(circularray_type);

    rbtree_type = new RbtreeType("<Rbtree>");
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

    mapitembyageiter_type = new RecordType("Mapitembyage_iter", { value_metatype, value_metatype });
    root_scope->add(mapitembyageiter_type);

    mapitembyorderiter_type = new RecordType("Mapitembyorder_iter", { value_metatype, value_metatype });
    root_scope->add(mapitembyorderiter_type);

    mapindexbyageiter_type = new RecordType("Mapindexbyage_iter", { value_metatype, value_metatype });
    root_scope->add(mapindexbyageiter_type);

    mapindexbyorderiter_type = new RecordType("Mapindexbyorder_iter", { value_metatype, value_metatype });
    root_scope->add(mapindexbyorderiter_type);

    sliceelemiter_type = new RecordType("Sliceelem_iter", { value_metatype });
    root_scope->add(sliceelemiter_type);
    
    sliceindexiter_type = new RecordType("Sliceindex_iter", { value_metatype });
    root_scope->add(sliceindexiter_type);
    
    sliceitemiter_type = new RecordType("Sliceitem_iter", { value_metatype });
    root_scope->add(sliceitemiter_type);

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
    UNIT_TS = { unit_type };
    WHATEVER_TS = { whatever_type };
    WHATEVER_TUPLE1_CODE_TS = { code_type, tuple1_type, whatever_type };
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
    ANY_UNINITIALIZED_TS = { uninitialized_type, any_type };
    BARE_UNINITIALIZED_TS = { uninitialized_type, bare_type };
    STRINGTEMPLATE_TS = { stringtemplate_type };
    ANY_ARRAY_TS = { array_type, any_type };
    ANY_ARRAY_LVALUE_TS = { lvalue_type, array_type, any_type };
    SAME_ARRAY_TS = { array_type, same_type };
    SAME_LINEARRAY_REF_LVALUE_TS = { lvalue_type, ref_type, linearray_type, same_type };
    ANY_QUEUE_TS = { queue_type, any_type };
    ANY_QUEUE_LVALUE_TS = { lvalue_type, queue_type, any_type };
    SAME_QUEUE_TS = { queue_type, same_type };
    SAME_CIRCULARRAY_REF_LVALUE_TS = { lvalue_type, ref_type, circularray_type, same_type };
    ANY_RBTREE_REF_TS = { ref_type, rbtree_type, any_type };
    ANY_RBTREE_REF_LVALUE_TS = { lvalue_type, ref_type, rbtree_type, any_type };
    SAME_RBTREE_REF_LVALUE_TS = { lvalue_type, ref_type, rbtree_type, same_type };
    SAME_SAME2_ITEM_RBTREE_REF_LVALUE_TS = { lvalue_type, ref_type, rbtree_type, item_type, same_type, same2_type };
    ANY_RBTREE_PTR_TS = { ptr_type, rbtree_type, any_type };
    TUPLE0_CODE_TS = { code_type, tuple0_type };
    BOOLEAN_TUPLE1_CODE_TS = { code_type, tuple1_type, boolean_type };
    STREAMIFIABLE_TS = { streamifiable_type };
    ANYTUPLE_ITERATOR_TS = { iterator_type, anytuple_type };
    SAMETUPLE_ITERATOR_TS = { iterator_type, sametuple_type };
    SAME_SAME2_TUPLE2_ITERATOR_TS = { iterator_type, tuple2_type, same_type, same2_type };
    INTEGER_TUPLE1_ITERATOR_TS = { iterator_type, tuple1_type, integer_type };
    INTEGER_TUPLE1_ITERABLE_TS = { iterable_type, tuple1_type, integer_type };
    SAME_TUPLE1_TS = { tuple1_type, same_type };
    INTEGER_TUPLE1_TS = { tuple1_type, integer_type };
    SAME_SAME2_TUPLE2_TS = { tuple2_type, same_type, same2_type };
    INTEGER_SAME_TUPLE2_TS = { tuple2_type, integer_type, same_type };
    SAME_TUPLE1_ITERABLE_TS = { iterable_type, tuple1_type, same_type };
    SAME_LVALUE_TUPLE1_TS = { tuple1_type, lvalue_type, same_type };
    INTEGER_SAME_LVALUE_TUPLE2_TS = { tuple2_type, integer_type, lvalue_type, same_type };
    SAME_SAME2_LVALUE_TUPLE2_TS = { tuple2_type, same_type, lvalue_type, same2_type };
    SAMETUPLE_TS = { sametuple_type };
    ANYTUPLE_ITERABLE_TS = { iterable_type, anytuple_type };
    SAMETUPLE_ITERABLE_TS = { iterable_type, sametuple_type };
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
    SAME_SAME2_MAPITEMBYAGEITER_TS = { mapitembyageiter_type, same_type, same2_type };
    SAME_SAME2_MAPITEMBYORDERITER_TS = { mapitembyorderiter_type, same_type, same2_type };
    SAME_SAME2_MAPINDEXBYAGEITER_TS = { mapindexbyageiter_type, same_type, same2_type };
    SAME_SAME2_MAPINDEXBYORDERITER_TS = { mapindexbyorderiter_type, same_type, same2_type };
    SAME_SLICEELEMITER_TS = { sliceelemiter_type, same_type };
    SAME_SLICEINDEXITER_TS = { sliceindexiter_type, same_type };
    SAME_SLICEITEMITER_TS = { sliceitemiter_type, same_type };
    COLON_TS = { colon_type };

    SAME_SAMEID2_NOSYVALUE_ITEM_RBTREE_REF_TS = { ref_type, rbtree_type, item_type, same_type, nosyvalue_type, sameid2_type };
    SAMEID_NOSYVALUE_SAME2_ITEM_RBTREE_REF_TS = { ref_type, rbtree_type, item_type, nosyvalue_type, sameid_type, same2_type };
    SAMEID_NOSYVALUE_RBTREE_REF_TS = { ref_type, rbtree_type,  nosyvalue_type, sameid_type };

    ANY_SET_TS = { set_type, any_type };
    ANY_SET_LVALUE_TS = { lvalue_type, set_type, any_type };
    SAME_SET_TS = { set_type, same_type };
    ANY_ANY2_MAP_TS = { map_type, any_type, any2_type };
    ANY_ANY2_MAP_LVALUE_TS = { lvalue_type, map_type, any_type, any2_type };
    SAME_SAME2_MAP_TS = { map_type, same_type, same2_type };
    ANY_ANYID2_WEAKVALUEMAP_TS = { weakvaluemap_type, any_type, anyid2_type };
    ANYID_WEAKSET_TS = { weakset_type, anyid_type };
    ANYID_ANY2_WEAKINDEXMAP_TS = { weakindexmap_type, anyid_type, any2_type };
}


void implement(Scope *implementor_scope, PivotRequirement pr, TypeSpec interface_ts, std::string implementation_name, std::vector<Identifier *> contents) {
    TypeSpec implementor_ts = implementor_scope->get_pivot_ts();
    std::cerr << "XXX built-in implementing " << implementor_ts << " " << implementation_name << " as " << interface_ts << "\n";
    
    Implementation *implementation = new Implementation(implementation_name, pr, interface_ts, AS_ROLE);
    implementor_scope->add(implementation);
    
    for (Identifier *i : contents) {
        // Necessary to handle nested implementations
        Associable *a = implementation->lookup_associable(implementation_name);
        if (!a)
            throw INTERNAL_ERROR;

        i->name = implementation_name + QUALIFIER_NAME + i->name;  // TODO: ugly!
        
        if (!a->check_associated(i))
            throw INTERNAL_ERROR;
            
        implementor_scope->add(i);

        if (i->get_pivot_ts() != implementation->get_pivot_ts()) {
            std::cerr << "XXXX " << i->get_pivot_ts() << " != " << implementation->get_pivot_ts() << "\n";
            throw INTERNAL_ERROR;  // sanity check
        }
    }
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

    Scope *integer_metascope = integer_metatype->make_inner_scope();
    
    for (auto &item : integer_rvalue_operations)
        integer_metascope->add(new TemplateOperation<IntegerOperationValue>(item.name, item.operation));

    for (auto &item : integer_lvalue_operations)
        integer_metascope->add(new TemplateOperation<IntegerOperationValue>(item.name, item.operation));

    integer_metascope->leave();

    //Scope *integer_lmetascope = integer_metatype->make_lvalue_scope();


    //integer_lmetascope->leave();
    
    for (auto t : {
        integer_type, integer32_type, integer16_type, integer8_type,
        unsigned_integer_type, unsigned_integer32_type, unsigned_integer16_type, unsigned_integer8_type
    }) {
        t->make_inner_scope()->leave();
    }
    
    Scope *integer_is = integer_type->get_inner_scope();
    integer_is->enter();
    integer_is->add(new TemplateIdentifier<CountupValue>("countup", RVALUE_PIVOT));
    integer_is->add(new TemplateIdentifier<CountdownValue>("countdown", RVALUE_PIVOT));
    integer_is->leave();
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

    Scope *float_scope = float_type->make_inner_scope();

    for (auto &item : float_rvalue_operations)
        float_scope->add(new TemplateOperation<FloatOperationValue>(item.name, item.operation));

    float_scope->add(new TemplateIdentifier<FloatIsnanValue>("is_nan", RVALUE_PIVOT));

    float_scope->add(new ImportedFloatFunction("log", "log", RVALUE_PIVOT, NO_TS, FLOAT_TS));
    float_scope->add(new ImportedFloatFunction("exp", "exp", RVALUE_PIVOT, NO_TS, FLOAT_TS));
    float_scope->add(new ImportedFloatFunction("pow", "binary_exponent", RVALUE_PIVOT, FLOAT_TS, FLOAT_TS));

    for (auto &item : float_lvalue_operations)
        float_scope->add(new TemplateOperation<FloatOperationValue>(item.name, item.operation));

    float_scope->leave();
}


void define_interfaces() {
    // Streamifiable interface
    TypeSpec spts = STREAMIFIABLE_TS;
    DataScope *sis = streamifiable_type->make_inner_scope();
    Function *sf = new Function("streamify",
        RVALUE_PIVOT,
        GENERIC_FUNCTION,
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
    TypeSpec jpts = ANYTUPLE_ITERABLE_TS;
    DataScope *jis = iterable_type->make_inner_scope();
    Function *xf = new Function("iter",
        RVALUE_PIVOT,
        GENERIC_FUNCTION,
        TSs {},
        Ss {},
        TSs { SAMETUPLE_ITERATOR_TS },
        NULL,
        NULL
    );
    jis->add(xf);
    iterable_type->complete_type();
    jis->leave();

    // Iterator interface
    TypeSpec ipts = ANYTUPLE_ITERATOR_TS;
    DataScope *iis = iterator_type->make_inner_scope();
    Function *nf = new Function("next",
        LVALUE_PIVOT,
        LVALUE_FUNCTION,
        TSs {},
        Ss {},
        TSs { SAMETUPLE_TS },  // FIXME: this is a hack
        iterator_done_exception_type,
        NULL
    );
    iis->add(nf);
    iterator_type->complete_type();
    iis->leave();
    
    // Application abstract
    TypeSpec apts = { ptr_type, application_type };
    DataScope *ais = application_type->make_inner_scope();
    Function *tf = new Function("start",
        RVALUE_PIVOT,
        GENERIC_FUNCTION,
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
void define_container_iterator(Type *iter_type, TypeSpec container_ts, TypeSpec tuple_ts) {
    DataScope *aiis = iter_type->make_inner_scope();

    // Order matters!
    aiis->add(new Variable("container", container_ts.lvalue()));
    aiis->add(new Variable("value", INTEGER_LVALUE_TS));

    implement(aiis, RVALUE_PIVOT, tuple_ts.prefix(iterable_type), "iterable", {
        new Identity("iter")
    });

    implement(aiis, LVALUE_PIVOT, tuple_ts.prefix(iterator_type), "iterator", {
        new TemplateIdentifier<NextValue>("next", LVALUE_PIVOT),
    });
    
    iter_type->complete_type();
    aiis->leave();
}


template <typename NextValue>
void define_slice_iterator(Type *iter_type, TypeSpec container_ts, TypeSpec tuple_ts) {
    DataScope *aiis = iter_type->make_inner_scope();

    // Order matters!
    aiis->add(new Variable("container", container_ts.lvalue()));
    aiis->add(new Variable("front", INTEGER_LVALUE_TS));
    aiis->add(new Variable("length", INTEGER_LVALUE_TS));
    aiis->add(new Variable("value", INTEGER_LVALUE_TS));

    implement(aiis, RVALUE_PIVOT, tuple_ts.prefix(iterable_type), "iterable", {
        new Identity("iter")
    });

    implement(aiis, LVALUE_PIVOT, tuple_ts.prefix(iterator_type), "iterator", {
        new TemplateIdentifier<NextValue>("next", LVALUE_PIVOT)
    });
    
    iter_type->complete_type();
    aiis->leave();
}


void define_iterators() {
    // Counter operations
    for (auto is_down : { false, true }) {
        RecordType *counter_type = ptr_cast<RecordType>(is_down ? countdown_type : countup_type);
        TypeSpec COUNTER_TS = { counter_type };
    
        DataScope *cis = counter_type->make_inner_scope();
    
        cis->add(new Variable("limit", INTEGER_LVALUE_TS));  // Order matters!
        cis->add(new Variable("value", INTEGER_LVALUE_TS));

        implement(cis, RVALUE_PIVOT, INTEGER_TUPLE1_ITERABLE_TS, "iterable", {
            new Identity("iter")
        });

        Identifier *next_fn;
        
        if (!is_down) {
            next_fn = new TemplateIdentifier<CountupNextValue>("next", LVALUE_PIVOT);
        }
        else {
            next_fn = new TemplateIdentifier<CountdownNextValue>("next", LVALUE_PIVOT);
        }

        implement(cis, LVALUE_PIVOT, INTEGER_TUPLE1_ITERATOR_TS, "iter", {
            next_fn
        });
        
        counter_type->complete_type();
        cis->leave();
    }

    // Item type for itemized iteration
    RecordType *item_type = ptr_cast<RecordType>(::item_type);
    DataScope *itis = item_type->make_inner_scope();

    itis->add(new Variable("index", SAME_LVALUE_TS));  // Order matters!
    itis->add(new Variable("value", SAME2_LVALUE_TS));
    
    item_type->complete_type();
    itis->leave();
    
    TypeSpec INTEGER_SAME_TUPLE2_ITERATOR_TS = { iterator_type, tuple2_type, integer_type, same_type };

    // Array Iterator operations
    define_container_iterator<ArrayNextElemValue>(arrayelemiter_type, SAME_ARRAY_TS, SAME_LVALUE_TUPLE1_TS);
    define_container_iterator<ArrayNextIndexValue>(arrayindexiter_type, SAME_ARRAY_TS, INTEGER_TUPLE1_TS);
    define_container_iterator<ArrayNextItemValue>(arrayitemiter_type, SAME_ARRAY_TS, INTEGER_SAME_LVALUE_TUPLE2_TS);

    // Circularray Iterator operations
    define_container_iterator<QueueNextElemValue>(queueelemiter_type, SAME_QUEUE_TS, SAME_LVALUE_TUPLE1_TS);
    define_container_iterator<QueueNextIndexValue>(queueindexiter_type, SAME_QUEUE_TS, INTEGER_TUPLE1_TS);
    define_container_iterator<QueueNextItemValue>(queueitemiter_type, SAME_QUEUE_TS, INTEGER_SAME_LVALUE_TUPLE2_TS);

    // Set Iterator operations
    define_container_iterator<SetNextElemByAgeValue>(setelembyageiter_type, SAME_SET_TS, SAME_TUPLE1_TS);
    define_container_iterator<SetNextElemByOrderValue>(setelembyorderiter_type, SAME_SET_TS, SAME_TUPLE1_TS);

    // Map Iterator operations
    define_container_iterator<MapNextItemByAgeValue>(mapitembyageiter_type, SAME_SAME2_MAP_TS, SAME_SAME2_LVALUE_TUPLE2_TS);
    define_container_iterator<MapNextItemByOrderValue>(mapitembyorderiter_type, SAME_SAME2_MAP_TS, SAME_SAME2_LVALUE_TUPLE2_TS);
    define_container_iterator<MapNextIndexByAgeValue>(mapindexbyageiter_type, SAME_SAME2_MAP_TS, SAME_TUPLE1_TS);
    define_container_iterator<MapNextIndexByOrderValue>(mapindexbyorderiter_type, SAME_SAME2_MAP_TS, SAME_TUPLE1_TS);

    // Slice Iterator operations
    define_slice_iterator<SliceNextElemValue>(sliceelemiter_type, SAME_ARRAY_TS, SAME_LVALUE_TUPLE1_TS);
    define_slice_iterator<SliceNextIndexValue>(sliceindexiter_type, SAME_ARRAY_TS, INTEGER_TUPLE1_TS);
    define_slice_iterator<SliceNextItemValue>(sliceitemiter_type, SAME_ARRAY_TS, INTEGER_SAME_LVALUE_TUPLE2_TS);
}


void define_character() {
    Scope *char_scope = character_type->make_inner_scope();

    char_scope->add(new TemplateOperation<IntegerOperationValue>("compare", COMPARE));
    char_scope->add(new CharacterRawStreamifiableImplementation("raw"));

    char_scope->add(new SysvFunction("Character__is_alnum", "is_alnum", RVALUE_PIVOT, GENERIC_FUNCTION, {}, {}, { BOOLEAN_TS }));
    char_scope->add(new SysvFunction("Character__is_alpha", "is_alpha", RVALUE_PIVOT, GENERIC_FUNCTION, {}, {}, { BOOLEAN_TS }));
    char_scope->add(new SysvFunction("Character__is_ascii", "is_ascii", RVALUE_PIVOT, GENERIC_FUNCTION, {}, {}, { BOOLEAN_TS }));
    char_scope->add(new SysvFunction("Character__is_blank", "is_blank", RVALUE_PIVOT, GENERIC_FUNCTION, {}, {}, { BOOLEAN_TS }));
    char_scope->add(new SysvFunction("Character__is_cntrl", "is_cntrl", RVALUE_PIVOT, GENERIC_FUNCTION, {}, {}, { BOOLEAN_TS }));
    char_scope->add(new SysvFunction("Character__is_digit", "is_digit", RVALUE_PIVOT, GENERIC_FUNCTION, {}, {}, { BOOLEAN_TS }));
    char_scope->add(new SysvFunction("Character__is_lower", "is_lower", RVALUE_PIVOT, GENERIC_FUNCTION, {}, {}, { BOOLEAN_TS }));
    char_scope->add(new SysvFunction("Character__is_punct", "is_punct", RVALUE_PIVOT, GENERIC_FUNCTION, {}, {}, { BOOLEAN_TS }));
    char_scope->add(new SysvFunction("Character__is_space", "is_space", RVALUE_PIVOT, GENERIC_FUNCTION, {}, {}, { BOOLEAN_TS }));
    char_scope->add(new SysvFunction("Character__is_upper", "is_upper", RVALUE_PIVOT, GENERIC_FUNCTION, {}, {}, { BOOLEAN_TS }));
    char_scope->add(new SysvFunction("Character__is_xdigit", "is_xdigit", RVALUE_PIVOT, GENERIC_FUNCTION, {}, {}, { BOOLEAN_TS }));

    char_scope->add(new TemplateOperation<IntegerOperationValue>("assign other", ASSIGN));

    char_scope->leave();
}


void define_string() {
    RecordType *record_type = ptr_cast<RecordType>(string_type);
    DataScope *is = record_type->make_inner_scope();

    TypeSpec MEMBER_TS = CHARACTER_ARRAY_LVALUE_TS;
    is->add(new Variable("chars", MEMBER_TS));

    is->add(new RecordWrapperIdentifier("length", RVALUE_PIVOT, MEMBER_TS, INTEGER_TS, "length"));
    is->add(new RecordWrapperIdentifier("binary_plus", RVALUE_PIVOT, MEMBER_TS, STRING_TS, "binary_plus", "chars"));
    is->add(new RecordWrapperIdentifier("index", RVALUE_PIVOT, MEMBER_TS, CHARACTER_TS, "index"));

    is->add(new TemplateOperation<StringOperationValue>("is_equal", EQUAL));
    is->add(new TemplateOperation<StringOperationValue>("not_equal", NOT_EQUAL));
    is->add(new TemplateOperation<StringOperationValue>("compare", COMPARE));

    implement(is, RVALUE_PIVOT, TypeSpec { iterable_type, tuple1_type, character_type }, "iterable", {
        new RecordWrapperIdentifier("iter", RVALUE_PIVOT, MEMBER_TS, TypeSpec { arrayelemiter_type, character_type }, "elements")
    });

    is->add(new RecordWrapperIdentifier("elements", RVALUE_PIVOT, MEMBER_TS, TypeSpec { arrayelemiter_type, character_type }, "elements"));
    is->add(new RecordWrapperIdentifier("indexes", RVALUE_PIVOT, MEMBER_TS, TypeSpec { arrayindexiter_type, character_type }, "indexes"));
    is->add(new RecordWrapperIdentifier("items", RVALUE_PIVOT, MEMBER_TS, TypeSpec { arrayitemiter_type, character_type }, "items"));

    is->add(new StringRawStreamifiableImplementation("raw"));

    // String operations
    is->add(new SysvFunction("C__encode_utf8", "encode_utf8", RVALUE_PIVOT, GENERIC_FUNCTION, TSs {}, {}, TSs { UNSIGNED_INTEGER8_ARRAY_TS }));

    is->add(new RecordWrapperIdentifier("assign_plus", LVALUE_PIVOT, MEMBER_TS, STRING_TS, "assign_plus", "chars"));
    is->add(new RecordWrapperIdentifier("realloc", LVALUE_PIVOT, MEMBER_TS, STRING_LVALUE_TS, "realloc"));

    is->add(new TemplateOperation<RecordOperationValue>("assign other", ASSIGN));

    record_type->complete_type();
    is->leave();
}


void define_slice(RootScope *root_scope) {
    TypeSpec SAME_ARRAY_LVALUE_TS = { lvalue_type, array_type, same_type };
    
    RecordType *record_type = ptr_cast<RecordType>(slice_type);
    DataScope *is = record_type->make_inner_scope();

    is->add(new Variable("array", SAME_ARRAY_LVALUE_TS));
    is->add(new Variable("front", INTEGER_LVALUE_TS));
    is->add(new Variable("length", INTEGER_LVALUE_TS));

    is->add(new TemplateIdentifier<SliceIndexValue>("index", RVALUE_PIVOT));
    is->add(new TemplateIdentifier<SliceFindValue>("find", RVALUE_PIVOT));
    is->add(new TemplateIdentifier<SliceSliceValue>("slice", RVALUE_PIVOT));

    implement(is, RVALUE_PIVOT, SAME_TUPLE1_ITERABLE_TS, "iterable", {
        new TemplateIdentifier<SliceElemIterValue>("iter", RVALUE_PIVOT)
    });

    is->add(new TemplateIdentifier<SliceElemIterValue>("elements", RVALUE_PIVOT));
    is->add(new TemplateIdentifier<SliceIndexIterValue>("indexes", RVALUE_PIVOT));
    is->add(new TemplateIdentifier<SliceItemIterValue>("items", RVALUE_PIVOT));

    record_type->complete_type();
    is->leave();

    // This is for a concrete type parameter only
    ExtensionScope *es = new ExtensionScope(is);
    root_scope->add(es);
    es->set_pivot_ts(BYTE_SLICE_TS);
    es->enter();
    es->add(new SysvFunction("C__decode_utf8_slice", "decode_utf8", RVALUE_PIVOT, GENERIC_FUNCTION, TSs {}, {}, TSs { STRING_TS }));
    es->leave();
}


void define_weakref() {
    RecordType *record_type = ptr_cast<RecordType>(weakref_type);
    DataScope *is = record_type->make_inner_scope();

    TypeSpec MEMBER_TS = SAMEID_NOSYVALUE_LVALUE_TS;
    TypeSpec NOSY_TS = MEMBER_TS.rvalue().prefix(nosyref_type).prefix(ref_type);

    is->add(new Variable("nosyref", NOSY_TS));

    is->add(new TemplateOperation<RecordOperationValue>("compare", COMPARE));

    is->add(new TemplateOperation<RecordOperationValue>("assign other", ASSIGN));

    record_type->complete_type();
    is->leave();
}


void define_option() {
    DataScope *is = option_type->make_inner_scope();

    is->add(new TemplateOperation<OptionOperationValue>("compare", COMPARE));

    is->add(new TemplateOperation<OptionOperationValue>("assign other", ASSIGN));

    option_type->complete_type();
    is->leave();
}


void define_array(RootScope *root_scope) {
    DataScope *array_scope = array_type->make_inner_scope();

    array_scope->add(new Variable("linearray", SAME_LINEARRAY_REF_LVALUE_TS));

    array_scope->add(new TemplateIdentifier<ArrayLengthValue>("length", RVALUE_PIVOT));
    array_scope->add(new TemplateIdentifier<ArrayRemoveValue>("remove", LVALUE_PIVOT));
    array_scope->add(new TemplateIdentifier<ArrayConcatenationValue>("binary_plus", RVALUE_PIVOT));
    array_scope->add(new TemplateIdentifier<ArrayIndexValue>("index", RVALUE_PIVOT));
    array_scope->add(new TemplateIdentifier<ArraySortValue>("sort", LVALUE_PIVOT));
    array_scope->add(new TemplateIdentifier<ArraySliceValue>("slice", RVALUE_PIVOT));
    
    // Array iterable operations
    implement(array_scope, RVALUE_PIVOT, SAME_TUPLE1_ITERABLE_TS, "iterable", {
        new TemplateIdentifier<ArrayElemIterValue>("iter", RVALUE_PIVOT)
    });

    array_scope->add(new TemplateIdentifier<ArrayElemIterValue>("elements", RVALUE_PIVOT));
    array_scope->add(new TemplateIdentifier<ArrayIndexIterValue>("indexes", RVALUE_PIVOT));
    array_scope->add(new TemplateIdentifier<ArrayItemIterValue>("items", RVALUE_PIVOT));

    array_scope->add(new TemplateOperation<ArrayReallocValue>("realloc", LTWEAK));
    array_scope->add(new TemplateIdentifier<ArrayRefillValue>("refill", LVALUE_PIVOT));
    array_scope->add(new TemplateIdentifier<ArrayExtendValue>("assign_plus", LVALUE_PIVOT));
    array_scope->add(new TemplateIdentifier<ArrayPushValue>("push", LVALUE_PIVOT));
    array_scope->add(new TemplateIdentifier<ArrayPopValue>("pop", LVALUE_PIVOT));

    array_type->complete_type();
    array_scope->leave();
    
    ExtensionScope *es = new ExtensionScope(array_scope);
    root_scope->add(es);
    es->set_pivot_ts(UNSIGNED_INTEGER8_ARRAY_TS);
    es->enter();
    es->add(new SysvFunction("C__decode_utf8", "decode_utf8", RVALUE_PIVOT, GENERIC_FUNCTION, TSs {}, {}, TSs { STRING_TS }));
    es->leave();
}


void define_queue() {
    Scope *queue_scope = queue_type->make_inner_scope();

    queue_scope->add(new Variable("circularray", SAME_CIRCULARRAY_REF_LVALUE_TS));

    queue_scope->add(new TemplateIdentifier<QueueLengthValue>("length", RVALUE_PIVOT));
    queue_scope->add(new TemplateIdentifier<QueueIndexValue>("index", RVALUE_PIVOT));
    
    // Queue iterable operations
    implement(queue_scope, RVALUE_PIVOT, SAME_TUPLE1_ITERABLE_TS, "iterable", {
        new TemplateIdentifier<QueueElemIterValue>("iter", RVALUE_PIVOT)
    });

    queue_scope->add(new TemplateIdentifier<QueueElemIterValue>("elements", RVALUE_PIVOT));
    queue_scope->add(new TemplateIdentifier<QueueIndexIterValue>("indexes", RVALUE_PIVOT));
    queue_scope->add(new TemplateIdentifier<QueueItemIterValue>("items", RVALUE_PIVOT));

    queue_scope->add(new TemplateIdentifier<QueuePushValue>("push", LVALUE_PIVOT));
    queue_scope->add(new TemplateIdentifier<QueuePopValue>("pop", LVALUE_PIVOT));
    queue_scope->add(new TemplateIdentifier<QueueUnshiftValue>("unshift", LVALUE_PIVOT));
    queue_scope->add(new TemplateIdentifier<QueueShiftValue>("shift", LVALUE_PIVOT));

    queue_type->complete_type();
    queue_scope->leave();
}


void define_set() {
    TypeSpec MEMBER_TS = SAME_RBTREE_REF_LVALUE_TS;
    
    DataScope *is = set_type->make_inner_scope();

    is->add(new Variable("rbtree", MEMBER_TS));

    is->add(new TemplateIdentifier<SetLengthValue>("length", RVALUE_PIVOT));
    is->add(new TemplateIdentifier<SetHasValue>("has", RVALUE_PIVOT));

    // Iteration
    implement(is, RVALUE_PIVOT, SAME_TUPLE1_ITERABLE_TS, "iterable", {
        new TemplateIdentifier<SetElemByAgeIterValue>("iter", RVALUE_PIVOT)
    });
    
    is->add(new TemplateIdentifier<SetElemByAgeIterValue>("elements_by_age", RVALUE_PIVOT));
    is->add(new TemplateIdentifier<SetElemByOrderIterValue>("elements_by_order", RVALUE_PIVOT));

    is->add(new TemplateIdentifier<SetAddValue>("add", LVALUE_PIVOT));
    is->add(new TemplateIdentifier<SetRemoveValue>("remove", LVALUE_PIVOT));
        
    set_type->complete_type();
    is->leave();
}


void define_map() {
    TypeSpec MEMBER_TS = SAME_SAME2_ITEM_RBTREE_REF_LVALUE_TS;
    
    DataScope *is = map_type->make_inner_scope();

    is->add(new Variable("rbtree", MEMBER_TS));
    
    is->add(new TemplateIdentifier<MapLengthValue>("length", RVALUE_PIVOT));
    is->add(new TemplateIdentifier<MapHasValue>("has", RVALUE_PIVOT));
    is->add(new TemplateIdentifier<MapIndexValue>("index", RVALUE_PIVOT));

    // Iteration
    implement(is, RVALUE_PIVOT, SAME_TUPLE1_ITERABLE_TS, "iterable", {
        new TemplateIdentifier<MapIndexByAgeIterValue>("iter", RVALUE_PIVOT)
    });

    is->add(new TemplateIdentifier<MapItemByAgeIterValue>("items_by_age", RVALUE_PIVOT));
    is->add(new TemplateIdentifier<MapItemByOrderIterValue>("items_by_order", RVALUE_PIVOT));

    is->add(new TemplateIdentifier<MapIndexByAgeIterValue>("indexes_by_age", RVALUE_PIVOT));
    is->add(new TemplateIdentifier<MapIndexByOrderIterValue>("indexes_by_order", RVALUE_PIVOT));

    is->add(new TemplateIdentifier<MapAddValue>("add", LVALUE_PIVOT));
    is->add(new TemplateIdentifier<MapRemoveValue>("remove", LVALUE_PIVOT));
    
    map_type->complete_type();
    is->leave();
}


void define_weakvaluemap() {
    TypeSpec ELEM_TS = SAME_SAMEID2_NOSYVALUE_ITEM_TS;
    TypeSpec NOSYTREE_TS = ELEM_TS.prefix(rbtree_type).prefix(ref_type).prefix(nosytree_type).prefix(ref_type);

    DataScope *is = weakvaluemap_type->make_inner_scope();

    is->add(new Variable("nosytree", NOSYTREE_TS));
    
    is->add(new NosytreeTemplateIdentifier<NosyRbtreeLengthValue>("length", RVALUE_PIVOT, ELEM_TS));
    is->add(new NosytreeTemplateIdentifier<NosyRbtreeHasValue>("has", RVALUE_PIVOT, ELEM_TS));
    is->add(new NosytreeTemplateIdentifier<NosyRbtreeIndexValue>("index", RVALUE_PIVOT, ELEM_TS));

    // Must not define iteration due to the volatility of this container

    is->add(new NosytreeTemplateIdentifier<NosyRbtreeAddItemValue>("add", LVALUE_PIVOT, ELEM_TS));
    is->add(new NosytreeTemplateIdentifier<NosyRbtreeRemoveValue>("remove", LVALUE_PIVOT, ELEM_TS));
    
    weakvaluemap_type->complete_type();
    is->leave();
}


void define_weakindexmap() {
    TypeSpec ELEM_TS = SAMEID_NOSYVALUE_SAME2_ITEM_TS;
    TypeSpec NOSYTREE_TS = ELEM_TS.prefix(rbtree_type).prefix(ref_type).prefix(nosytree_type).prefix(ref_type);
    
    DataScope *is = weakindexmap_type->make_inner_scope();

    is->add(new Variable("nosytree", NOSYTREE_TS));

    is->add(new NosytreeTemplateIdentifier<NosyRbtreeLengthValue>("length", RVALUE_PIVOT, ELEM_TS));
    is->add(new NosytreeTemplateIdentifier<NosyRbtreeHasValue>("has", RVALUE_PIVOT, ELEM_TS));
    is->add(new NosytreeTemplateIdentifier<NosyRbtreeIndexValue>("index", RVALUE_PIVOT, ELEM_TS));
    
    // Must not define iteration due to the volatility of this container

    is->add(new NosytreeTemplateIdentifier<NosyRbtreeAddItemValue>("add", LVALUE_PIVOT, ELEM_TS));
    is->add(new NosytreeTemplateIdentifier<NosyRbtreeRemoveValue>("remove", LVALUE_PIVOT, ELEM_TS));

    weakindexmap_type->complete_type();
    is->leave();
}


void define_weakset() {
    TypeSpec ELEM_TS = SAMEID_NOSYVALUE_TS;
    TypeSpec NOSYTREE_TS = ELEM_TS.prefix(rbtree_type).prefix(ref_type).prefix(nosytree_type).prefix(ref_type);
    
    DataScope *is = weakset_type->make_inner_scope();

    is->add(new Variable("nosytree", NOSYTREE_TS));
    
    is->add(new NosytreeTemplateIdentifier<NosyRbtreeLengthValue>("length", RVALUE_PIVOT, ELEM_TS));
    is->add(new NosytreeTemplateIdentifier<NosyRbtreeHasValue>("has", RVALUE_PIVOT, ELEM_TS));

    // Must not define iteration due to the volatility of this container

    is->add(new NosytreeTemplateIdentifier<NosyRbtreeAddValue>("add", LVALUE_PIVOT, ELEM_TS));
    is->add(new NosytreeTemplateIdentifier<NosyRbtreeRemoveValue>("remove", LVALUE_PIVOT, ELEM_TS));
    
    weakset_type->complete_type();
    is->leave();
}


void builtin_colon(Scope *root_scope) {
    colon_scope->add(new TemplateOperation<IfValue>("if", TWEAK));
    colon_scope->add(new TemplateIdentifier<RepeatValue>("repeat", NO_PIVOT));
    colon_scope->add(new TemplateIdentifier<ForEachValue>("for", NO_PIVOT));
    colon_scope->add(new TemplateIdentifier<SwitchValue>("switch", NO_PIVOT));
    colon_scope->add(new TemplateIdentifier<RaiseValue>("raise", NO_PIVOT));
    colon_scope->add(new TemplateIdentifier<TryValue>("try", NO_PIVOT));
    colon_scope->add(new TemplateIdentifier<IsValue>("is", NO_PIVOT));
    colon_scope->add(new TemplateOperation<FunctionReturnValue>("return", TWEAK));
    colon_scope->add(new TemplateIdentifier<PassValue>("pass", NO_PIVOT));
    colon_scope->add(new TemplateIdentifier<DieValue>("die", NO_PIVOT));
    colon_scope->add(new TemplateIdentifier<EvaluateValue>("evaluate", NO_PIVOT));

    colon_scope->add(new TemplateIdentifier<FunctorDefinitionValue>("functor", NO_PIVOT));

    colon_scope->add(new TemplateIdentifier<PtrCastValue>("ptr", NO_PIVOT));

    colon_scope->add(new TemplateIdentifier<FunctionDefinitionValue>("Function", NO_PIVOT));
    colon_scope->add(new TemplateIdentifier<ProcedureDefinitionValue>("Procedure", NO_PIVOT));
    colon_scope->add(new TemplateIdentifier<InitializerDefinitionValue>("Initializer", NO_PIVOT));
    colon_scope->add(new TemplateIdentifier<FinalizerDefinitionValue>("Finalizer", NO_PIVOT));
    colon_scope->add(new TemplateIdentifier<PlainRoleDefinitionValue>("Role", NO_PIVOT));
    colon_scope->add(new TemplateIdentifier<AutoRoleDefinitionValue>("Auto", NO_PIVOT));
    colon_scope->add(new TemplateIdentifier<RequireRoleDefinitionValue>("Require", NO_PIVOT));
    colon_scope->add(new TemplateIdentifier<ProvideDefinitionValue>("Provide", NO_PIVOT));
    colon_scope->add(new TemplateIdentifier<ImplementationDefinitionValue>("Implementation", NO_PIVOT));
    colon_scope->add(new TemplateIdentifier<GlobalDefinitionValue>("Global", NO_PIVOT));

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
    Scope *is = std_type->make_inner_scope();
    
    Declaration *std = new GlobalNamespace("Std", STD_TS);
    root_scope->add(std);

    is->add(new SysvFunction("Std__printi", "printi", RVALUE_PIVOT, GENERIC_FUNCTION, INTEGER_TSS, value_names, NO_TSS));
    is->add(new SysvFunction("Std__printc", "printc", RVALUE_PIVOT, GENERIC_FUNCTION, UNSIGNED_INTEGER8_TSS, value_names, NO_TSS));
    is->add(new SysvFunction("Std__printd", "printd", RVALUE_PIVOT, GENERIC_FUNCTION, FLOAT_TSS, value_names, NO_TSS));
    is->add(new SysvFunction("Std__printb", "printb", RVALUE_PIVOT, GENERIC_FUNCTION, UNSIGNED_INTEGER8_ARRAY_TSS, value_names, NO_TSS));
    is->add(new SysvFunction("Std__prints", "prints", RVALUE_PIVOT, GENERIC_FUNCTION, TSs { STRING_TS }, value_names, NO_TSS));
    is->add(new SysvFunction("Std__printp", "printp", RVALUE_PIVOT, GENERIC_FUNCTION, TSs { ANYID_REF_LVALUE_TS }, value_names, NO_TSS));  // needs Lvalue to avoid ref copy

    is->add(new SysvFunction("Std__parse_ws", "parse_ws", RVALUE_PIVOT, GENERIC_FUNCTION, TSs { STRING_TS, INTEGER_LVALUE_TS }, { "str", "idx" }, NO_TSS));
    is->add(new SysvFunction("Std__parse_identifier", "parse_identifier", RVALUE_PIVOT, GENERIC_FUNCTION, TSs { STRING_TS, INTEGER_LVALUE_TS }, { "str", "idx" }, { STRING_TS }, parse_exception_type));
    is->add(new SysvFunction("Std__parse_integer", "parse_integer", RVALUE_PIVOT, GENERIC_FUNCTION, TSs { STRING_TS, INTEGER_LVALUE_TS }, { "str", "idx" }, { INTEGER_TS }, parse_exception_type));
    is->add(new SysvFunction("Std__parse_float", "parse_float", RVALUE_PIVOT, GENERIC_FUNCTION, TSs { STRING_TS, INTEGER_LVALUE_TS }, { "str", "idx" }, { FLOAT_TS }, parse_exception_type));
    is->add(new SysvFunction("Std__parse_jstring", "parse_jstring", RVALUE_PIVOT, GENERIC_FUNCTION, TSs { STRING_TS, INTEGER_LVALUE_TS }, { "str", "idx" }, { STRING_TS }, parse_exception_type));

    is->add(new SysvFunction("Std__print_jstring", "print_jstring", RVALUE_PIVOT, GENERIC_FUNCTION, TSs { STRING_TS, STRING_LVALUE_TS }, { "str", "stream" }, {}));

    is->add(new TemplateIdentifier<FrameNameValue>("frame_name", RVALUE_PIVOT));
    is->add(new TemplateIdentifier<FrameStuffValue>("frame_stuff", RVALUE_PIVOT));
    is->add(new TemplateIdentifier<DoubleStackValue>("double_stack", RVALUE_PIVOT));

    std_type->complete_type();
    is->leave();
}


RootScope *init_builtins() {
    RootScope *root_scope = new RootScope;
    root_scope->set_name("<root>");
    root_scope->enter();

    builtin_types(root_scope);

    define_interfaces();

    define_character();
    define_string();
    define_slice(root_scope);
    
    define_iterators();
    
    define_array(root_scope);
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
        
    // Boolean operations
    Scope *bool_scope = boolean_type->make_inner_scope();
    bool_scope->add(new TemplateOperation<BooleanOperationValue>("compare", COMPARE));
    bool_scope->add(new TemplateIdentifier<BooleanNotValue>("logical not", RVALUE_PIVOT));
    bool_scope->add(new TemplateIdentifier<BooleanAndValue>("logical and", RVALUE_PIVOT));
    bool_scope->add(new TemplateIdentifier<BooleanOrValue>("logical or", RVALUE_PIVOT));
    bool_scope->add(new TemplateOperation<BooleanOperationValue>("assign other", ASSIGN));
    bool_scope->leave();

    // Enum operations
    Scope *enum_metascope = enumeration_metatype->make_inner_scope();
    enum_metascope->add(new TemplateOperation<IntegerOperationValue>("is_equal", EQUAL));
    enum_metascope->add(new TemplateOperation<IntegerOperationValue>("assign other", ASSIGN));
    enum_metascope->leave();

    // Treenum operations
    Scope *treenum_metascope = treenumeration_metatype->make_inner_scope();
    treenum_metascope->add(new TemplateOperation<IntegerOperationValue>("is_equal", EQUAL));
    treenum_metascope->add(new TemplateOperation<IntegerOperationValue>("assign other", ASSIGN));
    treenum_metascope->leave();

    // Record operations
    Scope *record_metascope = record_metatype->make_inner_scope();
    record_metascope->add(new TemplateOperation<RecordOperationValue>("assign other", ASSIGN));
    record_metascope->leave();

    // Reference operations
    typedef TemplateOperation<ReferenceOperationValue> ReferenceOperation;
    Scope *ref_scope = ref_type->make_inner_scope();
    ref_scope->add(new ReferenceOperation("is_equal", EQUAL));
    ref_scope->add(new ReferenceOperation("not_equal", NOT_EQUAL));
    ref_scope->add(new ReferenceOperation("assign other", ASSIGN));
    ref_scope->leave();

    typedef TemplateOperation<PointerOperationValue> PointerOperation;
    Scope *ptr_scope = ptr_type->make_inner_scope();
    ptr_scope->add(new PointerOperation("is_equal", EQUAL));
    ptr_scope->add(new PointerOperation("not_equal", NOT_EQUAL));
    ptr_scope->add(new PointerOperation("assign other", ASSIGN));
    ptr_scope->leave();

    // Unpacking
    Scope *tms = tuple_metatype->get_inner_scope();
    tms->enter();
    tms->add(new Unpacking("assign other"));
    tms->leave();

    // Initializing
    Scope *uns = uninitialized_type->make_inner_scope();
    uns->add(new TemplateIdentifier<CreateValue>("assign other", RVALUE_PIVOT));  // Kinda yes
    uns->leave();
    
    // Builtin controls
    builtin_colon(root_scope);
    
    // Library functions
    builtin_runtime(root_scope);

    return root_scope;
}

