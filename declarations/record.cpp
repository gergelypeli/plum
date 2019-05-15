
class RecordType: public Type, public PartialInitializable {
public:
    std::vector<Variable *> member_variables;
    std::vector<TypeSpec> member_tss;  // rvalues, for the initializer arguments
    std::vector<std::string> member_names;
    Function *streamify_function;
    bool is_single;

    RecordType(std::string n, Metatypes param_metatypes)
        :Type(n, param_metatypes, record_metatype) {
        is_single = false;
        streamify_function = NULL;
    }

    virtual bool complete_type() {
        bool has_custom_compare = false;
        Implementation *streamifiable_implementation = NULL;

        for (auto &c : inner_scope->contents) {
            Variable *v = ptr_cast<Variable>(c.get());
            
            if (v) {
                member_variables.push_back(v);
                
                if (!ptr_cast<InterfaceType>(v->alloc_ts[0])) {
                    member_tss.push_back(v->alloc_ts.rvalue());
                    member_names.push_back(v->name);
                }
            }
            
            Identifier *i = ptr_cast<Identifier>(c.get());
            if (i && i->name == "compare")
                has_custom_compare = true;
                
            Implementation *imp = ptr_cast<Implementation>(c.get());
            if (imp && imp->is_autoconv() && imp->alloc_ts == STREAMIFIABLE_TS)
                streamifiable_implementation = imp;
                
            Function *f = ptr_cast<Function>(c.get());
            if (f && streamifiable_implementation && f->associated == streamifiable_implementation)
                streamify_function = f;
        }

        if (!has_custom_compare)
            inner_scope->add(make_record_compare());
        
        if (member_variables.size() == 1)
            is_single = true;
        
        std::cerr << "Record " << name << " has " << member_variables.size() << " member variables.\n";
        return true;
    }

    virtual Allocation measure(TypeMatch tm) {
        if (is_single)
            return typesubst(member_tss[0], tm).measure();
        else
            return inner_scope->get_size(tm);  // May round up
    }

    virtual void store(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        if (is_single) {
            typesubst(member_tss[0], tm).store(s, t, x64);
            return;
        }
    
        int stack_size = tm[0].measure_stack();
        
        switch (s.where * t.where) {
        case NOWHERE_STACK:
            x64->op(SUBQ, RSP, stack_size);
            create(tm, Storage(), Storage(MEMORY, Address(RSP, 0)), x64);
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
        case MEMORY_MEMORY:  // duplicates data
            for (auto &var : member_variables)
                var->store(tm, Storage(MEMORY, s.address), Storage(MEMORY, t.address), x64);
            return;
        default:
            Type::store(tm, s, t, x64);
        }
    }

    virtual void create(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        if (is_single) {
            typesubst(member_tss[0], tm).create(s, t, x64);
            return;
        }

        int stack_size = tm[0].measure_stack();

        switch (s.where * t.where) {
        case NOWHERE_MEMORY:
            for (auto &var : member_variables)
                var->create(tm, Storage(), Storage(MEMORY, t.address), x64);
            return;
        case STACK_MEMORY:
            x64->runtime->copy(Address(RSP, 0), t.address, tm[0].measure_raw());
            x64->op(ADDQ, RSP, stack_size);
            return;
        case MEMORY_MEMORY:  // duplicates data
            for (auto &var : member_variables)
                var->create(tm, Storage(MEMORY, s.address), Storage(MEMORY, t.address), x64);
            return;
        default:
            Type::create(tm, s, t, x64);
        }
    }

    virtual void destroy(TypeMatch tm, Storage s, X64 *x64) {
        if (is_single) {
            typesubst(member_tss[0], tm).destroy(s, x64);
            return;
        }

        if (s.where == MEMORY) {
            for (auto &var : member_variables)  // FIXME: reverse!
                var->destroy(tm, Storage(MEMORY, s.address), x64);
        }
        else
            Type::destroy(tm, s, x64);
    }

    virtual StorageWhere where(TypeMatch tm, AsWhat as_what) {
        if (is_single) {
            return typesubst(member_tss[0], tm).where(as_what);
        }

        return (
            as_what == AS_VALUE ? STACK :
            as_what == AS_VARIABLE ? MEMORY :
            as_what == AS_ARGUMENT ? MEMORY :
            as_what == AS_LVALUE_ARGUMENT ? ALIAS :
            throw INTERNAL_ERROR
        );
    }

