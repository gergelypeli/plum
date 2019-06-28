#include "../plum.h"


RaisingDummy::RaisingDummy(Unwound u)
    :Declaration() {
    unwound = u;
}

bool RaisingDummy::may_omit_finalization() {
    return true;
}

void RaisingDummy::set_outer_scope(Scope *os) {
    Declaration::set_outer_scope(os);
    
    outer_scope->be_unwindable(unwound);
}




Label Autoconvertible::get_autoconv_table_label(TypeMatch tm, Cx *cx) {
    throw INTERNAL_ERROR;
}

std::vector<AutoconvEntry> Autoconvertible::get_autoconv_table(TypeMatch tm) {
    throw INTERNAL_ERROR;
}




Label Methodlike::get_method_label(Cx *cx) {
    throw INTERNAL_ERROR;
}

std::string Methodlike::get_method_name() {
    throw INTERNAL_ERROR;
}




std::vector<std::string> PartialInitializable::get_partial_initializable_names() {
    throw INTERNAL_ERROR;
}




void VirtualEntry::compile(TypeMatch tm, Cx *cx) {
}

Label VirtualEntry::get_virtual_entry_label(TypeMatch tm, Cx *cx) {
    throw INTERNAL_ERROR;
}

std::ostream &VirtualEntry::out_virtual_entry(std::ostream &os, TypeMatch tm) {
    throw INTERNAL_ERROR;
}

VirtualEntry::~VirtualEntry() {
}




AutoconvVirtualEntry::AutoconvVirtualEntry(Autoconvertible *a) {
    autoconvertible = a;
}

Label AutoconvVirtualEntry::get_virtual_entry_label(TypeMatch tm, Cx *cx) {
    return autoconvertible->get_autoconv_table_label(tm, cx);
}

std::ostream &AutoconvVirtualEntry::out_virtual_entry(std::ostream &os, TypeMatch tm) {
    // A bit overkill, just for debugging
    std::vector<AutoconvEntry> act = autoconvertible->get_autoconv_table(tm);
    
    os << "CONV to";
    
    if (act.size()) {
        for (auto ace : act)
            os << " " << ace.role_ts;
    }
    else
        os << " nothing";
        
    return os;
}




FfwdVirtualEntry::FfwdVirtualEntry(Allocation o) {
    offset = o;
}

Label FfwdVirtualEntry::get_virtual_entry_label(TypeMatch tm, Cx *cx) {
    Label label;
    cx->absolute_label(label, -allocsubst(offset, tm).concretize());  // forcing an int into an unsigned64...
    return label;
}

std::ostream &FfwdVirtualEntry::out_virtual_entry(std::ostream &os, TypeMatch tm) {
    return os << "FFWD " << -allocsubst(offset, tm).concretize();
}




MethodVirtualEntry::MethodVirtualEntry(Methodlike *m) {
    method = m;
}

Label MethodVirtualEntry::get_virtual_entry_label(TypeMatch tm, Cx *cx) {
    // We're not yet ready to compile templated functions
    if (tm[1] != NO_TS)
        throw INTERNAL_ERROR;
        
    //std::cerr << "Function entry " << name << ".\n";
    return method->get_method_label(cx);
}

std::ostream &MethodVirtualEntry::out_virtual_entry(std::ostream &os, TypeMatch tm) {
    return os << "FUNC " << method->get_method_name();
}




PatchMethodVirtualEntry::PatchMethodVirtualEntry(Methodlike *m, int o) {
    method = m;
    offset = o;
}

void PatchMethodVirtualEntry::compile(TypeMatch tm, Cx *cx) {
    cx->code_label(trampoline_label);
    //cx->runtime->log("TRAMPOLINE!");
    cx->op(MOVQ, R11, offset);
    cx->op(JMP, method->get_method_label(cx));
}

Label PatchMethodVirtualEntry::get_virtual_entry_label(TypeMatch tm, Cx *cx) {
    // We're not yet ready to compile templated functions
    if (tm[1] != NO_TS)
        throw INTERNAL_ERROR;
        
    //std::cerr << "Function entry " << name << ".\n";
    return trampoline_label;
}

std::ostream &PatchMethodVirtualEntry::out_virtual_entry(std::ostream &os, TypeMatch tm) {
    return os << "FUNC " << method->get_method_name() << " (" << offset << ")";
}

