
#include "value.cpp"
#include "type.cpp"
#include "generic.cpp"
#include "equality.cpp"
#include "block.cpp"
#include "integer.cpp"
#include "boolean.cpp"
#include "iterator.cpp"
#include "container.cpp"
#include "array.cpp"
#include "queue.cpp"
#include "string.cpp"
#include "reference.cpp"
#include "record.cpp"
#include "multi.cpp"
#include "control.cpp"
#include "stream.cpp"
#include "class.cpp"
#include "literal.cpp"
#include "typedefinition.cpp"
#include "function.cpp"
#include "option.cpp"
#include "float.cpp"
#include "weakref.cpp"
#include "debug.cpp"

// These Rbtree related defines are used only to keep the code a bit more readable.
// Register usage:
// ROOTX - index of current node
// R10 - return of operation
// THISX, THATX - child indexes
// SELFX - address of the tree
// KEYX - address of key (input), dark soul (output during removal)

#define SELFX R8
#define KEYX  R9
#define ROOTX RBX
#define THISX RCX
#define THATX RDX
#define RBTREE_CLOB Regs(SELFX, ROOTX, KEYX, THISX, THATX)

#include "rbtree_helpers.cpp"
#include "rbtree.cpp"
#include "rbtree_mapset.cpp"
#include "rbtree_weakmapset.cpp"

#undef SELFX
#undef KEYX
#undef ROOTX
#undef THISX
#undef THATX
