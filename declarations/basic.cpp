#include "../plum.h"


BasicType::BasicType(std::string n, unsigned s, bool iu, MetaType *mt)
    :Type(n, {}, mt ? mt : value_metatype) {
    size = s;
    os = (s == 1 ? 0 : s == 2 ? 1 : s == 4 ? 2 : s == 8 ? 3 : throw INTERNAL_ERROR);        
    is_unsigned = iu;
}

Allocation BasicType::measure(TypeMatch tm) {
    return Allocation(size);
}

void BasicType::store(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    // Only R10 is usable as scratch
    BinaryOp mov = MOVQ % os;
    
    switch (s.where * t.where) {
    case NOWHERE_REGISTER:  // this is used by the boolean and operation
        cx->op(mov, t.reg, 0);
        return;
    case NOWHERE_STACK:  // this is used by pushing optional function arguments
        cx->op(PUSHQ, 0);
        return;

    case CONSTANT_NOWHERE:
        return;
    case CONSTANT_CONSTANT:
        return;
    case CONSTANT_REGISTER:
        cx->op(mov, t.reg, s.value);
        return;
    case CONSTANT_STACK:
        cx->op(PUSHQ, s.value);
        return;
    case CONSTANT_MEMORY:
        cx->op(mov, t.address, s.value);
        return;
        
    case FLAGS_NOWHERE:
        return;
    case FLAGS_FLAGS:
        return;
    case FLAGS_REGISTER:
        cx->op(bitset(s.cc), t.reg);
        return;
    case FLAGS_STACK:
        cx->op(bitset(s.cc), R10B);
        cx->op(PUSHQ, R10);
        return;
    case FLAGS_MEMORY:
        cx->op(bitset(s.cc), t.address);
        return;

    case REGISTER_NOWHERE:
        return;
    case REGISTER_REGISTER:
        if (s.reg != t.reg)
            cx->op(mov, t.reg, s.reg);
        return;
    case REGISTER_STACK:
        cx->op(PUSHQ, s.reg);
        return;
    case REGISTER_MEMORY:
        cx->op(mov, t.address, s.reg);
        return;

    case STACK_NOWHERE:
        cx->op(POPQ, R10);
        return;
    case STACK_REGISTER:
        cx->op(POPQ, t.reg);
        return;
    case STACK_STACK:
        return;
    case STACK_MEMORY:
        if (size == INTEGER_SIZE)
            cx->op(POPQ, t.address);
        else {
            cx->op(POPQ, R10);
            cx->op(mov, t.address, R10);
        }
        return;

    case MEMORY_NOWHERE:
        return;
    case MEMORY_REGISTER:
        cx->op(mov, t.reg, s.address);
        return;
    case MEMORY_STACK:
        if (size == INTEGER_SIZE)
            cx->op(PUSHQ, s.address);
        else {
            cx->op(mov, R10, s.address);
            cx->op(PUSHQ, R10);
        }
        return;
    case MEMORY_MEMORY:
        cx->op(mov, R10, s.address);
        cx->op(mov, t.address, R10);
        return;
    
    default:
        Type::store(tm, s, t, cx);
    }
}

void BasicType::create(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    BinaryOp mov = MOVQ % os;
    
    switch (s.where * t.where) {
    case NOWHERE_MEMORY:
        cx->op(mov, t.address, 0);
        return;
    case CONSTANT_MEMORY:
        cx->op(mov, t.address, s.value);
        return;
    case FLAGS_MEMORY:
        cx->op(bitset(s.cc), t.address);
        return;
    case REGISTER_MEMORY:
        cx->op(mov, t.address, s.reg);
        return;
    case STACK_MEMORY:
        if (size == INTEGER_SIZE)
            cx->op(POPQ, t.address);
        else {
            cx->op(POPQ, R10);
            cx->op(mov, t.address, R10);
        }
        return;
    case MEMORY_MEMORY:
        cx->op(mov, R10, s.address);
        cx->op(mov, t.address, R10);
        return;
    default:
        throw INTERNAL_ERROR;
    }
}

