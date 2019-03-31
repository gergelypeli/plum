
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
        return t == ref_type || t == ptr_type || t == string_type || ptr_cast<HeapType>(t) ? 0 : ADDRESS_SIZE;
    }

    virtual Allocation measure(TypeMatch tm) {
        Allocation a = tm[1].measure();
        a.bytes += get_flag_size(tm[1]);
        return a;
    }

    virtual void store(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        int stack_size = tm[0].measure_stack();
        int flag_size = get_flag_size(tm[1]);
        
        switch (s.where * t.where) {
        case NOWHERE_STACK:
            x64->op(SUBQ, RSP, stack_size);
            x64->op(MOVQ, Address(RSP, 0), OPTION_FLAG_NONE);
            return;
        case STACK_NOWHERE:
            destroy(tm, Storage(MEMORY, Address(RSP, 0)), x64);
            x64->op(ADDQ, RSP, stack_size);
            return;
        case STACK_STACK:
            return;
        case STACK_MEMORY:
            destroy(tm, t, x64);
            create(tm, s, t, x64);
            return;
        case MEMORY_NOWHERE:
            return;
        case MEMORY_STACK:
            x64->op(SUBQ, RSP, stack_size);
            create(tm, s, Storage(MEMORY, Address(RSP, 0)), x64);
            return;
        case MEMORY_MEMORY: { // must work for self-assignment
            Label s_none, none_some, some_none, end;
            
            x64->op(CMPQ, s.address, OPTION_FLAG_NONE);
            x64->op(JE, s_none);

            // s some
            x64->op(CMPQ, t.address, OPTION_FLAG_NONE);
            x64->op(JE, some_none);

            // some_some
            tm[1].store(s + flag_size, t + flag_size, x64);
            x64->op(JMP, end);

            // some_none
            x64->code_label(some_none);
            if (flag_size == ADDRESS_SIZE)
                x64->op(MOVQ, t.address, OPTION_FLAG_NONE + 1);

            tm[1].create(s + flag_size, t + flag_size, x64);
            x64->op(JMP, end);

            x64->code_label(s_none);
            x64->op(CMPQ, t.address, OPTION_FLAG_NONE);
            x64->op(JE, end);  // none_none

            // none_some
            tm[1].destroy(t + flag_size, x64);
            x64->op(MOVQ, t.address, OPTION_FLAG_NONE);

            x64->code_label(end);
        }
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
        case NOWHERE_MEMORY:
            x64->op(MOVQ, t.address, OPTION_FLAG_NONE);
            return;
        case STACK_MEMORY:
            x64->runtime->copy(Address(RSP, 0), t.address, tm[0].measure_raw());
            x64->op(ADDQ, RSP, stack_size);
            return;
        case MEMORY_MEMORY:  // duplicates data
            x64->op(CMPQ, s.address, OPTION_FLAG_NONE);
            x64->op(JE, none);
            
            if (flag_size == ADDRESS_SIZE)
                x64->op(MOVQ, t.address, OPTION_FLAG_NONE + 1);
                
            tm[1].create(s + flag_size, t + flag_size, x64);
            x64->op(JMP, end);

            x64->code_label(none);
            x64->op(MOVQ, t.address, OPTION_FLAG_NONE);
            
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
            x64->op(CMPQ, s.address, OPTION_FLAG_NONE);
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
            //as_what == AS_PIVOT_ARGUMENT ? MEMORY :
            as_what == AS_LVALUE_ARGUMENT ? ALIAS :
            throw INTERNAL_ERROR
        );
    }
    
    virtual void equal(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        int flag_size = get_flag_size(tm[1]);

        if (s.where == MEMORY && t.where == MEMORY) {
            Label end;
            x64->op(MOVQ, R10, s.address);
            x64->op(CMPQ, R10, t.address);
            x64->op(JNE, end);
            x64->op(CMPQ, R10, OPTION_FLAG_NONE);
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
            x64->op(CMPQ, s.address, OPTION_FLAG_NONE);
            x64->op(SETE, R11B);
            x64->op(CMPQ, t.address, OPTION_FLAG_NONE);
            x64->op(SETE, R10B);
            
            x64->op(SUBB, R10B, R11B);
            x64->op(JNE, end);  // exactly one was none, order is decided, R10B, flags as expected
            x64->op(CMPB, R11B, 0);
            x64->op(JE, end);  // both were none, equality is decided, R10B, flags as expected
            
            // neither are none, must compare according to the type parameter
            tm[1].compare(Storage(MEMORY, s.address + flag_size), Storage(MEMORY, t.address + flag_size), x64);
            x64->code_label(end);
            return;
        }
        else
            throw INTERNAL_ERROR;
    }

    virtual void streamify(TypeMatch tm, X64 *x64) {
        Label os_label = x64->once->compile(compile_streamification, tm[1]);
        
        x64->op(CALL, os_label);
    }
    
    static void compile_streamification(Label label, TypeSpec some_ts, X64 *x64) {
        Label some, ok;
        
        x64->code_label_local(label, some_ts.symbolize() + "_option_streamification");

        x64->op(CMPQ, Address(RSP, ADDRESS_SIZE + ALIAS_SIZE), OPTION_FLAG_NONE);
        x64->op(JNE, some);
        
        // `none
        streamify_ascii("`none", Address(RSP, ADDRESS_SIZE), x64);
        x64->op(JMP, ok);
        
        // `some
        x64->code_label(some);
        streamify_ascii("`some", Address(RSP, ADDRESS_SIZE), x64);
        
        if (some_ts != UNIT_TS) {
            // Okay, just for the sake of correctness
            streamify_ascii("(", Address(RSP, ADDRESS_SIZE), x64);

            unsigned some_size = some_ts.measure_stack();
            unsigned copy_count = some_size / ADDRESS_SIZE;
            unsigned flag_count = get_flag_size(some_ts) / ADDRESS_SIZE;

            // Make a borrowed copy of the value in the stack.
            for (unsigned j = 0; j < copy_count; j++) {
                // skip ret address, alias, flag
                x64->op(PUSHQ, Address(RSP, ADDRESS_SIZE * (copy_count + 2 + flag_count - 1)));
            }
        
            // Skip the flag, but copy the stream alias, too
            x64->op(PUSHQ, Address(RSP, ADDRESS_SIZE * (copy_count + 2 - 1)));
        
            some_ts.streamify(x64);
        
            x64->op(ADDQ, RSP, ADDRESS_SIZE * (copy_count + 1));

            streamify_ascii(")", Address(RSP, ADDRESS_SIZE), x64);
        }
        
        x64->code_label(ok);
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
                return make<OptionNoneValue>(tm[0]);
            else if (n == "some")
                return make<OptionSomeValue>(tm[0]);
                
            std::cerr << "Can't initialize Option as " << n << "!\n";
            return NULL;
        }
    }

    virtual Value *lookup_matcher(TypeMatch tm, std::string n, Value *pivot, Scope *s) {
        if (n == "none")
            return make<OptionNoneMatcherValue>(pivot, tm);
        else if (n == "some")
            return make<OptionSomeMatcherValue>(pivot, tm);
            
        std::cerr << "Can't match Option as " << n << "!\n";
        return NULL;
    }
};


