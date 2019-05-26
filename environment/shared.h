typedef short int16;
typedef unsigned short unsigned16;
typedef int int32;
typedef unsigned int unsigned32;
typedef long long int64;
typedef unsigned long long unsigned64;

enum Error {
    TOKEN_ERROR, TREE_ERROR, TUPLE_ERROR, TYPE_ERROR,
    INTERNAL_ERROR, ASM_ERROR
};


static const int HEAP_HEADER_SIZE = 32;
static const int HEAP_HEADER_OFFSET = -32;
//static const int HEAP_WEAKREFCOUNT_OFFSET = -8;  // must be the same
static const int HEAP_REFCOUNT_OFFSET = -16;
static const int HEAP_FINALIZER_OFFSET = -24;
static const int HEAP_NEXT_OFFSET = -32;  // must be the first one

static const int FCB_SIZE = 40;
static const int FCB_NEXT_OFFSET = 0;  // must be the first one
static const int FCB_PREV_OFFSET = 8;
static const int FCB_CALLBACK_OFFSET = 16;
static const int FCB_PAYLOAD1_OFFSET = 24;
static const int FCB_PAYLOAD2_OFFSET = 32;
static const int FCB_NIL = 0;

static const int LINEARRAY_HEADER_SIZE = 16;
static const int LINEARRAY_RESERVATION_OFFSET = 0;
static const int LINEARRAY_LENGTH_OFFSET = 8;
static const int LINEARRAY_ELEMS_OFFSET = 16;
static const int LINEARRAY_MINIMUM_RESERVATION = 8;

static const int CIRCULARRAY_HEADER_SIZE = 32;
static const int CIRCULARRAY_RESERVATION_OFFSET = 0;
static const int CIRCULARRAY_LENGTH_OFFSET = 8;
static const int CIRCULARRAY_FRONT_OFFSET = 16;
static const int CIRCULARRAY_ELEMS_OFFSET = 32;
static const int CIRCULARRAY_MINIMUM_RESERVATION = 8;

static const int CLASS_HEADER_SIZE = 8;
static const int CLASS_VT_OFFSET = 0;
static const int CLASS_MEMBERS_OFFSET = 8;

static const int VT_HEADER_LOW_INDEX = 0;
static const int VT_HEADER_HIGH_INDEX = 2;
static const int VT_AUTOCONV_INDEX = 0;
static const int VT_FASTFORWARD_INDEX = 1;

static const int RBTREE_HEADER_SIZE = 48;
static const int RBTREE_RESERVATION_OFFSET = 0;
static const int RBTREE_LENGTH_OFFSET = 8;
static const int RBTREE_ROOT_OFFSET = 16;
static const int RBTREE_FIRST_OFFSET = 24;
static const int RBTREE_LAST_OFFSET = 32;
static const int RBTREE_VACANT_OFFSET = 40;
static const int RBTREE_ELEMS_OFFSET = 48;
static const int RBTREE_MINIMUM_RESERVATION = 8;

static const int RBNODE_HEADER_SIZE = 32;
static const int RBNODE_PRED_OFFSET = 0;  // PREV + RED = PRED
static const int RBNODE_NEXT_OFFSET = 8;
static const int RBNODE_LEFT_OFFSET = 16;
static const int RBNODE_RIGHT_OFFSET = 24;
static const int RBNODE_VALUE_OFFSET = 32;
static const int RBNODE_NIL = 0;
static const int RBNODE_RED_BIT = 1;
static const int RBNODE_BLACKEN_MASK = -2;

static const int NOSYVALUE_SIZE = 8;
static const int NOSYVALUE_RAW_OFFSET = 0;

static const int NOSYTREE_MEMBER_OFFSET = 0;

static const int NOSYREF_MEMBER_OFFSET = 0;

static const int FRAME_INFO_SIZE = 24;
static const int FRAME_INFO_START_OFFSET = 0;
static const int FRAME_INFO_END_OFFSET = 8;
static const int FRAME_INFO_NAME_OFFSET = 16;

static const int ADDRESS_SIZE = 8;
static const int REFERENCE_SIZE = 8;
static const int POINTER_SIZE = 8;
static const int ALIAS_SIZE = 16;
static const int RIP_SIZE = 8;
static const int INTEGER_SIZE = 8;
static const int CHARACTER_SIZE = 2;
static const int FLOAT_SIZE = 8;

static const int ERRNO_TREENUM_OFFSET = 10;
static const int NO_EXCEPTION = 0;
static const int RETURN_EXCEPTION = -1;

static const int OPTION_FLAG_NONE = 0;

static const int CHARACTER_SINGLEQUOTE = 39;
static const int CHARACTER_DOUBLEQUOTE = 34;
static const int CHARACTER_LEFTBRACE = 123;
static const int CHARACTER_RIGHTBRACE = 125;
static const int CHARACTER_COMMA = 44;


bool decode_utf8_buffer(const char *bytes, int64 byte_length, unsigned16 *characters, int64 character_length, int64 *byte_count, int64 *character_count);
bool encode_utf8_buffer(const unsigned16 *characters, int64 character_length, char *bytes, int64 byte_length, int64 *character_count, int64 *byte_count);
bool parse_float(const unsigned16 *characters, int64 character_length, double *result, int64 *character_count);
bool parse_unteger(const unsigned16 *characters, int64 character_length, unsigned64 *result, int64 *character_count);
