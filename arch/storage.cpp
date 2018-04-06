
enum StorageWhere {
    // No result
    NOWHERE = 0,
    // Integer or pointer constant
    CONSTANT = 1,
    // The value is in EFLAGS with the specified condition
    FLAGS = 2,
    // The value is in the specified register
    REGISTER = 3,
    // The value is on the top of the stack
    STACK = 4,
    // The value is at the specified address
    MEMORY = 5,
    // The address of the value is on the top of the stack
    ALISTACK = 6,
    // The address of the value is ate the specified address (always RBP based)
    ALIAS = 7
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
    Label label;
    
    Storage()
        :label(Label::LEAVE_UNDEFINED) {
        where = NOWHERE;
        value = 0;
        cc = CC_NONE;
        reg = NOREG;
        sse = NOSSE;
    }

    Storage(StorageWhere w)
        :label(Label::LEAVE_UNDEFINED) {
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

    Storage(StorageWhere w, int v)
        :label(Label::LEAVE_UNDEFINED) {
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

    Storage(StorageWhere w, ConditionCode c)
        :label(Label::LEAVE_UNDEFINED) {
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

    Storage(StorageWhere w, Register r)
        :label(Label::LEAVE_UNDEFINED) {
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

    Storage(StorageWhere w, SseRegister s)
        :label(Label::LEAVE_UNDEFINED) {
        if (w != REGISTER) {
            std::cerr << "Wrong Storage!\n";
            throw INTERNAL_ERROR;
        }

        where = w;
        value = 0;
        cc = CC_NONE;
        reg = NOREG;
        sse = s;
    }

    Storage(StorageWhere w, Label l)
        :label(l) {
        if (w != CONSTANT) {
            std::cerr << "Wrong Storage!\n";
            throw INTERNAL_ERROR;
        }

        where = w;
        value = 0;
        cc = CC_NONE;
        reg = NOREG;
        sse = NOSSE;
    }
    
    Storage(StorageWhere w, Address a)
        :label(Label::LEAVE_UNDEFINED) {
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
            return reg != NOREG ? Regs(reg) : sse != NOSSE ? Regs(sse) : Regs();
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


Storage operator+(Storage &s, int offset) {
    if (s.where == MEMORY)
        return Storage(MEMORY, s.address + offset);
    else
        throw INTERNAL_ERROR;
}


std::ostream &operator<<(std::ostream &os, Storage &s) {
    if (s.where == NOWHERE)
        os << "NOWHERE";
    else if (s.where == CONSTANT)
        os << "CONSTANT(" << s.value << ")";
    else if (s.where == FLAGS)
        os << "FLAGS(" << s.cc << ")";
    else if (s.where == REGISTER)
        os << "REGISTER(" << s.reg << ")";
    else if (s.where == STACK)
        os << "STACK";
    else if (s.where == MEMORY)
        os << "MEMORY(" << s.address.base << (s.address.offset >= 0 ? "+" : "") << s.address.offset << ")";
    else if (s.where == ALISTACK)
        os << "ALISTACK";
    else if (s.where == ALIAS)
        os << "ALIAS(" << s.address.base << (s.address.offset >= 0 ? "+" : "") << s.address.offset << ")";
    else
        os << "???";
        
    return os;
}


enum StorageWhereWhere {
    NOWHERE_NOWHERE=00,  NOWHERE_CONSTANT=01,  NOWHERE_FLAGS=02,  NOWHERE_REGISTER=03,  NOWHERE_STACK=04,  NOWHERE_MEMORY=05,  NOWHERE_ALISTACK=06,  NOWHERE_ALIAS=07,
    CONSTANT_NOWHERE=10, CONSTANT_CONSTANT=11, CONSTANT_FLAGS=12, CONSTANT_REGISTER=13, CONSTANT_STACK=14, CONSTANT_MEMORY=15, CONSTANT_ALISTACK=16, CONSTANT_ALIAS=17,
    FLAGS_NOWHERE=20,    FLAGS_CONSTANT=21,    FLAGS_FLAGS=22,    FLAGS_REGISTER=23,    FLAGS_STACK=24,    FLAGS_MEMORY=25,    FLAGS_ALISTACK=26,    FLAGS_ALIAS=27,
    REGISTER_NOWHERE=30, REGISTER_CONSTANT=31, REGISTER_FLAGS=32, REGISTER_REGISTER=33, REGISTER_STACK=34, REGISTER_MEMORY=35, REGISTER_ALISTACK=36, REGISTER_ALIAS=37,
    STACK_NOWHERE=40,    STACK_CONSTANT=41,    STACK_FLAGS=42,    STACK_REGISTER=43,    STACK_STACK=44,    STACK_MEMORY=45,    STACK_ALISTACK=46,    STACK_ALIAS=47,
    MEMORY_NOWHERE=50,   MEMORY_CONSTANT=51,   MEMORY_FLAGS=52,   MEMORY_REGISTER=53,   MEMORY_STACK=54,   MEMORY_MEMORY=55,   MEMORY_ALISTACK=56,   MEMORY_ALIAS=57,
    ALISTACK_NOWHERE=60, ALISTACK_CONSTANT=61, ALISTACK_FLAGS=62, ALISTACK_REGISTER=63, ALISTACK_STACK=64, ALISTACK_MEMORY=65, ALISTACK_ALISTACK=66, ALISTACK_ALIAS=67,
    ALIAS_NOWHERE=70,    ALIAS_CONSTANT=71,    ALIAS_FLAGS=72,    ALIAS_REGISTER=73,    ALIAS_STACK=74,    ALIAS_MEMORY=75,    ALIAS_ALISTACK=76,    ALIAS_ALIAS=77
};

StorageWhereWhere operator*(StorageWhere l, StorageWhere r) {
    return (StorageWhereWhere)(l * 10 + r);
}