class UnionType: public Type {
public:
    // TODO: allow default initialization if the first type is Unit?
    std::vector<std::string> tags;
    std::vector<TypeSpec> tss;

    UnionType(std::string n)
        :Type(n, Metatypes {}, value_metatype) {
    }

    virtual bool complete_type() {
        for (auto &c : inner_scope->contents) {
            Variable *v = ptr_cast<Variable>(c.get());
            
            if (v) {
                if (!ptr_cast<InterfaceType>(v->alloc_ts[0])) {
                    tss.push_back(v->alloc_ts.rvalue());
                    tags.push_back(v->name);
                }
            }
            else {
                std::cerr << "Only variables can be declared in an Union!\n";
                return false;
            }
            /*
            Identifier *i = ptr_cast<Identifier>(c.get());
            if (i && i->name == "compare")
                has_custom_compare = true;
                
            Implementation *imp = ptr_cast<Implementation>(c.get());
            if (imp && imp->is_autoconv() && imp->alloc_ts == STREAMIFIABLE_TS)
                streamifiable_implementation = imp;
                
            Function *f = ptr_cast<Function>(c.get());
            if (f && streamifiable_implementation && f->associated == streamifiable_implementation)
                streamify_function = f;
                
            if (f && f->type == INITIALIZER_FUNCTION)
                member_initializers.push_back(f);

            if (f && f->type == LVALUE_FUNCTION)
                member_procedures.push_back(f);
            */
        }

        std::cerr << "Union " << name << " has " << tss.size() << " members.\n";
        return true;
    }

