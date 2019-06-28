#include "../plum.h"


OptionType::OptionType(std::string n)
    :Type(n, Metatypes { value_metatype }, value_metatype) {
}

int OptionType::get_flag_size(TypeSpec some_ts) {
    // NOTE: the option is `none only if the first 8-bytes integer is 0.
    // This is because we may use an implicit flag if the first 8 bytes are known not be
    // all zeroes, such as a reference.
    Type *t = some_ts[0];
    return t == ref_type || t == ptr_type || t == string_type ? 0 : ADDRESS_SIZE;
}

Allocation OptionType::measure(TypeMatch tm) {
    Allocation a = tm[1].measure();
    a.bytes += get_flag_size(tm[1]);
    return a;
}

void OptionType::store(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    int stack_size = tm[0].measure_stack();
    int flag_size = get_flag_size(tm[1]);
    
    switch (s.where * t.where) {
    case NOWHERE_STACK:
        cx->op(SUBQ, RSP, stack_size);
        cx->op(MOVQ, Address(RSP, 0), OPTION_FLAG_NONE);
        return;
    case STACK_NOWHERE:
        destroy(tm, Storage(MEMORY, Address(RSP, 0)), cx);
        cx->op(ADDQ, RSP, stack_size);
        return;
    case STACK_STACK:
        return;
    case STACK_MEMORY:
        destroy(tm, t, cx);
        create(tm, s, t, cx);
        return;
    case MEMORY_NOWHERE:
        return;
    case MEMORY_STACK:
        cx->op(SUBQ, RSP, stack_size);
        create(tm, s, Storage(MEMORY, Address(RSP, 0)), cx);
        return;
    case MEMORY_MEMORY: { // must work for self-assignment
        Label s_none, none_some, some_none, end;
        
        cx->op(CMPQ, s.address, OPTION_FLAG_NONE);
        cx->op(JE, s_none);

        // s some
        cx->op(CMPQ, t.address, OPTION_FLAG_NONE);
        cx->op(JE, some_none);

        // some_some
        tm[1].store(s + flag_size, t + flag_size, cx);
        cx->op(JMP, end);

        // some_none
        cx->code_label(some_none);
        if (flag_size == ADDRESS_SIZE)
            cx->op(MOVQ, t.address, OPTION_FLAG_NONE + 1);

        tm[1].create(s + flag_size, t + flag_size, cx);
        cx->op(JMP, end);

        cx->code_label(s_none);
        cx->op(CMPQ, t.address, OPTION_FLAG_NONE);
        cx->op(JE, end);  // none_none

        // none_some
        tm[1].destroy(t + flag_size, cx);
        cx->op(MOVQ, t.address, OPTION_FLAG_NONE);

        cx->code_label(end);
    }
        return;
    default:
        Type::store(tm, s, t, cx);
    }
}

void OptionType::create(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    int stack_size = tm[0].measure_stack();
    int flag_size = get_flag_size(tm[1]);
    Label none, end;

    switch (s.where * t.where) {
    case NOWHERE_MEMORY:
        cx->op(MOVQ, t.address, OPTION_FLAG_NONE);
        return;
    case STACK_MEMORY:
        cx->runtime->copy(Address(RSP, 0), t.address, tm[0].measure_raw());
        cx->op(ADDQ, RSP, stack_size);
        return;
    case MEMORY_MEMORY:  // duplicates data
        cx->op(CMPQ, s.address, OPTION_FLAG_NONE);
        cx->op(JE, none);
        
        if (flag_size == ADDRESS_SIZE)
            cx->op(MOVQ, t.address, OPTION_FLAG_NONE + 1);
            
        tm[1].create(s + flag_size, t + flag_size, cx);
        cx->op(JMP, end);

        cx->code_label(none);
        cx->op(MOVQ, t.address, OPTION_FLAG_NONE);
        
        cx->code_label(end);
        return;
    default:
        throw INTERNAL_ERROR;
    }
}

