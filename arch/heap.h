const int HEAP_HEADER_SIZE = 32;
const int HEAP_HEADER_OFFSET = -32;
const int HEAP_REFCOUNT_OFFSET = -32;
const int HEAP_WEAKCOUNT_OFFSET = -24;
const int HEAP_FINALIZER_OFFSET = -16;

const int ARRAY_HEADER_SIZE = 32;
const int ARRAY_RESERVATION_OFFSET = 0;
const int ARRAY_LENGTH_OFFSET = 8;
const int ARRAY_FRONT_OFFSET = 16;
const int ARRAY_ELEMS_OFFSET = 32;

const int CLASS_HEADER_SIZE = 8;
const int CLASS_VT_OFFSET = 0;
const int CLASS_MEMBERS_OFFSET = 8;

const int ADDRESS_SIZE = 8;
const int REFERENCE_SIZE = 8;
const int ALIAS_SIZE = 8;
const int INTEGER_SIZE = 8;
const int CHARACTER_SIZE = 2;