    static int get_flag_size() {
        // We're more flexible than Option, so always allocate a tag field
        return ADDRESS_SIZE;
    }

    virtual Allocation measure(TypeMatch tm) {
        int max_size = 0;
        
        for (auto &ts : tss) {
            int size = typesubst(ts, tm).measure_stack();
            
            if (size > max_size)
                max_size = size;
        }
        
        return Allocation(max_size + get_flag_size());
    }

    virtual void store(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        int stack_size = tm[0].measure_stack();
        int flag_size = get_flag_size();
        
        switch (s.where * t.where) {
        //case NOWHERE_STACK:
        //    x64->op(SUBQ, RSP, stack_size);
        //    x64->op(MOVQ, Address(RSP, 0), OPTION_FLAG_NONE);
        //    return;
        case STACK_NOWHERE:
            destroy(tm, Storage(MEMORY, Address(RSP, 0)), x64);
            x64->op(ADDQ, RSP, stack_size);
            return;
        case STACK_STACK:
            return;
        case STACK_MEMORY:
            destroy(tm, t, x64);
            create(tm, s, t, x64);
            return;
        case MEMORY_NOWHERE:
            return;
        case MEMORY_STACK:
            x64->op(SUBQ, RSP, stack_size);
            create(tm, s, Storage(MEMORY, Address(RSP, 0)), x64);
            return;
        case MEMORY_MEMORY: { // must work for self-assignment
            Label end, same;
            
            x64->op(MOVQ, R10, s.address);
            x64->op(CMPQ, R10, t.address);
            x64->op(JE, same);
            
            destroy(tm, t, x64);
            create(tm, s, t, x64);
            x64->op(JMP, end);
            
            x64->code_label(same);
            // Can't just destroy t first, it may be the same as s
            
            for (unsigned i = 0; i < tss.size(); i++) {
                Label skip;
                
                x64->op(CMPQ, R10, i);
                x64->op(JNE, skip);
                
                tss[i].store(s + flag_size, t + flag_size, x64);
                x64->op(JMP, end);
                
                x64->code_label(skip);
            }

            x64->runtime->die("Invalid Union!");
            
            x64->code_label(end);
        }
            return;
        default:
            Type::store(tm, s, t, x64);
        }
    }

