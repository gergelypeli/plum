const int HEAP_HEADER_SIZE = 32;
const int HEAP_HEADER_OFFSET = -32;
//const int HEAP_WEAKREFCOUNT_OFFSET = -8;  // must be the same
const int HEAP_REFCOUNT_OFFSET = -16;
const int HEAP_FINALIZER_OFFSET = -24;
const int HEAP_NEXT_OFFSET = -32;  // must be the first one

const int FCB_SIZE = 40;
const int FCB_NEXT_OFFSET = 0;  // must be the first one
const int FCB_PREV_OFFSET = 8;
const int FCB_CALLBACK_OFFSET = 16;
const int FCB_PAYLOAD1_OFFSET = 24;
const int FCB_PAYLOAD2_OFFSET = 32;
const int FCB_NIL = 0;

const int ARRAY_HEADER_SIZE = 16;
const int ARRAY_RESERVATION_OFFSET = 0;
const int ARRAY_LENGTH_OFFSET = 8;
const int ARRAY_ELEMS_OFFSET = 16;
const int ARRAY_MINIMUM_RESERVATION = 8;

const int CIRCULARRAY_HEADER_SIZE = 32;
const int CIRCULARRAY_RESERVATION_OFFSET = 0;
const int CIRCULARRAY_LENGTH_OFFSET = 8;
const int CIRCULARRAY_FRONT_OFFSET = 16;
const int CIRCULARRAY_ELEMS_OFFSET = 32;
const int CIRCULARRAY_MINIMUM_RESERVATION = 8;

const int CLASS_HEADER_SIZE = 8;
const int CLASS_VT_OFFSET = 0;
const int CLASS_MEMBERS_OFFSET = 8;

const int VT_BASEVT_INDEX = 0;
const int VT_DISTANCE_INDEX = 1;

const int RBTREE_HEADER_SIZE = 48;
const int RBTREE_RESERVATION_OFFSET = 0;
const int RBTREE_LENGTH_OFFSET = 8;
const int RBTREE_ROOT_OFFSET = 16;
const int RBTREE_FIRST_OFFSET = 24;
const int RBTREE_LAST_OFFSET = 32;
const int RBTREE_VACANT_OFFSET = 40;
const int RBTREE_ELEMS_OFFSET = 48;
const int RBTREE_MINIMUM_RESERVATION = 8;

const int RBNODE_HEADER_SIZE = 32;
const int RBNODE_PRED_OFFSET = 0;  // PREV + RED = PRED
const int RBNODE_NEXT_OFFSET = 8;
const int RBNODE_LEFT_OFFSET = 16;
const int RBNODE_RIGHT_OFFSET = 24;
const int RBNODE_VALUE_OFFSET = 32;
const int RBNODE_NIL = 0;
const int RBNODE_RED_BIT = 1;
const int RBNODE_BLACKEN_MASK = -2;

const int NOSYOBJECT_SIZE = 16;
const int NOSYOBJECT_PTR_OFFSET = 0;
const int NOSYOBJECT_FCB_OFFSET = 8;

const int NOSYVALUE_SIZE = 16;
const int NOSYVALUE_PTR_OFFSET = 0;
const int NOSYVALUE_FCB_OFFSET = 8;

const int ADDRESS_SIZE = 8;
const int REFERENCE_SIZE = 8;
const int POINTER_SIZE = 8;
const int ALIAS_SIZE = 8;
const int INTEGER_SIZE = 8;
const int CHARACTER_SIZE = 2;
const int FLOAT_SIZE = 8;

const int ERRNO_TREENUM_OFFSET = 10;
const int NO_EXCEPTION = 0;
const int RETURN_EXCEPTION = -1;
