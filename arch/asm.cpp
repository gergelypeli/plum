#include "../plum.h"


Asm::Asm() {
    accounter = NULL;
    emitter = NULL;
}


Asm::~Asm() {
}


void Asm::setup(Accounter *a, Emitter *e) {
    accounter = a;
    emitter = e;
}


void Asm::code_byte(char x) {
    if (emitter) emitter->code_byte(x); else throw ASM_ERROR;
}


void Asm::code_word(int16 x) {
    if (emitter) emitter->code_word(x); else throw ASM_ERROR;
}


void Asm::code_dword(int x) {
    if (emitter) emitter->code_dword(x); else throw ASM_ERROR;
}


void Asm::code_qword(int64 x) {
    if (emitter) emitter->code_qword(x); else throw ASM_ERROR;
}


void Asm::account(int mod) {
    if (accounter && accounter->is_on())
        accounter->adjust_stack_usage(mod);
}


void Asm::cant_account() {
    if (accounter && accounter->is_on())
        throw ASM_ERROR;
}