void OptionType::destroy(TypeMatch tm, Storage s, Cx *cx) {
    int flag_size = get_flag_size(tm[1]);
    Label none;
    
    if (s.where == MEMORY) {
        cx->op(CMPQ, s.address, OPTION_FLAG_NONE);
        cx->op(JE, none);
        tm[1].destroy(s + flag_size, cx);
        cx->code_label(none);
    }
    else
        throw INTERNAL_ERROR;
}

StorageWhere OptionType::where(TypeMatch tm, AsWhat as_what) {
    return (
        as_what == AS_VALUE ? STACK :
        as_what == AS_VARIABLE ? MEMORY :
        as_what == AS_ARGUMENT ? MEMORY :
        //as_what == AS_PIVOT_ARGUMENT ? MEMORY :
        as_what == AS_LVALUE_ARGUMENT ? ALIAS :
        throw INTERNAL_ERROR
    );
}

void OptionType::equal(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    int flag_size = get_flag_size(tm[1]);

    if (s.where == MEMORY && t.where == MEMORY) {
        Label end;
        cx->op(MOVQ, R10, s.address);
        cx->op(CMPQ, R10, t.address);
        cx->op(JNE, end);
        cx->op(CMPQ, R10, OPTION_FLAG_NONE);
        cx->op(JE, end);
        tm[1].equal(Storage(MEMORY, s.address + flag_size), Storage(MEMORY, t.address + flag_size), cx);
        cx->code_label(end);
        return;
    }
    else
        throw INTERNAL_ERROR;
}

void OptionType::compare(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    int flag_size = get_flag_size(tm[1]);

    if (s.where == MEMORY && t.where == MEMORY) {
        Label end;
        cx->op(CMPQ, s.address, OPTION_FLAG_NONE);
        cx->op(SETE, R11B);
        cx->op(CMPQ, t.address, OPTION_FLAG_NONE);
        cx->op(SETE, R10B);
        
        cx->op(SUBB, R10B, R11B);
        cx->op(JNE, end);  // exactly one was none, order is decided, R10B, flags as expected
        cx->op(CMPB, R11B, 0);
        cx->op(JE, end);  // both were none, equality is decided, R10B, flags as expected
        
        // neither are none, must compare according to the type parameter
        tm[1].compare(Storage(MEMORY, s.address + flag_size), Storage(MEMORY, t.address + flag_size), cx);
        cx->code_label(end);
        return;
    }
    else
        throw INTERNAL_ERROR;
}

void OptionType::streamify(TypeMatch tm, Cx *cx) {
    Label os_label = cx->once->compile(compile_streamification, tm[1]);
    
    cx->op(CALL, os_label);
}

void OptionType::compile_streamification(Label label, TypeSpec some_ts, Cx *cx) {
    Label some, ok;
    Address value_addr(RSP, ADDRESS_SIZE + RIP_SIZE + ALIAS_SIZE);
    Address alias_addr(RSP, ADDRESS_SIZE + RIP_SIZE);
    
    cx->code_label_local(label, some_ts.prefix(option_type).symbolize("streamification"));
    cx->prologue();

    cx->op(CMPQ, value_addr, OPTION_FLAG_NONE);
    cx->op(JNE, some);
    
    // `none
    streamify_ascii("`none", alias_addr, cx);
    cx->op(JMP, ok);
    
    // `some
    cx->code_label(some);
    streamify_ascii("`some", alias_addr, cx);
    
    if (some_ts != UNIT_TS) {
        // Okay, just for the sake of correctness
        streamify_ascii("(", alias_addr, cx);

        unsigned some_size = some_ts.measure_stack();
        unsigned copy_count = some_size / ADDRESS_SIZE;
        unsigned flag_count = get_flag_size(some_ts) / ADDRESS_SIZE;

        // Make a borrowed copy of the value in the stack.
        for (unsigned j = 0; j < copy_count; j++) {
            // skip ret address, alias, flag
            cx->op(PUSHQ, value_addr + ADDRESS_SIZE * (copy_count + flag_count - 1));
        }
    
        // Skip the flag, but copy the stream alias, too
        cx->op(PUSHQ, 0);
        cx->op(PUSHQ, alias_addr + ADDRESS_SIZE * copy_count + ADDRESS_SIZE);
    
        // Invoking a custom streamification may relocate the stack, so the
        // passed stream alias may be fixed, must propagate it upwards.
        some_ts.streamify(cx);

        cx->op(POPQ, alias_addr + ADDRESS_SIZE * copy_count + ADDRESS_SIZE);
        cx->op(POPQ, R10);
        cx->op(ADDQ, RSP, ADDRESS_SIZE * copy_count);

        streamify_ascii(")", alias_addr, cx);
    }
    
    cx->code_label(ok);
    cx->epilogue();
}

