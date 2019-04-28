
class BasicType: public Type {
public:
    unsigned size;
    int os;
    bool is_unsigned;

    BasicType(std::string n, unsigned s, bool iu, MetaType *mt = NULL)
        :Type(n, {}, mt ? mt : value_metatype) {
        size = s;
        os = (s == 1 ? 0 : s == 2 ? 1 : s == 4 ? 2 : s == 8 ? 3 : throw INTERNAL_ERROR);        
        is_unsigned = iu;
    }
    
    virtual Allocation measure(TypeMatch tm) {
        return Allocation(size);
    }

    virtual void store(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        // Only R10 is usable as scratch
        BinaryOp mov = MOVQ % os;
        
        switch (s.where * t.where) {
        case NOWHERE_REGISTER:  // this is used by the boolean and operation
            x64->op(mov, t.reg, 0);
            return;
        case NOWHERE_STACK:  // this is used by pushing optional function arguments
            x64->op(PUSHQ, 0);
            return;

        case CONSTANT_NOWHERE:
            return;
        case CONSTANT_CONSTANT:
            return;
        case CONSTANT_REGISTER:
            x64->op(mov, t.reg, s.value);
            return;
        case CONSTANT_STACK:
            x64->op(PUSHQ, s.value);
            return;
        case CONSTANT_MEMORY:
            x64->op(mov, t.address, s.value);
            return;
            
        case FLAGS_NOWHERE:
            return;
        case FLAGS_FLAGS:
            return;
        case FLAGS_REGISTER:
            x64->op(bitset(s.cc), t.reg);
            return;
        case FLAGS_STACK:
            x64->op(bitset(s.cc), R10B);
            x64->op(PUSHQ, R10);
            return;
        case FLAGS_MEMORY:
            x64->op(bitset(s.cc), t.address);
            return;

        case REGISTER_NOWHERE:
            return;
        case REGISTER_REGISTER:
            if (s.reg != t.reg)
                x64->op(mov, t.reg, s.reg);
            return;
        case REGISTER_STACK:
            x64->op(PUSHQ, s.reg);
            return;
        case REGISTER_MEMORY:
            x64->op(mov, t.address, s.reg);
            return;

        case STACK_NOWHERE:
            x64->op(POPQ, R10);
            return;
        case STACK_REGISTER:
            x64->op(POPQ, t.reg);
            return;
        case STACK_STACK:
            return;
        case STACK_MEMORY:
            if (size == INTEGER_SIZE)
                x64->op(POPQ, t.address);
            else {
                x64->op(POPQ, R10);
                x64->op(mov, t.address, R10);
            }
            return;

        case MEMORY_NOWHERE:
            return;
        case MEMORY_REGISTER:
            x64->op(mov, t.reg, s.address);
            return;
        case MEMORY_STACK:
            if (size == INTEGER_SIZE)
                x64->op(PUSHQ, s.address);
            else {
                x64->op(mov, R10, s.address);
                x64->op(PUSHQ, R10);
            }
            return;
        case MEMORY_MEMORY:
            x64->op(mov, R10, s.address);
            x64->op(mov, t.address, R10);
            return;
        
        default:
            Type::store(tm, s, t, x64);
        }
    }

