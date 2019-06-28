#include "../plum.h"


RecordType::RecordType(std::string n, Metatypes param_metatypes)
    :Type(n, param_metatypes, record_metatype) {
    is_single = false;
    streamify_function = NULL;
}

bool RecordType::complete_type() {
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

    if (!has_custom_compare) {
        Declaration *d = new TemplateOperation<RecordOperationValue>("compare", COMPARE);
        inner_scope->add(d);
    }
    
    if (member_variables.size() == 1)
        is_single = true;
    
    std::cerr << "Record " << name << " has " << member_variables.size() << " member variables.\n";
    return true;
}

Allocation RecordType::measure(TypeMatch tm) {
    if (is_single)
        return typesubst(member_tss[0], tm).measure();
    else
        return inner_scope->get_size(tm);  // May round up
}

void RecordType::store(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    if (is_single) {
        typesubst(member_tss[0], tm).store(s, t, cx);
        return;
    }

    int stack_size = tm[0].measure_stack();
    
    switch (s.where * t.where) {
    case NOWHERE_STACK:
        cx->op(SUBQ, RSP, stack_size);
        create(tm, Storage(), Storage(MEMORY, Address(RSP, 0)), cx);
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
    case MEMORY_MEMORY:  // duplicates data
        for (auto &var : member_variables)
            var->store(tm, Storage(MEMORY, s.address), Storage(MEMORY, t.address), cx);
        return;
    default:
        Type::store(tm, s, t, cx);
    }
}

void RecordType::create(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    if (is_single) {
        typesubst(member_tss[0], tm).create(s, t, cx);
        return;
    }

    int stack_size = tm[0].measure_stack();

    switch (s.where * t.where) {
    case NOWHERE_MEMORY:
        for (auto &var : member_variables)
            var->create(tm, Storage(), Storage(MEMORY, t.address), cx);
        return;
    case STACK_MEMORY:
        cx->runtime->copy(Address(RSP, 0), t.address, tm[0].measure_raw());
        cx->op(ADDQ, RSP, stack_size);
        return;
    case MEMORY_MEMORY:  // duplicates data
        for (auto &var : member_variables)
            var->create(tm, Storage(MEMORY, s.address), Storage(MEMORY, t.address), cx);
        return;
    default:
        Type::create(tm, s, t, cx);
    }
}

void RecordType::destroy(TypeMatch tm, Storage s, Cx *cx) {
    if (is_single) {
        typesubst(member_tss[0], tm).destroy(s, cx);
        return;
    }

    if (s.where == MEMORY) {
        for (auto &var : member_variables)  // FIXME: reverse!
            var->destroy(tm, Storage(MEMORY, s.address), cx);
    }
    else
        Type::destroy(tm, s, cx);
}