void BasicType::destroy(TypeMatch tm, Storage s, Cx *cx) {
    if (s.where == MEMORY)
        ;
    else
        throw INTERNAL_ERROR;
}

void BasicType::equal(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    // No need to take care of STACK here, GenericOperationValue takes care of it
    // Only R10 is usable as scratch
    BinaryOp MOV = MOVQ % os;
    BinaryOp CMP = CMPQ % os;
    
    switch (s.where * t.where) {
    case CONSTANT_CONSTANT:
        cx->op(MOV, R10, s.value);
        cx->op(CMP, R10, t.value);
        break;
    case CONSTANT_REGISTER:
        cx->op(MOV, R10, s.value);
        cx->op(CMP, R10, t.reg);
        break;
    case CONSTANT_MEMORY:
        cx->op(MOV, R10, s.value);
        cx->op(CMP, R10, t.address);
        break;

    case REGISTER_CONSTANT:
        cx->op(CMP, s.reg, t.value);
        break;
    case REGISTER_REGISTER:
        cx->op(CMP, s.reg, t.reg);
        break;
    case REGISTER_MEMORY:
        cx->op(CMP, s.reg, t.address);
        break;

    case STACK_CONSTANT:
        cx->op(POPQ, R10);
        cx->op(CMP, R10, t.value);
        break;

    case MEMORY_CONSTANT:
        cx->op(CMP, s.address, t.value);
        break;
    case MEMORY_REGISTER:
        cx->op(CMP, s.address, t.reg);
        break;
    case MEMORY_MEMORY:
        cx->op(MOV, R10, s.address);
        cx->op(CMP, R10, t.address);
        break;
        
    default:
        throw INTERNAL_ERROR;
    }
}

void BasicType::compare(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    equal(tm, s, t, cx);
    cx->runtime->r10bcompar(is_unsigned);
}

StorageWhere BasicType::where(TypeMatch tm, AsWhat as_what) {
    return (
        as_what == AS_VALUE ? REGISTER :
        as_what == AS_VARIABLE ? MEMORY :
        as_what == AS_ARGUMENT ? MEMORY :
        //as_what == AS_PIVOT_ARGUMENT ? MEMORY :
        as_what == AS_LVALUE_ARGUMENT ? ALIAS :
        throw INTERNAL_ERROR
    );
}

Storage BasicType::optimal_value_storage(TypeMatch tm, Regs preferred) {
    if (preferred.has_gpr())
        return Storage(REGISTER, preferred.get_gpr());
    else
        return Storage(STACK);
}

bool BasicType::get_unsigned() {
    return is_unsigned;
}




IntegerType::IntegerType(std::string n, unsigned s, bool iu)
    :BasicType(n, s, iu, integer_metatype) {
}

void IntegerType::streamify(TypeMatch tm, Cx *cx) {
    // SysV
    auto arg_regs = cx->abi_arg_regs();
    Address value_addr(RSP, ALIAS_SIZE);
    Address alias_addr(RSP, 0);
    
    if (is_unsigned) {
        if (size == 1)
            cx->op(MOVZXBQ, arg_regs[0], value_addr);
        else if (size == 2)
            cx->op(MOVZXWQ, arg_regs[0], value_addr);
        else if (size == 4)
            cx->op(MOVZXDQ, arg_regs[0], value_addr);
        else if (size == 8)
            cx->op(MOVQ, arg_regs[0], value_addr);
        else
            throw INTERNAL_ERROR;

        cx->op(MOVQ, arg_regs[1], alias_addr);
        cx->runtime->call_sysv(cx->runtime->sysv_streamify_unteger_label);
    }
    else {
        if (size == 1)
            cx->op(MOVSXBQ, arg_regs[0], value_addr);
        else if (size == 2)
            cx->op(MOVSXWQ, arg_regs[0], value_addr);
        else if (size == 4)
            cx->op(MOVSXDQ, arg_regs[0], value_addr);
        else if (size == 8)
            cx->op(MOVQ, arg_regs[0], value_addr);
        else
            throw INTERNAL_ERROR;

        cx->op(MOVQ, arg_regs[1], alias_addr);
        cx->runtime->call_sysv(cx->runtime->sysv_streamify_integer_label);
    }
}

