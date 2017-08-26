
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
    Declaration *previous_declaration;
    Label finalization_label;
    bool need_finalization_label;

    Declaration() {
        outer_scope = NULL;
        previous_declaration = NULL;
        need_finalization_label = false;
    }

    virtual ~Declaration() {
    }
    
    virtual void added(Marker marker) {
        outer_scope = marker.scope;
        previous_declaration = marker.last;
    }

    virtual Value *match(std::string name, Value *pivot, TypeMatch &match) {
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


#include "scope.cpp"
#include "identifier.cpp"
#include "type.cpp"
#include "typespec.cpp"
