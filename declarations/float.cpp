
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
        case MEMORY_REGISTER:
            x64->op(MOVSD, t.sse, s.address);
            break;
        case MEMORY_STACK:
            x64->op(PUSHQ, s.address);
            break;
        case MEMORY_MEMORY:
            x64->op(MOVQ, RBX, s.address);
            x64->op(MOVQ, t.address, RBX);
            break;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual void create(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        switch (s.where * t.where) {
        case REGISTER_MEMORY:
            x64->op(MOVSD, t.address, s.sse);
            break;
        case MEMORY_MEMORY:
            x64->op(MOVQ, RBX, s.address);
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

    virtual void equal(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        // No need to take care of STACK here, GenericOperationValue takes care of it.
        // Values are equal iff ZF && !PF. Thanks, NaN!
        
        switch (s.where * t.where) {
        case REGISTER_REGISTER:
            x64->op(COMISD, s.sse, t.sse);
            x64->op(SETP, BH);
            x64->op(SETE, BL);
            x64->op(CMPW, BX, 1);
            break;
        case REGISTER_MEMORY:
            x64->op(COMISD, s.sse, t.address);
            x64->op(SETP, BH);
            x64->op(SETE, BL);
            x64->op(CMPW, BX, 1);
            break;
        case MEMORY_REGISTER:
            x64->op(COMISD, t.sse, s.address);  // swapped arguments, but hey!
            x64->op(SETP, BH);
            x64->op(SETE, BL);
            x64->op(CMPW, BX, 1);
            break;
        case MEMORY_MEMORY:
            x64->op(MOVSD, XMM0, s.address);
            x64->op(COMISD, XMM0, t.address);
            x64->op(SETP, BH);
            x64->op(SETE, BL);
            x64->op(CMPW, BX, 1);
            break;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual void compare(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        // We need to do something with NaN-s, so do what Java does, and treat them
        // as greater than everything, including positive infinity. Chuck Norris likes this.
        Label finite, end;
        
        switch (s.where * t.where) {
        case REGISTER_REGISTER:
            x64->op(COMISD, s.sse, t.sse);
            x64->op(JNP, finite);
            
            // BH=1 iff s is finite, BL=1 iff t is finite
            x64->op(COMISD, s.sse, s.sse);
            x64->op(SETNP, BH);
            x64->op(COMISD, t.sse, t.sse);
            x64->op(SETNP, BL);
            x64->op(SUBB, BL, BH);
            x64->op(JMP, end);
            
            x64->code_label(finite);
            x64->blcompar(true);
            x64->code_label(end);
            break;
        case REGISTER_MEMORY:
            x64->op(COMISD, s.sse, t.address);
            x64->op(JNP, finite);
            
            // BH=1 iff s is finite, BL=1 iff t is finite
            x64->op(COMISD, s.sse, s.sse);
            x64->op(SETNP, BH);
            x64->op(MOVSD, XMM0, t.address);
            x64->op(COMISD, XMM0, XMM0);
            x64->op(SETNP, BL);
            x64->op(SUBB, BL, BH);
            x64->op(JMP, end);
            
            x64->code_label(finite);
            x64->blcompar(true);
            x64->code_label(end);
            break;
        case MEMORY_REGISTER:
            x64->op(MOVSD, XMM0, s.address);
            x64->op(COMISD, XMM0, t.sse);
            x64->op(JNP, finite);
            
            // BH=1 iff s is finite, BL=1 iff t is finite
            x64->op(COMISD, XMM0, XMM0);
            x64->op(SETNP, BH);
            x64->op(COMISD, t.sse, t.sse);
            x64->op(SETNP, BL);
            x64->op(SUBB, BL, BH);
            x64->op(JMP, end);
            
            x64->code_label(finite);
            x64->blcompar(true);
            x64->code_label(end);
            break;

            break;
        case MEMORY_MEMORY:
            x64->op(MOVSD, XMM0, s.address);
            x64->op(COMISD, XMM0, t.address);
            x64->op(JNP, finite);
            
            // BH=1 iff s is finite, BL=1 iff t is finite
            x64->op(COMISD, XMM0, XMM0);
            x64->op(SETNP, BH);
            x64->op(MOVSD, XMM0, t.address);
            x64->op(COMISD, XMM0, XMM0);
            x64->op(SETNP, BL);
            x64->op(SUBB, BL, BH);
            x64->op(JMP, end);
            
            x64->code_label(finite);
            x64->blcompar(true);
            x64->code_label(end);
            break;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual StorageWhere where(TypeMatch tm, AsWhat as_what, bool as_lvalue) {
        return (
            as_what == AS_VALUE ? REGISTER :
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
    
    virtual Value *lookup_initializer(TypeMatch tm, std::string name) {
        if (name == "nan")
            return make_float_value(FLOAT_TS, NAN);
        else if (name == "pinf")
            return make_float_value(FLOAT_TS, INFINITY);
        else if (name == "ninf")
            return make_float_value(FLOAT_TS, -INFINITY);
        else {
            std::cerr << "No Float initializer called " << name << "!\n";
            return NULL;
        }
    }
};
