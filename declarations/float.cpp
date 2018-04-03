
// NOTE: CONSTANT storage means having a Label.def_index in s.value.

class FloatType: public Type {
public:
    FloatType(std::string n, Type *mt = NULL)
        :Type(n, {}, mt ? mt : value_metatype) {
        make_inner_scope(TypeSpec { this });
    }
    
    virtual Allocation measure(TypeMatch tm) {
        return Allocation(FLOAT_SIZE);
    }

    virtual void store(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        switch (s.where * t.where) {
        case STACK_NOWHERE:
            x64->op(ADDQ, RSP, FLOAT_SIZE);
            break;
        case MEMORY_NOWHERE:
            break;
        case MEMORY_STACK:
            x64->op(PUSHQ, s.address);
            break;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual void create(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        switch (s.where * t.where) {
        case CONSTANT_MEMORY:
            x64->op(MOVQ, RBX, Label::thaw(s.value));
            x64->op(MOVQ, t.address, RBX);
            break;
        default:
            throw INTERNAL_ERROR;
        }
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
    
    virtual void streamify(TypeMatch tm, bool repr, X64 *x64) {
        // SysV
        x64->op(MOVSD, XMM0, Address(RSP, ALIAS_SIZE));
        x64->op(MOVQ, RDI, Address(RSP, 0));
        
        Label label;
        x64->code_label_import(label, "streamify_float");
        x64->runtime->call_sysv(label);
    }
};
