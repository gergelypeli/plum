
class BasicType: public Type {
public:
    unsigned size;
    int os;
    bool is_unsigned;

    BasicType(std::string n, unsigned s, bool iu)
        :Type(n, 0) {
        size = s;
        os = (s == 1 ? 0 : s == 2 ? 1 : s == 4 ? 2 : s == 8 ? 3 : throw INTERNAL_ERROR);        
        is_unsigned = iu;
    }
    
    virtual unsigned measure(TypeSpecIter tsi, StorageWhere where) {
        switch (where) {
        case STACK:
            return stack_size(size);
        case MEMORY:
            return size;
        default:
            return Type::measure(tsi, where);
        }
    }

    virtual void store(TypeSpecIter tsi, Storage s, Storage t, X64 *x64) {
        // Only RBX is usable as scratch
        BinaryOp mov = MOVQ % os;
        
        switch (s.where * t.where) {
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
            x64->op(s.bitset, t.reg);
            return;
        case FLAGS_STACK:
            x64->op(s.bitset, BL);
            x64->op(PUSHQ, RBX);
            return;
        case FLAGS_MEMORY:
            x64->op(s.bitset, t.address);
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
            if (size == 8)
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
            if (size == 8)
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
            Type::store(tsi, s, t, x64);
        }
    }

