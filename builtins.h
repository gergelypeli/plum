
// Vizipok-csodapok!
Type *metatype_hypertype = NULL;

Type *type_metatype = NULL;

Type *any_type = NULL;
Type *any2_type = NULL;
Type *any3_type = NULL;
Type *anyid_type = NULL;
Type *anyid2_type = NULL;
Type *anyid3_type = NULL;
Type *same_type = NULL;
Type *same2_type = NULL;
Type *same3_type = NULL;
Type *sameid_type = NULL;
Type *sameid2_type = NULL;
Type *sameid3_type = NULL;

Type *enumeration_metatype = NULL;
Type *treenumeration_metatype = NULL;
Type *integer_metatype = NULL;
Type *record_metatype = NULL;
Type *class_metatype = NULL;
Type *interface_metatype = NULL;
Type *implementation_metatype = NULL;

Type *ovalue_type = NULL;
Type *lvalue_type = NULL;
Type *dvalue_type = NULL;
Type *code_type = NULL;
Type *void_type = NULL;
Type *zero_type = NULL;
Type *partial_type = NULL;
Type *uninitialized_type = NULL;
Type *initializable_type = NULL;

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
Type *ref_type = NULL;
Type *weakref_type = NULL;
Type *weakanchorage_type = NULL;
Type *autoweakref_type = NULL;
Type *weakanchor_type = NULL;
Type *role_type = NULL;
Type *array_type = NULL;
Type *circularray_type = NULL;
Type *rbtree_type = NULL;
Type *string_type = NULL;
Type *option_type = NULL;
Type *optionis_type = NULL;
Type *optionas_type = NULL;
Type *stack_type = NULL;
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
Type *circularrayelemiter_type = NULL;
Type *circularrayindexiter_type = NULL;
Type *circularrayitemiter_type = NULL;
Type *rbtreeelembyageiter_type = NULL;
Type *rbtreeelembyorderiter_type = NULL;
Type *equalitymatcher_type = NULL;

TreenumerationType *iterator_done_exception_type = NULL;
TreenumerationType *container_full_exception_type = NULL;
TreenumerationType *container_empty_exception_type = NULL;
TreenumerationType *container_lent_exception_type = NULL;
TreenumerationType *match_unmatched_exception_type = NULL;
TreenumerationType *code_break_exception_type = NULL;

InterfaceType *streamifiable_type = NULL;
InterfaceType *iterator_type = NULL;
InterfaceType *iterable_type = NULL;