StorageWhere RecordType::where(TypeMatch tm, AsWhat as_what) {
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

Storage RecordType::optimal_value_storage(TypeMatch tm, Regs preferred) {
    if (is_single) {
        return typesubst(member_tss[0], tm).optimal_value_storage(preferred);
    }

    return Storage(STACK);
}
    
unsigned RecordType::comparable_member_count() {
    return member_variables.size();
}

void RecordType::equal(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    if (is_single) {
        typesubst(member_tss[0], tm).equal(s, t, cx);
        return;
    }

    if (s.where == MEMORY && t.where == MEMORY) {
        Label end;
        if ((s.regs() | t.regs()) & EQUAL_CLOB)
            throw INTERNAL_ERROR;
        
        for (unsigned i = 0; i < comparable_member_count(); i++) {
            if (i > 0)
                cx->op(JNE, end);

            member_variables[i]->equal(tm, s, t, cx);
        }
        
        cx->code_label(end);
        return;
    }
    else
        throw INTERNAL_ERROR;
}

void RecordType::compare(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    if ((s.regs() | t.regs()) & COMPARE_CLOB)
        throw INTERNAL_ERROR;
        
    if (is_single) {
        typesubst(member_tss[0], tm).compare(s, t, cx);
        return;
    }

    if (s.where == MEMORY && t.where == MEMORY) {
        Label end;

        for (unsigned i = 0; i < comparable_member_count(); i++) {
            if (i > 0)
                cx->op(JNE, end);
                
            member_variables[i]->compare(tm, s, t, cx);
        }
        
        cx->code_label(end);
        return;
    }
    else
        throw INTERNAL_ERROR;
}

void RecordType::streamify(TypeMatch tm, Cx *cx) {
    if (streamify_function) {
        // The pivot is on the stack as rvalue, and the stream as lvalue.
        cx->op(CALL, streamify_function->get_label(cx));
    }
    else {
        Address value_addr(RSP, ALIAS_SIZE);
        Address alias_addr(RSP, 0);
        
        streamify_ascii("{", alias_addr, cx);
        
        bool did = false;
        
        for (auto v : member_variables) {
            if (did)
                streamify_ascii(",", alias_addr, cx);
            
            did = true;
            
            cx->op(LEA, RAX, value_addr);
            
            TypeSpec mts = v->get_typespec(tm);
            Storage s = v->get_storage(tm, Storage(MEMORY, Address(RAX, 0)));
            Storage t = Storage(STACK);
            mts.store(s, t, cx);
            cx->op(PUSHQ, 0);
            cx->op(PUSHQ, Address(RSP, mts.measure_stack() + ADDRESS_SIZE));
            
            // Invoking a custom streamification may relocate the stack, so the
            // passed stream alias may be fixed, must propagate it upwards.
            mts.streamify(cx);
            
            cx->op(POPQ, Address(RSP, mts.measure_stack() + ADDRESS_SIZE));
            cx->op(POPQ, R10);
            mts.store(t, Storage(), cx);
        }

        streamify_ascii("}", alias_addr, cx);
    }
}

Value *RecordType::lookup_initializer(TypeMatch tm, std::string n, Scope *scope) {
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

std::vector<TypeSpec> RecordType::get_member_tss(TypeMatch &match) {
    std::vector<TypeSpec> tss;
    
    for (auto &ts : member_tss)
        tss.push_back(typesubst(ts, match));
        
    return tss;
}

std::vector<std::string> RecordType::get_partial_initializable_names() {
    return member_names;
}

void RecordType::type_info(TypeMatch tm, Cx *cx) {
    // This must be a concrete parametrization
    unsigned size = measure(tm).concretize();
    
    cx->dwarf->begin_structure_type_info(tm[0].symbolize(), size);
    
    debug_inner_scopes(tm, cx);
    
    cx->dwarf->end_info();
}




StringType::StringType(std::string n)
    :RecordType(n, Metatypes {}) {
}

void StringType::equal(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    if (s.where == MEMORY && t.where == MEMORY) {
        if (t.address.base == RSP)
            throw INTERNAL_ERROR;
            
        cx->op(PUSHQ, s.address);
        cx->op(PUSHQ, t.address);
    }
    else if ((s.where != STACK) || (t.where != STACK))
        throw INTERNAL_ERROR;
    
    Label streq_label = cx->once->compile(compile_stringeq);
    cx->op(CALL, streq_label);  // ZF as expected
    
    if (s.where == MEMORY && t.where == MEMORY) {
        cx->op(LEA, RSP, Address(RSP, 2 * ADDRESS_SIZE));  // preserve ZF
    }
}

void StringType::compile_stringeq(Label label, Cx *cx) {
    cx->code_label_local(label, "String__equality");
    cx->prologue();
    Label sete, done;

    cx->op(PUSHQ, RAX);
    cx->op(PUSHQ, RCX);
    
    cx->op(MOVQ, RAX, Address(RSP, ADDRESS_SIZE + RIP_SIZE + 3 * ADDRESS_SIZE));
    cx->op(MOVQ, R10, Address(RSP, ADDRESS_SIZE + RIP_SIZE + 2 * ADDRESS_SIZE));
    
    cx->op(CMPQ, RAX, R10);
    cx->op(JE, done);  // identical, must be equal, ZF as expected
    
    cx->op(MOVQ, RCX, Address(RAX, LINEARRAY_LENGTH_OFFSET));
    cx->op(CMPQ, RCX, Address(R10, LINEARRAY_LENGTH_OFFSET));
    cx->op(JNE, done);  // different length, can't be equal, ZF as expected
    
    cx->op(PUSHQ, RSI);
    cx->op(PUSHQ, RDI);
    cx->op(LEA, RSI, Address(RAX, LINEARRAY_ELEMS_OFFSET));
    cx->op(LEA, RDI, Address(R10, LINEARRAY_ELEMS_OFFSET));
    cx->op(REPECMPSW);  // no flags set if RCX=0
    cx->op(POPQ, RDI);
    cx->op(POPQ, RSI);

    cx->op(CMPQ, RCX, 0);  // equal, if all compared, ZF as expected
    
    cx->code_label(done);

    cx->op(POPQ, RCX);
    cx->op(POPQ, RAX);
    
    cx->epilogue();
}

void StringType::compare(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    if (s.where == MEMORY && t.where == MEMORY) {
        if (s.address.base == RSP || t.address.base == RSP) {
            cx->op(MOVQ, R10, s.address);
            cx->op(MOVQ, R11, t.address);
            cx->op(PUSHQ, R10);
            cx->op(PUSHQ, R11);
        }
        else {
            cx->op(PUSHQ, s.address);
            cx->op(PUSHQ, t.address);
        }
    }
    else if ((s.where != STACK) || (t.where != STACK))
        throw INTERNAL_ERROR;

    Label strcmp_label = cx->once->compile(compile_stringcmp);
    cx->op(CALL, strcmp_label);  // R10B, flags as expected
    
    if (s.where == MEMORY && t.where == MEMORY) {
        cx->op(LEA, RSP, Address(RSP, 2 * ADDRESS_SIZE));  // preserve ZF
    }
}

void StringType::compile_stringcmp(Label label, Cx *cx) {
    // Expects arguments on the stack, returns R10B/flags.
    cx->code_label_local(label, "String__comparison");
    cx->prologue();
    
    cx->op(PUSHQ, RAX);
    cx->op(PUSHQ, RCX);
    cx->op(PUSHQ, RDX);
    cx->op(PUSHQ, RSI);
    cx->op(PUSHQ, RDI);
    
    Label s_longer, begin, end;
    cx->op(MOVQ, RAX, Address(RSP, ADDRESS_SIZE + RIP_SIZE + 6 * ADDRESS_SIZE));  // s
    cx->op(MOVQ, RDX, Address(RSP, ADDRESS_SIZE + RIP_SIZE + 5 * ADDRESS_SIZE));  // t
    
    cx->op(MOVB, R10B, 0);  // assume equality
    cx->op(MOVQ, RCX, Address(RAX, LINEARRAY_LENGTH_OFFSET));
    cx->op(CMPQ, RCX, Address(RDX, LINEARRAY_LENGTH_OFFSET));
    cx->op(JE, begin);
    cx->op(JA, s_longer);
    
    cx->op(MOVB, R10B, -1);  // s is shorter, on common equality s is less
    cx->op(JMP, begin);

    cx->code_label(s_longer);
    cx->op(MOVB, R10B, 1);  // s is longer, on common equality s is greater
    cx->op(MOVQ, RCX, Address(RDX, LINEARRAY_LENGTH_OFFSET));
    
    cx->code_label(begin);
    cx->op(LEA, RSI, Address(RAX, LINEARRAY_ELEMS_OFFSET));
    cx->op(LEA, RDI, Address(RDX, LINEARRAY_ELEMS_OFFSET));
    cx->op(CMPB, R10B, R10B);  // only to initialize flags for equality
    cx->op(REPECMPSW);  // no flags set if RCX=0
    
    cx->op(JE, end);  // common part was equal, result is according to preset R10B
    
    cx->runtime->r10bcompar(true);  // set R10B according to the detected difference
    
    cx->code_label(end);
    cx->op(CMPB, R10B, 0);  // must set flags even if R10B was preset
    
    cx->op(POPQ, RDI);
    cx->op(POPQ, RSI);
    cx->op(POPQ, RDX);
    cx->op(POPQ, RCX);
    cx->op(POPQ, RAX);
    
    cx->epilogue();
}

void StringType::streamify(TypeMatch tm, Cx *cx) {
    // Escaped and quoted
    Address alias_addr(RSP, 0);
    Label st_label = cx->once->compile(compile_esc_streamification);

    streamify_ascii("\"", alias_addr, cx);
    
    cx->op(CALL, st_label);  // clobbers all

    streamify_ascii("\"", alias_addr, cx);
}

void StringType::compile_esc_streamification(Label label, Cx *cx) {
    // RAX - target array, RCX - size, R10 - source array, R11 - alias
    Label char_str_label = cx->once->compile(CharacterType::compile_str_streamification);
    Label loop, check;
    Address value_addr(RSP, ADDRESS_SIZE + RIP_SIZE + ALIAS_SIZE);
    Address alias_addr(RSP, ADDRESS_SIZE + RIP_SIZE);
    
    cx->code_label_local(label, "String__esc_streamification");
    
    cx->prologue();
    
    cx->op(MOVQ, RCX, 0);
    cx->op(JMP, check);
    
    cx->code_label(loop);
    cx->op(PUSHQ, RCX);
    cx->op(MOVW, R10W, Address(RAX, RCX, Address::SCALE_2, LINEARRAY_ELEMS_OFFSET));
    cx->op(PUSHQ, R10);
    cx->op(PUSHQ, 0);
    cx->op(PUSHQ, alias_addr + 3 * ADDRESS_SIZE);
    cx->op(CALL, char_str_label);  // clobbers all
    cx->op(ADDQ, RSP, ADDRESS_SIZE + ALIAS_SIZE);
    cx->op(POPQ, RCX);
    cx->op(INCQ, RCX);
    
    cx->code_label(check);
    cx->op(MOVQ, RAX, value_addr);  // reference to the string
    cx->op(CMPQ, RCX, Address(RAX, LINEARRAY_LENGTH_OFFSET));
    cx->op(JB, loop);
    
    cx->epilogue();
}

Value *StringType::lookup_initializer(TypeMatch tm, std::string n, Scope *scope) {
    std::cerr << "No String initializer " << n << "!\n";
    return NULL;
}

Value *StringType::lookup_matcher(TypeMatch tm, std::string n, Value *pivot, Scope *scope) {
    if (n == "re")
        return make<StringRegexpMatcherValue>(pivot, tm);
        
    return RecordType::lookup_matcher(tm, n, pivot, scope);
}




SliceType::SliceType(std::string n)
    :RecordType(n, Metatypes { value_metatype }) {
}

Value *SliceType::lookup_initializer(TypeMatch tm, std::string n, Scope *scope) {
    if (n == "empty")
        return make<SliceEmptyValue>(tm);
    else if (n == "all")
        return make<SliceAllValue>(tm);
    
    std::cerr << "No Slice initializer " << n << "!\n";
    return NULL;
}




ItemType::ItemType(std::string n)
    :RecordType(n, Metatypes { value_metatype, value_metatype }) {
}

unsigned ItemType::comparable_member_count() {
    return 1;
}


