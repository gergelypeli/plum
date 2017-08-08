
class Type: public Declaration {
public:
    std::string name;
    unsigned parameter_count;
    
    Type(std::string n, unsigned pc) {
        name = n;
        parameter_count = pc;
    }
    
    virtual unsigned get_parameter_count() {
        return parameter_count;
    }
    
    virtual void set_name(std::string n) {
        name = n;
    }
    
    virtual Value *match(std::string name, Value *pivot, TypeMatch &match) {
        if (name != this->name)
            return NULL;
            
        if (parameter_count == 0) {
            if (!typematch(VOID_TS, pivot, match))
                return NULL;
                
            TypeSpec ts = { type_type, this };
            
            return make_type_value(ts);
        }
        else if (parameter_count == 1) {
            if (!typematch(ANY_TYPE_TS, pivot, match))
                return NULL;
                
            TypeSpec ts = match[1].prefix(this).prefix(type_type);
            // FIXME: do something with pivot!
            
            return make_type_value(ts);
        }
        else
            throw INTERNAL_ERROR;
    }
    
    virtual StorageWhere where(TypeSpecIter tsi, bool is_arg, bool is_lvalue) {
        std::cerr << "Nowhere type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }

    virtual Storage boolval(TypeSpecIter tsi, Storage, X64 *, bool probe) {
        std::cerr << "Unboolable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }

    virtual unsigned measure(TypeSpecIter tsi, StorageWhere where) {
        switch (where) {
        case ALISTACK:
            return 8;
        case ALIAS:
            return 8;
        default:
            std::cerr << "Unmeasurable type: " << name << "!\n";
            throw INTERNAL_ERROR;
        }
    }

    virtual void store(TypeSpecIter this_tsi, Storage s, Storage t, X64 *x64) {
        switch (s.where * t.where) {
        case MEMORY_ALISTACK:
            x64->op(LEA, RBX, s.address);
            x64->op(PUSHQ, RBX);
            return;
        case MEMORY_ALIAS:
            x64->op(LEA, RBX, s.address);
            x64->op(MOVQ, t.address, RBX);
            return;
            
        case ALISTACK_NOWHERE:
            x64->op(ADDQ, RSP, 8);
            return;
        case ALISTACK_MEMORY:
            if (t.address.base == NOREG || t.address.base == RBP || t.address.base == RSP || t.address.index != NOREG || t.address.offset != 0)
                throw INTERNAL_ERROR;
                
            x64->op(POPQ, t.address.base);
            return;
        case ALISTACK_ALISTACK:
            return;
        case ALISTACK_ALIAS:
            x64->op(POPQ, t.address);
            return;
            
        case ALIAS_NOWHERE:
            return;
        case ALIAS_MEMORY:
            if (t.address.base == NOREG || t.address.base == RBP || t.address.base == RSP || t.address.index != NOREG || t.address.offset != 0)
                throw INTERNAL_ERROR;
                
            x64->op(MOVQ, t.address.base, s.address);
            return;
        case ALIAS_ALISTACK:
            x64->op(PUSHQ, s.address);
            return;
        case ALIAS_ALIAS:
            x64->op(MOVQ, RBX, s.address);
            x64->op(MOVQ, t.address, RBX);
            return;
            
        default:
            std::cerr << "Unstorable type: " << name << "!\n";
            throw INTERNAL_ERROR;
        }
    }
    
    virtual void create(TypeSpecIter tsi, Storage s, Storage t, X64 *x64) {
        std::cerr << "Uncreatable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }

    virtual void destroy(TypeSpecIter tsi, Storage s, X64 *x64) {
        std::cerr << "Undestroyable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }

    virtual Value *lookup_initializer(TypeSpecIter tsi, std::string n, Scope *scope) {
        std::cerr << "Uninitializable type: " << name << "!\n";
        throw INTERNAL_ERROR;
    }
    
    virtual Scope *get_inner_scope() {
        return NULL;
    }
};


class SpecialType: public Type {
public:
    SpecialType(std::string name, unsigned pc)
        :Type(name, pc) {
    }
    
    //virtual unsigned measure(TypeSpecIter, StorageWhere) {
    //    return 0;
    //}

    //virtual StorageWhere where(TypeSpecIter tsi) {
    //    return NOWHERE;
    //}

    virtual void store(TypeSpecIter tsi, Storage s, Storage t, X64 *x64) {
        if (s.where != NOWHERE || t.where != NOWHERE) {
            std::cerr << "Invalid special store from " << s << " to " << t << "!\n";
            throw INTERNAL_ERROR;
        }
    }
};


class BasicType: public Type {
public:
    unsigned size;
    int os;

    BasicType(std::string n, unsigned s)
        :Type(n, 0) {
        size = s;
        os = (s == 1 ? 0 : s == 2 ? 1 : s == 4 ? 2 : s == 8 ? 3 : throw INTERNAL_ERROR);        
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
};


class IntegerType: public BasicType {
public:
    bool is_not_signed;
    
    IntegerType(std::string n, unsigned s, bool iu)
        :BasicType(n, s) {
        is_not_signed = iu;
    }
    
    virtual bool is_unsigned() {
        return is_not_signed;
    }

    virtual Scope *get_inner_scope() {
        return integer_metatype->get_inner_scope();
    }
};


class BooleanType: public BasicType {
public:
    std::unique_ptr<Scope> inner_scope;
    
    BooleanType(std::string n, unsigned s)
        :BasicType(n, s) {
        inner_scope.reset(new Scope);
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

    virtual Scope *get_inner_scope() {
        return inner_scope.get();
    }
};


class CharacterType: public BasicType {
public:
    std::unique_ptr<Scope> inner_scope;
    
    CharacterType(std::string n, unsigned s)
        :BasicType(n, s) {
        inner_scope.reset(new Scope);
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

    virtual Scope *get_inner_scope() {
        return inner_scope.get();
    }
};


class ReferenceType: public Type {
public:
    ReferenceType(std::string name)
        :Type(name, 1) {
    }
    
    virtual unsigned measure(TypeSpecIter tsi, StorageWhere where) {
        switch (where) {
        case STACK:
            return 8;
        case MEMORY:
            return 8;
        default:
            return Type::measure(tsi, where);
        }
    }

    virtual void store(TypeSpecIter tsi, Storage s, Storage t, X64 *x64) {
        switch (s.where * t.where) {
        case REGISTER_NOWHERE:
            x64->decref(s.reg);
            return;
        case REGISTER_REGISTER:
            if (s.reg != t.reg)
                x64->op(MOVQ, t.reg, s.reg);
            return;
        case REGISTER_STACK:
            x64->op(PUSHQ, s.reg);
            return;
        case REGISTER_MEMORY:
            x64->op(XCHGQ, t.address, s.reg);
            x64->decref(s.reg);
            return;

        case STACK_NOWHERE:
            x64->op(POPQ, RBX);
            x64->decref(RBX);
            return;
        case STACK_REGISTER:
            x64->op(POPQ, t.reg);
            return;
        case STACK_STACK:
            return;
        case STACK_MEMORY:
            x64->op(POPQ, RBX);
            x64->op(XCHGQ, RBX, t.address);
            x64->decref(RBX);
            return;

        case MEMORY_NOWHERE:
            return;
        case MEMORY_REGISTER:
            x64->op(MOVQ, t.reg, s.address);
            x64->incref(t.reg);
            return;
        case MEMORY_STACK:
            x64->op(MOVQ, RBX, s.address);
            x64->incref(RBX);
            x64->op(PUSHQ, RBX);
            return;
        case MEMORY_MEMORY:
            x64->op(MOVQ, RBX, s.address);
            x64->incref(RBX);
            x64->op(XCHGQ, RBX, t.address);
            x64->decref(RBX);
            return;
        default:
            Type::store(tsi, s, t, x64);
        }
    }

    virtual void create(TypeSpecIter tsi, Storage s, Storage t, X64 *x64) {
        // Assume the target MEMORY is uninitialized
        
        switch (s.where * t.where) {
        case NOWHERE_MEMORY:
            x64->op(MOVQ, t.address, 0);
            return;
        case REGISTER_MEMORY:
            x64->op(MOVQ, t.address, s.reg);
            return;
        case STACK_MEMORY:
            x64->op(POPQ, t.address);
            return;
        case MEMORY_MEMORY:
            x64->op(MOVQ, RBX, s.address);
            x64->incref(RBX);
            x64->op(MOVQ, t.address, RBX);
            return;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual void destroy(TypeSpecIter , Storage s, X64 *x64) {
        if (s.where == MEMORY) {
            x64->op(MOVQ, RBX, s.address);
            x64->decref(RBX);
        }
        else
            throw INTERNAL_ERROR;
    }

    virtual StorageWhere where(TypeSpecIter, bool is_arg, bool is_lvalue) {
        return (is_arg ? (is_lvalue ? ALIAS : MEMORY) : (is_lvalue ? MEMORY : REGISTER));
    }

    virtual Storage boolval(TypeSpecIter , Storage s, X64 *x64, bool probe) {
        switch (s.where) {
        case CONSTANT:
            return Storage(CONSTANT, s.value != 0);
        case REGISTER:
            if (!probe)
                x64->decref(s.reg);
                
            x64->op(CMPQ, s.reg, 0);
            return Storage(FLAGS, SETNE);
        case MEMORY:
            x64->op(CMPQ, s.address, 0);
            return Storage(FLAGS, SETNE);
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Value *lookup_initializer(TypeSpecIter tsi, std::string name, Scope *scope) {
        if (name == "null") {
            return make_null_reference_value(TypeSpec(tsi));
        }
        else {
            std::cerr << "No reference initializer called " << name << "!\n";
            return NULL;
        }
    }
};


class HeapType: public Type {
public:
    std::unique_ptr<Scope> inner_scope;

    HeapType(std::string name, unsigned pc)
        :Type(name, pc) {
        inner_scope.reset(new Scope);
    }
    
    virtual Scope *get_inner_scope() {
        return inner_scope.get();
    }
};


HeapType *heap_type_cast(Type *t) {
    return dynamic_cast<HeapType *>(t);
}


class AttributeType: public Type {
public:
    AttributeType(std::string n)
        :Type(n, 1) {
    }

    virtual StorageWhere where(TypeSpecIter this_tsi, bool is_arg, bool is_lvalue) {
        this_tsi++;
        return (*this_tsi)->where(this_tsi, is_arg, is_lvalue || this == lvalue_type);
    }

    virtual Storage boolval(TypeSpecIter this_tsi, Storage s, X64 *x64, bool probe) {
        this_tsi++;
        return (*this_tsi)->boolval(this_tsi, s, x64, probe);
    }
    
    virtual unsigned measure(TypeSpecIter tsi, StorageWhere where) {
        tsi++;
        return (*tsi)->measure(tsi, where);
    }

    virtual void store(TypeSpecIter tsi, Storage s, Storage t, X64 *x64) {
        tsi++;
        return (*tsi)->store(tsi, s, t, x64);
    }

    virtual void create(TypeSpecIter tsi, Storage s, Storage t, X64 *x64) {
        tsi++;
        (*tsi)->create(tsi, s, t, x64);
    }

    virtual void destroy(TypeSpecIter tsi, Storage s, X64 *x64) {
        tsi++;
        (*tsi)->destroy(tsi, s, x64);
    }
};


class EnumerationType: public BasicType {
public:
    std::vector<std::string> keywords;
    Label stringifications_label;

    EnumerationType(std::string n, std::vector<std::string> kw, Label sl)
        :BasicType(n, 1) {  // TODO: different sizes based on the keyword count!
        
        keywords = kw;
        stringifications_label = sl;
    }
    
    virtual Value *lookup_initializer(TypeSpecIter tsi, std::string n, Scope *scope) {
        for (unsigned i = 0; i < keywords.size(); i++)
            if (keywords[i] == n)
                return make_basic_value(TypeSpec(tsi), i);
        
        return NULL;
    }
    
    virtual Scope *get_inner_scope() {
        return enumeration_metatype->get_inner_scope();
    }
};


class RecordType: public Type {
public:
    Scope *inner_scope;
    std::vector<std::pair<TypeSpec, int>> members;

    RecordType(std::string n, Scope *is)
        :Type(n, 0) {
        inner_scope = is;
    }
    
    virtual void allocate() {
        for (auto &c : inner_scope->contents) {
            Variable *v = dynamic_cast<Variable *>(c.get());
            
            if (v) {
                int offset = v->get_storage(Storage(MEMORY, Address(RAX, 0))).address.offset;
                members.push_back(std::make_pair(v->var_ts, offset));
            }
        }
    }
    
    virtual unsigned measure(TypeSpecIter tsi, StorageWhere where) {
        switch (where) {
        case MEMORY:
            return inner_scope->get_size();
        case STACK:
            return stack_size(inner_scope->get_size());
        default:
            return Type::measure(tsi, where);
        }
    }

    virtual void store(TypeSpecIter tsi, Storage s, Storage t, X64 *x64) {
        switch (s.where * t.where) {
        case MEMORY_NOWHERE:
            return;
        case MEMORY_MEMORY:  // duplicates data
            for (auto &member : members)
                member.first.store(Storage(MEMORY, s.address + member.second), Storage(MEMORY, t.address + member.second), x64);
            return;
        default:
            Type::store(tsi, s, t, x64);
        }
    }

    virtual void create(TypeSpecIter tsi, Storage s, Storage t, X64 *x64) {
        switch (s.where * t.where) {
        case NOWHERE_MEMORY:
            for (auto &member : members)
                member.first.create(Storage(), Storage(MEMORY, t.address + member.second), x64);
            return;
        case MEMORY_MEMORY:  // duplicates data
            for (auto &member : members)
                member.first.create(Storage(MEMORY, s.address + member.second), Storage(MEMORY, t.address + member.second), x64);
            return;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual void destroy(TypeSpecIter tsi, Storage s, X64 *x64) {
        if (s.where == MEMORY)
            for (auto &member : members)  // FIXME: reverse!
                member.first.destroy(Storage(MEMORY, s.address + member.second), x64);
        else
            throw INTERNAL_ERROR;
    }

    virtual StorageWhere where(TypeSpecIter tsi, bool is_arg, bool is_lvalue) {
        return (is_arg ? ALIAS : MEMORY);
    }
    
    virtual Storage boolval(TypeSpecIter tsi, Storage s, X64 *x64, bool probe) {
        Address address;
        
        switch (s.where) {
        case MEMORY:
            address = s.address;
            break;
        default:
            throw INTERNAL_ERROR;
        }
        
        Label done;
        
        for (auto &member : members) {
            Storage t = member.first.boolval(Storage(MEMORY, address + member.second), x64, true);
            
            if (t.where == FLAGS && t.bitset == SETNE)
                x64->op(JNE, done);
            else
                throw INTERNAL_ERROR;
        }

        x64->code_label(done);
        
        return Storage(FLAGS, SETNE);
    }
    
    virtual Value *lookup_initializer(TypeSpecIter tsi, std::string n, Scope *scope) {
        //Variable *new_variable = new Variable("<new>", VOID_TS, TypeSpec(tsi));
        //scope->add(new_variable);
        
        // Initialize a hidden value, and pass it to a method
        //DeclarationValue *dv = new DeclarationValue("<new>");
        //TypeSpec ts(tsi);
        //Value *right = new TypeValue(ts.prefix(type_type));
        //dv->use(right, scope);
        TypeSpec ts(tsi);
        Value *dv = make_declaration_by_type("<new>", ts, scope);

        TypeMatch match;
        Value *value = inner_scope->lookup(n, dv, match);

        if (value)
            return value;
            
        std::cerr << "Can't initialize record as " << n << "!\n";
        
        // OK, we gonna leak dv here, because it's just not possible to delete it.
        //   error: possible problem detected in invocation of delete operator
        //   error: ‘dv’ has incomplete type
        //   note: neither the destructor nor the class-specific operator delete
        //     will be called, even if they are declared when the class is defined
        // Thanks, C++!
        
        return NULL;
    }
    
    virtual Scope *get_inner_scope() {
        return inner_scope;
    }
};


class MetaType: public Type {
public:
    std::unique_ptr<Scope> inner_scope;
    
    MetaType(std::string name)
        :Type(name, 0) {
        
        inner_scope.reset(new Scope);
    }
    
    virtual Scope *get_inner_scope() {
        return inner_scope.get();
    }
};


class IntegerMetaType: public MetaType {
public:
    IntegerMetaType(std::string name)
        :MetaType(name) {
    }
    
    virtual Value *match(std::string name, Value *pivot, TypeMatch &match) {
        if (name != this->name)
            return NULL;

        if (!typematch(VOID_TS, pivot, match))
            return NULL;

        return make_integer_definition_value();
    }
};


class EnumerationMetaType: public MetaType {
public:
    EnumerationMetaType(std::string name)
        :MetaType(name) {
    }
    
    virtual Value *match(std::string name, Value *pivot, TypeMatch &match) {
        if (name != this->name)
            return NULL;
            
        if (!typematch(VOID_TS, pivot, match))
            return NULL;
            
        return make_enumeration_definition_value();
    }
};


class RecordMetaType: public MetaType {
public:
    RecordMetaType(std::string name)
        :MetaType(name) {
    }
    
    virtual Value *match(std::string name, Value *pivot, TypeMatch &match) {
        if (name != this->name)
            return NULL;
            
        if (!typematch(VOID_TS, pivot, match))
            return NULL;
            
        return make_record_definition_value();
    }
};