TypeSpec NO_TS;
TypeSpec VOID_TS;
TypeSpec ZERO_TS;
TypeSpec MULTI_TS;
TypeSpec MULTI_LVALUE_TS;
TypeSpec MULTI_TYPE_TS;
TypeSpec ANY_TS;
TypeSpec ANY_OVALUE_TS;
TypeSpec ANY_LVALUE_TS;
TypeSpec SAME_TS;
TypeSpec SAME_LVALUE_TS;
TypeSpec SAME2_LVALUE_TS;
TypeSpec BOOLEAN_TS;
TypeSpec INTEGER_TS;
TypeSpec INTEGER_LVALUE_TS;
TypeSpec INTEGER_OVALUE_TS;
TypeSpec BOOLEAN_LVALUE_TS;
TypeSpec UNSIGNED_INTEGER8_TS;
TypeSpec UNSIGNED_INTEGER8_ARRAY_REF_TS;
TypeSpec UNTEGER_TS;
TypeSpec CHARACTER_TS;
TypeSpec CHARACTER_LVALUE_TS;
TypeSpec CHARACTER_ARRAY_REF_TS;
TypeSpec CHARACTER_ARRAY_REF_LVALUE_TS;
TypeSpec ANYID_REF_TS;
TypeSpec ANYID_REF_LVALUE_TS;
TypeSpec ANYID_WEAKREF_TS;
TypeSpec ANYID_WEAKREF_LVALUE_TS;
TypeSpec ANYID_AUTOWEAKREF_TS;
TypeSpec ANYID_AUTOWEAKREF_LVALUE_TS;
TypeSpec SAMEID_WEAKANCHORAGE_REF_LVALUE_TS;
TypeSpec ANY_UNINITIALIZED_TS;
TypeSpec VOID_UNINITIALIZED_TS;
TypeSpec ANY_ARRAY_REF_TS;
TypeSpec ANY_ARRAY_REF_LVALUE_TS;
TypeSpec SAME_ARRAY_REF_LVALUE_TS;
TypeSpec ANY_CIRCULARRAY_REF_TS;
TypeSpec ANY_CIRCULARRAY_REF_LVALUE_TS;
TypeSpec SAME_CIRCULARRAY_REF_LVALUE_TS;
TypeSpec ANY_RBTREE_REF_TS;
TypeSpec ANY_RBTREE_REF_LVALUE_TS;
TypeSpec SAME_RBTREE_REF_LVALUE_TS;
TypeSpec SAME_SAME2_ITEM_RBTREE_REF_LVALUE_TS;
TypeSpec VOID_CODE_TS;
TypeSpec BOOLEAN_CODE_TS;
TypeSpec STRING_TS;
TypeSpec STRING_LVALUE_TS;
TypeSpec STRING_ARRAY_REF_TS;
TypeSpec ANY_OPTION_TS;
TypeSpec ANY_OPTION_LVALUE_TS;
TypeSpec OPTIONSELECTOR_TS;
TypeSpec ANY_STACK_REF_TS;
TypeSpec ANY_QUEUE_REF_TS;
TypeSpec ANY_SET_REF_TS;
TypeSpec ANY_ANY2_MAP_REF_TS;
TypeSpec ANY_STACK_WEAKREF_TS;
TypeSpec ANY_QUEUE_WEAKREF_TS;
TypeSpec ANY_SET_WEAKREF_TS;
TypeSpec ANY_ANY2_MAP_WEAKREF_TS;
TypeSpec ANY_ANYID2_WEAKVALUEMAP_WEAKREF_TS;
TypeSpec SAME_SAMEID2_WEAKANCHOR_MAP_TS;
TypeSpec SAME_SAMEID2_WEAKANCHOR_MAP_WEAKREF_TS;
TypeSpec SAME_SAMEID2_WEAKANCHOR_ITEM_RBTREE_REF_LVALUE_TS;
TypeSpec ANYID_ANY2_WEAKINDEXMAP_WEAKREF_TS;
TypeSpec SAMEID_WEAKANCHOR_SAME2_MAP_WEAKREF_TS;
TypeSpec SAMEID_WEAKANCHOR_SAME2_MAP_TS;
TypeSpec SAMEID_WEAKANCHOR_SAME2_ITEM_RBTREE_REF_LVALUE_TS;
TypeSpec ANYID_WEAKSET_WEAKREF_TS;
TypeSpec SAMEID_WEAKANCHOR_ZERO_MAP_WEAKREF_TS;
TypeSpec SAMEID_WEAKANCHOR_ZERO_MAP_TS;
TypeSpec SAMEID_WEAKANCHOR_ZERO_ITEM_RBTREE_REF_LVALUE_TS;
TypeSpec STREAMIFIABLE_TS;
TypeSpec ANY_ITERATOR_TS;
TypeSpec SAME_ITERATOR_TS;
TypeSpec ANY_ITERABLE_TS;
TypeSpec SAME_ITERABLE_TS;
TypeSpec INTEGER_ITERATOR_TS;
TypeSpec COUNTUP_TS;
TypeSpec COUNTDOWN_TS;
TypeSpec SAME_ARRAYELEMITER_TS;
TypeSpec SAME_ARRAYINDEXITER_TS;
TypeSpec SAME_ARRAYITEMITER_TS;
TypeSpec SAME_CIRCULARRAYELEMITER_TS;
TypeSpec SAME_CIRCULARRAYINDEXITER_TS;
TypeSpec SAME_CIRCULARRAYITEMITER_TS;
TypeSpec SAME_RBTREEELEMBYAGEITER_TS;
TypeSpec SAME_RBTREEELEMBYORDERITER_TS;
TypeSpec ANY_ANY2_ITEM_TS;
TypeSpec SAME_SAME2_ITEM_TS;
TypeSpec INTEGER_SAME_ITEM_TS;
