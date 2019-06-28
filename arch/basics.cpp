#include "../plum.h"

const char *REGISTER_NAMES[] = {
    "RAX", "RCX", "RDX", "RBX", "RSP", "RBP", "RSI", "RDI",
    "R8",  "R9",  "R10", "R11", "R12", "R13", "R14", "R15"
};


const char *register_name(Register r) {
    return (r == NOREG ? "---" : REGISTER_NAMES[r]);
}

std::ostream &operator << (std::ostream &os, const Register r) {
    os << register_name(r);
    return os;
}


const char *FPR_REGISTER_NAMES[] = {
    "FPR0", "FPR1", "FPR2", "FPR3", "FPR4", "FPR5", "FPR6", "FPR7",
    "FPR8", "FPR9", "FPR10", "FPR11", "FPR12", "FPR13", "FPR14", "FPR15",
};


const char *fpregister_name(FpRegister r) {
    return (r == NOFPR ? "---" : FPR_REGISTER_NAMES[r]);
}


std::ostream &operator << (std::ostream &os, const FpRegister r) {
    os << fpregister_name(r);
    return os;
}


const char *CONDITION_NAMES[] = {
    "OVERFLOW", "NOT_OVERFLOW",
    "BELOW", "ABOVE_EQUAL",
    "EQUAL", "NOT_EQUAL",
    "BELOW_EQUAL", "ABOVE",
    "SIGN", "NOT_SIGN",
    "PARITY", "NOT_PARITY",
    "LESS", "GREATER_EQUAL",
    "LESS_EQUAL", "GREATER"
};


std::ostream &operator << (std::ostream &os, const ConditionCode cc) {
    os << (cc == CC_NONE ? "---" : CONDITION_NAMES[cc]);
    return os;
}


ConditionCode negated(ConditionCode cc) {
    // The lowest bit negates the condition meaning
    return cc != CC_NONE ? (ConditionCode)(cc ^ 1) : throw ASM_ERROR;
}


ConditionCode swapped(ConditionCode cc) {
    return (
        cc == CC_EQUAL ? CC_EQUAL :
        cc == CC_NOT_EQUAL ? CC_NOT_EQUAL :
        cc == CC_BELOW ? CC_ABOVE :
        cc == CC_ABOVE ? CC_BELOW :
        cc == CC_BELOW_EQUAL ? CC_ABOVE_EQUAL :
        cc == CC_ABOVE_EQUAL ? CC_BELOW_EQUAL :
        cc == CC_LESS ? CC_GREATER :
        cc == CC_GREATER ? CC_LESS :
        cc == CC_LESS_EQUAL ? CC_GREATER_EQUAL :
        cc == CC_GREATER_EQUAL ? CC_LESS_EQUAL :
        throw ASM_ERROR
    );
}

    
Regs::Regs(unsigned64 a) {
    available = a;
}

void Regs::validate(Register r) {
    if (r == NOREG || r == RSP || r == RBP || r == R10 || r == R11)
        throw ASM_ERROR;
}

void Regs::validate(FpRegister s) {
    if (s == NOFPR || s == FPR14 || s == FPR15)
        throw ASM_ERROR;
}

Regs::Regs() {
    available = 0;
}

Regs Regs::all() {
    return Regs(ALL_MASK);
}

Regs Regs::allregs() {
    return Regs(ALL_MASK & ~STACKVARS & ~HEAPVARS);
}

Regs Regs::stackvars() {
    return Regs(STACKVARS);
}

Regs Regs::heapvars() {
    return Regs(HEAPVARS);
}

Regs::Regs(Register r) {
    validate(r);
    available = (1UL << (int)r);
}

Regs::Regs(Register r1, Register r2) {
    validate(r1);
    validate(r2);
    available = (1UL << (int)r1) | (1UL << (int)r2);
}

Regs::Regs(Register r1, Register r2, Register r3) {
    validate(r1);
    validate(r2);
    validate(r3);
    available = (1UL << (int)r1) | (1UL << (int)r2) | (1UL << (int)r3);
}

Regs::Regs(Register r1, Register r2, Register r3, Register r4) {
    validate(r1);
    validate(r2);
    validate(r3);
    validate(r4);
    available = (1UL << (int)r1) | (1UL << (int)r2) | (1UL << (int)r3) | (1UL << (int)r4);
}

Regs::Regs(Register r1, Register r2, Register r3, Register r4, Register r5) {
    validate(r1);
    validate(r2);
    validate(r3);
    validate(r4);
    validate(r5);
    available = (1UL << (int)r1) | (1UL << (int)r2) | (1UL << (int)r3) | (1UL << (int)r4) | (1UL << (int)r5);
}

Regs::Regs(Register r1, Register r2, Register r3, Register r4, Register r5, Register r6) {
    validate(r1);
    validate(r2);
    validate(r3);
    validate(r4);
    validate(r5);
    validate(r6);
    available = (1UL << (int)r1) | (1UL << (int)r2) | (1UL << (int)r3) | (1UL << (int)r4) | (1UL << (int)r5) | (1UL << (int)r6);
}

