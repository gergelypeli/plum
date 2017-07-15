
// Declarations

class Declaration {
public:
    Scope *outer;
    int offset;

    virtual Value *match(std::string, Value *) {
        return NULL;
    }
    
    virtual void allocate() {
    }
    
    virtual Label get_rollback_label() {
        std::cerr << "Can't roll back to this declaration!\n";
        throw INTERNAL_ERROR;
    }
};


#include "scope.cpp"
#include "identifier.cpp"
#include "type.cpp"
