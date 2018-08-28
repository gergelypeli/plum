
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
    // The address of the value is on the top of the stack
    ALISTACK = 7,
    // The address of the value is ate the specified address (always RBP based)
    ALIAS = 8
};


StorageWhere stacked(StorageWhere w) {
    return (w == MEMORY ? STACK : w == ALIAS ? ALISTACK : throw INTERNAL_ERROR);
}


struct Storage {
    StorageWhere where;
    int value;  // Must be 32-bit only, greater values must be loaded to registers.
    ConditionCode cc;
    Register reg;
    SseRegister sse;
    Address address;
    
    Storage() {
        where = NOWHERE;
        value = 0;
        cc = CC_NONE;
        reg = NOREG;
        sse = NOSSE;
    }

    Storage(StorageWhere w) {
        if (w != STACK && w != ALISTACK) {
            std::cerr << "Incomplete Storage!\n";
            throw INTERNAL_ERROR;
        }
        
        where = w;
        value = 0;
        cc = CC_NONE;
        reg = NOREG;
        sse = NOSSE;
    }

    Storage(StorageWhere w, int v) {
        if (w != CONSTANT) {
            std::cerr << "Wrong Storage!\n";
            throw INTERNAL_ERROR;
        }

        where = w;
        value = v;
        cc = CC_NONE;
        reg = NOREG;
        sse = NOSSE;
    }

    Storage(StorageWhere w, ConditionCode c) {
        if (w != FLAGS) {
            std::cerr << "Wrong Storage!\n";
            throw INTERNAL_ERROR;
        }

        where = w;
        value = 0;
        cc = c;
        reg = NOREG;
        sse = NOSSE;
    }

    Storage(StorageWhere w, Register r) {
        if (w != REGISTER) {
            std::cerr << "Wrong Storage!\n";
            throw INTERNAL_ERROR;
        }

        where = w;
        value = 0;
        cc = CC_NONE;
        reg = r;
        sse = NOSSE;
    }

    Storage(StorageWhere w, SseRegister s) {
        if (w != SSEREGISTER) {
            std::cerr << "Wrong Storage!\n";
            throw INTERNAL_ERROR;
        }

        where = w;
        value = 0;
        cc = CC_NONE;
        reg = NOREG;
        sse = s;
    }

    Storage(StorageWhere w, Address a) {
        if (w != MEMORY && w != ALIAS) {
            std::cerr << "Wrong Storage!\n";
            throw INTERNAL_ERROR;
        }

        where = w;
        value = 0;
        cc = CC_NONE;
        reg = NOREG;
        sse = NOSSE;
        address = a;
    }

    Regs regs() {
        switch (where) {
        case NOWHERE:
            return Regs();
        case CONSTANT:
            return Regs();
        case FLAGS:
            return Regs();
        case REGISTER:
            return reg != NOREG ? Regs(reg) : Regs();
        case SSEREGISTER:
            return sse != NOSSE ? Regs(sse) : Regs();
        case STACK:
        case ALISTACK:
            return Regs();
        case MEMORY:
        case ALIAS:
            // Although RBX and RSP based addresses can be used locally, they shouldn't be
            // passed between Value-s, so no one should be interested in their clobbed registers.
            // In those cases just crash, as RBX and RSP are also illegal in a Regs.
            if (address.base != NOREG && address.base != RBP) {
                if (address.index != NOREG)
                    return Regs(address.base, address.index);
                else
                    return Regs(address.base);
            }
            else if (address.index != NOREG)
                return Regs(address.index);
            else
                return Regs();
        default:
            throw INTERNAL_ERROR;
        }
    }
};


Storage operator+(const Storage &s, int offset) {
    if (s.where == MEMORY)
        return Storage(MEMORY, s.address + offset);
    else
        throw INTERNAL_ERROR;
}