    virtual void create(TypeSpecIter tsi, Storage s, Storage t, X64 *x64) {
        BinaryOp mov = MOVQ % os;
        
        switch (s.where * t.where) {
        case NOWHERE_MEMORY:
            x64->op(mov, t.address, 0);
            return;
        case NOWHERE_REGISTER:  // this is used by the boolean and operation
            x64->op(mov, t.reg, 0);
            return;
        case NOWHERE_STACK:  // this is used by pushing optional function arguments
            x64->op(PUSHQ, 0);
            return;
        case CONSTANT_MEMORY:
            x64->op(mov, t.address, s.value);
            return;
        case FLAGS_MEMORY:
            x64->op(s.bitset, t.address);
            return;
        case REGISTER_MEMORY:
            x64->op(mov, t.address, s.reg);
            return;
        case STACK_MEMORY:
            if (size == 8)
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

    virtual void destroy(TypeSpecIter tsi, Storage s, X64 *x64) {
        if (s.where == MEMORY)
            ;
        else
            throw INTERNAL_ERROR;
    }

    virtual bool compare(TypeSpecIter tsi, Storage s, Storage t, X64 *x64) {
        // Only RBX is usable as scratch
        BinaryOp MOV = MOVQ % os;
        BinaryOp CMP = CMPQ % os;
        
        switch (s.where * t.where) {
        case CONSTANT_CONSTANT:
            x64->op(MOV, RBX, s.value);
            x64->op(CMP, RBX, t.value);
            return is_unsigned;
        case CONSTANT_REGISTER:
            x64->op(MOV, RBX, s.value);
            x64->op(CMP, RBX, t.reg);
            return is_unsigned;
        case CONSTANT_STACK:
            x64->op(MOV, RBX, s.value);
            x64->op(CMP, RBX, Address(RSP, 0));
            x64->op(POPQ, RBX);
            return is_unsigned;
        case CONSTANT_MEMORY:
            x64->op(MOV, RBX, s.value);
            x64->op(CMP, RBX, t.address);
            return is_unsigned;
            
        case REGISTER_REGISTER:
            x64->op(CMP, s.reg, t.reg);
            return is_unsigned;
        case REGISTER_STACK:
            x64->op(POPQ, RBX);
            x64->op(CMP, s.reg, RBX);
            return is_unsigned;
        case REGISTER_MEMORY:
            x64->op(CMP, s.reg, t.address);
            return is_unsigned;

        case STACK_REGISTER:
            x64->op(POPQ, RBX);
            x64->op(CMP, RBX, t.reg);
            return is_unsigned;
        case STACK_STACK:
            x64->op(POPQ, RBX);
            x64->op(CMP, RBX, Address(RSP, 0));
            x64->op(POPQ, RBX);
            return is_unsigned;
        case STACK_MEMORY:
            x64->op(POPQ, RBX);
            x64->op(CMP, RBX, t.address);
            return is_unsigned;

        case MEMORY_REGISTER:
            x64->op(CMP, s.address, t.reg);
            return is_unsigned;
        case MEMORY_STACK:
            x64->op(POPQ, RBX);
            x64->op(CMP, s.address, RBX);
            return is_unsigned;
        case MEMORY_MEMORY:
            x64->op(MOV, RBX, s.address);
            x64->op(CMP, RBX, t.address);
            return is_unsigned;
        default:
            throw INTERNAL_ERROR;
        }
    }
    
    virtual StorageWhere where(TypeSpecIter tsi, bool is_arg, bool is_lvalue) {
        return (is_arg ? (is_lvalue ? ALIAS : MEMORY) : (is_lvalue ? MEMORY : REGISTER));
    }

    virtual Storage boolval(TypeSpecIter tsi, Storage s, X64 *x64, bool probe) {
        // None of these cases destroy the original value, so they all pass for probing
        
        switch (s.where) {
        case CONSTANT:
            return Storage(CONSTANT, s.value != 0);
        case FLAGS:
            return Storage(FLAGS, s.bitset);
        case REGISTER:
            x64->op(CMPQ % os, s.reg, 0);
            return Storage(FLAGS, SETNE);
        case MEMORY:
            x64->op(CMPQ % os, s.address, 0);
            return Storage(FLAGS, SETNE);
        default:
            throw INTERNAL_ERROR;
        }
    }
    
    virtual bool get_unsigned() {
        return is_unsigned;
    }
};


class IntegerType: public BasicType {
public:
    IntegerType(std::string n, unsigned s, bool iu)
        :BasicType(n, s, iu) {
    }
    
    virtual Scope *get_inner_scope(TypeSpecIter tsi) {
        return integer_metatype->get_inner_scope(tsi);
    }
};


class BooleanType: public BasicType {
public:
    std::unique_ptr<DataScope> inner_scope;
    
    BooleanType(std::string n, unsigned s)
        :BasicType(n, s, true) {
        inner_scope.reset(new DataScope);
        inner_scope->set_pivot_type_hint(TypeSpec { this });
    }

    virtual Value *lookup_initializer(TypeSpecIter tsi, std::string name, Scope *scope) {
        if (name == "false")
            return make_basic_value(TypeSpec(tsi), 0);
        else if (name == "true")
            return make_basic_value(TypeSpec(tsi), 1);
        else {
            std::cerr << "No Boolean initializer called " << name << "!\n";
            return NULL;
        }
    }

    virtual Scope *get_inner_scope(TypeSpecIter tsi) {
        return inner_scope.get();
    }
};


class CharacterType: public BasicType {
public:
    std::unique_ptr<DataScope> inner_scope;
    
    CharacterType(std::string n, unsigned s)
        :BasicType(n, s, true) {
        inner_scope.reset(new DataScope);
        inner_scope->set_pivot_type_hint(TypeSpec { this });
    }

    virtual Value *lookup_initializer(TypeSpecIter tsi, std::string name, Scope *scope) {
        if (name == "zero")
            return make_basic_value(TypeSpec(tsi), 0);
        else if (name == "unicode")
            return make_unicode_character_value();
        else {
            std::cerr << "No Character initializer called " << name << "!\n";
            return NULL;
        }
    }

    virtual Scope *get_inner_scope(TypeSpecIter tsi) {
        return inner_scope.get();
    }
};


class EnumerationType: public BasicType {
public:
    std::vector<std::string> keywords;
    Label stringifications_label;

    EnumerationType(std::string n, std::vector<std::string> kw, Label sl)
        :BasicType(n, 1, true) {  // TODO: different sizes based on the keyword count!
        keywords = kw;
        stringifications_label = sl;
    }
    
    virtual Value *lookup_initializer(TypeSpecIter tsi, std::string n, Scope *scope) {
        for (unsigned i = 0; i < keywords.size(); i++)
            if (keywords[i] == n)
                return make_basic_value(TypeSpec(tsi), i);
        
        return NULL;
    }
    
    virtual Scope *get_inner_scope(TypeSpecIter tsi) {
        return enumeration_metatype->get_inner_scope(tsi);
    }
};


class TreenumerationType: public EnumerationType {
public:
    Label tails_label;

    TreenumerationType(std::string n, std::vector<std::string> kw, Label sl, Label tl)
        :EnumerationType(n, kw, sl) {
        tails_label = tl;
    }
    
    virtual Value *lookup_initializer(TypeSpecIter tsi, std::string n, Scope *scope) {
        for (unsigned i = 0; i < keywords.size(); i++)
            if (keywords[i] == n)
                return make_basic_value(TypeSpec(tsi), i);
        
        return NULL;
    }
    
    virtual Scope *get_inner_scope(TypeSpecIter tsi) {
        return treenumeration_metatype->get_inner_scope(tsi);
    }
};
