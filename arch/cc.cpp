#include "../plum.h"


Cc::Cc() {
    elf = NULL;
}

Cc::~Cc() {
}


void Cc::process_definitions() {
    for (auto &kv : defs) {
        Def &d(kv.second);

        switch (d.type) {
        case DEF_CODE: break;
        case DEF_DATA: break;
        case DEF_ABSOLUTE: break;
        case DEF_CODE_IMPORT:
            d.symbol_index = elf->import(d.name);
            break;
        case DEF_CODE_EXPORT:
            d.symbol_index = elf->export_code(d.name, d.location, d.size, d.is_global);
            break;
        case DEF_DATA_EXPORT:
            d.symbol_index = elf->export_data(d.name, d.location, d.size, d.is_global);
            break;
        case DEF_ABSOLUTE_EXPORT:
            d.symbol_index = elf->export_absolute(d.name, d.location, d.size, d.is_global);
            break;
        default:
            std::cerr << "Huh?\n";
            throw ASM_ERROR;
        }
    }
}


void Cc::done(std::string filename) {
    process_definitions();
    process_relocations();
    
    elf->set_code(code);
    elf->set_data(data);
    
    elf->done(filename);
    
    delete elf;
}


void Cc::add_def(Label label, const Def &def) {
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


void Cc::absolute_label(Label c, unsigned64 value, unsigned size) {
    add_def(c, Def(DEF_ABSOLUTE, value, size, "", false));
}


void Cc::data_align(int bytes) {
    data.resize((data.size() + (bytes - 1)) & ~(bytes - 1));
}


void Cc::data_blob(void *blob, int length) {
    data.resize(data.size() + length);
    memcpy(data.data() + data.size() - length, blob, length);
}


void Cc::data_byte(char x) {
    data.push_back(x);
}


void Cc::data_word(int16 x) {
    data.resize(data.size() + 2);
    *(short *)(data.data() + data.size() - 2) = x;
}


void Cc::data_dword(int x) {
    data.resize(data.size() + 4);
    *(int *)(data.data() + data.size() - 4) = x;
}


void Cc::data_qword(int64 x) {
    data.resize(data.size() + 8);
    *(int64 *)(data.data() + data.size() - 8) = x;
}


void Cc::data_zstring(std::string s) {
    for (char c : s)
        data.push_back(c);
        
    data.push_back(0);
}


void Cc::data_double(double x) {
    data.resize(data.size() + 8);
    *(double *)(data.data() + data.size() - 8) = x;
}


void Cc::data_label(Label c, unsigned size) {
    add_def(c, Def(DEF_DATA, data.size(), size, "", false));
}


void Cc::data_label_local(Label c, std::string name, unsigned size) {
    add_def(c, Def(DEF_DATA_EXPORT, data.size(), size, name, false));
}


void Cc::data_label_global(Label c, std::string name, unsigned size) {
    add_def(c, Def(DEF_DATA_EXPORT, data.size(), size, name, true));
}


void Cc::code_byte(char x) {
    code.push_back(x);
}


void Cc::code_word(int16 x) {
    code.resize(code.size() + 2);
    *(int16 *)(code.data() + code.size() - 2) = x;
}


void Cc::code_dword(int x) {
    code.resize(code.size() + 4);
    *(int *)(code.data() + code.size() - 4) = x;
}


void Cc::code_qword(int64 x) {
    code.resize(code.size() + 8);
    *(int64 *)(code.data() + code.size() - 8) = x;
}


void Cc::code_label(Label c, unsigned size) {
    add_def(c, Def(DEF_CODE, code.size(), size, "", false));
}


void Cc::code_label_import(Label c, std::string name) {
    add_def(c, Def(DEF_CODE_IMPORT, 0, 0, name, false));
}


void Cc::code_label_local(Label c, std::string name, unsigned size) {
    add_def(c, Def(DEF_CODE_EXPORT, code.size(), size, name, false));
}


void Cc::code_label_global(Label c, std::string name, unsigned size) {
    add_def(c, Def(DEF_CODE_EXPORT, code.size(), size, name, true));
}


int Cc::get_pc() {
    return code.size();
}


int Cc::get_dc() {
    return data.size();
}