std::ostream &operator<<(std::ostream &os, const Storage &s) {
    if (s.where == NOWHERE)
        os << "NOWHERE";
    else if (s.where == CONSTANT)
        os << "CONSTANT(" << s.value << ")";
    else if (s.where == FLAGS)
        os << "FLAGS(" << s.cc << ")";
    else if (s.where == REGISTER)
        os << "REGISTER(" << s.reg << ")";
    else if (s.where == SSEREGISTER)
        os << "SSEREGISTER(" << s.sse << ")";
    else if (s.where == STACK)
        os << "STACK";
    else if (s.where == MEMORY)
        os << "MEMORY(" << s.address << ")";
    else if (s.where == ALISTACK)
        os << "ALISTACK";
    else if (s.where == ALIAS)
        os << "ALIAS(" << s.address << ")";
    else
        os << "???";
        
    return os;
}


enum StorageWhereWhere {
    NOWHERE_NOWHERE=00,     NOWHERE_CONSTANT=01,     NOWHERE_FLAGS=02,     NOWHERE_REGISTER=03,     NOWHERE_SSEREGISTER=04,     NOWHERE_STACK=05,     NOWHERE_MEMORY=06,     NOWHERE_ALISTACK=07,     NOWHERE_ALIAS=8,
    CONSTANT_NOWHERE=10,    CONSTANT_CONSTANT=11,    CONSTANT_FLAGS=12,    CONSTANT_REGISTER=13,    CONSTANT_SSEREGISTER=14,    CONSTANT_STACK=15,    CONSTANT_MEMORY=16,    CONSTANT_ALISTACK=17,    CONSTANT_ALIAS=18,
    FLAGS_NOWHERE=20,       FLAGS_CONSTANT=21,       FLAGS_FLAGS=22,       FLAGS_REGISTER=23,       FLAGS_SSEREGISTER=24,       FLAGS_STACK=25,       FLAGS_MEMORY=26,       FLAGS_ALISTACK=27,       FLAGS_ALIAS=28,
    REGISTER_NOWHERE=30,    REGISTER_CONSTANT=31,    REGISTER_FLAGS=32,    REGISTER_REGISTER=33,    REGISTER_SSEREGISTER=34,    REGISTER_STACK=35,    REGISTER_MEMORY=36,    REGISTER_ALISTACK=37,    REGISTER_ALIAS=38,
    SSEREGISTER_NOWHERE=40, SSEREGISTER_CONSTANT=41, SSEREGISTER_FLAGS=42, SSEREGISTER_REGISTER=43, SSEREGISTER_SSEREGISTER=44, SSEREGISTER_STACK=45, SSEREGISTER_MEMORY=46, SSEREGISTER_ALISTACK=47, SSEREGISTER_ALIAS=48,
    STACK_NOWHERE=50,       STACK_CONSTANT=51,       STACK_FLAGS=52,       STACK_REGISTER=53,       STACK_SSEREGISTER=54,       STACK_STACK=55,       STACK_MEMORY=56,       STACK_ALISTACK=57,       STACK_ALIAS=58,
    MEMORY_NOWHERE=60,      MEMORY_CONSTANT=61,      MEMORY_FLAGS=62,      MEMORY_REGISTER=63,      MEMORY_SSEREGISTER=64,      MEMORY_STACK=65,      MEMORY_MEMORY=66,      MEMORY_ALISTACK=67,      MEMORY_ALIAS=68,
    ALISTACK_NOWHERE=70,    ALISTACK_CONSTANT=71,    ALISTACK_FLAGS=72,    ALISTACK_REGISTER=73,    ALISTACK_SSEREGISTER=74,    ALISTACK_STACK=75,    ALISTACK_MEMORY=76,    ALISTACK_ALISTACK=77,    ALISTACK_ALIAS=78,
    ALIAS_NOWHERE=80,       ALIAS_CONSTANT=81,       ALIAS_FLAGS=82,       ALIAS_REGISTER=83,       ALIAS_SSEREGISTER=84,       ALIAS_STACK=85,       ALIAS_MEMORY=86,       ALIAS_ALISTACK=87,       ALIAS_ALIAS=88
};

StorageWhereWhere operator*(StorageWhere l, StorageWhere r) {
    return (StorageWhereWhere)(l * 10 + r);
}

