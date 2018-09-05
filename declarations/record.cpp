
class RecordType: public Type {
public:
    std::vector<Variable *> member_variables;
    std::vector<TypeSpec> member_tss;  // rvalues, for the initializer arguments
    std::vector<std::string> member_names;

    RecordType(std::string n, Metatypes param_metatypes)
        :Type(n, param_metatypes, record_metatype) {
    }

    virtual bool complete_type() {
        inner_scope->add(make_record_compare());

        for (auto &c : inner_scope->contents) {
            Variable *v = ptr_cast<Variable>(c.get());
            
            if (v) {
                member_variables.push_back(v);
                member_tss.push_back(v->alloc_ts.rvalue());
                member_names.push_back(v->name);
            }
        }
        
        std::cerr << "Record " << name << " has " << member_variables.size() << " member variables.\n";
        return true;
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
            Type::create(tm, s, t, x64);
        }
    }

    virtual void destroy(TypeMatch tm, Storage s, X64 *x64) {
        if (s.where == MEMORY) {
            for (auto &var : member_variables)  // FIXME: reverse!
                var->destroy(tm, Storage(MEMORY, s.address), x64);
        }
        else
            Type::destroy(tm, s, x64);
    }

    virtual StorageWhere where(TypeMatch tm, AsWhat as_what) {
        return (
            as_what == AS_VALUE ? STACK :
            as_what == AS_VARIABLE ? MEMORY :
            as_what == AS_ARGUMENT ? MEMORY :
            as_what == AS_PIVOT_ARGUMENT ? ALIAS :
            as_what == AS_LVALUE_ARGUMENT ? ALIAS :
            throw INTERNAL_ERROR
        );
    }
        
    virtual unsigned comparable_member_count() {
        return member_variables.size();
    }

    virtual void equal(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        if (s.where == MEMORY && t.where == MEMORY) {
            Label end;
            if ((s.regs() | t.regs()) & EQUAL_CLOB)
                throw INTERNAL_ERROR;
            
            for (unsigned i = 0; i < comparable_member_count(); i++) {
                member_variables[i]->equal(tm, s, t, x64);
                x64->op(JNE, end);
            }
            
            x64->code_label(end);
            return;
        }
        else
            throw INTERNAL_ERROR;
    }
    
    virtual void compare(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        if (s.where == MEMORY && t.where == MEMORY) {
            if ((s.regs() | t.regs()) & COMPARE_CLOB)
                throw INTERNAL_ERROR;
                
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

    virtual Value *lookup_initializer(TypeMatch tm, std::string n) {
        //TypeSpec ts(tsi);

        // NOTE: initializers must only appear in code scopes, and there all types
        // must be concrete, not having free parameters. Also, the automatic variable is
        // put in the local scope, so there will be no pivot for it to derive any
        // type parameters from. 

        if (n == "{}") {
            // Anonymous initializer
            return make<RecordInitializerValue>(tm);
        }
        else {
            // Named initializer
            Value *pre = make<RecordPreinitializerValue>(tm[0]);

            Value *value = inner_scope->lookup(n, pre);

            // FIXME: check if the method is Void!
            if (value)
                return make<RecordPostinitializerValue>(value);
            
            std::cerr << "Can't initialize record as " << n << "!\n";
            return NULL;
        }
    }

    virtual Value *lookup_matcher(TypeMatch tm, std::string n, Value *pivot) {
        std::cerr << "No Record matchers yet!\n";
        throw INTERNAL_ERROR;
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
};


class StringType: public RecordType {
public:
    StringType(std::string n)
        :RecordType(n, Metatypes {}) {
    }

    virtual void equal(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        Label streq_label = x64->once->compile(compile_stringeq);
        
        if (s.where == MEMORY && t.where == MEMORY) {
            x64->op(MOVQ, RBX, t.address);  // may be RSP-based
            x64->op(PUSHQ, s.address);
            x64->op(PUSHQ, RBX);
            
            x64->op(CALL, streq_label);  // ZF as expected
            
            x64->op(LEA, RSP, Address(RSP, 2 * ADDRESS_SIZE));  // preserve ZF
        }
        else
            throw INTERNAL_ERROR;
    }
    
    static void compile_stringeq(Label label, X64 *x64) {
        x64->code_label_local(label, "stringeq");
        Label sete, done;

        x64->op(PUSHQ, RAX);
        x64->op(PUSHQ, RCX);
        
        x64->op(MOVQ, RAX, Address(RSP, 32));
        x64->op(MOVQ, RBX, Address(RSP, 24));
        
        x64->op(CMPQ, RAX, RBX);
        x64->op(JE, done);  // identical, must be equal, ZF as expected
        
        x64->op(MOVQ, RCX, Address(RAX, ARRAY_LENGTH_OFFSET));
        x64->op(CMPQ, RCX, Address(RBX, ARRAY_LENGTH_OFFSET));
        x64->op(JNE, done);  // different length, can't be equal, ZF as expected
        
        x64->op(PUSHQ, RSI);
        x64->op(PUSHQ, RDI);
        x64->op(LEA, RSI, Address(RAX, ARRAY_ELEMS_OFFSET));
        x64->op(LEA, RDI, Address(RBX, ARRAY_ELEMS_OFFSET));
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
        Label strcmp_label = x64->once->compile(compile_stringcmp);

        if (s.where == MEMORY && t.where == MEMORY) {
            x64->op(MOVQ, RBX, t.address);  // may be RSP-based
            x64->op(PUSHQ, s.address);
            x64->op(PUSHQ, RBX);
            
            x64->op(CALL, strcmp_label);  // BL, flags as expected
            
            x64->op(LEA, RSP, Address(RSP, 2 * ADDRESS_SIZE));  // preserve flags
        }
        else
            throw INTERNAL_ERROR;
    }

    static void compile_stringcmp(Label label, X64 *x64) {
        // Expects arguments on the stack, returns BL/flags.
        x64->code_label_local(label, "stringcmp");
        
        x64->op(PUSHQ, RAX);
        x64->op(PUSHQ, RCX);
        x64->op(PUSHQ, RDX);
        x64->op(PUSHQ, RSI);
        x64->op(PUSHQ, RDI);
        
        Label s_longer, begin, end;
        x64->op(MOVQ, RAX, Address(RSP, 56));  // s
        x64->op(MOVQ, RDX, Address(RSP, 48));  // t
        
        x64->op(MOVB, BL, 0);  // assume equality
        x64->op(MOVQ, RCX, Address(RAX, ARRAY_LENGTH_OFFSET));
        x64->op(CMPQ, RCX, Address(RDX, ARRAY_LENGTH_OFFSET));
        x64->op(JE, begin);
        x64->op(JA, s_longer);
        
        x64->op(MOVB, BL, -1);  // s is shorter, on common equality s is less
        x64->op(JMP, begin);

        x64->code_label(s_longer);
        x64->op(MOVB, BL, 1);  // s is longer, on common equality s is greater
        x64->op(MOVQ, RCX, Address(RDX, ARRAY_LENGTH_OFFSET));
        
        x64->code_label(begin);
        x64->op(LEA, RSI, Address(RAX, ARRAY_ELEMS_OFFSET));
        x64->op(LEA, RDI, Address(RDX, ARRAY_ELEMS_OFFSET));
        x64->op(CMPB, BL, BL);  // only to initialize flags for equality
        x64->op(REPECMPSW);  // no flags set if RCX=0
        
        x64->op(JE, end);  // common part was equal, result is according to preset BL
        
        x64->blcompar(true);  // set BL according to the detected difference
        
        x64->code_label(end);
        x64->op(CMPB, BL, 0);  // must set flags even if BL was preset
        
        x64->op(POPQ, RDI);
        x64->op(POPQ, RSI);
        x64->op(POPQ, RDX);
        x64->op(POPQ, RCX);
        x64->op(POPQ, RAX);
        x64->op(RET);
    }

    virtual void streamify(TypeMatch tm, bool repr, X64 *x64) {
        // TODO: and escape everything if repr
        
        if (repr) {
            x64->op(PUSHQ, 34);  // double quote
            x64->op(PUSHQ, Address(RSP, 8));
            x64->op(CALL, x64->once->compile(CharacterType::compile_streamification));
            x64->op(ADDQ, RSP, 16);
        }
        
        Label ss_label = x64->once->compile(compile_streamification);
        x64->op(CALL, ss_label);

        if (repr) {
            x64->op(PUSHQ, 34);  // double quote
            x64->op(PUSHQ, Address(RSP, 8));
            x64->op(CALL, x64->once->compile(CharacterType::compile_streamification));
            x64->op(ADDQ, RSP, 16);
        }
    }

    static void compile_streamification(Label label, X64 *x64) {
        // RAX - target array, RBX - tmp, RCX - size, RDX - source array, RDI - alias
        Label preappend_array = x64->once->compile(compile_array_preappend, CHARACTER_TS);
        
        x64->code_label_local(label, "string_streamification");
        
        x64->op(MOVQ, RDI, Address(RSP, ADDRESS_SIZE));  // alias to the stream reference
        x64->op(MOVQ, RDX, Address(RSP, ADDRESS_SIZE + ALIAS_SIZE));  // reference to the string
        
        x64->op(MOVQ, RAX, Address(RDI, 0));
        x64->op(MOVQ, RBX, Address(RDX, ARRAY_LENGTH_OFFSET));

        x64->op(CALL, preappend_array);
        
        x64->op(MOVQ, Address(RDI, 0), RAX);  // RDI no longer needed

        x64->op(LEA, RDI, Address(RAX, ARRAY_ELEMS_OFFSET));
        x64->op(ADDQ, RDI, Address(RAX, ARRAY_LENGTH_OFFSET));
        x64->op(ADDQ, RDI, Address(RAX, ARRAY_LENGTH_OFFSET));  // Yes, added twice

        x64->op(LEA, RSI, Address(RDX, ARRAY_ELEMS_OFFSET));
        x64->op(MOVQ, RCX, Address(RDX, ARRAY_LENGTH_OFFSET));
        x64->op(ADDQ, Address(RAX, ARRAY_LENGTH_OFFSET), RCX);
        x64->op(SHLQ, RCX, 1);
        
        x64->op(REPMOVSB);
        
        x64->op(RET);
    }

    virtual Value *lookup_initializer(TypeMatch tm, std::string n) {
        if (n == "empty") {
            return make<StringLiteralValue>("");
        }
        
        std::cerr << "No String initializer " << n << "!\n";
        return NULL;
    }

    virtual Value *lookup_matcher(TypeMatch tm, std::string n, Value *pivot) {
        if (n == "re")
            return make<StringRegexpMatcherValue>(pivot, tm);
            
        std::cerr << "Can't match String as " << n << "!\n";
        return NULL;
    }
};


class SliceType: public RecordType {
public:
    SliceType(std::string n)
        :RecordType(n, Metatypes { value_metatype }) {
    }

    virtual Value *lookup_initializer(TypeMatch tm, std::string n) {
        if (n == "empty")
            return make<SliceEmptyValue>(tm);
        else if (n == "all")
            return make<SliceAllValue>(tm);
        
        std::cerr << "No Slice initializer " << n << "!\n";
        return NULL;
    }

    virtual Value *lookup_matcher(TypeMatch tm, std::string n, Value *pivot) {
        std::cerr << "Can't match Slice as " << n << "!\n";
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


class AutoweakrefType: public RecordType {
public:
    AutoweakrefType(std::string n)
        :RecordType(n, Metatypes { identity_metatype }) {
    }

    virtual Value *lookup_initializer(TypeMatch tm, std::string n) {
        TypeSpec member_ts = tm[1].prefix(weakanchorage_type).prefix(ref_type);
        
        Value *v = member_ts.lookup_initializer(n);
        if (v) 
            return make<RecordWrapperValue>(v, NO_TS, tm[0], "", "");
        
        std::cerr << "No Autoweakref initializer " << n << "!\n";
        return NULL;
    }
    
    virtual Value *lookup_matcher(TypeMatch tm, std::string n, Value *p) {
        TypeSpec member_ts = tm[1].prefix(weakanchorage_type).prefix(ref_type);
        p = make<RecordUnwrapValue>(member_ts, p);
        p = make<ReferenceBorrowValue>(p, tm);
        
        Value *v = member_ts.reprefix(ref_type, ptr_type).lookup_matcher(n, p);
        if (v)
            return v;
        
        std::cerr << "No Autoweakref matcher " << n << "!\n";
        return NULL;
    }
};