    virtual Storage optimal_value_storage(TypeMatch tm, Regs preferred) {
        if (is_single) {
            return typesubst(member_tss[0], tm).optimal_value_storage(preferred);
        }
    
        return Storage(STACK);
    }
        
    virtual unsigned comparable_member_count() {
        return member_variables.size();
    }

    virtual void equal(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        if (is_single) {
            typesubst(member_tss[0], tm).equal(s, t, x64);
            return;
        }

        if (s.where == MEMORY && t.where == MEMORY) {
            Label end;
            if ((s.regs() | t.regs()) & EQUAL_CLOB)
                throw INTERNAL_ERROR;
            
            for (unsigned i = 0; i < comparable_member_count(); i++) {
                if (i > 0)
                    x64->op(JNE, end);

                member_variables[i]->equal(tm, s, t, x64);
            }
            
            x64->code_label(end);
            return;
        }
        else
            throw INTERNAL_ERROR;
    }
    
    virtual void compare(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        if ((s.regs() | t.regs()) & COMPARE_CLOB)
            throw INTERNAL_ERROR;
            
        if (is_single) {
            typesubst(member_tss[0], tm).compare(s, t, x64);
            return;
        }

        if (s.where == MEMORY && t.where == MEMORY) {
            Label end;

            for (unsigned i = 0; i < comparable_member_count(); i++) {
                if (i > 0)
                    x64->op(JNE, end);
                    
                member_variables[i]->compare(tm, s, t, x64);
            }
            
            x64->code_label(end);
            return;
        }
        else
            throw INTERNAL_ERROR;
    }

    virtual void streamify(TypeMatch tm, X64 *x64) {
        if (streamify_function) {
            // The pivot is on the stack as rvalue, and the stream as lvalue.
            x64->op(CALL, streamify_function->get_label(x64));
        }
        else {
            Address value_addr(RSP, ALIAS_SIZE);
            Address alias_addr(RSP, 0);
            
            streamify_ascii("{", alias_addr, x64);
            
            bool did = false;
            
            for (auto v : member_variables) {
                if (did)
                    streamify_ascii(",", alias_addr, x64);
                
                did = true;
                
                x64->op(LEA, RAX, value_addr);
                
                TypeSpec mts = v->get_typespec(tm);
                Storage s = v->get_storage(tm, Storage(MEMORY, Address(RAX, 0)));
                Storage t = Storage(STACK);
                mts.store(s, t, x64);
                x64->op(PUSHQ, 0);
                x64->op(PUSHQ, Address(RSP, mts.measure_stack() + ADDRESS_SIZE));
                
                // Invoking a custom streamification may relocate the stack, so the
                // passed stream alias may be fixed, must propagate it upwards.
                mts.streamify(x64);
                
                x64->op(POPQ, Address(RSP, mts.measure_stack() + ADDRESS_SIZE));
                x64->op(POPQ, R10);
                mts.store(t, Storage(), x64);
            }

            streamify_ascii("}", alias_addr, x64);
        }
    }

