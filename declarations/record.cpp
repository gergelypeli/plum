
class RecordType: public Type {
public:
    std::vector<Variable *> member_variables;
    std::vector<TypeSpec> member_tss;  // rvalues, for the initializer arguments
    std::vector<std::string> member_names;

    RecordType(std::string n, int pc)
        :Type(n, pc) {
    }

    virtual void complete_type() {
        for (auto &c : inner_scope->contents) {
            Variable *v = dynamic_cast<Variable *>(c.get());
            
            if (v) {
                member_variables.push_back(v);
                member_tss.push_back(v->var_ts.rvalue());
                member_names.push_back(v->name);
            }
        }
        
        std::cerr << "Record " << name << " has " << member_variables.size() << " member variables.\n";
    }
    
    virtual Allocation measure(TypeMatch tm) {
        return inner_scope->get_size(tm);
    }

    virtual void store(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        int stack_size = tm[0].measure_stack();
        
        switch (s.where * t.where) {
        //case REGISTER_STACK:  // for the sake of String, FIXME: check size and stuff!
        //    x64->op(PUSHQ, s.reg);
        //    return;
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
            for (auto &var : member_variables)
                var->store(tm, Storage(MEMORY, s.address), Storage(MEMORY, t.address), x64);
            return;
        default:
            Type::store(tm, s, t, x64);
        }
    }

