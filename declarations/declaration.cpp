
// Declarations

class Declaration {
public:
    Scope *outer_scope;
    Label finalization_label;
    bool need_finalization_label;
    
    Declaration() {
        outer_scope = NULL;
        need_finalization_label = false;
    }

    virtual ~Declaration() {
    }
    
    virtual void set_outer_scope(Scope *os) {
        // Must first remove then add
        if (outer_scope && os)
            throw INTERNAL_ERROR;
            
        outer_scope = os;
    }

    virtual void outer_scope_left() {
        // Nothing to do here
    }

    virtual bool is_called(std::string name) {
        return false;
    }

    virtual Value *match(std::string name, Value *pivot) {
        return NULL;
    }

    virtual bool is_transient() {
        return false;
    }
    
    virtual void allocate() {
    }
    
    virtual void finalize(X64 *x64) {
        if (need_finalization_label)
            x64->code_label(finalization_label);
    }
    
    virtual void jump_to_finalization(X64 *x64) {
        need_finalization_label = true;
        x64->op(JMP, finalization_label);
    }

    virtual DataScope *find_inner_scope(std::string name) {
        return NULL;
    }
};


class RaisingDummy: public Declaration {
public:
    RaisingDummy()
        :Declaration() {
    }

    virtual bool is_transient() {
        return true;  // So that transparent try scopes keep it inside
    }
};


class VirtualEntry {
public:
    virtual Label get_virtual_entry_label(TypeMatch tm, X64 *x64) {
        throw INTERNAL_ERROR;
    }
};