    virtual void create(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        BinaryOp mov = MOVQ % os;
        
        switch (s.where * t.where) {
        case NOWHERE_MEMORY:
            x64->op(mov, t.address, 0);
            return;
        case CONSTANT_MEMORY:
            x64->op(mov, t.address, s.value);
            return;
        case FLAGS_MEMORY:
            x64->op(bitset(s.cc), t.address);
            return;
        case REGISTER_MEMORY:
            x64->op(mov, t.address, s.reg);
            return;
        case STACK_MEMORY:
            if (size == INTEGER_SIZE)
                x64->op(POPQ, t.address);
            else {
                x64->op(POPQ, R10);
                x64->op(mov, t.address, R10);
            }
            return;
        case MEMORY_MEMORY:
            x64->op(mov, R10, s.address);
            x64->op(mov, t.address, R10);
            return;
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
        // No need to take care of STACK here, GenericOperationValue takes care of it
        // Only R10 is usable as scratch
        BinaryOp MOV = MOVQ % os;
        BinaryOp CMP = CMPQ % os;
        
        switch (s.where * t.where) {
        case CONSTANT_CONSTANT:
            x64->op(MOV, R10, s.value);
            x64->op(CMP, R10, t.value);
            break;
        case CONSTANT_REGISTER:
            x64->op(MOV, R10, s.value);
            x64->op(CMP, R10, t.reg);
            break;
        case CONSTANT_MEMORY:
            x64->op(MOV, R10, s.value);
            x64->op(CMP, R10, t.address);
            break;

        case REGISTER_CONSTANT:
            x64->op(CMP, s.reg, t.value);
            break;
        case REGISTER_REGISTER:
            x64->op(CMP, s.reg, t.reg);
            break;
        case REGISTER_MEMORY:
            x64->op(CMP, s.reg, t.address);
            break;

        case MEMORY_CONSTANT:
            x64->op(CMP, s.address, t.value);
            break;
        case MEMORY_REGISTER:
            x64->op(CMP, s.address, t.reg);
            break;
        case MEMORY_MEMORY:
            x64->op(MOV, R10, s.address);
            x64->op(CMP, R10, t.address);
            break;
            
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual void compare(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        equal(tm, s, t, x64);
        x64->runtime->r10bcompar(is_unsigned);
    }
    
    virtual StorageWhere where(TypeMatch tm, AsWhat as_what) {
        return (
            as_what == AS_VALUE ? REGISTER :
            as_what == AS_VARIABLE ? MEMORY :
            as_what == AS_ARGUMENT ? MEMORY :
            //as_what == AS_PIVOT_ARGUMENT ? MEMORY :
            as_what == AS_LVALUE_ARGUMENT ? ALIAS :
            throw INTERNAL_ERROR
        );
    }

    virtual Storage optimal_value_storage(TypeMatch tm, Regs preferred) {
        Register r = preferred.get_any();
        return Storage(REGISTER, r);
    }

    virtual bool get_unsigned() {
        return is_unsigned;
    }
};


class IntegerType: public BasicType {
public:
    IntegerType(std::string n, unsigned s, bool iu)
        :BasicType(n, s, iu, integer_metatype) {
    }
    
    virtual void streamify(TypeMatch tm, X64 *x64) {
        // SysV
        Address value_addr(RSP, ALIAS_SIZE);
        Address alias_addr(RSP, 0);
        
        if (is_unsigned) {
            if (size == 1)
                x64->op(MOVZXBQ, RDI, value_addr);
            else if (size == 2)
                x64->op(MOVZXWQ, RDI, value_addr);
            else if (size == 4)
                x64->op(MOVZXDQ, RDI, value_addr);
            else if (size == 8)
                x64->op(MOVQ, RDI, value_addr);
            else
                throw INTERNAL_ERROR;

            x64->op(MOVQ, RSI, alias_addr);
            x64->runtime->call_sysv(x64->runtime->sysv_streamify_unteger_label);
        }
        else {
            if (size == 1)
                x64->op(MOVSXBQ, RDI, value_addr);
            else if (size == 2)
                x64->op(MOVSXWQ, RDI, value_addr);
            else if (size == 4)
                x64->op(MOVSXDQ, RDI, value_addr);
            else if (size == 8)
                x64->op(MOVQ, RDI, value_addr);
            else
                throw INTERNAL_ERROR;

            x64->op(MOVQ, RSI, alias_addr);
            x64->runtime->call_sysv(x64->runtime->sysv_streamify_integer_label);
        }
    }
};


class BooleanType: public BasicType {
public:
    BooleanType(std::string n, unsigned s)
        :BasicType(n, s, true) {
    }

    virtual void streamify(TypeMatch tm, X64 *x64) {
        // SysV
        Address value_addr(RSP, ALIAS_SIZE);
        Address alias_addr(RSP, 0);

        x64->op(MOVQ, RDI, value_addr);
        x64->op(MOVQ, RSI, alias_addr);
        x64->runtime->call_sysv(x64->runtime->sysv_streamify_boolean_label);
    }

    virtual Value *lookup_initializer(TypeMatch tm, std::string name, Scope *scope) {
        if (name == "false")
            return make<BasicValue>(tm[0], 0);
        else if (name == "true")
            return make<BasicValue>(tm[0], 1);
        else {
            std::cerr << "No Boolean initializer called " << name << "!\n";
            return NULL;
        }
    }
};


class CharacterType: public BasicType {
public:
    CharacterType(std::string n, unsigned s)
        :BasicType(n, s, true) {
    }

    virtual void streamify(TypeMatch tm, X64 *x64) {
        // Escaped quoted
        Label esc_label = x64->once->compile(compile_esc_streamification);
        x64->op(CALL, esc_label);  // clobbers all
    }

    static void insert_pre_streamification(X64 *x64) {
        Address value_addr(RSP, RIP_SIZE + ALIAS_SIZE);
        Address alias_addr(RSP, RIP_SIZE);

        x64->op(MOVQ, R10, 5);  // worst case will be five character escapes
        stream_preappend2(alias_addr, x64);

        x64->op(MOVZXWQ, R10, value_addr);
        x64->op(MOVQ, RCX, Address(RAX, LINEARRAY_LENGTH_OFFSET));
        x64->op(LEA, RBX, Address(RAX, RCX, Address::SCALE_2, LINEARRAY_ELEMS_OFFSET));  // stream end

        // RAX - stream ref, RBX - stream end, R10 - character
    }

    static void compile_ascii_table(Label label, X64 *x64) {
        x64->data_align(8);
        x64->data_label(label);

        for (unsigned i = 0; i < 128; i++) {
            std::string name = character_name(i);

            unsigned64 x = (
                name.size() == 0 ? i :  // unescaped
                name.size() == 2 ? name[0] | name[1] << 16 :  // two-letter name
                name.size() == 3 ? name[0] | name[1] << 16 | (unsigned64)name[2] << 32 : // three-letter name
                throw INTERNAL_ERROR
            );
            
            x64->data_qword(x);
        }
    }

    static void compile_esc_streamification(Label label, X64 *x64) {
        Label ascii_table_label = x64->once->compile(compile_ascii_table);
        Label unescaped, escaped_two, escaped_three;

        x64->code_label_local(label, "Character__esc_streamification");
        
        insert_pre_streamification(x64);

        x64->op(CMPQ, R10, 128);
        x64->op(JAE, unescaped);
        x64->op(LEA, R11, Address(ascii_table_label, 0));
        x64->op(MOVQ, R10, Address(R11, R10, Address::SCALE_8, 0));
        x64->op(CMPQ, R10, 0xffffff);
        x64->op(JA, escaped_three);
        x64->op(CMPQ, R10, 0xffff);
        x64->op(JA, escaped_two);

        // unescaped character: "X"
        x64->code_label(unescaped);
        x64->op(MOVW, Address(RBX, 0), '"');
        x64->op(MOVW, Address(RBX, 2), R10W);
        x64->op(MOVW, Address(RBX, 4), '"');
        x64->op(ADDQ, Address(RAX, LINEARRAY_LENGTH_OFFSET), 3);
        x64->op(RET);

        // two characters: `XY
        x64->code_label(escaped_two);
        x64->op(MOVW, Address(RBX, 0), '`');
        x64->op(MOVD, Address(RBX, 2), R10D);
        x64->op(ADDQ, Address(RAX, LINEARRAY_LENGTH_OFFSET), 3);
        x64->op(RET);
        
        // three characters: `XYZ
        x64->code_label(escaped_three);
        x64->op(MOVW, Address(RBX, 0), '`');
        x64->op(MOVQ, Address(RBX, 2), R10);
        x64->op(ADDQ, Address(RAX, LINEARRAY_LENGTH_OFFSET), 4);
        x64->op(RET);
    }

    static void compile_str_streamification(Label label, X64 *x64) {
        Label ascii_table_label = x64->once->compile(compile_ascii_table);
        Label unescaped, escaped_two, escaped_three;

        x64->code_label_local(label, "Character__str_streamification");
        
        insert_pre_streamification(x64);

        x64->op(CMPQ, R10, 128);
        x64->op(JAE, unescaped);
        x64->op(LEA, R11, Address(ascii_table_label, 0));
        x64->op(MOVQ, R10, Address(R11, R10, Address::SCALE_8, 0));
        x64->op(CMPQ, R10, 0xffffff);
        x64->op(JA, escaped_three);
        x64->op(CMPQ, R10, 0xffff);
        x64->op(JA, escaped_two);

        // unescaped character: X
        x64->code_label(unescaped);
        x64->op(MOVW, Address(RBX, 0), R10W);
        x64->op(ADDQ, Address(RAX, LINEARRAY_LENGTH_OFFSET), 1);
        x64->op(RET);

        // two characters: {XY}
        x64->code_label(escaped_two);
        x64->op(MOVW, Address(RBX, 0), '{');
        x64->op(MOVD, Address(RBX, 2), R10D);
        x64->op(MOVW, Address(RBX, 6), '}');
        x64->op(ADDQ, Address(RAX, LINEARRAY_LENGTH_OFFSET), 4);
        x64->op(RET);
        
        // three characters: {XYZ}
        x64->code_label(escaped_three);
        x64->op(MOVW, Address(RBX, 0), '{');
        x64->op(MOVQ, Address(RBX, 2), R10);
        x64->op(MOVW, Address(RBX, 8), '}');
        x64->op(ADDQ, Address(RAX, LINEARRAY_LENGTH_OFFSET), 5);
        x64->op(RET);
    }

    virtual Value *lookup_initializer(TypeMatch tm, std::string name, Scope *scope) {
        int cc = character_code(name);
        
        if (cc < 0)
            cc = uni_code(name);
            
        if (cc >= 0)
            return make<BasicValue>(tm[0], cc);
        else if (name == "unicode")
            return make<UnicodeCharacterValue>();
        else {
            std::cerr << "No Character initializer called " << name << "!\n";
            return NULL;
        }
    }
};


class EnumerationType: public BasicType {
public:
    std::vector<std::string> keywords;

    EnumerationType(std::string n, std::vector<std::string> kw, MetaType *mt = NULL)
        :BasicType(n, 1, true, mt ? mt : enumeration_metatype) {  // TODO: different sizes based on the keyword count!
        keywords = kw;
    }

    virtual void streamify(TypeMatch tm, X64 *x64) {
        Label es_label = x64->once->compile(compile_streamification);

        x64->op(LEA, RBX, Address(get_stringifications_label(x64), 0));  // table start
        x64->op(CALL, es_label);  // clobbers all
    }
    
    static void compile_streamification(Label label, X64 *x64) {
        // RAX - target array, RBX - table start, RCX - size, R10 - source enum
        Address value_addr(RSP, RIP_SIZE + ALIAS_SIZE);
        Address alias_addr(RSP, RIP_SIZE);

        x64->code_label_local(label, "enum_streamification");

        x64->op(MOVQ, R10, value_addr);  // the enum

        // Find the string for this enum value
        x64->op(ANDQ, R10, 0xFF);
        x64->op(MOVQ, R10, Address(RBX, R10, Address::SCALE_8, 0));
            
        x64->op(PUSHQ, R10);  // save enum string
            
        x64->op(MOVQ, R10, Address(R10, LINEARRAY_LENGTH_OFFSET));

        stream_preappend2(alias_addr + ADDRESS_SIZE, x64);
        
        x64->op(POPQ, R10);  // enum string

        x64->op(LEA, RDI, Address(RAX, LINEARRAY_ELEMS_OFFSET));
        x64->op(ADDQ, RDI, Address(RAX, LINEARRAY_LENGTH_OFFSET));
        x64->op(ADDQ, RDI, Address(RAX, LINEARRAY_LENGTH_OFFSET));  // Yes, added twice (CHARACTER_SIZE)

        x64->op(LEA, RSI, Address(R10, LINEARRAY_ELEMS_OFFSET));
        x64->op(MOVQ, RCX, Address(R10, LINEARRAY_LENGTH_OFFSET));
        x64->op(ADDQ, Address(RAX, LINEARRAY_LENGTH_OFFSET), RCX);
        x64->op(IMUL3Q, RCX, RCX, CHARACTER_SIZE);
        
        x64->op(REPMOVSB);
        
        x64->op(RET);
    }

    virtual Value *lookup_initializer(TypeMatch tm, std::string n, Scope *scope) {
        for (unsigned i = 0; i < keywords.size(); i++)
            if (keywords[i] == n)
                return make<BasicValue>(tm[0], i);
        
        return NULL;
    }
    
    //DataScope *get_inner_scope() {
    //    return enumeration_metatype->get_inner_scope();
    //}

    virtual Label get_stringifications_label(X64 *x64) {
        return x64->once->compile(compile_stringifications, TypeSpec { this });
    }
    
    static void compile_stringifications(Label label, TypeSpec ts, X64 *x64) {
        EnumerationType *t = ptr_cast<EnumerationType>(ts[0]);
        std::vector<Label> labels;
        
        for (auto &keyword : t->keywords) 
            labels.push_back(x64->runtime->data_heap_string(decode_utf8(keyword)));
            
        x64->data_label_local(label, ts.symbolize("stringifications"));
        
        for (auto &l : labels)
            x64->data_reference(l);  // 64-bit absolute
    }
    
    unsigned get_keyword_index(std::string kw) {
        for (unsigned i = 0; i < keywords.size(); i++)
            if (keywords[i] == kw)
                return i;
        
        throw INTERNAL_ERROR;
    }
};


class TreenumerationType: public EnumerationType {
public:
    std::vector<unsigned> parents;

    TreenumerationType(std::string n, std::vector<std::string> kw, std::vector<unsigned> ps)
        :EnumerationType(n, kw, treenumeration_metatype) {
        if (kw.size() != ps.size())
            throw INTERNAL_ERROR;
            
        // The numbering must start from 1, as 0 is reserved to be the root of all values,
        // and also for NO_EXCEPTION for raise.
        if (kw[0] != "" || ps[0] != 0)
            throw INTERNAL_ERROR;
            
        for (unsigned i = 1; i < ps.size(); i++)
            if (ps[i] >= i) {
                std::cerr << "Invalid treenum!\n";
                throw INTERNAL_ERROR;
            }
            
        parents = ps;
    }
    
    virtual Value *lookup_initializer(TypeMatch tm, std::string n, Scope *scope) {
        for (unsigned i = 0; i < keywords.size(); i++)
            if (keywords[i] == n)
                return make<BasicValue>(tm[0], i);
        
        return NULL;
    }

    virtual Value *lookup_matcher(TypeMatch tm, std::string n, Value *pivot, Scope *scope) {
        for (unsigned i = 0; i < keywords.size(); i++)
            if (keywords[i] == n)
                return make<TreenumerationMatcherValue>(i, pivot);
        
        if (n == "any")
            return make<TreenumerationAnyMatcherValue>(pivot);
        
        return EnumerationType::lookup_matcher(tm, n, pivot, scope);
    }
    
    //DataScope *get_inner_scope() {
    //    return treenumeration_metatype->get_inner_scope();
    //}

    virtual Label get_parents_label(X64 *x64) {
        return x64->once->compile(compile_parents, TypeSpec { this });
    }
    
    static void compile_parents(Label label, TypeSpec ts, X64 *x64) {
        TreenumerationType *t = ptr_cast<TreenumerationType>(ts[0]);
        x64->data_label_local(label, ts.symbolize("parents"));
        
        for (unsigned p : t->parents)
            x64->data_byte(p);
    }
};