    virtual Value *lookup_initializer(TypeMatch tm, std::string n, Scope *scope) {
        //TypeSpec ts(tsi);

        // NOTE: initializers must only appear in code scopes, and there all types
        // must be concrete, not having free parameters. Also, the automatic variable is
        // put in the local scope, so there will be no pivot for it to derive any
        // type parameters from. 

        if (n == "{") {
            // Anonymous initializer
            return make<RecordInitializerValue>(tm);
        }
        else {
            // Named initializer
            Value *pre = make<RecordPreinitializerValue>(tm[0]);

            Value *value = inner_scope->lookup(n, pre, scope);
            
            if (value) {
                // FIXME: check if the method is Void!
                return make<RecordPostinitializerValue>(value);
            }
            
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
    
    virtual std::vector<std::string> get_partial_initializable_names() {
        return member_names;
    }

    virtual void type_info(TypeMatch tm, X64 *x64) {
        // This must be a concrete parametrization
        unsigned size = measure(tm).concretize();
        
        x64->dwarf->begin_structure_type_info(tm[0].symbolize(), size);
        
        debug_inner_scopes(tm, x64);
        
        x64->dwarf->end_info();
    }
};


class StringType: public RecordType {
public:
    StringType(std::string n)
        :RecordType(n, Metatypes {}) {
    }

    virtual void equal(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        if (s.where == MEMORY && t.where == MEMORY) {
            if (t.address.base == RSP)
                throw INTERNAL_ERROR;
                
            x64->op(PUSHQ, s.address);
            x64->op(PUSHQ, t.address);
        }
        else if ((s.where != STACK && s.where != BSTACK) || (t.where != STACK && t.where != BSTACK))
            throw INTERNAL_ERROR;
        
        Label streq_label = x64->once->compile(compile_stringeq);
        x64->op(CALL, streq_label);  // ZF as expected
        
        if (s.where == MEMORY && t.where == MEMORY) {
            x64->op(LEA, RSP, Address(RSP, 2 * ADDRESS_SIZE));  // preserve ZF
        }
    }
    
    static void compile_stringeq(Label label, X64 *x64) {
        x64->code_label_local(label, "String__equality");
        Label sete, done;

        x64->op(PUSHQ, RAX);
        x64->op(PUSHQ, RCX);
        
        x64->op(MOVQ, RAX, Address(RSP, 32));
        x64->op(MOVQ, R10, Address(RSP, 24));
        
        x64->op(CMPQ, RAX, R10);
        x64->op(JE, done);  // identical, must be equal, ZF as expected
        
        x64->op(MOVQ, RCX, Address(RAX, LINEARRAY_LENGTH_OFFSET));
        x64->op(CMPQ, RCX, Address(R10, LINEARRAY_LENGTH_OFFSET));
        x64->op(JNE, done);  // different length, can't be equal, ZF as expected
        
        x64->op(PUSHQ, RSI);
        x64->op(PUSHQ, RDI);
        x64->op(LEA, RSI, Address(RAX, LINEARRAY_ELEMS_OFFSET));
        x64->op(LEA, RDI, Address(R10, LINEARRAY_ELEMS_OFFSET));
        x64->op(REPECMPSW);  // no flags set if RCX=0
        x64->op(POPQ, RDI);
        x64->op(POPQ, RSI);

        x64->op(CMPQ, RCX, 0);  // equal, if all compared, ZF as expected
        
        x64->code_label(done);

        x64->op(POPQ, RCX);
        x64->op(POPQ, RAX);
        
        x64->op(RET);
    }

    virtual void compare(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        if (s.where == MEMORY && t.where == MEMORY) {
            if (s.address.base == RSP || t.address.base == RSP) {
                x64->op(MOVQ, R10, s.address);
                x64->op(MOVQ, R11, t.address);
                x64->op(PUSHQ, R10);
                x64->op(PUSHQ, R11);
            }
            else {
                x64->op(PUSHQ, s.address);
                x64->op(PUSHQ, t.address);
            }
        }
        else if ((s.where != STACK && s.where != BSTACK) || (t.where != STACK && t.where != BSTACK))
            throw INTERNAL_ERROR;

        Label strcmp_label = x64->once->compile(compile_stringcmp);
        x64->op(CALL, strcmp_label);  // R10B, flags as expected
        
        if (s.where == MEMORY && t.where == MEMORY) {
            x64->op(LEA, RSP, Address(RSP, 2 * ADDRESS_SIZE));  // preserve ZF
        }
    }

    static void compile_stringcmp(Label label, X64 *x64) {
        // Expects arguments on the stack, returns R10B/flags.
        x64->code_label_local(label, "String__comparison");
        
        x64->op(PUSHQ, RAX);
        x64->op(PUSHQ, RCX);
        x64->op(PUSHQ, RDX);
        x64->op(PUSHQ, RSI);
        x64->op(PUSHQ, RDI);
        
        Label s_longer, begin, end;
        x64->op(MOVQ, RAX, Address(RSP, 56));  // s
        x64->op(MOVQ, RDX, Address(RSP, 48));  // t
        
        x64->op(MOVB, R10B, 0);  // assume equality
        x64->op(MOVQ, RCX, Address(RAX, LINEARRAY_LENGTH_OFFSET));
        x64->op(CMPQ, RCX, Address(RDX, LINEARRAY_LENGTH_OFFSET));
        x64->op(JE, begin);
        x64->op(JA, s_longer);
        
        x64->op(MOVB, R10B, -1);  // s is shorter, on common equality s is less
        x64->op(JMP, begin);

        x64->code_label(s_longer);
        x64->op(MOVB, R10B, 1);  // s is longer, on common equality s is greater
        x64->op(MOVQ, RCX, Address(RDX, LINEARRAY_LENGTH_OFFSET));
        
        x64->code_label(begin);
        x64->op(LEA, RSI, Address(RAX, LINEARRAY_ELEMS_OFFSET));
        x64->op(LEA, RDI, Address(RDX, LINEARRAY_ELEMS_OFFSET));
        x64->op(CMPB, R10B, R10B);  // only to initialize flags for equality
        x64->op(REPECMPSW);  // no flags set if RCX=0
        
        x64->op(JE, end);  // common part was equal, result is according to preset R10B
        
        x64->runtime->r10bcompar(true);  // set R10B according to the detected difference
        
        x64->code_label(end);
        x64->op(CMPB, R10B, 0);  // must set flags even if R10B was preset
        
        x64->op(POPQ, RDI);
        x64->op(POPQ, RSI);
        x64->op(POPQ, RDX);
        x64->op(POPQ, RCX);
        x64->op(POPQ, RAX);
        x64->op(RET);
    }

    virtual void streamify(TypeMatch tm, X64 *x64) {
        // Escaped and quoted
        Address alias_addr(RSP, 0);
        Label st_label = x64->once->compile(compile_esc_streamification);

        streamify_ascii("\"", alias_addr, x64);
        
        x64->op(CALL, st_label);  // clobbers all

        streamify_ascii("\"", alias_addr, x64);
    }

    static void compile_esc_streamification(Label label, X64 *x64) {
        // RAX - target array, RCX - size, R10 - source array, R11 - alias
        Label char_str_label = x64->once->compile(CharacterType::compile_str_streamification);
        Label loop, check;
        Address value_addr(RSP, RIP_SIZE + ALIAS_SIZE);
        Address alias_addr(RSP, RIP_SIZE);
        
        x64->code_label_local(label, "String__esc_streamification");
        
        x64->op(MOVQ, RCX, 0);
        x64->op(JMP, check);
        
        x64->code_label(loop);
        x64->op(PUSHQ, RCX);
        x64->op(MOVW, R10W, Address(RAX, RCX, Address::SCALE_2, LINEARRAY_ELEMS_OFFSET));
        x64->op(PUSHQ, R10);
        x64->op(PUSHQ, 0);
        x64->op(PUSHQ, alias_addr + 3 * ADDRESS_SIZE);
        x64->op(CALL, char_str_label);  // clobbers all
        x64->op(ADDQ, RSP, ADDRESS_SIZE + ALIAS_SIZE);
        x64->op(POPQ, RCX);
        x64->op(INCQ, RCX);
        
        x64->code_label(check);
        x64->op(MOVQ, RAX, value_addr);  // reference to the string
        x64->op(CMPQ, RCX, Address(RAX, LINEARRAY_LENGTH_OFFSET));
        x64->op(JB, loop);
        
        x64->op(RET);
    }

    virtual Value *lookup_initializer(TypeMatch tm, std::string n, Scope *scope) {
        std::cerr << "No String initializer " << n << "!\n";
        return NULL;
    }

    virtual Value *lookup_matcher(TypeMatch tm, std::string n, Value *pivot, Scope *scope) {
        if (n == "re")
            return make<StringRegexpMatcherValue>(pivot, tm);
            
        return RecordType::lookup_matcher(tm, n, pivot, scope);
    }
};


class SliceType: public RecordType {
public:
    SliceType(std::string n)
        :RecordType(n, Metatypes { value_metatype }) {
    }

    virtual Value *lookup_initializer(TypeMatch tm, std::string n, Scope *scope) {
        if (n == "empty")
            return make<SliceEmptyValue>(tm);
        else if (n == "all")
            return make<SliceAllValue>(tm);
        
        std::cerr << "No Slice initializer " << n << "!\n";
        return NULL;
    }
};


class ItemType: public RecordType {
public:
    ItemType(std::string n)
        :RecordType(n, Metatypes { value_metatype, value_metatype }) {
    }

    virtual unsigned comparable_member_count() {
        return 1;
    }
};