void IntegerType::type_info(TypeMatch tm, Cx *cx) {
    cx->dwarf->base_type_info(name, size, is_unsigned ? DW_ATE_unsigned : DW_ATE_signed);
}




BooleanType::BooleanType(std::string n, unsigned s)
    :BasicType(n, s, true) {
}

void BooleanType::streamify(TypeMatch tm, Cx *cx) {
    // SysV
    auto arg_regs = cx->abi_arg_regs();
    Address value_addr(RSP, ALIAS_SIZE);
    Address alias_addr(RSP, 0);

    cx->op(MOVQ, arg_regs[0], value_addr);
    cx->op(MOVQ, arg_regs[1], alias_addr);
    cx->runtime->call_sysv(cx->runtime->sysv_streamify_boolean_label);
}

Value *BooleanType::lookup_initializer(TypeMatch tm, std::string name, Scope *scope) {
    if (name == "false")
        return make<BasicValue>(tm[0], 0);
    else if (name == "true")
        return make<BasicValue>(tm[0], 1);
    else {
        std::cerr << "No Boolean initializer called " << name << "!\n";
        return NULL;
    }
}

void BooleanType::type_info(TypeMatch tm, Cx *cx) {
    cx->dwarf->base_type_info(name, size, DW_ATE_boolean);
}




CharacterType::CharacterType(std::string n, unsigned s)
    :BasicType(n, s, true) {
}

void CharacterType::streamify(TypeMatch tm, Cx *cx) {
    // Escaped quoted
    Label esc_label = cx->once->compile(compile_esc_streamification);
    cx->op(CALL, esc_label);  // clobbers all
}

void CharacterType::insert_pre_streamification(Cx *cx) {
    Address value_addr(RSP, ADDRESS_SIZE + RIP_SIZE + ALIAS_SIZE);
    Address alias_addr(RSP, ADDRESS_SIZE + RIP_SIZE);

    cx->prologue();
    
    cx->op(MOVQ, R10, 5);  // worst case will be five character escapes
    stream_preappend2(alias_addr, cx);

    cx->op(MOVZXWQ, R10, value_addr);
    cx->op(MOVQ, RCX, Address(RAX, LINEARRAY_LENGTH_OFFSET));
    cx->op(LEA, RBX, Address(RAX, RCX, Address::SCALE_2, LINEARRAY_ELEMS_OFFSET));  // stream end

    // RAX - stream ref, RBX - stream end, R10 - character
}

void CharacterType::compile_ascii_table(Label label, Cx *cx) {
    cx->data_align(8);
    cx->data_label(label);

    for (unsigned i = 0; i < 128; i++) {
        std::string name = character_name(i);

        unsigned64 x = (
            name.size() == 0 ? i :  // unescaped
            name.size() == 2 ? name[0] | name[1] << 16 :  // two-letter name
            name.size() == 3 ? name[0] | name[1] << 16 | (unsigned64)name[2] << 32 : // three-letter name
            throw INTERNAL_ERROR
        );
        
        cx->data_qword(x);
    }
}

void CharacterType::compile_esc_streamification(Label label, Cx *cx) {
    Label ascii_table_label = cx->once->compile(compile_ascii_table);
    Label unescaped, escaped_two, escaped_three;

    cx->code_label_local(label, "Character__esc_streamification");
    
    insert_pre_streamification(cx);

    cx->op(CMPQ, R10, 128);
    cx->op(JAE, unescaped);
    cx->op(LEA, R11, Address(ascii_table_label, 0));
    cx->op(MOVQ, R10, Address(R11, R10, Address::SCALE_8, 0));
    cx->op(CMPQ, R10, 0xffffff);
    cx->op(JA, escaped_three);
    cx->op(CMPQ, R10, 0xffff);
    cx->op(JA, escaped_two);

    // unescaped character: "X"
    cx->code_label(unescaped);
    cx->op(MOVW, Address(RBX, 0), '"');
    cx->op(MOVW, Address(RBX, 2), R10W);
    cx->op(MOVW, Address(RBX, 4), '"');
    cx->op(ADDQ, Address(RAX, LINEARRAY_LENGTH_OFFSET), 3);
    cx->epilogue();

    // two characters: `XY
    cx->code_label(escaped_two);
    cx->op(MOVW, Address(RBX, 0), '`');
    cx->op(MOVD, Address(RBX, 2), R10D);
    cx->op(ADDQ, Address(RAX, LINEARRAY_LENGTH_OFFSET), 3);
    cx->epilogue();
    
    // three characters: `XYZ
    cx->code_label(escaped_three);
    cx->op(MOVW, Address(RBX, 0), '`');
    cx->op(MOVQ, Address(RBX, 2), R10);
    cx->op(ADDQ, Address(RAX, LINEARRAY_LENGTH_OFFSET), 4);
    cx->epilogue();
}