Value *OptionType::lookup_initializer(TypeMatch tm, std::string n, Scope *scope) {
    if (n == "{") {
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

Value *OptionType::lookup_matcher(TypeMatch tm, std::string n, Value *pivot, Scope *s) {
    if (n == "none")
        return make<OptionNoneMatcherValue>(pivot, tm);
    else if (n == "some")
        return make<OptionSomeMatcherValue>(pivot, tm);
        
    return Type::lookup_matcher(tm, n, pivot, s);
}

void OptionType::type_info(TypeMatch tm, Cx *cx) {
    // Yes, this is kinda insane
    
    TypeSpec some_ts = tm[1];
    int flag_size = get_flag_size(some_ts);
    unsigned some_ts_index = cx->once->type_info(some_ts);
    unsigned flag_ts_index = cx->once->type_info(INTEGER_TS);  // needs ADDRESS_SIZE
    unsigned size = tm[0].measure_raw();
    Label label;
    unsigned discr_index = label.def_index;
    
    cx->dwarf->begin_structure_type_info(tm[0].symbolize(), size);
    
    cx->dwarf->begin_variant_part_info(discr_index);
    cx->dwarf->info_def(discr_index);
    
    if (flag_size) {
        cx->dwarf->member_info("<flag>", 0, flag_ts_index, true);
    
        cx->dwarf->begin_variant_info(1);
        cx->dwarf->member_info("<some>", flag_size, some_ts_index, true);
        cx->dwarf->end_info();
    }
    else {
        // Dwarf can't handle an implicit flag, so just output a member
        cx->dwarf->member_info("<some>", 0, some_ts_index, true);
    }
    
    cx->dwarf->end_info();
    
    cx->dwarf->end_info();
}


// TODO: allow default initialization if the first type is Unit?

UnionType::UnionType(std::string n)
    :Type(n, Metatypes {}, value_metatype) {
}

bool UnionType::complete_type() {
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

int UnionType::get_flag_size() {
    // We're more flexible than Option, so always allocate a tag field
    return ADDRESS_SIZE;
}

Allocation UnionType::measure(TypeMatch tm) {
    int max_size = 0;
    
    for (auto &ts : tss) {
        int size = typesubst(ts, tm).measure_stack();
        
        if (size > max_size)
            max_size = size;
    }
    
    return Allocation(max_size + get_flag_size());
}

void UnionType::store(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    int stack_size = tm[0].measure_stack();
    int flag_size = get_flag_size();
    
    switch (s.where * t.where) {
    //case NOWHERE_STACK:
    //    cx->op(SUBQ, RSP, stack_size);
    //    cx->op(MOVQ, Address(RSP, 0), OPTION_FLAG_NONE);
    //    return;
    case STACK_NOWHERE:
        destroy(tm, Storage(MEMORY, Address(RSP, 0)), cx);
        cx->op(ADDQ, RSP, stack_size);
        return;
    case STACK_STACK:
        return;
    case STACK_MEMORY:
        destroy(tm, t, cx);
        create(tm, s, t, cx);
        return;
    case MEMORY_NOWHERE:
        return;
    case MEMORY_STACK:
        cx->op(SUBQ, RSP, stack_size);
        create(tm, s, Storage(MEMORY, Address(RSP, 0)), cx);
        return;
    case MEMORY_MEMORY: { // must work for self-assignment
        Label end, same;
        
        cx->op(MOVQ, R10, s.address);
        cx->op(CMPQ, R10, t.address);
        cx->op(JE, same);
        
        destroy(tm, t, cx);
        create(tm, s, t, cx);
        cx->op(JMP, end);
        
        cx->code_label(same);
        // Can't just destroy t first, it may be the same as s
        
        for (unsigned i = 0; i < tss.size(); i++) {
            Label skip;
            
            cx->op(CMPQ, R10, i);
            cx->op(JNE, skip);
            
            tss[i].store(s + flag_size, t + flag_size, cx);
            cx->op(JMP, end);
            
            cx->code_label(skip);
        }

        cx->runtime->die("Invalid Union!");
        
        cx->code_label(end);
    }
        return;
    default:
        Type::store(tm, s, t, cx);
    }
}

void UnionType::create(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    int stack_size = tm[0].measure_stack();
    int flag_size = get_flag_size();
    Label none, end;

    switch (s.where * t.where) {
    //case NOWHERE_MEMORY:
    //    cx->op(MOVQ, t.address, OPTION_FLAG_NONE);
    //    return;
    case STACK_MEMORY:
        cx->runtime->copy(Address(RSP, 0), t.address, tm[0].measure_raw());
        cx->op(ADDQ, RSP, stack_size);
        return;
    case MEMORY_MEMORY: {  // duplicates data
        Label end;

        cx->op(MOVQ, R10, s.address);
        cx->op(MOVQ, t.address, R10);
        
        for (unsigned i = 0; i < tss.size(); i++) {
            Label skip;
            
            cx->op(CMPQ, R10, i);
            cx->op(JNE, skip);
            
            tss[i].create(s + flag_size, t + flag_size, cx);
            cx->op(JMP, end);
            
            cx->code_label(skip);
        }

        cx->runtime->die("Invalid Union!");
        
        cx->code_label(end);
    }
        return;
    default:
        throw INTERNAL_ERROR;
    }
}

void UnionType::destroy(TypeMatch tm, Storage s, Cx *cx) {
    int flag_size = get_flag_size();
    
    if (s.where == MEMORY) {
        Label end;

        cx->op(MOVQ, R10, s.address);
        
        for (unsigned i = 0; i < tss.size(); i++) {
            Label skip;
            
            cx->op(CMPQ, R10, i);
            cx->op(JNE, skip);
            
            tss[i].destroy(s + flag_size, cx);
            cx->op(JMP, end);
            
            cx->code_label(skip);
        }

        cx->runtime->die("Invalid Union!");
        
        cx->code_label(end);
    }
    else
        throw INTERNAL_ERROR;
}

StorageWhere UnionType::where(TypeMatch tm, AsWhat as_what) {
    return (
        as_what == AS_VALUE ? STACK :
        as_what == AS_VARIABLE ? MEMORY :
        as_what == AS_ARGUMENT ? MEMORY :
        as_what == AS_LVALUE_ARGUMENT ? ALIAS :
        throw INTERNAL_ERROR
    );
}

void UnionType::equal(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    int flag_size = get_flag_size();

    if (s.where == MEMORY && t.where == MEMORY) {
        Label end;
        cx->op(MOVQ, R10, s.address);
        cx->op(CMPQ, R10, t.address);
        cx->op(JNE, end);

        for (unsigned i = 0; i < tss.size(); i++) {
            Label skip;
            
            cx->op(CMPQ, R10, i);
            cx->op(JNE, skip);
            
            tss[i].equal(s + flag_size, t + flag_size, cx);
            cx->op(JMP, end);
            
            cx->code_label(skip);
        }

        cx->runtime->die("Invalid Union!");

        cx->code_label(end);
        return;
    }
    else
        throw INTERNAL_ERROR;
}

void UnionType::compare(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    int flag_size = get_flag_size();

    if (s.where == MEMORY && t.where == MEMORY) {
        Label end, equal;
        
        cx->op(MOVQ, R10, s.address);
        cx->op(CMPQ, R10, t.address);
        cx->op(JE, equal);
        
        cx->runtime->r10bcompar(true);
        cx->op(JMP, end);
        
        cx->code_label(equal);
        
        for (unsigned i = 0; i < tss.size(); i++) {
            Label skip;
            
            cx->op(CMPQ, R10, i);
            cx->op(JNE, skip);
            
            tss[i].compare(s + flag_size, t + flag_size, cx);
            cx->op(JMP, end);
            
            cx->code_label(skip);
        }
        
        cx->runtime->die("Invalid Union!");

        cx->code_label(end);
        return;
    }
    else
        throw INTERNAL_ERROR;
}

void UnionType::streamify(TypeMatch tm, Cx *cx) {
    Label os_label = cx->once->compile(compile_streamification, tm[0]);
    
    cx->op(CALL, os_label);
}

void UnionType::compile_streamification(Label label, TypeSpec union_ts, Cx *cx) {
    unsigned union_size = union_ts.measure_stack();
    unsigned copy_count = union_size / ADDRESS_SIZE - 1;  // without the flag
    UnionType *ut = ptr_cast<UnionType>(union_ts[0]);

    cx->code_label_local(label, union_ts.symbolize("streamification"));
    cx->prologue();
    Label end;
    Address value_addr(RSP, ADDRESS_SIZE + RIP_SIZE + ALIAS_SIZE);
    Address alias_addr(RSP, ADDRESS_SIZE + RIP_SIZE);

    for (unsigned i = 0; i < ut->tss.size(); i++) {
        Label skip;
        
        cx->op(CMPQ, value_addr, i);
        cx->op(JNE, skip);
        
        // Streamify the tag first
        streamify_ascii("`" + ut->tags[i], alias_addr, cx);
        
        if (ut->tss[i] != UNIT_TS) {
            streamify_ascii("(", alias_addr, cx);
        
            // Then the field. Make a borrowed copy of the value in the stack.
            for (unsigned j = 0; j < copy_count; j++) {
                // skip ret address, alias, flag
                cx->op(PUSHQ, value_addr + ADDRESS_SIZE * copy_count);
            }
        
            // Skip the flag, but copy the stream alias, too
            cx->op(PUSHQ, 0);
            cx->op(PUSHQ, alias_addr + ADDRESS_SIZE * copy_count + ADDRESS_SIZE);
        
            // Invoking a custom streamification may relocate the stack, so the
            // passed stream alias may be fixed, must propagate it upwards.
            ut->tss[i].streamify(cx);
        
            cx->op(POPQ, alias_addr + ADDRESS_SIZE * copy_count + ADDRESS_SIZE);
            cx->op(POPQ, R10);
            cx->op(ADDQ, RSP, ADDRESS_SIZE * copy_count);

            streamify_ascii(")", alias_addr, cx);
        }
        
        cx->op(JMP, end);
        
        cx->code_label(skip);
    }

    cx->runtime->die("Invalid Union!");

    cx->code_label(end);
    cx->epilogue();
}

Value *UnionType::lookup_initializer(TypeMatch tm, std::string n, Scope *scope) {
    if (n == "{") {
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

Value *UnionType::lookup_matcher(TypeMatch tm, std::string n, Value *pivot, Scope *s) {
    for (unsigned i = 0; i < tss.size(); i++) {
        if (n == tags[i])
            return make<UnionMatcherValue>(pivot, tm[0], tss[i], i);
    }

    return Type::lookup_matcher(tm, n, pivot, s);
}

void UnionType::type_info(TypeMatch tm, Cx *cx) {
    // This is actually better
    
    int flag_size = get_flag_size();
    unsigned flag_ts_index = cx->once->type_info(INTEGER_TS);  // needs ADDRESS_SIZE
    unsigned size = tm[0].measure_raw();
    Label label;
    unsigned discr_index = label.def_index;
    
    cx->dwarf->begin_structure_type_info(tm[0].symbolize(), size);
    
    cx->dwarf->begin_variant_part_info(discr_index);

    cx->dwarf->info_def(discr_index);
    cx->dwarf->member_info("<flag>", 0, flag_ts_index, true);
    
    for (unsigned i = 0; i < tss.size(); i++) {
        unsigned ts_index = cx->once->type_info(tss[i]);
        
        cx->dwarf->begin_variant_info(i);
        cx->dwarf->member_info("<" + tags[i] + ">", flag_size, ts_index, true);
        cx->dwarf->end_info();
    }
    
    cx->dwarf->end_info();
    
    cx->dwarf->end_info();
}
