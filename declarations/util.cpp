
class TemporaryAlias: public Declaration {
public:
    Allocation offset;

    TemporaryAlias()
        :Declaration() {
    }
    
    virtual void allocate() {
        offset = outer_scope->reserve(ALIAS_SIZE);
    }
    
    virtual Storage get_local_storage() {
        Storage ls = outer_scope->get_local_storage();
        
        if (ls.where != MEMORY)
            throw INTERNAL_ERROR;
            
        return Storage(ALIAS, ls.address + offset.concretize(), 0);
    }
    
    virtual Storage process(Storage s, X64 *x64) {
        Storage ts = get_local_storage();
        
        if (s.where == MEMORY) {
            if (s.address.base == RBP) {
                if (s.address.index != NOREG)
                    throw INTERNAL_ERROR;
                
                // Stack-local addresses are handled in compile time
                return s;
            }
            else if (s.address.base == RSP) {
                // RecordPreinitializer and FunctionCall can handle this, but they can
                // store RBP-relative values and fix later, while we can't.
                throw INTERNAL_ERROR;
            }
            else {
                // Dynamic addresses will be stored, and used as an ALIAS
                x64->op(LEA, R10, s.address);
                x64->op(MOVQ, ts.address, R10);
                
                return ts;
            }
        }
        else if (s.where == ALIAS) {
            if (s.address.base == RBP && s.address.index == NOREG) {
                // Stack-local addresses are handled in compile time
                return s;
            }
            else
                throw INTERNAL_ERROR;
        }
        else
            throw INTERNAL_ERROR;
    }
};


// Extend the lifetime of Lvalue containers until the end of the innermost scope
// If created, it must be used, no runtime checks are made. This also means that
// if an operation raises an exception, this must be set before that, because the
// decref will happen anyway.
class TemporaryReference: public Declaration {
public:
    bool is_used;
    Allocation offset;
    
    TemporaryReference()
        :Declaration() {
        is_used = false;
    }
    
    virtual void allocate() {
        offset = outer_scope->reserve(Allocation(REFERENCE_SIZE));
    }

    virtual Address get_address() {
        is_used = true;
        return Address(RBP, offset.concretize());
    }
    
    virtual void finalize(X64 *x64) {
        if (!is_used)
            return;
            
        x64->op(MOVQ, R10, get_address());
        x64->runtime->decref(R10);
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


//VirtualEntry *make_method_virtual_entry(Function *f) {
//    return new MethodVirtualEntry(f);
//}


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

