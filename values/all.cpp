
#include "value.h"
#include "type.h"
#include "generic.h"
#include "equality.h"
#include "block.h"
#include "integer.h"
#include "boolean.h"
#include "iterator.h"
#include "container.h"
#include "array.h"
#include "queue.h"
#include "string.h"
#include "reference.h"
#include "record.h"
#include "multi.h"
#include "control.h"
#include "stream.h"
#include "class.h"
#include "literal.h"
#include "typedefinition.h"
#include "function.h"
#include "option.h"
#include "float.h"
#include "weakref.h"
#include "debug.h"

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

#include "rbtree_helpers.h"
#include "rbtree.h"
#include "rbtree_mapset.h"
#include "rbtree_weakmapset.h"

#include "rbtree_helpers.cpp"
#include "rbtree.cpp"
#include "rbtree_mapset.cpp"
#include "rbtree_weakmapset.cpp"

#undef SELFX
#undef KEYX
#undef ROOTX
#undef THISX
#undef THATX
