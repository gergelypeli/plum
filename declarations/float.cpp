
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
        case NOWHERE_SSEREGISTER:
            x64->op(MOVSD, t.sse, Address(x64->runtime->float_zero_label, 0));
            break;
        case NOWHERE_STACK:
            x64->op(PUSHQ, 0);  // using that 0.0 is represented as all zeroes
            break;

        case SSEREGISTER_NOWHERE:
            break;
            
        case STACK_NOWHERE:
            x64->op(ADDQ, RSP, FLOAT_SIZE);
            break;
            
        case MEMORY_NOWHERE:
            break;
        case MEMORY_SSEREGISTER:
            x64->op(MOVSD, t.sse, s.address);
            break;
        case MEMORY_STACK:
            x64->op(PUSHQ, s.address);
            break;
        case MEMORY_MEMORY:
            x64->op(MOVQ, R10, s.address);
            x64->op(MOVQ, t.address, R10);
            break;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual void create(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        switch (s.where * t.where) {
        case NOWHERE_MEMORY:
            x64->op(MOVQ, t.address, 0);  // using that 0.0 is represented as all zeroes
            break;
        case SSEREGISTER_MEMORY:
            x64->op(MOVSD, t.address, s.sse);
            break;
        case MEMORY_MEMORY:
            x64->op(MOVQ, R10, s.address);
            x64->op(MOVQ, t.address, R10);
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
        case SSEREGISTER_SSEREGISTER:
            x64->op(COMISD, s.sse, t.sse);
            break;
        case SSEREGISTER_MEMORY:
            x64->op(COMISD, s.sse, t.address);
            break;
        case MEMORY_SSEREGISTER:
            x64->op(COMISD, t.sse, s.address);  // swapped arguments, but hey!
            break;
        case MEMORY_MEMORY:
            x64->op(MOVSD, XMM15, s.address);
            x64->op(COMISD, XMM15, t.address);
            break;
        default:
            throw INTERNAL_ERROR;
        }

        x64->op(SETP, R11B);  // 1 iff unordered
        x64->op(SETNE, R10B);  // 1 iff not equal
        x64->op(ANDB, R10B, R11B);  // ZF iff (ordered and equal)
    }

    virtual void compare(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        // We need to do something with NaN-s, so do what Java does, and treat them
        // as greater than everything, including positive infinity. Chuck Norris likes this.
        Label finite, end;
        
        switch (s.where * t.where) {
        case SSEREGISTER_SSEREGISTER:
            x64->op(COMISD, s.sse, t.sse);
            x64->op(JNP, finite);
            
            // R11B=1 iff s is finite, R10B=1 iff t is finite
            x64->op(COMISD, s.sse, s.sse);
            x64->op(SETNP, R11B);
            x64->op(COMISD, t.sse, t.sse);
            x64->op(SETNP, R10B);
            x64->op(SUBB, R10B, R11B);
            x64->op(JMP, end);
            
            x64->code_label(finite);
            x64->runtime->r10bcompar(true);
            x64->code_label(end);
            break;
        case SSEREGISTER_MEMORY:
            x64->op(COMISD, s.sse, t.address);
            x64->op(JNP, finite);
            
            // R11B=1 iff s is finite, R10B=1 iff t is finite
            x64->op(COMISD, s.sse, s.sse);
            x64->op(SETNP, R11B);
            x64->op(MOVSD, XMM15, t.address);
            x64->op(COMISD, XMM15, XMM15);
            x64->op(SETNP, R10B);
            x64->op(SUBB, R10B, R11B);
            x64->op(JMP, end);
            
            x64->code_label(finite);
            x64->runtime->r10bcompar(true);
            x64->code_label(end);
            break;
        case MEMORY_SSEREGISTER:
            x64->op(MOVSD, XMM15, s.address);
            x64->op(COMISD, XMM15, t.sse);
            x64->op(JNP, finite);
            
            // R11B=1 iff s is finite, R10B=1 iff t is finite
            x64->op(COMISD, XMM15, XMM15);
            x64->op(SETNP, R11B);
            x64->op(COMISD, t.sse, t.sse);
            x64->op(SETNP, R10B);
            x64->op(SUBB, R10B, R11B);
            x64->op(JMP, end);
            
            x64->code_label(finite);
            x64->runtime->r10bcompar(true);
            x64->code_label(end);
            break;

            break;
        case MEMORY_MEMORY:
            x64->op(MOVSD, XMM15, s.address);
            x64->op(COMISD, XMM15, t.address);
            x64->op(JNP, finite);
            
            // R11B=1 iff s is finite, R10B=1 iff t is finite
            x64->op(COMISD, XMM15, XMM15);
            x64->op(SETNP, R11B);
            x64->op(MOVSD, XMM15, t.address);
            x64->op(COMISD, XMM15, XMM15);
            x64->op(SETNP, R10B);
            x64->op(SUBB, R10B, R11B);
            x64->op(JMP, end);
            
            x64->code_label(finite);
            x64->runtime->r10bcompar(true);
            x64->code_label(end);
            break;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual StorageWhere where(TypeMatch tm, AsWhat as_what) {
        return (
            as_what == AS_VALUE ? SSEREGISTER :
            as_what == AS_VARIABLE ? MEMORY :
            as_what == AS_ARGUMENT ? MEMORY :
            as_what == AS_PIVOT_ARGUMENT ? MEMORY :
            as_what == AS_LVALUE_ARGUMENT ? ALIAS :
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
    
    virtual Value *lookup_initializer(TypeMatch tm, std::string name, Scope *scope) {
        if (name == "nan")
            return make<FloatValue>(FLOAT_TS, NAN);
        else if (name == "pinf")
            return make<FloatValue>(FLOAT_TS, INFINITY);
        else if (name == "ninf")
            return make<FloatValue>(FLOAT_TS, -INFINITY);
        else {
            std::cerr << "No Float initializer called " << name << "!\n";
            return NULL;
        }
    }
};