void CharacterType::compile_str_streamification(Label label, Cx *cx) {
    Label ascii_table_label = cx->once->compile(compile_ascii_table);
    Label unescaped, escaped_two, escaped_three;

    cx->code_label_local(label, "Character__str_streamification");
    
    insert_pre_streamification(cx);

    cx->op(CMPQ, R10, 128);
    cx->op(JAE, unescaped);
    cx->op(LEA, R11, Address(ascii_table_label, 0));
    cx->op(MOVQ, R10, Address(R11, R10, Address::SCALE_8, 0));
    cx->op(CMPQ, R10, 0xffffff);
    cx->op(JA, escaped_three);
    cx->op(CMPQ, R10, 0xffff);
    cx->op(JA, escaped_two);

    // unescaped character: X
    cx->code_label(unescaped);
    cx->op(MOVW, Address(RBX, 0), R10W);
    cx->op(ADDQ, Address(RAX, LINEARRAY_LENGTH_OFFSET), 1);
    cx->epilogue();

    // two characters: {XY}
    cx->code_label(escaped_two);
    cx->op(MOVW, Address(RBX, 0), '{');
    cx->op(MOVD, Address(RBX, 2), R10D);
    cx->op(MOVW, Address(RBX, 6), '}');
    cx->op(ADDQ, Address(RAX, LINEARRAY_LENGTH_OFFSET), 4);
    cx->epilogue();
    
    // three characters: {XYZ}
    cx->code_label(escaped_three);
    cx->op(MOVW, Address(RBX, 0), '{');
    cx->op(MOVQ, Address(RBX, 2), R10);
    cx->op(MOVW, Address(RBX, 8), '}');
    cx->op(ADDQ, Address(RAX, LINEARRAY_LENGTH_OFFSET), 5);
    cx->epilogue();
}

