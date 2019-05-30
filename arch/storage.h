
// These are explicitly numbered because of the cross product
enum StorageWhere {
    // No result
    NOWHERE = 0,
    // Integer or pointer constant
    CONSTANT = 1,
    // The value is in EFLAGS with the specified condition
    FLAGS = 2,
    // The value is in the specified register
    REGISTER = 3,
    // The value is in an SSE register
    SSEREGISTER = 4,
    // The value is on the top of the stack
    STACK = 5,
    // The value is at the specified address
    MEMORY = 6,
    // The address of the value is on the top of the stack (function argument only!)
    ALISTACK = 7,
    // The address of the value is ate the specified address (always RBP based)
    ALIAS = 8,
    // Borrowed reference in a register
    //BREGISTER = 9,
    // Borrowed reference value on the top of the stack
    BSTACK = 10
};


StorageWhere stacked(StorageWhere w);


struct Storage {
    StorageWhere where;
    int value;  // Must be 32-bit only, greater values must be loaded to registers.
    ConditionCode cc;
    Register reg;
    SseRegister sse;
    Address address;
    
    Storage();
    Storage(StorageWhere w);
    Storage(StorageWhere w, int v);
    Storage(StorageWhere w, ConditionCode c);
    Storage(StorageWhere w, Register r);
    Storage(StorageWhere w, SseRegister s);
    Storage(StorageWhere w, Address a);
    Storage(StorageWhere w, Address a, int v);
    Regs regs();
    Storage access(int stack_offset);
};


Storage operator+(const Storage &s, int offset);


std::ostream &operator<<(std::ostream &os, const Storage &s);


enum StorageWhereWhere {
    NOWHERE_NOWHERE=0x00,     NOWHERE_CONSTANT=0x01,     NOWHERE_FLAGS=0x02,     NOWHERE_REGISTER=0x03,     NOWHERE_SSEREGISTER=0x04,     NOWHERE_STACK=0x05,     NOWHERE_MEMORY=0x06,     NOWHERE_ALISTACK=0x07,     NOWHERE_ALIAS=0x08,      NOWHERE_BREGISTER=0x09,     NOWHERE_BSTACK=0x0a,
    CONSTANT_NOWHERE=0x10,    CONSTANT_CONSTANT=0x11,    CONSTANT_FLAGS=0x12,    CONSTANT_REGISTER=0x13,    CONSTANT_SSEREGISTER=0x14,    CONSTANT_STACK=0x15,    CONSTANT_MEMORY=0x16,    CONSTANT_ALISTACK=0x17,    CONSTANT_ALIAS=0x18,     CONSTANT_BREGISTER=0x19,    CONSTANT_BSTACK=0x1a,
    FLAGS_NOWHERE=0x20,       FLAGS_CONSTANT=0x21,       FLAGS_FLAGS=0x22,       FLAGS_REGISTER=0x23,       FLAGS_SSEREGISTER=0x24,       FLAGS_STACK=0x25,       FLAGS_MEMORY=0x26,       FLAGS_ALISTACK=0x27,       FLAGS_ALIAS=0x28,        FLAGS_BREGISTER=0x29,       FLAGS_BSTACK=0x2a,
    REGISTER_NOWHERE=0x30,    REGISTER_CONSTANT=0x31,    REGISTER_FLAGS=0x32,    REGISTER_REGISTER=0x33,    REGISTER_SSEREGISTER=0x34,    REGISTER_STACK=0x35,    REGISTER_MEMORY=0x36,    REGISTER_ALISTACK=0x37,    REGISTER_ALIAS=0x38,     REGISTER_BREGISTER=0x39,    REGISTER_BSTACK=0x3a,
    SSEREGISTER_NOWHERE=0x40, SSEREGISTER_CONSTANT=0x41, SSEREGISTER_FLAGS=0x42, SSEREGISTER_REGISTER=0x43, SSEREGISTER_SSEREGISTER=0x44, SSEREGISTER_STACK=0x45, SSEREGISTER_MEMORY=0x46, SSEREGISTER_ALISTACK=0x47, SSEREGISTER_ALIAS=0x48,  SSEREGISTER_BREGISTER=0x49, SSEREGISTER_BSTACK=0x4a,
    STACK_NOWHERE=0x50,       STACK_CONSTANT=0x51,       STACK_FLAGS=0x52,       STACK_REGISTER=0x53,       STACK_SSEREGISTER=0x54,       STACK_STACK=0x55,       STACK_MEMORY=0x56,       STACK_ALISTACK=0x57,       STACK_ALIAS=0x58,        STACK_BREGISTER=0x59,       STACK_BSTACK=0x5a,
    MEMORY_NOWHERE=0x60,      MEMORY_CONSTANT=0x61,      MEMORY_FLAGS=0x62,      MEMORY_REGISTER=0x63,      MEMORY_SSEREGISTER=0x64,      MEMORY_STACK=0x65,      MEMORY_MEMORY=0x66,      MEMORY_ALISTACK=0x67,      MEMORY_ALIAS=0x68,       MEMORY_BREGISTER=0x69,      MEMORY_BSTACK=0x6a,
    ALISTACK_NOWHERE=0x70,    ALISTACK_CONSTANT=0x71,    ALISTACK_FLAGS=0x72,    ALISTACK_REGISTER=0x73,    ALISTACK_SSEREGISTER=0x74,    ALISTACK_STACK=0x75,    ALISTACK_MEMORY=0x76,    ALISTACK_ALISTACK=0x77,    ALISTACK_ALIAS=0x78,     ALISTACK_BREGISTER=0x79,    ALISTACK_BSTACK=0x7a,
    ALIAS_NOWHERE=0x80,       ALIAS_CONSTANT=0x81,       ALIAS_FLAGS=0x82,       ALIAS_REGISTER=0x83,       ALIAS_SSEREGISTER=0x84,       ALIAS_STACK=0x85,       ALIAS_MEMORY=0x86,       ALIAS_ALISTACK=0x87,       ALIAS_ALIAS=0x88,        ALIAS_BREGISTER=0x89,       ALIAS_BSTACK=0x8a,
    BREGISTER_NOWHERE=0x90,   BREGISTER_CONSTANT=0x91,   BREGISTER_FLAGS=0x92,   BREGISTER_REGISTER=0x93,   BREGISTER_SSEREGISTER=0x94,   BREGISTER_STACK=0x95,   BREGISTER_MEMORY=0x96,   BREGISTER_ALISTACK=0x97,   BREGISTER_ALIAS=0x98,    BREGISTER_BREGISTER=0x99,   BREGISTER_BSTACK=0x9a,
    BSTACK_NOWHERE=0xa0,      BSTACK_CONSTANT=0xa1,      BSTACK_FLAGS=0xa2,      BSTACK_REGISTER=0xa3,      BSTACK_SSEREGISTER=0xa4,      BSTACK_STACK=0xa5,      BSTACK_MEMORY=0xa6,      BSTACK_ALISTACK=0xa7,      BSTACK_ALIAS=0xa8,       BSTACK_BREGISTER=0xa9,      BSTACK_BSTACK=0xaa
};

StorageWhereWhere operator*(StorageWhere l, StorageWhere r);
