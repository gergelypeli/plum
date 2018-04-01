
class FloatType: public Type {
public:
    FloatType(std::string n, Type *mt = NULL)
        :Type(n, {}, mt ? mt : value_metatype) {
    }
    
    virtual Allocation measure(TypeMatch tm) {
        return Allocation(FLOAT_SIZE);
    }

    virtual void store(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        if (s.where == MEMORY && t.where == NOWHERE)
            ;
        else
            throw INTERNAL_ERROR;
    }

    virtual void create(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        if (s.where == NOWHERE && t.where == MEMORY)
            ;
        else
            throw INTERNAL_ERROR;
    }

    virtual void destroy(TypeMatch tm, Storage s, X64 *x64) {
        if (s.where == MEMORY)
            ;
        else
            throw INTERNAL_ERROR;
    }

    //virtual void equal(TypeMatch tm, Storage s, Storage t, X64 *x64) {
    //}

    //virtual void compare(TypeMatch tm, Storage s, Storage t, X64 *x64, Label less, Label greater) {
    //}
    
    virtual StorageWhere where(TypeMatch tm, AsWhat as_what, bool as_lvalue) {
        return (
            as_what == AS_VALUE ? NOWHERE :
            as_what == AS_VARIABLE ? MEMORY :
            as_what == AS_ARGUMENT ? (as_lvalue ? ALIAS : MEMORY) :
            throw INTERNAL_ERROR
        );
    }
};
