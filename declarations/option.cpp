
class OptionType: public Type {
public:
    OptionType(std::string n)
        :Type(n, 1) {
    }

    virtual Allocation measure(TypeMatch tm) {
        Allocation a = tm[1].measure();
        a.bytes += INTEGER_TS.measure_stack();
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
        int flag_size = INTEGER_TS.measure_stack();
        Label none;

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
            x64->op(MOVQ, RBX, s.address);
            x64->op(MOVQ, t.address, RBX);
            x64->op(CMPQ, RBX, 0);
            x64->op(JE, none);
            tm[1].create(s + flag_size, t + flag_size, x64);
            x64->code_label(none);
            return;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual void destroy(TypeMatch tm, Storage s, X64 *x64) {
        int flag_size = INTEGER_TS.measure_stack();
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

    virtual StorageWhere where(TypeMatch tm, bool is_arg, bool is_lvalue) {
        return (is_arg ? (is_lvalue ? ALIAS : MEMORY) : (is_lvalue ? MEMORY : STACK));
    }
    
    virtual Storage boolval(TypeMatch tm, Storage s, X64 *x64, bool probe) {
        Address address;
        int stack_size = tm[0].measure_stack();
        
        switch (s.where) {
        case STACK:
            if (!probe) {
                destroy(tm, Storage(MEMORY, Address(RSP, 0)), x64);
                x64->op(CMPQ, Address(RSP, 0), 0);
                x64->op(LEA, RSP, Address(RSP, stack_size));
            }
            else {
                x64->op(CMPQ, Address(RSP, 0), 0);
            }
            break;
        case MEMORY:
            x64->op(CMPQ, s.address, 0);
            break;
        default:
            throw INTERNAL_ERROR;
        }
        
        return Storage(FLAGS, SETNE);
    }
    
    virtual void compare(TypeMatch tm, Storage s, Storage t, X64 *x64, Label less, Label greater) {
        int stack_size = tm[0].measure_stack();
        int flag_size = INTEGER_TS.measure_stack();

        if (s.where == MEMORY && t.where == MEMORY) {
            Label eq;
            x64->op(MOVQ, RBX, s.address);
            x64->op(CMPQ, RBX, t.address);
            x64->op(JL, less);
            x64->op(JG, greater);
            x64->op(CMPQ, RBX, 0);
            x64->op(JE, eq);
            tm[1].compare(Storage(MEMORY, s.address + flag_size), Storage(MEMORY, t.address + flag_size), x64, less, greater);
            x64->code_label(eq);
            return;
        }

        StorageWhereWhere stw = s.where * t.where;  // s and t may be overwritten
        Label xless, xgreater, cleanup;

        switch (stw) {
        case STACK_STACK:
            compare(tm, Storage(MEMORY, Address(RSP, 0)), Storage(MEMORY, Address(RSP, stack_size)), x64, xless, xgreater);
            break;
        case STACK_MEMORY:
            compare(tm, Storage(MEMORY, Address(RSP, 0)), t, x64, xless, xgreater);
            break;
        case MEMORY_STACK:
            compare(tm, s, Storage(MEMORY, Address(RSP, 0)), x64, xless, xgreater);
            break;
        default:
            throw INTERNAL_ERROR;
        }
        
        x64->op(PUSHQ, 0);
        x64->op(JMP, cleanup);
        
        x64->code_label(xless);
        x64->op(PUSHQ, -1);
        x64->op(JMP, cleanup);
        
        x64->code_label(xgreater);
        x64->op(PUSHQ, 1);
        
        x64->code_label(cleanup);
        int pop;

        switch (stw) {
        case STACK_STACK:
            destroy(tm, Storage(MEMORY, Address(RSP, ADDRESS_SIZE)), x64);
            destroy(tm, Storage(MEMORY, Address(RSP, ADDRESS_SIZE + stack_size)), x64);
            pop = 2 * stack_size + ADDRESS_SIZE;
            break;
        case STACK_MEMORY:
            destroy(tm, Storage(MEMORY, Address(RSP, ADDRESS_SIZE)), x64);
            pop = stack_size + ADDRESS_SIZE;
            break;
        case MEMORY_STACK:
            destroy(tm, Storage(MEMORY, Address(RSP, ADDRESS_SIZE)), x64);
            pop = stack_size + ADDRESS_SIZE;
            break;
        default:
            throw INTERNAL_ERROR;
        }

        x64->op(CMPQ, Address(RSP, 0), 0);
        x64->op(LEA, RSP, Address(RSP, pop));
        x64->op(JL, less);
        x64->op(JG, greater);
    }

    virtual void streamify(TypeMatch tm, bool repr, X64 *x64) {
        Label os_label = x64->once->compile(compile_streamification, tm[1]);
        Label ok;
        
        x64->op(CALL, os_label);
        
        x64->op(CMPB, Address(RSP, ALIAS_SIZE), 0);
        x64->op(JE, ok);
        
        x64->op(POPQ, RBX);  // stream alias
        x64->op(ADDQ, RSP, 8);
        x64->op(PUSHQ, RBX);  // overwrite flag
        tm[1].streamify(true, x64);
        x64->op(POPQ, RBX);
        x64->op(PUSHQ, 1);
        x64->op(PUSHQ, RBX);
                
        x64->code_label(ok);
    }
    
    static void compile_streamification(Label label, TypeSpec some_ts, X64 *x64) {
        Label none_label = x64->data_heap_string(decode_utf8("`none"));
        Label some_label = x64->data_heap_string(decode_utf8("`some "));
        Label some, ok;
        
        x64->code_label_local(label, "option_streamification");

        x64->op(CMPQ, Address(RSP, ADDRESS_SIZE + ALIAS_SIZE), 0);  // the flag
        x64->op(JNE, some);
        
        // `none
        x64->op(LEARIP, RBX, none_label);
        x64->op(JMP, ok);
        
        // `some
        x64->code_label(some);
        x64->op(LEARIP, RBX, some_label);
        
        x64->code_label(ok);
        x64->op(PUSHQ, RBX);
        x64->op(PUSHQ, Address(RSP, ADDRESS_SIZE + ADDRESS_SIZE));
        STRING_TS.streamify(false, x64);
        x64->op(ADDQ, RSP, 16);
        x64->op(RET);
    }

    virtual Value *lookup_initializer(TypeMatch tm, std::string n, Scope *scope) {
        if (n == "{}") {
            // Anonymous initializers rejected
            return NULL;
        }
        else {
            // Named initializer
            
            if (n == "none")
                return make_option_none_value(tm[0]);
            else if (n == "some")
                return make_option_some_value(tm[0]);
                
            std::cerr << "Can't initialize Option as " << n << "!\n";
            return NULL;
        }
    }
};


class OptionIsType: public Type {
public:
    OptionIsType(std::string n)
        :Type(n, 1) {
    }
};


class OptionAsType: public Type {
public:
    OptionAsType(std::string n)
        :Type(n, 1) {
    }
};
