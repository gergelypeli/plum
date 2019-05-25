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
