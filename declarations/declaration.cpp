
// Declarations

class Declaration {
public:
    Scope *outer_scope;
    Label finalization_label;
    bool need_finalization_label;
    bool is_finalized;  // sanity check
    
    Declaration() {
        outer_scope = NULL;
        need_finalization_label = false;
        is_finalized = false;
    }

    virtual ~Declaration() {
    }
    
    virtual void set_outer_scope(Scope *os) {
        // Must first remove then add
        if (outer_scope && os)
            throw INTERNAL_ERROR;
            
        outer_scope = os;
        
        if (outer_scope) {
            // The outer scope was obviously entered if we're added to it
            outer_scope_entered();
        }
        else {
            // The outer scope was obviously entered if we're removed from it
            outer_scope_left();
        }
    }

    virtual void outer_scope_entered() {
        // Nothing to do here
    }

    virtual void outer_scope_left() {
        // Nothing to do here
    }

    virtual Value *match(std::string name, Value *pivot, Scope *scope) {
        return NULL;
    }

    virtual bool is_transient() {
        return false;
    }

    virtual bool is_typedefinition(std::string n) {
        return false;
    }
    
    virtual void allocate() {
    }
    
    virtual void finalize(X64 *x64) {
        if (is_finalized)
            throw INTERNAL_ERROR;
            
        if (need_finalization_label)
            x64->code_label(finalization_label);
            
        is_finalized = true;
    }
    
    virtual void jump_to_finalization(X64 *x64) {
        need_finalization_label = true;
        x64->op(JMP, finalization_label);
    }

    virtual DataScope *find_inner_scope(std::string name) {
        return NULL;
    }
    
    virtual void debug(TypeMatch tm, X64 *x64) {
    }
};

