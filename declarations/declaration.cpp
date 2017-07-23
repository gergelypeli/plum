
enum FinalizationType {
    SCOPE_FINALIZATION, UNWINDING_FINALIZATION
};

// Declarations

class Declaration {
public:
    Scope *outer_scope;
    Declaration *previous_declaration;

    Declaration() {
        outer_scope = NULL;
        previous_declaration = NULL;
    }
    
    virtual void added(Scope *os, Declaration *pd) {
        outer_scope = os;
        previous_declaration = pd;
    }

    virtual Value *match(std::string name, Value *pivot) {
        return NULL;
    }
    
    virtual void allocate() {
    }
    
    virtual void finalize(FinalizationType ft, Storage s, X64 *x64) {
        if (previous_declaration)
            previous_declaration->finalize(ft, s, x64);
    }
};


#include "scope.cpp"
#include "identifier.cpp"
#include "type.cpp"