    virtual void create(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        int stack_size = tm[0].measure_stack();
        int flag_size = get_flag_size();
        Label none, end;

        switch (s.where * t.where) {
        //case NOWHERE_MEMORY:
        //    x64->op(MOVQ, t.address, OPTION_FLAG_NONE);
        //    return;
        case STACK_MEMORY:
            x64->runtime->copy(Address(RSP, 0), t.address, tm[0].measure_raw());
            x64->op(ADDQ, RSP, stack_size);
            return;
        case MEMORY_MEMORY: {  // duplicates data
            Label end;

            x64->op(MOVQ, R10, s.address);
            x64->op(MOVQ, t.address, R10);
            
            for (unsigned i = 0; i < tss.size(); i++) {
                Label skip;
                
                x64->op(CMPQ, R10, i);
                x64->op(JNE, skip);
                
                tss[i].create(s + flag_size, t + flag_size, x64);
                x64->op(JMP, end);
                
                x64->code_label(skip);
            }

            x64->runtime->die("Invalid Union!");
            
            x64->code_label(end);
        }
            return;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual void destroy(TypeMatch tm, Storage s, X64 *x64) {
        int flag_size = get_flag_size();
        
        if (s.where == MEMORY) {
            Label end;

            x64->op(MOVQ, R10, s.address);
            
            for (unsigned i = 0; i < tss.size(); i++) {
                Label skip;
                
                x64->op(CMPQ, R10, i);
                x64->op(JNE, skip);
                
                tss[i].destroy(s + flag_size, x64);
                x64->op(JMP, end);
                
                x64->code_label(skip);
            }

            x64->runtime->die("Invalid Union!");
            
            x64->code_label(end);
        }
        else
            throw INTERNAL_ERROR;
    }

    virtual StorageWhere where(TypeMatch tm, AsWhat as_what) {
        return (
            as_what == AS_VALUE ? STACK :
            as_what == AS_VARIABLE ? MEMORY :
            as_what == AS_ARGUMENT ? MEMORY :
            as_what == AS_LVALUE_ARGUMENT ? ALIAS :
            throw INTERNAL_ERROR
        );
    }
    
    virtual void equal(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        int flag_size = get_flag_size();

        if (s.where == MEMORY && t.where == MEMORY) {
            Label end;
            x64->op(MOVQ, R10, s.address);
            x64->op(CMPQ, R10, t.address);
            x64->op(JNE, end);

            for (unsigned i = 0; i < tss.size(); i++) {
                Label skip;
                
                x64->op(CMPQ, R10, i);
                x64->op(JNE, skip);
                
                tss[i].equal(s + flag_size, t + flag_size, x64);
                x64->op(JMP, end);
                
                x64->code_label(skip);
            }

            x64->runtime->die("Invalid Union!");

            x64->code_label(end);
            return;
        }
        else
            throw INTERNAL_ERROR;
    }
    
    virtual void compare(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        int flag_size = get_flag_size();

        if (s.where == MEMORY && t.where == MEMORY) {
            Label end, equal;
            
            x64->op(MOVQ, R10, s.address);
            x64->op(CMPQ, R10, t.address);
            x64->op(JE, equal);
            
            x64->runtime->r10bcompar(true);
            x64->op(JMP, end);
            
            x64->code_label(equal);
            
            for (unsigned i = 0; i < tss.size(); i++) {
                Label skip;
                
                x64->op(CMPQ, R10, i);
                x64->op(JNE, skip);
                
                tss[i].compare(s + flag_size, t + flag_size, x64);
                x64->op(JMP, end);
                
                x64->code_label(skip);
            }
            
            x64->runtime->die("Invalid Union!");

            x64->code_label(end);
            return;
        }
        else
            throw INTERNAL_ERROR;
    }

    virtual void streamify(TypeMatch tm, X64 *x64) {
        Label os_label = x64->once->compile(compile_streamification, tm[0]);
        
        x64->op(CALL, os_label);
    }
    
    static void compile_streamification(Label label, TypeSpec union_ts, X64 *x64) {
        unsigned union_size = union_ts.measure_stack();
        unsigned copy_count = union_size / ADDRESS_SIZE - 1;  // without the flag
        UnionType *ut = ptr_cast<UnionType>(union_ts[0]);
    
        x64->code_label_local(label, union_ts.symbolize() + "_streamification");
        Label end;

        for (unsigned i = 0; i < ut->tss.size(); i++) {
            Label skip;
            
            x64->op(CMPQ, Address(RSP, ADDRESS_SIZE + ALIAS_SIZE), i);
            x64->op(JNE, skip);
            
            // Streamify the tag first
            streamify_ascii("`" + ut->tags[i], Address(RSP, ADDRESS_SIZE), x64);
            
            if (ut->tss[i] != UNIT_TS) {
                streamify_ascii("(", Address(RSP, ADDRESS_SIZE), x64);
            
                // Then the field. Make a borrowed copy of the value in the stack.
                for (unsigned j = 0; j < copy_count; j++) {
                    // skip ret address, alias, flag
                    x64->op(PUSHQ, Address(RSP, ADDRESS_SIZE * (copy_count + 3 - 1)));
                }
            
                // Skip the flag, but copy the stream alias, too
                x64->op(PUSHQ, Address(RSP, ADDRESS_SIZE * (copy_count + 2 - 1)));
            
                ut->tss[i].streamify(x64);
            
                x64->op(ADDQ, RSP, ADDRESS_SIZE * (copy_count + 1));

                streamify_ascii(")", Address(RSP, ADDRESS_SIZE), x64);
            }
            
            x64->op(JMP, end);
            
            x64->code_label(skip);
        }

        x64->runtime->die("Invalid Union!");

        x64->code_label(end);
        x64->op(RET);
    }

    virtual Value *lookup_initializer(TypeMatch tm, std::string n, Scope *scope) {
        if (n == "{}") {
            // Anonymous initializers rejected
            return NULL;
        }
        else {
            // Named initializer
            
            for (unsigned i = 0; i < tss.size(); i++) {
                if (n == tags[i])
                    return make<UnionValue>(tm[0], tss[i], i);
            }
                
            std::cerr << "Can't initialize Union as " << n << "!\n";
            return NULL;
        }
    }

    virtual Value *lookup_matcher(TypeMatch tm, std::string n, Value *pivot, Scope *s) {
        for (unsigned i = 0; i < tss.size(); i++) {
            if (n == tags[i])
                return make<UnionMatcherValue>(pivot, tm[0], tss[i], i);
        }

        std::cerr << "Can't match Union as " << n << "!\n";
        return NULL;
    }
};
