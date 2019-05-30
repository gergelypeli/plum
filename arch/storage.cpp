#include "../plum.h"


StorageWhere stacked(StorageWhere w) {
    return (
        w == NOWHERE ? NOWHERE :
        w == MEMORY ? STACK :
        w == ALIAS ? ALISTACK :
        throw INTERNAL_ERROR
    );
}


    Storage::Storage() {
        where = NOWHERE;
        value = 0;
        cc = CC_NONE;
        reg = NOREG;
        sse = NOSSE;
    }

    Storage::Storage(StorageWhere w) {
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

    Storage::Storage(StorageWhere w, int v) {
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

    Storage::Storage(StorageWhere w, ConditionCode c) {
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

    Storage::Storage(StorageWhere w, Register r) {
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

    Storage::Storage(StorageWhere w, SseRegister s) {
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

    Storage::Storage(StorageWhere w, Address a) {
        if (w != MEMORY) {
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

    Storage::Storage(StorageWhere w, Address a, int v) {
        if (w != ALIAS) {
            std::cerr << "Wrong Storage!\n";
            throw INTERNAL_ERROR;
        }

        where = w;
        value = v;
        cc = CC_NONE;
        reg = NOREG;
        sse = NOSSE;
        address = a;
    }

    Regs Storage::regs() {
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
            // Although RSP, R10, R11 based addresses can be used locally, they shouldn't be
            // passed between Value-s, so no one should be interested in their clobbed registers.
            // In those cases just crash, as these registers are also illegal in a Regs.
            // Passing RBP based addresses is fine, and RBP won't be included in the result.
            if (address.base != NOREG && address.base != RBP && address.base != RSP) {
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
    
    Storage Storage::access(int stack_offset) {
        if (where == MEMORY || where == ALIAS)
            return *this;
        else if (where == ALISTACK)
            return Storage(ALIAS, Address(RSP, stack_offset), 0);
        else
            throw INTERNAL_ERROR;
    }


Storage operator+(const Storage &s, int offset) {
    if (s.where == MEMORY)
        return Storage(s.where, s.address + offset);
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
        os << "ALIAS(" << s.address << "+" << s.value << ")";
    else
        os << "???";
        
    return os;
}


StorageWhereWhere operator*(StorageWhere l, StorageWhere r) {
    return (StorageWhereWhere)(l * 16 + r);
}