Regs::Regs(FpRegister s) {
    validate(s);
    available = (1UL << ((int)s + 16));
}

Regs Regs::operator |(Regs other) {
    return Regs(available | other.available);
}

Regs Regs::operator &(Regs other) {
    return Regs(available & other.available);
}

Regs Regs::operator ~() {
    return Regs(~available & ALL_MASK);
}

bool Regs::operator ==(Regs other) {
    return available == other.available;
}

bool Regs::operator !=(Regs other) {
    return available != other.available;
}

Regs::operator bool() {
    return available != 0;
}

bool Regs::has_gpr() {
    return (available & GPR_MASK) != 0;
}

bool Regs::has_fpr() {
    return (available & FPR_MASK) != 0;
}

int Regs::count_gpr() {
    int n = 0;
    
    for (int i=0; i<REGS_TOTAL; i++)
        if (available & GPR_MASK & (1UL << i)) {
            n++;
        }

    return n;
}

int Regs::count_fpr() {
    int n = 0;
    
    for (int i=0; i<REGS_TOTAL; i++)
        if (available & FPR_MASK & (1UL << i)) {
            n++;
        }

    return n;
}

Register Regs::get_gpr() {
    for (int i=0; i<REGS_TOTAL; i++)
        if (available & GPR_MASK & (1UL << i)) {
            return (Register)i;
        }

    std::cerr << "No available register!\n";
    throw ASM_ERROR;
}

FpRegister Regs::get_fpr() {
    for (int i=0; i<REGS_TOTAL; i++)
        if (available & FPR_MASK & (1UL << i)) {
            return (FpRegister)(i - 16);
        }

    std::cerr << "No available FP register!\n";
    throw ASM_ERROR;
}

void Regs::reserve_gpr(int requested) {
    int c = count_gpr();
    
    if (c >= requested)
        return;
        
    for (int i=0; i<REGS_TOTAL; i++) {
        unsigned64 x = GPR_MASK & (1UL << i);
        
        if (!x)
            continue;
            
        if (available & x)
            continue;

        available |= x;
        c += 1;
            
        if (c == requested)
            return;
    }
}


Label::Label() {
    static unsigned last_def_index = 0;
    
    def_index = ++last_def_index;
    
    // If an undefined label is referenced, catch its creation here
    //if (def_index == 5277)
    //    abort();
}

Label::Label(const Label &c) {
    def_index = c.def_index;
}

Label::Label(LeaveUndefined) {
    def_index = 0;
}


Address::Address()
    :label(Label::LEAVE_UNDEFINED) {
    base = NOREG;
    index = NOREG;
    scale = SCALE_1;
    offset = 0;
}

Address::Address(Register b, int o)
    :label(Label::LEAVE_UNDEFINED) {
    if (b == NOREG) {
        std::cerr << "Address without base register!\n";
        throw ASM_ERROR;
    }
      
    base = b;
    index = NOREG;
    scale = SCALE_1;
    offset = o;
}

Address::Address(Register b, Register i, int o)
    :label(Label::LEAVE_UNDEFINED) {
    if (b == NOREG) {
        std::cerr << "Address without base register!\n";
        throw ASM_ERROR;
    }
      
    base = b;
    index = i;
    scale = SCALE_1;
    offset = o;
}

Address::Address(Register b, Register i, Scale s, int o)
    :label(Label::LEAVE_UNDEFINED) {
    if (b == NOREG) {
        std::cerr << "Address without base register!\n";
        throw ASM_ERROR;
    }

    base = b;
    index = i;
    scale = s;
    offset = o;
}

Address::Address(Label l, int o)
    :label(l) {
    base = NOREG;
    index = NOREG;
    scale = SCALE_1;
    offset = o;
}

Address Address::operator + (int x) const {
    Address a(*this);
    a.offset += x;
    return a;
}


std::ostream &operator << (std::ostream &os, const Address &a) {
    os << "[";
    
    if (a.base != NOREG) {
        os << a.base;
        
        if (a.index != NOREG) {
            os << "+" << a.index;
            
            if (a.scale != Address::SCALE_1)
                os << "*" << (a.scale == Address::SCALE_2 ? "2" : a.scale == Address::SCALE_4 ? "4" : "8");
        }
        
        if (a.offset > 0)
            os << "+" << a.offset;
        else if (a.offset < 0)
            os << a.offset;
    }
    else if (a.index != NOREG) {
        os << a.index;
            
        if (a.scale != Address::SCALE_1)
            os << "*" << (a.scale == Address::SCALE_2 ? "2" : a.scale == Address::SCALE_4 ? "4" : "8");
            
        if (a.offset > 0)
            os << "+" << a.offset;
        else if (a.offset < 0)
            os << a.offset;
    }
    else if (a.label.def_index != 0) {
        os << "RIP+#" << a.label.def_index;
        
        if (a.offset > 0)
            os << "+" << a.offset;
        else if (a.offset < 0)
            os << a.offset;
    }
    else {
        os << a.offset;
    }

    os << "]";
    
    return os;
}
