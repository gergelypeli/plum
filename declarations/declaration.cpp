
enum FinalizationType {
    SCOPE_FINALIZATION, UNWINDING_FINALIZATION
};


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

    Declaration() {
        outer_scope = NULL;
        previous_declaration = NULL;
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
    
    virtual void allocate() {
    }
    
    virtual void finalize(FinalizationType ft, Storage s, X64 *x64) {
        if (previous_declaration)
            previous_declaration->finalize(ft, s, x64);
        else if (ft != SCOPE_FINALIZATION)
            declaration_cast(outer_scope)->finalize(ft, s, x64);
    }
};


#include "scope.cpp"
#include "identifier.cpp"
#include "type.cpp"
#include "typespec.cpp"