    virtual void create(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        int stack_size = tm[0].measure_stack();

        switch (s.where * t.where) {
        case NOWHERE_STACK:
            x64->op(SUBQ, RSP, stack_size);
            create(tm, Storage(), Storage(MEMORY, Address(RSP, 0)), x64);
            return;
        case NOWHERE_MEMORY:
            for (auto &var : member_variables)
                var->create(tm, Storage(), Storage(MEMORY, t.address), x64);
            return;
        //case REGISTER_MEMORY:  // for the sake of String, FIXME: check sizes
        //    x64->op(MOVQ, t.address, s.reg);
        //    return;
        case STACK_MEMORY:
            create(tm, Storage(MEMORY, Address(RSP, 0)), t, x64);
            store(tm, s, Storage(), x64);
            return;
        case MEMORY_MEMORY:  // duplicates data
            for (auto &var : member_variables)
                var->create(tm, Storage(MEMORY, s.address), Storage(MEMORY, t.address), x64);
            return;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual void destroy(TypeMatch tm, Storage s, X64 *x64) {
        if (s.where == MEMORY) {
            for (auto &var : member_variables)  // FIXME: reverse!
                var->destroy(tm, Storage(MEMORY, s.address), x64);
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
            address = Address(RSP, 0);
            break;
        case MEMORY:
            address = s.address;
            break;
        default:
            throw INTERNAL_ERROR;
        }
        
        Label done;
        
        for (auto &var : member_variables) {
            Storage t = var->boolval(tm, Storage(MEMORY, address), x64, true);
            
            if (t.where == FLAGS && t.bitset == SETNE)
                x64->op(JNE, done);
            else
                throw INTERNAL_ERROR;
        }

        x64->code_label(done);
        
        if (!probe) {
            x64->op(SETNE, BL);
            x64->op(PUSHQ, RBX);
            destroy(tm, Storage(MEMORY, Address(RSP, INTEGER_SIZE)), x64);
            x64->op(POPQ, RBX);
            x64->op(ADDQ, RSP, stack_size);
            x64->op(CMPB, BL, 0);
        }
        
        return Storage(FLAGS, SETNE);
    }
    
    virtual void compare(TypeMatch tm, Storage s, Storage t, X64 *x64, Label less, Label greater) {
        int stack_size = tm[0].measure_stack();

        StorageWhereWhere stw = s.where * t.where;  // s and t may be overwritten

        switch (stw) {
        case STACK_STACK:
            x64->op(PUSHQ, RBP);
            x64->op(MOVQ, RBP, RSP);
            s = Storage(MEMORY, Address(RBP, ADDRESS_SIZE + stack_size));
            t = Storage(MEMORY, Address(RBP, ADDRESS_SIZE));
            break;
        case STACK_MEMORY:
            x64->op(PUSHQ, RBP);
            x64->op(MOVQ, RBP, RSP);
            s = Storage(MEMORY, Address(RBP, ADDRESS_SIZE));
            break;
        case MEMORY_STACK:
            x64->op(PUSHQ, RBP);
            x64->op(MOVQ, RBP, RSP);
            t = Storage(MEMORY, Address(RBP, ADDRESS_SIZE));
            break;
        case MEMORY_MEMORY:
            for (auto &var : member_variables) 
                var->compare(tm, s, t, x64, less, greater);
            return;
        default:
            throw INTERNAL_ERROR;
        }

        Label xless, xgreater, xend, xclean;

        for (auto &var : member_variables) 
            var->compare(tm, s, t, x64, xless, xgreater);
            
        x64->op(LEARIP, RBX, xend);
        x64->op(JMP, xclean);
        
        x64->code_label(xless);
        x64->op(LEARIP, RBX, less);
        x64->op(JMP, xclean);
        
        x64->code_label(xgreater);
        x64->op(LEARIP, RBX, greater);
        
        x64->code_label(xclean);
        x64->op(PUSHQ, RBX);

        switch (stw) {
        case STACK_STACK:
            destroy(tm, t + ADDRESS_SIZE, x64);
            destroy(tm, s + ADDRESS_SIZE, x64);
            x64->op(POPQ, RBX);
            x64->op(POPQ, RBP);
            x64->op(ADDQ, RSP, 2 * stack_size);
            break;
        case STACK_MEMORY:
            destroy(tm, s + ADDRESS_SIZE, x64);
            x64->op(POPQ, RBX);
            x64->op(POPQ, RBP);
            x64->op(ADDQ, RSP, stack_size);
            break;
        case MEMORY_STACK:
            destroy(tm, t + ADDRESS_SIZE, x64);
            x64->op(POPQ, RBX);
            x64->op(POPQ, RBP);
            x64->op(ADDQ, RSP, stack_size);
            break;
        default:
            throw INTERNAL_ERROR;
        }

        x64->op(JMP, RBX);
        x64->code_label(xend);
    }

    virtual Value *lookup_initializer(TypeMatch tm, std::string n, Scope *scope) {
        //TypeSpec ts(tsi);

        // NOTE: initializers must only appear in code scopes, and there all types
        // must be concrete, not having free parameters. Also, the automatic variable is
        // put in the local scope, so there will be no pivot for it to derive any
        // type parameters from. 
        
        if (n == "{}") {
            // Anonymous initializer
            //TypeMatch match = type_parameters_to_match(ts);
            return make_record_initializer_value(tm);
        }
        else {
            // Named initializer
            Value *pre = make_record_preinitializer_value(tm[0]);

            Value *value = inner_scope->lookup(n, pre);

            // FIXME: check if the method is Void!
            if (value)
                return make_record_postinitializer_value(value);
            
            std::cerr << "Can't initialize record as " << n << "!\n";
            return NULL;
        }
    }
    
    virtual std::vector<TypeSpec> get_member_tss(TypeMatch &match) {
        std::vector<TypeSpec> tss;
        for (auto &ts : member_tss)
            tss.push_back(typesubst(ts, match));
        return tss;
    }
    
    virtual std::vector<std::string> get_member_names() {
        return member_names;
    }

    virtual std::vector<Variable *> get_member_variables() {
        return member_variables;
    }
};


class StringType: public RecordType {
public:
    StringType(std::string n)
        :RecordType(n, 0) {
    }
    
    virtual void compare(TypeMatch tm, Storage s, Storage t, X64 *x64, Label less, Label greater) {
        Label strcmp_label = x64->once->compile(compile_stringcmp);

        switch (s.where * t.where) {
        case STACK_STACK:
            x64->op(XCHGQ, RAX, Address(RSP, REFERENCE_SIZE));
            x64->op(XCHGQ, RDX, Address(RSP, 0));
            break;
        case STACK_MEMORY:
            x64->op(PUSHQ, RDX);
            x64->op(MOVQ, RDX, t.address);
            x64->op(XCHGQ, RAX, Address(RSP, ADDRESS_SIZE));
            break;
        case MEMORY_STACK:
            x64->op(PUSHQ, RDX);
            x64->op(MOVQ, RDX, s.address);
            x64->op(XCHGQ, RAX, Address(RSP, ADDRESS_SIZE));
            x64->op(XCHGQ, RAX, RDX);
            break;
        case MEMORY_MEMORY:
            x64->op(PUSHQ, RAX);
            x64->op(PUSHQ, RDX);
            if (t.address.base != RAX && t.address.index != RAX) {
                x64->op(MOVQ, RAX, s.address);
                x64->op(MOVQ, RDX, t.address);
            }
            else if (s.address.base != RDX && s.address.index != RDX) {
                x64->op(MOVQ, RDX, t.address);
                x64->op(MOVQ, RAX, s.address);
            }
            else {
                x64->op(MOVQ, RAX, t.address);
                x64->op(MOVQ, RDX, s.address);
                x64->op(XCHGQ, RAX, RDX);
            }
            break;
        default:
            throw INTERNAL_ERROR;
        }

        x64->op(PUSHQ, RCX);
        x64->op(PUSHQ, RSI);
        x64->op(PUSHQ, RDI);
        x64->op(CALL, strcmp_label);  // result numerically in RBX, preserved by decref, below
        x64->op(POPQ, RDI);
        x64->op(POPQ, RSI);
        x64->op(POPQ, RCX);
        
        if (t.where == STACK)
            x64->decref(RDX);
            
        if (s.where == STACK)
            x64->decref(RAX);
            
        x64->op(POPQ, RDX);
        x64->op(POPQ, RAX);
        
        x64->op(CMPQ, RBX, 0);
        x64->op(JL, less);
        x64->op(JG, greater);
    }

    static void compile_stringcmp(Label label, X64 *x64) {
        // Expects RAX and RDX with the arguments, clobbers RCX, RSI, RDI, returns RBX.
        x64->code_label_local(label, "stringcmp");
        
        Label equal, less, greater, s_longer, begin;
        x64->op(MOVQ, RBX, 0);  // assume equality
        
        x64->op(CMPQ, RAX, RDX);
        x64->op(JE, equal);
        x64->op(CMPQ, RAX, 0);
        x64->op(JE, less);
        x64->op(CMPQ, RDX, 0);
        x64->op(JE, greater);

        x64->op(MOVQ, RCX, Address(RAX, ARRAY_LENGTH_OFFSET));
        x64->op(CMPQ, RCX, Address(RDX, ARRAY_LENGTH_OFFSET));
        x64->op(JE, begin);
        x64->op(JA, s_longer);
        
        x64->op(MOVQ, RBX, -1);  // t is longer, on common equality t is greater
        x64->op(JMP, begin);

        x64->code_label(s_longer);
        x64->op(MOVQ, RBX, 1);  // s is longer, on common equality s is greater
        x64->op(MOVQ, RCX, Address(RDX, ARRAY_LENGTH_OFFSET));
        
        x64->code_label(begin);
        x64->op(CMPQ, RCX, 0);
        x64->op(JE, equal);
        x64->op(LEA, RSI, Address(RAX, ARRAY_ELEMS_OFFSET));
        x64->op(LEA, RDI, Address(RDX, ARRAY_ELEMS_OFFSET));
        x64->op(REPECMPSW);
        x64->op(JE, equal);
        x64->op(JA, greater);
        
        x64->code_label(less);
        x64->op(MOVQ, RBX, -1);
        x64->op(JMP, equal);
        
        x64->code_label(greater);
        x64->op(MOVQ, RBX, 1);
                
        x64->code_label(equal);  // common parts are equal, RBX determines the result
        x64->op(RET);
    }

    virtual Value *lookup_initializer(TypeMatch tm, std::string n, Scope *scope) {
        if (n == "null") {
            return make_null_string_value();
        }
        else {
            std::cerr << "No String initializer " << n << "!\n";
            return NULL;
        }
    }
};

