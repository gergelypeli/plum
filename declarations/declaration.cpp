
struct Marker {
    Scope *scope;
    Declaration *last;
    
    Marker() {
        scope = NULL;
        last = NULL;
    }
};


Declaration *declaration_cast(Scope *);

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


#include "scope.cpp"
#include "identifier.cpp"
#include "type.cpp"


class Borrow: public Declaration {
public:
    Allocation offset;
    
    virtual void allocate() {
        offset = outer_scope->reserve(Allocation(REFERENCE_SIZE));
    }
    
    virtual Address get_address() {
        return Address(RBP, offset.concretize());
    }
    
    virtual void finalize(X64 *x64) {
        //x64->log("Unborrowing.");
        x64->op(MOVQ, RBX, get_address());
        x64->decref(RBX);
    }
};

