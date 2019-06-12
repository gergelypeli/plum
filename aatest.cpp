#include "plum.h"

#include "arch/elf.cpp"
#include "arch/elf_a64.cpp"
#include "arch/asm.cpp"
#include "arch/asm_a64.cpp"
#include "arch/emu.cpp"
#include "arch/emu_a64.cpp"
#include "arch/basics.cpp"


using namespace A;


int main() {
    Emu_A64 *e64 = new Emu_A64("aatest");
    Asm_A64 *a64 = e64->asm_a64;
    a64->setup(NULL, e64);
    
    a64->op(MOVZ, R10, 0x1234, LSL_16);
    a64->op(MOVK, R10, 0x1234, LSL_16);
    a64->op(MOVN, R10, 0x1234, LSL_16);
    
    a64->op(LDP, R10, R11, R12, 0x18);
    a64->op(LDP, R10, R11, R12, 0x18, false);
    a64->op(LDP, R10, R11, R12, 0x18, true);

    a64->op(STP, R10, R11, R12, 0x18);
    a64->op(STP, R10, R11, R12, 0x18, false);
    a64->op(STP, R10, R11, R12, 0x18, true);

    a64->op(LDRQ, R10, R12, 0x18);
    a64->op(LDRQ, R10, R12, 0x18, false);
    a64->op(LDRQ, R10, R12, 0x18, true);
    a64->op(LDRQ, R10, R12, R13);

    a64->op(LDRUW, R10, R12, 0x18);
    a64->op(LDRUH, R10, R12, 0x18);
    a64->op(LDRUB, R10, R12, 0x18);

    a64->op(LDRSW, R10, R12, 0x18);
    a64->op(LDRSH, R10, R12, 0x18);
    a64->op(LDRSB, R10, R12, 0x18);

    a64->op(STRQ, R10, R12, 0x19);
    a64->op(STRW, R10, R12, 0x19);
    a64->op(STRH, R10, R12, 0x19);
    a64->op(STRB, R10, R12, 0x19);
    
    a64->op(A::ADD, R10, R11, 100);
    a64->op(ADDS, R10, R11, 100);
    a64->op(SUB, R10, R11, 100);
    a64->op(SUBS, R10, R11, 100);
    
    a64->op(A::ADD, R10, R11, R12, SHIFT_LSL, 0);
    a64->op(ADDS, R10, R11, R12, SHIFT_LSL, 10);
    a64->op(SUB, R10, R11, R12, SHIFT_LSR, 10);
    a64->op(SUBS, R10, R11, R12, SHIFT_ASR, 10);
    
    a64->op(A::AND, R10, R11, R12, SHIFT_ROR, 10);
    a64->op(ANDS, R10, R11, R12, SHIFT_ROR, 10);
    a64->op(EOR, R10, R11, R12, SHIFT_ROR, 10);
    a64->op(ORR, R10, R11, R12, SHIFT_ROR, 10);
    
    a64->op(A::AND, R10, R11, 0, 4);
    a64->op(ANDS, R10, R11, 4, 8);
    a64->op(EOR, R10, R11, 8, 12);
    a64->op(ORR, R10, R11, 12, 16);

    a64->op(EON, R10, R11, R12, SHIFT_ROR, 10);
    a64->op(ORN, R10, R11, R12, SHIFT_ROR, 10);
    
    a64->op(MADD, R10, R11, R12, R13);
    a64->op(SDIV, R10, R11, R12);
    a64->op(UDIV, R10, R11, R12);

    a64->op(ASR, R10, R11, R12);
    a64->op(LSL, R10, R11, R12);
    a64->op(LSR, R10, R11, R12);
    a64->op(ROR, R10, R11, R12);

    a64->op(BFM, R10, R11, BitMask(3, 0));
    a64->op(SBFM, R10, R11, BitMask(3, 0));
    a64->op(UBFM, R10, R11, BitMask(3, 0));

    a64->op(A::NOP);
        
    Label somewhere;
    a64->op(B, somewhere);
    a64->op(B, R10);
    a64->op(A::BL, somewhere);
    a64->op(A::BL, R10);
    
    a64->op(B, CC_EQ, somewhere);
    a64->op(B, CC_LT, somewhere);
    a64->op(B, CC_GT, somewhere);
    a64->op(B, CC_AL, somewhere);

    a64->op(A::RET, R10);
    
    a64->op(CSEL, CC_EQ, R10, R11, R12);
    a64->op(CSINC, CC_EQ, R10, R11, R12);
    a64->op(CSINV, CC_EQ, R10, R11, R12);
    a64->op(CSNEG, CC_EQ, R10, R11, R12);

    a64->op(CBZ, R10, somewhere);
    a64->op(CBNZ, R10, somewhere);

    e64->done("/tmp/aatest.o");
    
    return 0;
}
