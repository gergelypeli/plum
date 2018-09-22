
class BasicType: public Type {
public:
    unsigned size;
    int os;
    bool is_unsigned;

    BasicType(std::string n, unsigned s, bool iu, Type *mt = NULL)
        :Type(n, {}, mt ? mt : value_metatype) {
        size = s;
        os = (s == 1 ? 0 : s == 2 ? 1 : s == 4 ? 2 : s == 8 ? 3 : throw INTERNAL_ERROR);        
        is_unsigned = iu;
    }
    
    virtual Allocation measure(TypeMatch tm) {
        return Allocation(size);
    }

    virtual void store(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        // Only RBX is usable as scratch
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
            x64->op(bitset(s.cc), BL);
            x64->op(PUSHQ, RBX);
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
            x64->op(POPQ, RBX);
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
                x64->op(POPQ, RBX);
                x64->op(mov, t.address, RBX);
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
                x64->op(mov, RBX, s.address);
                x64->op(PUSHQ, RBX);
            }
            return;
        case MEMORY_MEMORY:
            x64->op(mov, RBX, s.address);
            x64->op(mov, t.address, RBX);
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
                x64->op(POPQ, RBX);
                x64->op(mov, t.address, RBX);
            }
            return;
        case MEMORY_MEMORY:
            x64->op(mov, RBX, s.address);
            x64->op(mov, t.address, RBX);
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
        // Only RBX is usable as scratch
        BinaryOp MOV = MOVQ % os;
        BinaryOp CMP = CMPQ % os;
        
        switch (s.where * t.where) {
        case CONSTANT_CONSTANT:
            x64->op(MOV, RBX, s.value);
            x64->op(CMP, RBX, t.value);
            break;
        case CONSTANT_REGISTER:
            x64->op(MOV, RBX, s.value);
            x64->op(CMP, RBX, t.reg);
            break;
        case CONSTANT_MEMORY:
            x64->op(MOV, RBX, s.value);
            x64->op(CMP, RBX, t.address);
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
            x64->op(MOV, RBX, s.address);
            x64->op(CMP, RBX, t.address);
            break;
            
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual void compare(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        equal(tm, s, t, x64);
        x64->blcompar(is_unsigned);
    }
    
    virtual StorageWhere where(TypeMatch tm, AsWhat as_what) {
        return (
            as_what == AS_VALUE ? REGISTER :
            as_what == AS_VARIABLE ? MEMORY :
            as_what == AS_ARGUMENT ? MEMORY :
            as_what == AS_PIVOT_ARGUMENT ? MEMORY :
            as_what == AS_LVALUE_ARGUMENT ? ALIAS :
            throw INTERNAL_ERROR
        );
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
    
    virtual void streamify(TypeMatch tm, bool repr, X64 *x64) {
        // SysV
        Label label;
        
        if (is_unsigned) {
            x64->code_label_import(label, "streamify_unteger");
            
            if (size == 1)
                x64->op(MOVZXBQ, RDI, Address(RSP, ALIAS_SIZE));
            else if (size == 2)
                x64->op(MOVZXWQ, RDI, Address(RSP, ALIAS_SIZE));
            else if (size == 4)
                x64->op(MOVZXDQ, RDI, Address(RSP, ALIAS_SIZE));
            else if (size == 8)
                x64->op(MOVQ, RDI, Address(RSP, ALIAS_SIZE));
            else
                throw INTERNAL_ERROR;
        }
        else {
            x64->code_label_import(label, "streamify_integer");

            if (size == 1)
                x64->op(MOVSXBQ, RDI, Address(RSP, ALIAS_SIZE));
            else if (size == 2)
                x64->op(MOVSXWQ, RDI, Address(RSP, ALIAS_SIZE));
            else if (size == 4)
                x64->op(MOVSXDQ, RDI, Address(RSP, ALIAS_SIZE));
            else if (size == 8)
                x64->op(MOVQ, RDI, Address(RSP, ALIAS_SIZE));
            else
                throw INTERNAL_ERROR;
        }
        
        x64->op(MOVQ, RSI, Address(RSP, 0));
        x64->runtime->call_sysv(label);
    }

    DataScope *get_inner_scope() {
        return integer_metatype->get_inner_scope();
    }
};


class BooleanType: public BasicType {
public:
    BooleanType(std::string n, unsigned s)
        :BasicType(n, s, true) {
        //make_inner_scope(TypeSpec { this });
    }

    virtual void streamify(TypeMatch tm, bool repr, X64 *x64) {
        // SysV
        x64->op(MOVQ, RDI, Address(RSP, ALIAS_SIZE));
        x64->op(MOVQ, RSI, Address(RSP, 0));
        
        Label label;
        x64->code_label_import(label, "streamify_boolean");
        x64->runtime->call_sysv(label);
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
        //make_inner_scope(TypeSpec { this });
    }

    virtual void streamify(TypeMatch tm, bool repr, X64 *x64) {
        Label cs_label = x64->once->compile(compile_streamification);

        if (repr) {
            x64->op(PUSHQ, 39);  // single quote
            x64->op(PUSHQ, Address(RSP, 8));
            x64->op(CALL, cs_label);
            x64->op(ADDQ, RSP, 16);
        }

        x64->op(CALL, cs_label);

        if (repr) {
            x64->op(PUSHQ, 39);  // single quote
            x64->op(PUSHQ, Address(RSP, 8));
            x64->op(CALL, cs_label);
            x64->op(ADDQ, RSP, 16);
        }
    }
    
    static void compile_streamification(Label label, X64 *x64) {
        // RAX - target array, RBX - tmp, RCX - size, RDX - source character, RDI - alias
        Label preappend_array = x64->once->compile(compile_array_preappend, CHARACTER_TS);

        x64->code_label_local(label, "character_streamification");

        x64->op(MOVQ, RDI, Address(RSP, ADDRESS_SIZE));  // alias to the stream reference
        x64->op(MOVQ, RDX, Address(RSP, ADDRESS_SIZE + ALIAS_SIZE));  // the character

        x64->op(MOVQ, RAX, Address(RDI, 0));
        x64->op(MOVQ, RBX, 1);
        
        x64->op(CALL, preappend_array);
        
        x64->op(MOVQ, Address(RDI, 0), RAX);  // RDI no longer needed

        x64->op(LEA, RDI, Address(RAX, ARRAY_ELEMS_OFFSET));
        x64->op(ADDQ, RDI, Address(RAX, ARRAY_LENGTH_OFFSET));
        x64->op(ADDQ, RDI, Address(RAX, ARRAY_LENGTH_OFFSET));  // Yes, added twice

        x64->op(MOVW, Address(RDI, 0), DX);
            
        x64->op(ADDQ, Address(RAX, ARRAY_LENGTH_OFFSET), 1);

        x64->op(RET);
    }

    virtual Value *lookup_initializer(TypeMatch tm, std::string name, Scope *scope) {
        if (name == "zero")
            return make<BasicValue>(tm[0], 0);
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

    EnumerationType(std::string n, std::vector<std::string> kw, Type *mt = NULL)
        :BasicType(n, 1, true, mt ? mt : enumeration_metatype) {  // TODO: different sizes based on the keyword count!
        keywords = kw;
    }

    virtual void streamify(TypeMatch tm, bool repr, X64 *x64) {
        Label es_label = x64->once->compile(compile_streamification);

        x64->op(LEA, RBX, Address(get_stringifications_label(x64), 0));  // table start
        x64->op(CALL, es_label);
    }
    
    static void compile_streamification(Label label, X64 *x64) {
        // RAX - target array, RBX - table start, RCX - size, RDX - source enum, RDI - alias
        Label preappend_array = x64->once->compile(compile_array_preappend, CHARACTER_TS);

        x64->code_label_local(label, "enum_streamification");

        x64->op(MOVQ, RDI, Address(RSP, ADDRESS_SIZE));  // alias to the stream reference
        x64->op(MOVQ, RDX, Address(RSP, ADDRESS_SIZE + ALIAS_SIZE));  // the enum

        // Find the string for this enum value
        x64->op(ANDQ, RDX, 0xFF);
        x64->op(MOVQ, RDX, Address(RBX, RDX, ADDRESS_SIZE, 0));
            
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
    
    DataScope *get_inner_scope() {
        return enumeration_metatype->get_inner_scope();
    }

    virtual Label get_stringifications_label(X64 *x64) {
        return x64->once->compile(compile_stringifications, TypeSpec { this });
    }
    
    static void compile_stringifications(Label label, TypeSpec ts, X64 *x64) {
        EnumerationType *t = ptr_cast<EnumerationType>(ts[0]);
        std::vector<Label> labels;
        
        for (auto &keyword : t->keywords) 
            labels.push_back(x64->runtime->data_heap_string(decode_utf8(keyword)));
            
        x64->data_label_local(label, t->name + "_stringifications");
        
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
        
        return NULL;
    }
    
    DataScope *get_inner_scope() {
        return treenumeration_metatype->get_inner_scope();
    }

    virtual Label get_parents_label(X64 *x64) {
        return x64->once->compile(compile_parents, TypeSpec { this });
    }
    
    static void compile_parents(Label label, TypeSpec ts, X64 *x64) {
        TreenumerationType *t = ptr_cast<TreenumerationType>(ts[0]);
        x64->data_label_local(label, t->name + "_parents");
        
        for (unsigned p : t->parents)
            x64->data_byte(p);
    }
};
