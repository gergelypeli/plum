
class OptionType: public Type {
public:
    OptionType(std::string n)
        :Type(n, Metatypes { value_metatype }, value_metatype) {
    }

    static int get_flag_size(TypeSpec some_ts) {
        // NOTE: the option is `none only if the first 8-bytes integer is 0.
        // This is because we may use an implicit flag if the first 8 bytes are known not be
        // all zeroes, such as a reference.
        Type *t = some_ts[0];
        return t == ref_type || t == ptr_type || t == string_type || ptr_cast<HeapType>(t) ? 0 : INTEGER_SIZE;
    }

    virtual Allocation measure(TypeMatch tm) {
        Allocation a = tm[1].measure();
        a.bytes += get_flag_size(tm[1]);
        return a;
    }

    virtual void store(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        int stack_size = tm[0].measure_stack();
        
        switch (s.where * t.where) {
        case STACK_NOWHERE:
            destroy(tm, Storage(MEMORY, Address(RSP, 0)), x64);
            x64->op(ADDQ, RSP, stack_size);
            return;
        case STACK_STACK:
            return;
        case STACK_MEMORY:
            store(tm, Storage(MEMORY, Address(RSP, 0)), t, x64);
            store(tm, s, Storage(), x64);
            return;
        case MEMORY_NOWHERE:
            return;
        case MEMORY_STACK:
            x64->op(SUBQ, RSP, stack_size);
            create(tm, s, Storage(MEMORY, Address(RSP, 0)), x64);
            return;
        case MEMORY_MEMORY:  // duplicates data
            destroy(tm, t, x64);
            create(tm, s, t, x64);
            return;
        default:
            Type::store(tm, s, t, x64);
        }
    }