Value *CharacterType::lookup_initializer(TypeMatch tm, std::string name, Scope *scope) {
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

void CharacterType::type_info(TypeMatch tm, Cx *cx) {
    cx->dwarf->base_type_info(name, size, DW_ATE_UTF);
}



EnumerationType::EnumerationType(std::string n, std::vector<std::string> kw, MetaType *mt)
    :BasicType(n, 1, true, mt ? mt : enumeration_metatype) {  // TODO: different sizes based on the keyword count!
    keywords = kw;
}

void EnumerationType::streamify(TypeMatch tm, Cx *cx) {
    Label es_label = cx->once->compile(compile_streamification);

    cx->op(LEA, RBX, Address(get_stringifications_label(cx), 0));  // table start
    cx->op(CALL, es_label);  // clobbers all
}

void EnumerationType::compile_streamification(Label label, Cx *cx) {
    // RAX - target array, RBX - table start, RCX - size, R10 - source enum
    Address value_addr(RSP, ADDRESS_SIZE + RIP_SIZE + ALIAS_SIZE);
    Address alias_addr(RSP, ADDRESS_SIZE + RIP_SIZE);

    cx->code_label_local(label, "enum_streamification");
    cx->prologue();
    
    cx->op(MOVQ, R10, value_addr);  // the enum

    // Find the string for this enum value
    cx->op(ANDQ, R10, 0xFF);
    cx->op(MOVQ, R10, Address(RBX, R10, Address::SCALE_8, 0));
        
    cx->op(PUSHQ, R10);  // save enum string
        
    cx->op(MOVQ, R10, Address(R10, LINEARRAY_LENGTH_OFFSET));

    stream_preappend2(alias_addr + ADDRESS_SIZE, cx);
    
    cx->op(POPQ, R10);  // enum string

    cx->op(LEA, RDI, Address(RAX, LINEARRAY_ELEMS_OFFSET));
    cx->op(ADDQ, RDI, Address(RAX, LINEARRAY_LENGTH_OFFSET));
    cx->op(ADDQ, RDI, Address(RAX, LINEARRAY_LENGTH_OFFSET));  // Yes, added twice (CHARACTER_SIZE)

    cx->op(LEA, RSI, Address(R10, LINEARRAY_ELEMS_OFFSET));
    cx->op(MOVQ, RCX, Address(R10, LINEARRAY_LENGTH_OFFSET));
    cx->op(ADDQ, Address(RAX, LINEARRAY_LENGTH_OFFSET), RCX);
    cx->op(IMUL3Q, RCX, RCX, CHARACTER_SIZE);
    
    cx->op(REPMOVSB);
    
    cx->epilogue();
}

Value *EnumerationType::lookup_initializer(TypeMatch tm, std::string n, Scope *scope) {
    for (unsigned i = 0; i < keywords.size(); i++)
        if (keywords[i] == n)
            return make<BasicValue>(tm[0], i);
    
    return NULL;
}

Label EnumerationType::get_stringifications_label(Cx *cx) {
    return cx->once->compile(compile_stringifications, TypeSpec { this });
}

void EnumerationType::compile_stringifications(Label label, TypeSpec ts, Cx *cx) {
    EnumerationType *t = ptr_cast<EnumerationType>(ts[0]);
    std::vector<Label> labels;
    
    for (auto &keyword : t->keywords) 
        labels.push_back(cx->runtime->data_heap_string(decode_utf8(keyword)));
        
    cx->data_label_local(label, ts.symbolize("stringifications"));
    
    for (auto &l : labels)
        cx->data_reference(l);  // 64-bit absolute
}

unsigned EnumerationType::get_keyword_index(std::string kw) {
    for (unsigned i = 0; i < keywords.size(); i++)
        if (keywords[i] == kw)
            return i;
    
    throw INTERNAL_ERROR;
}

void EnumerationType::type_info(TypeMatch tm, Cx *cx) {
    cx->dwarf->begin_enumeration_type_info(name, 1);
    
    for (unsigned i = 0; i < keywords.size(); i++)
        if (i || keywords[i].size())  // don't emit treenum root values
            cx->dwarf->enumerator_info(keywords[i], i);
    
    cx->dwarf->end_info();
}



TreenumerationType::TreenumerationType(std::string n, std::vector<std::string> kw, std::vector<unsigned> ps)
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

Value *TreenumerationType::lookup_initializer(TypeMatch tm, std::string n, Scope *scope) {
    for (unsigned i = 0; i < keywords.size(); i++)
        if (keywords[i] == n)
            return make<BasicValue>(tm[0], i);
    
    return NULL;
}

Value *TreenumerationType::lookup_matcher(TypeMatch tm, std::string n, Value *pivot, Scope *scope) {
    for (unsigned i = 0; i < keywords.size(); i++)
        if (keywords[i] == n)
            return make<TreenumerationMatcherValue>(i, pivot);
    
    if (n == "any")
        return make<TreenumerationAnyMatcherValue>(pivot);
    
    return EnumerationType::lookup_matcher(tm, n, pivot, scope);
}

Label TreenumerationType::get_parents_label(Cx *cx) {
    return cx->once->compile(compile_parents, TypeSpec { this });
}

void TreenumerationType::compile_parents(Label label, TypeSpec ts, Cx *cx) {
    TreenumerationType *t = ptr_cast<TreenumerationType>(ts[0]);
    cx->data_label_local(label, ts.symbolize("parents"));
    
    for (unsigned p : t->parents)
        cx->data_byte(p);
}
