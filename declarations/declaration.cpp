
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


class Unwind {
public:
    virtual bool compile(Marker marker, X64 *x64) {
        throw INTERNAL_ERROR;
    }
};


class UnwindStack {
public:
    std::vector<Unwind *> stack;
    
    virtual void push(Unwind *u) {
        stack.push_back(u);
    }
    
    virtual void pop(Unwind *u) {
        if (u != stack.back())
            throw INTERNAL_ERROR;
            
        stack.pop_back();
    }
    
    virtual void compile(Marker marker, X64 *x64) {
        for (int i = stack.size() - 1; i >= 0; i--)
            if (stack[i]->compile(marker, x64))
                break;
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
        //if (previous_declaration)
        //    previous_declaration->finalize(ft, s, x64);
        //else if (ft != SCOPE_FINALIZATION)
        //    declaration_cast(outer_scope)->finalize(ft, s, x64);
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
