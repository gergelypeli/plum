

class RaisingDummy: public Declaration {
public:
    Unwound unwound;
    
    RaisingDummy(Unwound u)
        :Declaration() {
        unwound = u;
    }

    virtual bool may_omit_finalization() {
        return true;
    }

    virtual void set_outer_scope(Scope *os) {
        Declaration::set_outer_scope(os);
        
        outer_scope->be_unwindable(unwound);
    }
};


class Autoconvertible {
public:
    virtual Label get_autoconv_table_label(TypeMatch tm, X64 *x64) {
        throw INTERNAL_ERROR;
    }

    virtual std::vector<AutoconvEntry> get_autoconv_table(TypeMatch tm) {
        throw INTERNAL_ERROR;
    }
};


class Methodlike {
public:
    virtual Label get_method_label(X64 *x64) {
        throw INTERNAL_ERROR;
    }
    
    virtual std::string get_method_name() {
        throw INTERNAL_ERROR;
    }
};


class PartialInitializable {
public:
    virtual std::vector<std::string> get_partial_initializable_names() {
        throw INTERNAL_ERROR;
    }
};


class VirtualEntry {
public:
    virtual void compile(TypeMatch tm, X64 *x64) {
    }
    
    virtual Label get_virtual_entry_label(TypeMatch tm, X64 *x64) {
        throw INTERNAL_ERROR;
    }
    
    virtual std::ostream &out_virtual_entry(std::ostream &os, TypeMatch tm) {
        throw INTERNAL_ERROR;
    }
    
    virtual ~VirtualEntry() {
    }
};


class AutoconvVirtualEntry: public VirtualEntry {
public:
    Autoconvertible *autoconvertible;
    
    AutoconvVirtualEntry(Autoconvertible *a) {
        autoconvertible = a;
    }
    
    virtual Label get_virtual_entry_label(TypeMatch tm, X64 *x64) {
        return autoconvertible->get_autoconv_table_label(tm, x64);
    }

    virtual std::ostream &out_virtual_entry(std::ostream &os, TypeMatch tm) {
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
};


class FfwdVirtualEntry: public VirtualEntry {
public:
    Allocation offset;
    
    FfwdVirtualEntry(Allocation o) {
        offset = o;
    }
    
    virtual Label get_virtual_entry_label(TypeMatch tm, X64 *x64) {
        Label label;
        x64->absolute_label(label, -allocsubst(offset, tm).concretize());  // forcing an int into an unsigned64...
        return label;
    }

    virtual std::ostream &out_virtual_entry(std::ostream &os, TypeMatch tm) {
        return os << "FFWD " << -allocsubst(offset, tm).concretize();
    }
};


class MethodVirtualEntry: public VirtualEntry {
public:
    Methodlike *method;
    
    MethodVirtualEntry(Methodlike *m) {
        method = m;
    }
    
    virtual Label get_virtual_entry_label(TypeMatch tm, X64 *x64) {
        // We're not yet ready to compile templated functions
        if (tm[1] != NO_TS)
            throw INTERNAL_ERROR;
            
        //std::cerr << "Function entry " << name << ".\n";
        return method->get_method_label(x64);
    }

    virtual std::ostream &out_virtual_entry(std::ostream &os, TypeMatch tm) {
        return os << "FUNC " << method->get_method_name();
    }
};


class PatchMethodVirtualEntry: public VirtualEntry {
public:
    Label trampoline_label;
    Methodlike *method;
    int offset;
    
    PatchMethodVirtualEntry(Methodlike *m, int o) {
        method = m;
        offset = o;
    }
    
    virtual void compile(TypeMatch tm, X64 *x64) {
        x64->code_label(trampoline_label);
        //x64->runtime->log("TRAMPOLINE!");
        x64->op(MOVQ, R11, offset);
        x64->op(JMP, method->get_method_label(x64));
    }
    
    virtual Label get_virtual_entry_label(TypeMatch tm, X64 *x64) {
        // We're not yet ready to compile templated functions
        if (tm[1] != NO_TS)
            throw INTERNAL_ERROR;
            
        //std::cerr << "Function entry " << name << ".\n";
        return trampoline_label;
    }

    virtual std::ostream &out_virtual_entry(std::ostream &os, TypeMatch tm) {
        return os << "FUNC " << method->get_method_name() << " (" << offset << ")";
    }
};

