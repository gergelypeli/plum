#include "../plum.h"


Asm::Asm() {
}


Asm::~Asm() {
}


void Asm::set_accounting(Acc *a) {
    acc = a;
}


bool Asm::is_accounting() {
    return acc ? acc->is_on() : false;
}


void Asm::adjust_stack_usage(int mod) {
    if (acc) acc->adjust_stack_usage(mod);
}


void Asm::add_def(Label label, const Def &def) {
    if (!label.def_index) {
        std::cerr << "Can't define an undeclared label!\n";
        throw ASM_ERROR;
    }

    if (defs.count(label.def_index)) {
        std::cerr << "Can't redefine label!\n";
        throw ASM_ERROR;
    }

    defs.insert(decltype(defs)::value_type(label.def_index, def));
}


void Asm::absolute_label(Label c, unsigned64 value, unsigned size) {
    add_def(c, Def(DEF_ABSOLUTE, value, size, "", false));
}


void Asm::data_align(int bytes) {
    data.resize((data.size() + (bytes - 1)) & ~(bytes - 1));
}


void Asm::data_blob(void *blob, int length) {
    data.resize(data.size() + length);
    memcpy(data.data() + data.size() - length, blob, length);
}


void Asm::data_byte(char x) {
    data.push_back(x);
}


void Asm::data_word(int16 x) {
    data.resize(data.size() + 2);
    *(short *)(data.data() + data.size() - 2) = x;
}


void Asm::data_dword(int x) {
    data.resize(data.size() + 4);
    *(int *)(data.data() + data.size() - 4) = x;
}


void Asm::data_qword(int64 x) {
    data.resize(data.size() + 8);
    *(int64 *)(data.data() + data.size() - 8) = x;
}


void Asm::data_zstring(std::string s) {
    for (char c : s)
        data.push_back(c);
        
    data.push_back(0);
}


void Asm::data_double(double x) {
    data.resize(data.size() + 8);
    *(double *)(data.data() + data.size() - 8) = x;
}


void Asm::data_label(Label c, unsigned size) {
    add_def(c, Def(DEF_DATA, data.size(), size, "", false));
}


void Asm::data_label_local(Label c, std::string name, unsigned size) {
    add_def(c, Def(DEF_DATA_EXPORT, data.size(), size, name, false));
}


void Asm::data_label_global(Label c, std::string name, unsigned size) {
    add_def(c, Def(DEF_DATA_EXPORT, data.size(), size, name, true));
}


void Asm::code_byte(char x) {
    code.push_back(x);
}


void Asm::code_word(int16 x) {
    code.resize(code.size() + 2);
    *(int16 *)(code.data() + code.size() - 2) = x;
}


void Asm::code_dword(int x) {
    code.resize(code.size() + 4);
    *(int *)(code.data() + code.size() - 4) = x;
}


void Asm::code_qword(int64 x) {
    code.resize(code.size() + 8);
    *(int64 *)(code.data() + code.size() - 8) = x;
}


void Asm::code_label(Label c, unsigned size) {
    add_def(c, Def(DEF_CODE, code.size(), size, "", false));
}


void Asm::code_label_import(Label c, std::string name) {
    add_def(c, Def(DEF_CODE_IMPORT, 0, 0, name, false));
}


void Asm::code_label_local(Label c, std::string name, unsigned size) {
    add_def(c, Def(DEF_CODE_EXPORT, code.size(), size, name, false));
}


void Asm::code_label_global(Label c, std::string name, unsigned size) {
    add_def(c, Def(DEF_CODE_EXPORT, code.size(), size, name, true));
}


int Asm::get_pc() {
    return code.size();
}


int Asm::get_dc() {
    return data.size();
}