    virtual void create(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        int stack_size = tm[0].measure_stack();
        int flag_size = get_flag_size(tm[1]);
        Label none, end;

        switch (s.where * t.where) {
        case NOWHERE_STACK:
            x64->op(SUBQ, RSP, stack_size);
            x64->op(MOVQ, Address(RSP, 0), 0);
            return;
        case NOWHERE_MEMORY:
            x64->op(MOVQ, t.address, 0);
            return;
        case STACK_MEMORY:
            create(tm, Storage(MEMORY, Address(RSP, 0)), t, x64);
            destroy(tm, Storage(MEMORY, Address(RSP, 0)), x64);
            x64->op(ADDQ, RSP, stack_size);
            return;
        case MEMORY_MEMORY:  // duplicates data
            x64->op(CMPQ, s.address, 0);
            x64->op(JE, none);
            
            if (flag_size)
                x64->op(MOVQ, t.address, 1);
                
            tm[1].create(s + flag_size, t + flag_size, x64);
            x64->op(JMP, end);

            x64->code_label(none);
            x64->op(MOVQ, t.address, 0);
            
            x64->code_label(end);
            return;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual void destroy(TypeMatch tm, Storage s, X64 *x64) {
        int flag_size = get_flag_size(tm[1]);
        Label none;
        
        if (s.where == MEMORY) {
            x64->op(CMPQ, s.address, 0);
            x64->op(JE, none);
            tm[1].destroy(s + flag_size, x64);
            x64->code_label(none);
        }
        else
            throw INTERNAL_ERROR;
    }

    virtual StorageWhere where(TypeMatch tm, AsWhat as_what) {
        return (
            as_what == AS_VALUE ? STACK :
            as_what == AS_VARIABLE ? MEMORY :
            as_what == AS_ARGUMENT ? MEMORY :
            as_what == AS_PIVOT_ARGUMENT ? MEMORY :
            as_what == AS_LVALUE_ARGUMENT ? ALIAS :
            throw INTERNAL_ERROR
        );
    }
    
    virtual void equal(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        int flag_size = get_flag_size(tm[1]);

        if (s.where == MEMORY && t.where == MEMORY) {
            Label end;
            x64->op(MOVQ, RBX, s.address);
            x64->op(CMPQ, RBX, t.address);
            x64->op(JNE, end);
            x64->op(CMPQ, RBX, 0);
            x64->op(JE, end);
            tm[1].equal(Storage(MEMORY, s.address + flag_size), Storage(MEMORY, t.address + flag_size), x64);
            x64->code_label(end);
            return;
        }
        else
            throw INTERNAL_ERROR;
    }
    
    virtual void compare(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        int flag_size = get_flag_size(tm[1]);

        if (s.where == MEMORY && t.where == MEMORY) {
            Label end;
            x64->op(CMPQ, s.address, 0);
            x64->op(SETE, BH);
            x64->op(CMPQ, t.address, 0);
            x64->op(SETE, BL);
            
            x64->op(SUBB, BL, BH);
            x64->op(JNE, end);  // exactly one was none, order is decided, BL, flags as expected
            x64->op(CMPW, BX, 0);
            x64->op(JE, end);  // both were none, equality is decided, BL, flags as expected
            
            // neither are none, must compare according to the type parameter
            tm[1].compare(Storage(MEMORY, s.address + flag_size), Storage(MEMORY, t.address + flag_size), x64);
            x64->code_label(end);
            return;
        }
        else
            throw INTERNAL_ERROR;
    }

    virtual void streamify(TypeMatch tm, bool repr, X64 *x64) {
        int flag_size = get_flag_size(tm[1]);
        Label os_label = x64->once->compile(compile_streamification, tm[1]);
        Label ok;
        
        x64->op(CALL, os_label);
        
        x64->op(CMPQ, Address(RSP, ALIAS_SIZE), 0);
        x64->op(JE, ok);
        
        if (flag_size) {
            x64->op(POPQ, RBX);  // stream alias
            x64->op(ADDQ, RSP, 8);
            x64->op(PUSHQ, RBX);  // overwrite flag
        }
        
        tm[1].streamify(true, x64);
        
        if (flag_size) {
            x64->op(POPQ, RBX);
            x64->op(PUSHQ, 1);
            x64->op(PUSHQ, RBX);
        }
                
        x64->code_label(ok);
    }
    
    static void compile_streamification(Label label, TypeSpec some_ts, X64 *x64) {
        Label none_label = x64->runtime->data_heap_string(decode_utf8("`none"));
        Label some_label = x64->runtime->data_heap_string(decode_utf8("`some "));
        Label some, ok;
        
        x64->code_label_local(label, "option_streamification");

        x64->op(CMPQ, Address(RSP, ADDRESS_SIZE + ALIAS_SIZE), 0);  // the flag
        x64->op(JNE, some);
        
        // `none
        x64->op(LEA, RBX, Address(none_label, 0));
        x64->op(JMP, ok);
        
        // `some
        x64->code_label(some);
        x64->op(LEA, RBX, Address(some_label, 0));
        
        x64->code_label(ok);
        x64->op(PUSHQ, RBX);
        x64->op(PUSHQ, Address(RSP, ADDRESS_SIZE + ADDRESS_SIZE));
        STRING_TS.streamify(false, x64);
        x64->op(ADDQ, RSP, 16);
        x64->op(RET);
    }

    virtual Value *lookup_initializer(TypeMatch tm, std::string n) {
        if (n == "{}") {
            // Anonymous initializers rejected
            return NULL;
        }
        else {
            // Named initializer
            
            if (n == "none")
                return make<OptionNoneValue>(tm[0]);
            else if (n == "some")
                return make<OptionSomeValue>(tm[0]);
                
            std::cerr << "Can't initialize Option as " << n << "!\n";
            return NULL;
        }
    }

    virtual Value *lookup_matcher(TypeMatch tm, std::string n, Value *pivot) {
        if (n == "none")
            return make<OptionNoneMatcherValue>(pivot, tm);
        else if (n == "some")
            return make<OptionSomeMatcherValue>(pivot, tm);
            
        std::cerr << "Can't match Option as " << n << "!\n";
        return NULL;
    }
};
