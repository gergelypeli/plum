

// Once

Label Once::compile(FunctionCompiler fc) {
    return function_compilers.add(fc);
}


Label Once::compile(TypedFunctionCompiler tfc, TypeSpec ts) {
    return typed_function_compilers.add(make_pair(tfc, ts));
}


Label Once::compile(Deferrable *d) {
    return deferrables.add(d);
}


Label Once::import_got(std::string name) {
    return import_gots.add(name);
}


unsigned Once::type_info(TypeSpec ts) {
    if (ts == NO_TS)
        throw INTERNAL_ERROR;
        
    return type_die_offsets.add(ts).def_index;
}


void Once::for_all(X64 *x64) {
    // NOTE: once-functions may ask to once-compile other functions, so we must keep
    // on checking until no new compilations are requested.
    
    bool was_dirty = true;
    
    while (was_dirty) {
        was_dirty = false;
        
        while (function_compilers.is_dirty()) {
            auto kv = function_compilers.take();
            FunctionCompiler fc = kv.first;
            Label label = kv.second;
            
            fc(label, x64);
            was_dirty = true;
        }
        
        while (typed_function_compilers.is_dirty()) {
            auto kv = typed_function_compilers.take();
            TypedFunctionCompiler tfc = kv.first.first;
            TypeSpec ts = kv.first.second;
            Label label = kv.second;
            
            tfc(label, ts, x64);
            was_dirty = true;
        }
        
        while (deferrables.is_dirty()) {
            auto kv = deferrables.take();
            Deferrable *d = kv.first;
            Label label = kv.second;

            d->deferred_compile(label, x64);
            was_dirty = true;
        }
        
        while (import_gots.is_dirty()) {
            auto kv = import_gots.take();
            std::string name = kv.first;
            Label label = kv.second;
        
            Label shared_label;
            x64->code_label_import(shared_label, name);
        
            // symbol points to the function address in the data segment
            x64->data_label_local(label, name + "@GOT");
            x64->data_reference(shared_label);
            was_dirty = true;
        }
    }
}


void Once::for_debug(X64 *x64) {
    while (type_die_offsets.is_dirty()) {
        auto kv = type_die_offsets.take();
        TypeSpec ts = kv.first;
        Label label = kv.second;
        unsigned index = label.def_index;
        
        x64->dwarf->info_def(index);
        //std::cerr << "XXX type_info " << ts << "\n";
        ts.type_info(x64);
    }
}


// Unwind

void Unwind::push(Value *v) {
    stack.push_back(v);
}


void Unwind::pop(Value *v) {
    if (v != stack.back())
        throw INTERNAL_ERROR;
        
    stack.pop_back();
}


void Unwind::initiate(Declaration *last, X64 *x64) {
    int old_stack_usage = x64->accounting->mark();

    for (int i = stack.size() - 1; i >= 0; i--) {
        Scope *s = stack[i]->unwind(x64);
        
        if (s) {
            if (s != last->outer_scope)
                throw INTERNAL_ERROR;
                
            last->jump_to_finalization(x64);
            x64->accounting->rewind(old_stack_usage);
            
            return;
        }
    }
    
    throw INTERNAL_ERROR;
}



// Stack accounting

Accounting::Accounting() {
    am_on = false;
}


void Accounting::start() {
    if (am_on)
        throw ASM_ERROR;
        
    am_on = true;
    current_stack_usage = 0;
    highest_stack_usage = 0;

    //std::cerr << "XXX stack usage start\n";
}


int Accounting::stop() {
    if (!am_on)
        throw ASM_ERROR;
        
    if (current_stack_usage != 0)
        throw ASM_ERROR;
        
    am_on = false;
    //std::cerr << "XXX stack usage highest " << highest_stack_usage << "\n";

    return highest_stack_usage;
}


bool Accounting::pause() {
    bool old_on = am_on;
    am_on = false;
    return old_on;
}


void Accounting::unpause(bool old_on) {
    am_on = old_on;
}


int Accounting::mark() {
    return current_stack_usage;
}


void Accounting::rewind(int old_stack_usage) {
    if (old_stack_usage > highest_stack_usage)
        throw ASM_ERROR;
        
    current_stack_usage = old_stack_usage;
}


bool Accounting::is_on() {
    return am_on;
}


void Accounting::adjust_stack_usage(int mod) {
    current_stack_usage += mod;
    //std::cerr << "XXX stack usage " << mod << " => " << current_stack_usage << "\n";
    
    if (current_stack_usage < 0)
        throw ASM_ERROR;
    
    if (current_stack_usage > highest_stack_usage)
        highest_stack_usage = current_stack_usage;
}



// Runtime

Runtime::Runtime(X64 *x, unsigned application_size) {
    x64 = x;
    
    x64->code_label_global(code_start_label, "code_start");
    x64->data_label_global(data_start_label, "data_start");
    
    std::cerr << "Application size is " << application_size << " bytes.\n";
    x64->data_align(16);
    x64->data_label(application_label);
    for (unsigned i = 0; i < application_size; i++)
        x64->data_byte(0);
    
    x64->absolute_label(zero_label, 0);

    // Float constants are aligned to 16 bytes so SSE packed instructions can use it
    x64->data_align(16);
    x64->data_label(float_zero_label);
    x64->data_double(0.0);
    x64->data_double(0.0);

    x64->data_align(16);
    x64->data_label(float_minus_zero_label);
    x64->data_double(-0.0);
    x64->data_double(0.0);

    data_heap_header();
    x64->data_label(empty_array_label);
    x64->data_qword(0);  // reservation
    x64->data_qword(0);  // length

    x64->data_label(start_frame_label);
    x64->data_qword(0);
    
    x64->data_label(task_stack_address_label);
    x64->data_qword(0);

    x64->data_label(task_stack_size_label);
    x64->data_qword(0);

    x64->data_label_global(allocation_count_label, "allocation_count");
    x64->data_qword(0);

    x64->data_label_global(refcount_balance_label, "refcount_balance");
    x64->data_qword(0);

    x64->code_label_global(empty_function_label, "empty_function");
    x64->op(RET);

    die_unmatched_message_label = data_heap_string(decode_utf8("Fatal unmatched value: "));

    x64->code_label_import(sysv_malloc_label, "C__malloc");
    x64->code_label_import(sysv_aligned_alloc_label, "C__aligned_alloc");
    x64->code_label_import(sysv_free_label, "C__free");
    x64->code_label_import(sysv_realloc_label, "C__realloc");
    x64->code_label_import(sysv_mprotect_label, "C__mprotect");
    x64->code_label_import(sysv_memcpy_label, "C__memcpy");
    x64->code_label_import(sysv_memmove_label, "C__memmove");
    
    x64->code_label_import(sysv_log_label, "C__log");
    x64->code_label_import(sysv_logref_label, "C__logref");
    x64->code_label_import(sysv_dump_label, "C__dump");
    x64->code_label_import(sysv_die_label, "C__die");
    x64->code_label_import(sysv_dies_label, "C__dies");
    x64->code_label_import(sysv_die_uncaught_label, "C__die_uncaught");
    
    x64->code_label_import(sysv_sort_label, "C__sort");
    x64->code_label_import(sysv_string_regexp_match_label, "C__string_regexp_match");
    
    x64->code_label_import(sysv_streamify_integer_label, "C__streamify_integer");
    x64->code_label_import(sysv_streamify_unteger_label, "C__streamify_unteger");
    x64->code_label_import(sysv_streamify_boolean_label, "C__streamify_boolean");
    x64->code_label_import(sysv_streamify_float_label, "C__streamify_float");
    x64->code_label_import(sysv_streamify_pointer_label, "C__streamify_pointer");

    compile_incref_decref();
    compile_finalize();
    compile_heap_alloc();
    compile_heap_realloc();
    compile_fcb_alloc();
    compile_fcb_free();
    compile_finalize_reference_array();
    compile_lookup_source_info();
    compile_lookup_call_info();
    compile_lookup_frame_info();
    compile_caller_frame_info();
    compile_fix_stack();
    compile_double_stack();
    compile_call_sysv();
    
    compile_logging();
}

void Runtime::data_heap_header() {
    if (HEAP_HEADER_SIZE != 32 || HEAP_REFCOUNT_OFFSET != -16 || HEAP_FINALIZER_OFFSET != -24)
        throw ASM_ERROR;

    x64->data_align(8);
    x64->data_qword(0);  // next
    x64->data_reference(empty_function_label);  // finalizer
    x64->data_qword(1);  // artificial reference to prevent freeing
    x64->data_qword(0);  // padding, used to be weakrefcount
}

Label Runtime::data_heap_string(std::ustring characters) {
    if (LINEARRAY_HEADER_SIZE != 16 || LINEARRAY_RESERVATION_OFFSET != 0 || LINEARRAY_LENGTH_OFFSET != 8)
        throw ASM_ERROR;
    
    Label l;

    data_heap_header();
    x64->data_label(l);
    x64->data_qword(characters.size());
    x64->data_qword(characters.size());

    for (unsigned16 &c : characters)
        x64->data_word(c);

    return l;
}

void Runtime::call_sysv(Label l) {
    x64->op(LEA, R10, Address(l, 0));
    x64->op(CALL, call_sysv_label);
}

void Runtime::call_sysv_got(Label got_l) {
    x64->op(MOVQ, R10, Address(got_l, 0));
    x64->op(CALL, call_sysv_label);
}

void Runtime::compile_source_infos(std::vector<std::string> source_file_names) {
    x64->data_label(source_infos_length_label);
    x64->data_qword(source_file_names.size());
    
    std::vector<Label> labels;

    for (auto &source_file_name : source_file_names) {
        labels.push_back(data_heap_string(decode_utf8(source_file_name)));
    }
    
    x64->data_align(8);
    x64->data_label_global(source_infos_label, "source_infos");
    
    for (auto &label : labels) {
        x64->data_reference(label);
    }
}

void Runtime::add_func_info(std::string name, Label start, Label end) {
    func_infos.push_back({ name, start, end, Label() });
}

void Runtime::compile_func_infos() {
    x64->data_label(func_infos_length_label);
    x64->data_qword(func_infos.size());
    
    for (auto &fi : func_infos) {
        fi.name_label = x64->runtime->data_heap_string(decode_utf8(fi.name));
    }
    
    x64->data_align(8);
    x64->data_label_global(func_infos_label, "func_infos");
    
    for (auto &fi : func_infos) {
        x64->data_reference(fi.start_label);
        x64->data_reference(fi.end_label);
        x64->data_reference(fi.name_label);
    }
}

void Runtime::add_call_info(int file_index, int line_number) {
    call_infos.push_back(LineInfo { x64->get_pc(), file_index, line_number });
}

void Runtime::compile_call_infos() {
    x64->data_label(call_infos_length_label);
    x64->data_qword(call_infos.size());
    
    x64->data_align(8);
    x64->data_label_global(call_infos_label, "call_infos");
    
    for (auto &ci : call_infos) {
        x64->data_dword(ci.address);
        x64->data_word(ci.file_index);
        x64->data_word(ci.line_number);
    }
}

int Runtime::pusha(bool except_rax) {
    // RBX and the last 4 are preserved by the System V ABI
    // But we save RBX, because it's annoying to handle it separately
    // 10 registers total, including RAX

    if (!except_rax)
        x64->op(PUSHQ, RAX);
    
    x64->op(PUSHQ, RBX);
    x64->op(PUSHQ, RCX);
    x64->op(PUSHQ, RDX);
    x64->op(PUSHQ, RSI);
    x64->op(PUSHQ, RDI);
    x64->op(PUSHQ, R8);
    x64->op(PUSHQ, R9);
    x64->op(PUSHQ, R10);
    x64->op(PUSHQ, R11);

    return except_rax ? 72 : 80;
}

void Runtime::popa(bool except_rax) {
    x64->op(POPQ, R11);
    x64->op(POPQ, R10);
    x64->op(POPQ, R9);
    x64->op(POPQ, R8);
    x64->op(POPQ, RDI);
    x64->op(POPQ, RSI);
    x64->op(POPQ, RDX);
    x64->op(POPQ, RCX);
    x64->op(POPQ, RBX);

    if (!except_rax)
        x64->op(POPQ, RAX);
}

void Runtime::compile_finalize() {
    // finalize(pointer)
    // Preserves all registers, including the scratch ones
    
    x64->code_label_global(finalize_label, "finalize");
    const int ARGS = pusha() + ARGS_1;
    Label fcb_loop, fcb_cond;
    
    //log("Finalizing heap object.");
    x64->op(MOVQ, RAX, Address(RSP, ARGS + ARG_1));  // pointer
    x64->op(MOVQ, RAX, Address(RAX, HEAP_NEXT_OFFSET));  // FCB
    x64->op(JMP, fcb_cond);

    x64->code_label(fcb_loop);
    log("Triggering finalizer callback.");
    x64->op(PUSHQ, RAX);

    // Invoke FCB    
    x64->op(PUSHQ, Address(RAX, FCB_PAYLOAD1_OFFSET));
    x64->op(PUSHQ, Address(RAX, FCB_PAYLOAD2_OFFSET));
    x64->op(CALL, Address(RAX, FCB_CALLBACK_OFFSET));  // clobbers all
    x64->op(ADDQ, RSP, 2 * ADDRESS_SIZE);

    // Free FCB
    x64->op(POPQ, RDI);
    x64->op(PUSHQ, Address(RDI, FCB_NEXT_OFFSET));  // advance before free
    call_sysv(sysv_free_label);  // clobbers all
    x64->op(POPQ, RAX);

    x64->code_label(fcb_cond);
    x64->op(CMPQ, RAX, FCB_NIL);
    x64->op(JNE, fcb_loop);

    // Invoke finalizer
    //dump("finalizing");
    x64->op(MOVQ, RAX, Address(RSP, ARGS + ARG_1));  // pointer
    x64->op(PUSHQ, RAX);
    x64->op(CALL, Address(RAX, HEAP_FINALIZER_OFFSET));  // finalizers may clobber everything

    x64->op(POPQ, RDI);
    x64->op(SUBQ, RDI, HEAP_HEADER_SIZE);
    call_sysv(sysv_free_label);  // will probably clobber everything

    popa();
    x64->op(RET);
}

void Runtime::compile_heap_alloc() {
    // heap_alloc(size, finalizer)
    // Clobbers all registers
    
    x64->code_label_global(heap_alloc_label, "heap_alloc");
    const int ARGS = ARGS_2;
    
    x64->op(MOVQ, RDI, Address(RSP, ARGS + ARG_1));  // size arg
    x64->op(ADDQ, RDI, HEAP_HEADER_SIZE);
    call_sysv(sysv_malloc_label);
    
    x64->op(ADDQ, RAX, HEAP_HEADER_SIZE);
    x64->op(MOVQ, RBX, Address(RSP, ARGS + ARG_2));  // finalizer arg
    //dump("heap_alloc");
    
    x64->op(MOVQ, Address(RAX, HEAP_NEXT_OFFSET), 0);
    x64->op(MOVQ, Address(RAX, HEAP_FINALIZER_OFFSET), RBX);
    x64->op(MOVQ, Address(RAX, HEAP_REFCOUNT_OFFSET), 1);  // start from 1
    
    x64->op(INCQ, Address(refcount_balance_label, 0));  // necessary to keep the balance
    
    x64->op(RET);
}

void Runtime::compile_heap_realloc() {
    // heap_realloc(pointer, new_size)
    // Clobbers all registers

    x64->code_label_global(heap_realloc_label, "heap_realloc");
    const int ARGS = ARGS_2;
    Label realloc_ok;
    
    x64->op(MOVQ, RDI, Address(RSP, ARGS + ARG_1));  // pointer arg
    check_unshared(RDI);
    x64->op(JE, realloc_ok);
    
    die("Realloc of shared array!");

    x64->code_label(realloc_ok);
    x64->op(SUBQ, RDI, HEAP_HEADER_SIZE);
    x64->op(MOVQ, RSI, Address(RSP, ARGS + ARG_2));  // new_size arg
    x64->op(ADDQ, RSI, HEAP_HEADER_SIZE);
    call_sysv(sysv_realloc_label);
    
    x64->op(ADDQ, RAX, HEAP_HEADER_SIZE);
    x64->op(RET);
}

void Runtime::compile_fcb_alloc() {
    // fcb_alloc(pointer, callback, payload1, payload2)
    // Clobbers all registers

    x64->code_label_global(fcb_alloc_label, "fcb_alloc");
    const int ARGS = ARGS_4;

    x64->op(MOVQ, RDI, FCB_SIZE);
    call_sysv(sysv_malloc_label);
    
    x64->op(MOVQ, RBX, Address(RSP, ARGS + ARG_2));  // callback arg
    x64->op(MOVQ, Address(RAX, FCB_CALLBACK_OFFSET), RBX);
    x64->op(MOVQ, RBX, Address(RSP, ARGS + ARG_3));  // payload1 arg
    x64->op(MOVQ, Address(RAX, FCB_PAYLOAD1_OFFSET), RBX);
    x64->op(MOVQ, RBX, Address(RSP, ARGS + ARG_4));  // payload2 arg
    x64->op(MOVQ, Address(RAX, FCB_PAYLOAD2_OFFSET), RBX);
    
    x64->op(MOVQ, RBX, Address(RSP, ARGS + ARG_1));  // pointer arg
    x64->op(MOVQ, RCX, Address(RBX, HEAP_NEXT_OFFSET));
    x64->op(MOVQ, Address(RAX, FCB_NEXT_OFFSET), RCX);
    x64->op(MOVQ, Address(RBX, HEAP_NEXT_OFFSET), RAX);
    x64->op(ADDQ, RBX, HEAP_HEADER_OFFSET);
    x64->op(MOVQ, Address(RAX, FCB_PREV_OFFSET), RBX);
    //dump("alloc_fcb RAX=fcb, RBX=prev, RCX=next");

    Label no_next;
    x64->op(CMPQ, RCX, FCB_NIL);
    x64->op(JE, no_next);
    x64->op(MOVQ, Address(RCX, FCB_PREV_OFFSET), RAX);
    x64->code_label(no_next);

    x64->op(RET);
}

void Runtime::compile_fcb_free() {
    // fcb_free(pointer, callback, payload1, payload2)
    // Clobbers all registers
    
    x64->code_label_global(fcb_free_label, "fcb_free");
    Label check, loop, nope, no_next;
    const int ARGS = ARGS_4;

    x64->op(MOVQ, RAX, Address(RSP, ARGS + ARG_1));  // object
    x64->op(MOVQ, RBX, Address(RSP, ARGS + ARG_2));  // callback
    x64->op(MOVQ, RCX, Address(RSP, ARGS + ARG_3));  // payload1
    x64->op(MOVQ, RDX, Address(RSP, ARGS + ARG_4));  // payload2
    
    x64->op(MOVQ, RAX, Address(RAX, HEAP_NEXT_OFFSET));  // FCB
    x64->op(JMP, check);

    x64->code_label(loop);
    x64->op(CMPQ, Address(RAX, FCB_CALLBACK_OFFSET), RBX);
    x64->op(JNE, nope);
    x64->op(CMPQ, Address(RAX, FCB_PAYLOAD1_OFFSET), RCX);
    x64->op(JNE, nope);
    x64->op(CMPQ, Address(RAX, FCB_PAYLOAD2_OFFSET), RDX);
    x64->op(JNE, nope);
    
    // Found the FCB
    x64->op(MOVQ, RBX, Address(RAX, FCB_PREV_OFFSET));  // always valid
    x64->op(MOVQ, RCX, Address(RAX, FCB_NEXT_OFFSET));
    x64->op(MOVQ, Address(RBX, FCB_NEXT_OFFSET), RCX);
    x64->op(CMPQ, RCX, FCB_NIL);
    x64->op(JE, no_next);
    x64->op(MOVQ, Address(RCX, FCB_PREV_OFFSET), RBX);
    x64->code_label(no_next);

    x64->op(MOVQ, RDI, RAX);
    call_sysv(sysv_free_label);  // clobbers all

    x64->op(RET);
    
    x64->code_label(nope);
    x64->op(MOVQ, RAX, Address(RAX, FCB_NEXT_OFFSET));
    
    x64->code_label(check);
    x64->op(CMPQ, RAX, FCB_NIL);
    x64->op(JNE, loop);
    
    // Not found
    die("FCB not found!");
}

void Runtime::compile_finalize_reference_array() {
    // finalize_reference_array(pointer)
    // Clobbers all registers

    x64->code_label_global(finalize_reference_array_label, "finalize_reference_array");
    Label fra_cond, fra_loop;
    const int ARGS = ARGS_1;
    
    x64->op(MOVQ, RAX, Address(RSP, ARGS + ARG_1));
    x64->op(MOVQ, RCX, 0);
    x64->op(JMP, fra_cond);

    x64->code_label(fra_loop);
    x64->op(MOVQ, RBX, Address(RAX, RCX, Address::SCALE_8, LINEARRAY_ELEMS_OFFSET));
    decref(RBX);
    x64->op(INCQ, RCX);

    x64->code_label(fra_cond);
    x64->op(CMPQ, RCX, Address(RAX, LINEARRAY_LENGTH_OFFSET));
    x64->op(JB, fra_loop);

    x64->op(RET);
}

void Runtime::compile_incref_decref() {
    incref_labels.resize(REGISTER_COUNT);
    decref_labels.resize(REGISTER_COUNT);

    for (Register reg : { RAX, RBX, RCX, RDX, RSI, RDI, R8, R9, R10, R11, R12, R13, R14, R15 }) {
        Label il;
        // We use a standard stack frame only for debugging, should be cleaned up later
    
        // NOTE: preserves all registers, including the scratch ones
        x64->code_label_global(incref_labels[reg], std::string("incref_") + REGISTER_NAMES[reg]);
        x64->op(PUSHQ, RBP);
        x64->op(MOVQ, RBP, RSP);

        x64->op(INCQ, Address(reg, HEAP_REFCOUNT_OFFSET));
        //logref(std::string("INCREF ") + REGISTER_NAMES[reg], reg);

        x64->op(INCQ, Address(refcount_balance_label, 0));
        x64->op(POPQ, RBP);
        x64->op(RET);
    
        Label dl, dl2;
    
        // NOTE: preserves all registers, including the scratch ones
        x64->code_label_global(decref_labels[reg], std::string("decref_") + REGISTER_NAMES[reg]);
        x64->op(PUSHQ, RBP);
        x64->op(MOVQ, RBP, RSP);

        x64->op(DECQ, Address(reg, HEAP_REFCOUNT_OFFSET));
        //logref(std::string("DECREF ") + REGISTER_NAMES[reg], reg);
        x64->op(JNE, dl);

        x64->op(PUSHQ, reg);
        x64->op(CALL, finalize_label);
        x64->op(POPQ, reg);

        x64->code_label(dl);
        x64->op(DECQ, Address(refcount_balance_label, 0));
        x64->op(POPQ, RBP);
        x64->op(RET);
    }
}

void Runtime::compile_lookup_source_info() {
    // lookup_source_info(source_file_index)
    // result - RAX = String name
    // clobbers RAX, RBX, RCX
    
    x64->code_label_global(lookup_source_info_label, "lookup_source_info");
    Label found;

    x64->op(MOVQ, RBX, Address(RSP, ADDRESS_SIZE));  // index
    x64->op(LEA, RAX, Address(source_infos_label, 0));
    x64->op(MOVQ, RCX, Address(source_infos_length_label, 0));
    
    x64->op(CMPD, RBX, RCX);
    x64->op(JB, found);
    
    x64->op(MOVQ, RBX, 0);
    
    x64->code_label(found);
    x64->op(MOVQ, RAX, Address(RAX, RBX, Address::SCALE_8, 0));
    incref(RAX);

    x64->op(RET);
}

void Runtime::compile_lookup_call_info() {
    // lookup_call_info(rip)
    // result - RAX = pointer to the call info structure, or NULL
    // clobbers RAX, RBX, RCX
    
    x64->code_label_global(lookup_call_info_label, "lookup_call_info");
    Label loop, skip, end;

    x64->op(LEA, RBX, Address(code_start_label, 0));
    x64->op(NEGQ, RBX);
    x64->op(ADDQ, RBX, Address(RSP, ADDRESS_SIZE));  // relative RIP within the code segment
    x64->op(LEA, RAX, Address(call_infos_label, 0));
    x64->op(MOVQ, RCX, Address(call_infos_length_label, 0));
    
    x64->code_label(loop);
    x64->op(CMPD, EBX, Address(RAX, 0));
    x64->op(JE, end);
    
    x64->op(ADDQ, RAX, 8);
    x64->op(DECQ, RCX);
    x64->op(JNE, loop);
    
    // Not found
    x64->op(MOVQ, RAX, 0);
    
    x64->code_label(end);
    x64->op(RET);
}

void Runtime::compile_lookup_frame_info() {
    // lookup_frame_info(rip)
    // result - RAX = pointer to the frame info structure, or NULL
    // clobbers RAX, RBX, RCX
    
    x64->code_label_global(lookup_frame_info_label, "lookup_frame_info");
    Label loop, skip, end;

    x64->op(MOVQ, RBX, Address(RSP, ADDRESS_SIZE));
    x64->op(LEA, RAX, Address(func_infos_label, 0));
    x64->op(MOVQ, RCX, Address(func_infos_length_label, 0));
    
    x64->code_label(loop);
    x64->op(CMPQ, RBX, Address(RAX, FRAME_INFO_START_OFFSET));
    x64->op(JB, skip);
    x64->op(CMPQ, RBX, Address(RAX, FRAME_INFO_END_OFFSET));
    x64->op(JAE, skip);
    
    // Found the frame
    x64->op(JMP, end);
    
    x64->code_label(skip);
    x64->op(ADDQ, RAX, FRAME_INFO_SIZE);
    x64->op(DECQ, RCX);
    x64->op(JNE, loop);
    
    // Not found
    x64->op(MOVQ, RAX, 0);
    
    x64->code_label(end);
    x64->op(RET);
}

void Runtime::compile_caller_frame_info() {
    // caller_frame_info(up_count)
    // result - RAX = pointer to the frame info structure, or NULL
    //          RBX = pointer to the call info structure, or NULL
    // clobbers RAX, RBX, RCX
    
    Label up, lookup, nope, end;
    
    x64->code_label_global(caller_frame_info_label, "caller_frame_info");
    x64->op(PUSHQ, RBP);
    x64->op(MOVQ, RCX, Address(RSP, 2 * ADDRESS_SIZE));
    
    x64->op(MOVQ, RBX, RSP);
    x64->op(CMPQ, RCX, 0);
    x64->op(JE, lookup);
    
    x64->code_label(up);
    x64->op(MOVQ, RBX, Address(RBX, 0));
    x64->op(CMPQ, RBX, Address(start_frame_label, 0));  // Hit the top frame
    x64->op(JE, nope);
    x64->op(DECQ, RCX);
    x64->op(JNE, up);
    
    x64->code_label(lookup);
    x64->op(PUSHQ, 0);
    x64->op(PUSHQ, Address(RBX, ADDRESS_SIZE));  // caller RIP

    x64->op(CALL, lookup_call_info_label);
    x64->op(MOVQ, Address(RSP, ADDRESS_SIZE), RAX);
    x64->op(CALL, lookup_frame_info_label);
    
    x64->op(ADDQ, RSP, ADDRESS_SIZE);
    x64->op(POPQ, RBX);
    x64->op(JMP, end);
    
    x64->code_label(nope);
    x64->op(MOVQ, RAX, 0);
    x64->op(MOVQ, RBX, 0);
    
    x64->code_label(end);
    x64->op(POPQ, RBP);
    x64->op(RET);
}

void Runtime::fix_address(Address address) {
    Label skip;
    
    x64->op(CMPQ, address, RSI);
    x64->op(JB, skip);
    x64->op(CMPQ, address, RDI);
    x64->op(JAE, skip);
    x64->op(ADDQ, address, RDX);
    
    x64->code_label(skip);
}

void Runtime::compile_fix_stack() {
    // fix_stack(old_bottom, old_top, relocation)
    // clobbers all registers
    // must be run on the new stack, with relocated RBP and RSP
    // must be called from a helper function with a proper frame, so the current [RBP + 8]
    // points to the first app function that needs fixing.
    
    x64->code_label_global(fix_stack_label, "fix_stack");
    x64->op(PUSHQ, RBP);  // just saving, the caller must have a proper frame
    
    x64->op(MOVQ, RSI, Address(RSP, 4 * ADDRESS_SIZE));  // old_bottom
    x64->op(MOVQ, RDI, Address(RSP, 3 * ADDRESS_SIZE));  // old_top
    x64->op(MOVQ, RDX, Address(RSP, 2 * ADDRESS_SIZE));  // relocation

    Label loop, end;
    x64->code_label(loop);
    
    // find caller info
    x64->op(PUSHQ, Address(RBP, ADDRESS_SIZE));
    x64->op(CALL, lookup_frame_info_label);
    x64->op(ADDQ, RSP, ADDRESS_SIZE);
    
    // up to caller frame
    x64->op(MOVQ, RBX, Address(RBP, 0));
    x64->op(CMPQ, RBX, Address(start_frame_label, 0));
    x64->op(JE, end);
    
    x64->op(ADDQ, RBX, RDX);
    x64->op(MOVQ, Address(RBP, 0), RBX);
    x64->op(MOVQ, RBP, RBX);
    
    // invoke fix
    x64->op(CALL, Address(RAX, FRAME_INFO_END_OFFSET));
    
    x64->op(JMP, loop);
    
    x64->code_label(end);
    x64->op(POPQ, RBP);
    x64->op(RET);
}

void Runtime::compile_double_stack() {
    // This function is invoked on the task stack
    // clobbers all registers
    // must use a stack frame for the stack fix code
    
    x64->code_label_global(double_stack_label, "double_stack");
    x64->op(PUSHQ, RBP);
    x64->op(MOVQ, RBP, RSP);

    // Allocate new stack    
    x64->op(MOVQ, RDI, PAGE_SIZE);
    x64->op(MOVQ, RSI, Address(task_stack_size_label, 0));
    x64->op(SHLQ, RSI, 2);
    call_sysv(sysv_aligned_alloc_label);
    
    // Compute relocation offset of the stack tops (RBX is callee saved)
    x64->op(MOVQ, RBX, RAX);
    x64->op(ADDQ, RBX, Address(task_stack_size_label, 0));
    x64->op(SUBQ, RBX, Address(task_stack_address_label, 0));
    
    x64->op(MOVQ, RDI, RAX);
    x64->op(MOVQ, RSI, PAGE_SIZE);
    x64->op(MOVQ, RDX, PROT_NONE);
    call_sysv(sysv_mprotect_label);

    // Copy stack contents to the upper half of the new stack (skipping the guard page)
    x64->op(MOVQ, RDI, Address(task_stack_address_label, 0));
    x64->op(ADDQ, RDI, RBX);
    x64->op(ADDQ, RDI, PAGE_SIZE);
    x64->op(MOVQ, RSI, Address(task_stack_address_label, 0));
    x64->op(ADDQ, RSI, PAGE_SIZE);
    x64->op(MOVQ, RDX, Address(task_stack_size_label, 0));
    x64->op(SUBQ, RDX, PAGE_SIZE);
    call_sysv(sysv_memcpy_label);

    // Switch stacks
    x64->op(ADDQ, RSP, RBX);
    x64->op(ADDQ, RBP, RBX);

    // Drop old stack
    x64->op(MOVQ, RDI, Address(task_stack_address_label, 0));
    x64->op(MOVQ, RSI, PAGE_SIZE);
    x64->op(MOVQ, RDX, PROT_RW);
    call_sysv(sysv_mprotect_label);
    
    x64->op(MOVQ, RDI, Address(task_stack_address_label, 0));
    call_sysv(sysv_free_label);

    // Fix new stack
    x64->op(MOVQ, R10, Address(task_stack_address_label, 0));
    x64->op(PUSHQ, R10);  // old_bottom
    x64->op(ADDQ, R10, Address(task_stack_size_label, 0));
    x64->op(PUSHQ, R10);  // old_top
    x64->op(PUSHQ, RBX);  // relocation offset
    x64->op(CALL, fix_stack_label);
    x64->op(POPQ, RBX);
    x64->op(ADDQ, RSP, 2 * ADDRESS_SIZE);
    
    // Update info
    x64->op(SUBQ, RBX, Address(task_stack_size_label, 0));
    x64->op(ADDQ, Address(task_stack_address_label, 0), RBX);
    x64->op(SHLQ, Address(task_stack_size_label, 0), 1);
    
    // That should be it
    x64->op(POPQ, RBP);
    x64->op(RET);
}


void Runtime::compile_call_sysv() {
    // We need this indirection, because we're changing the stack back to the system
    // stack. If the code drops a core dump while in an external function, and someone
    // wants to analyze it with gdb, it starts whining when asked for a backtrace,
    // because the stack frame addresses are no longer monotonic, so the calls preceding
    // this point don't show up. There's no clean way to solve this, only a hack within
    // gdb, which checks if the function messing with the stack is called __morestack.
    // This is because the gcc split stacks are implemented this way. So right now the
    // only way to beat proper backtraces out of gdb is to call this function like that.
    
    // Also note that a SysV functions expects 16 bytes stack alignment, but since we're
    // just reusing the old stack, it is aligned that way.
    
    // And this only works with arguments all passed in registers.
    
    // And expects the callee address in R10.
    
    x64->code_label_local(call_sysv_label, "__morestack");
    x64->op(PUSHQ, RBP);
    x64->op(MOVQ, RBP, RSP);
    
    x64->op(MOVQ, RSP, Address(start_frame_label, 0));
    x64->op(CALL, R10);
    x64->op(MOVQ, RSP, RBP);
    
    x64->op(POPQ, RBP);
    x64->op(RET);
}


void Runtime::compile_start(Storage main_storage, std::vector<Label> initializer_labels, std::vector<Label> finalizer_labels) {
    // Invoked from Root after gathering the necessary pieces
    Label start;
    x64->code_label_global(start, "start");

    // Be nice to debuggers and set up a stack frame.
    // NOTE: the RBP must point at its older value and next to the return address,
    // so it has to be on the system stack. We won't need one on the task stack.
    // NOTE: Don't use Runtime::call_sys, that works on task stacks only! And
    // this frame is guaranteed to be aligned.
    x64->op(PUSHQ, RBP);
    x64->op(MOVQ, RBP, RSP);

    // Create the initial task stack with a guard page at the bottom
    x64->op(MOVQ, RDI, PAGE_SIZE);
    x64->op(MOVQ, RSI, INITIAL_STACK_SIZE);
    x64->op(CALL, sysv_aligned_alloc_label);
    
    x64->op(MOVQ, Address(task_stack_address_label, 0), RAX);
    x64->op(MOVQ, Address(task_stack_size_label, 0), INITIAL_STACK_SIZE);
    
    x64->op(MOVQ, RDI, RAX);
    x64->op(MOVQ, RSI, PAGE_SIZE);
    x64->op(MOVQ, RDX, PROT_NONE);
    x64->op(CALL, sysv_mprotect_label);
    
    // Switch to the new stack
    x64->op(MOVQ, Address(start_frame_label, 0), RSP);  // aligned
    x64->op(MOVQ, RBX, Address(task_stack_address_label, 0));
    x64->op(ADDQ, RBX, Address(task_stack_size_label, 0));
    x64->op(MOVQ, RSP, RBX);  // should be a single step

    // Invoke global initializers
    for (Label l : initializer_labels)
        x64->op(CALL, l);

    // Into the new world
    x64->op(MOVQ, R10, main_storage.address);
    x64->op(MOVQ, R11, Address(R10, CLASS_VT_OFFSET));
    x64->op(PUSHQ, R10);
    x64->op(CALL, Address(R11, -1 * ADDRESS_SIZE));
    x64->op(ADDQ, RSP, ADDRESS_SIZE);

    // Invoke global finalizers
    for (unsigned i = finalizer_labels.size(); i--;)
        x64->op(CALL, finalizer_labels[i]);
    
    // Switch back
    x64->op(MOVQ, RSP, Address(start_frame_label, 0));  // should be a single step
    
    // Drop the task stack
    x64->op(MOVQ, RDI, Address(task_stack_address_label, 0));
    x64->op(MOVQ, RSI, PAGE_SIZE);
    x64->op(MOVQ, RDX, PROT_RW);
    x64->op(CALL, sysv_mprotect_label);
    
    x64->op(MOVQ, RDI, Address(task_stack_address_label, 0));
    x64->op(CALL, sysv_free_label);
    
    x64->op(POPQ, RBP);
    x64->op(RET);
}

void Runtime::compile_logging() {
    // We want to preserve all registers, but also keep calling these functions the simplest.
    // Since we can't push the string address directly onto the stack, we push its
    // relative location in the data segment, and convert it to absolute address here.
    x64->code_label(log_label);
    x64->op(PUSHFQ);
    int offset = pusha();
    
    x64->op(LEA, RDI, Address(data_start_label, 0));
    x64->op(ADDQ, RDI, Address(RSP, offset + ADDRESS_SIZE + RIP_SIZE));
    
    call_sysv(sysv_log_label);
    
    popa();
    x64->op(POPFQ);
    x64->op(RET);

    // As above
    x64->code_label(logref_label);
    x64->op(PUSHFQ);
    offset = pusha();
    
    x64->op(LEA, RDI, Address(data_start_label, 0));
    x64->op(ADDQ, RDI, Address(RSP, offset + 2 * ADDRESS_SIZE + RIP_SIZE));
    x64->op(MOVQ, RSI, Address(RSP, offset + ADDRESS_SIZE + RIP_SIZE));
    
    call_sysv(sysv_logref_label);
    
    popa();
    x64->op(POPFQ);
    x64->op(RET);
    
    // As above
    x64->code_label(dump_label);
    x64->op(PUSHFQ);

    // NOTE: we'll adjust the saved RSP to its original value (STR+RIP+RFLAGS+RAX+RBX+RCX+RDX)
    for (Register r : { RAX, RBX, RCX, RDX, RSP, RBP, RSI, RDI, R8, R9, R10, R11, R12, R13, R14, R15 })
        x64->op(PUSHQ, r);

    x64->op(ADDQ, Address(RSP, 11 * ADDRESS_SIZE), 5 * ADDRESS_SIZE + RIP_SIZE + ADDRESS_SIZE);
    x64->op(LEA, RDI, Address(data_start_label, 0));
    x64->op(ADDQ, RDI, Address(RSP, 17 * ADDRESS_SIZE + RIP_SIZE));
    x64->op(MOVQ, RSI, RSP);
    
    call_sysv(sysv_dump_label);

    // NOTE: we don't pop the adjusted RSP
    for (Register r : { R15, R14, R13, R12, R11, R10, R9, R8, RDI, RSI, RBP, RDX, RDX, RCX, RBX, RAX })
        x64->op(POPQ, r);
        
    x64->op(POPFQ);
    x64->op(RET);
}

void Runtime::incref(Register reg) {
    if (reg == RSP || reg == RBP || reg == NOREG)
        throw ASM_ERROR;
    
    x64->op(CALL, incref_labels[reg]);
}

void Runtime::decref(Register reg) {
    if (reg == RSP || reg == RBP || reg == NOREG)
        throw ASM_ERROR;

    x64->op(CALL, decref_labels[reg]);
}

void Runtime::oneref(Register reg) {
    if (reg == RSP || reg == RBP || reg == NOREG)
        throw ASM_ERROR;

    x64->op(CMPQ, Address(reg, HEAP_REFCOUNT_OFFSET), 1);
}

void Runtime::heap_alloc() {
    x64->op(CALL, heap_alloc_label);
}

void Runtime::heap_realloc() {
    x64->op(CALL, heap_realloc_label);
}

void Runtime::check_unshared(Register r) {
    x64->op(CMPQ, Address(r, HEAP_REFCOUNT_OFFSET), 1);
}

void Runtime::r10bcompar(bool is_unsigned) {
    if (is_unsigned) {
        x64->op(SETB, R11B);
        x64->op(SETA, R10B);
        x64->op(SUBB, R10B, R11B);
    }
    else {
        x64->op(SETL, R11B);
        x64->op(SETG, R10B);
        x64->op(SUBB, R10B, R11B);
    }
    
    // R10B finally contains -1 iff below/less, +1 iff above/greater, 0 iff equal.
    // The flags are also set accordingly, now independently of the signedness.
}


void Runtime::copy(Address s, Address t, int size) {
    for (int i = 0; i < size / 8; i++) {
        x64->op(MOVQ, R10, s + i * 8);
        x64->op(MOVQ, t + i * 8, R10);
    }
    
    if (size & 4) {
        x64->op(MOVD, R10D, s + (size & ~7));
        x64->op(MOVD, t + (size & ~7), R10D);
    }
    
    if (size & 2) {
        x64->op(MOVW, R10W, s + (size & ~3));
        x64->op(MOVW, t + (size & ~3), R10W);
    }

    if (size & 1) {
        x64->op(MOVB, R10B, s + (size & ~1));
        x64->op(MOVB, t + (size & ~1), R10B);
    }
}


void Runtime::load_lvalue(Register reg, Register tmp, Storage ref_storage) {
    if (ref_storage.where == MEMORY) {
        x64->op(MOVQ, reg, ref_storage.address);
    }
    else if (ref_storage.where == ALIAS) {
        x64->op(MOVQ, tmp, ref_storage.address);
        x64->op(MOVQ, reg, Address(tmp, ref_storage.value));
    }
    else
        throw INTERNAL_ERROR;
}


void Runtime::store_lvalue(Register reg, Register tmp, Storage ref_storage) {
    if (ref_storage.where == MEMORY) {
        x64->op(MOVQ, ref_storage.address, reg);
    }
    else if (ref_storage.where == ALIAS) {
        x64->op(MOVQ, tmp, ref_storage.address);
        x64->op(MOVQ, Address(tmp, ref_storage.value), reg);
    }
    else
        throw INTERNAL_ERROR;
}


Address Runtime::make_address(Register base, Register index, int scale, int offset) {
    if (scale == 1)
        return Address(base, index, Address::SCALE_1, offset);
    else if (scale == 2)
        return Address(base, index, Address::SCALE_2, offset);
    else if (scale == 4)
        return Address(base, index, Address::SCALE_4, offset);
    else if (scale == 8)
        return Address(base, index, Address::SCALE_8, offset);
    else {
        x64->op(IMUL3Q, index, index, scale);
        return Address(base, index, offset);
    }
}


void Runtime::log(std::string message) {
    int message_offset = x64->data.size();
    x64->data_zstring(message);

    x64->op(PUSHQ, message_offset);
    x64->op(CALL, log_label);
    x64->op(LEA, RSP, Address(RSP, ADDRESS_SIZE));
}

void Runtime::logref(std::string message, Register r) {
    int message_offset = x64->data.size();
    x64->data_zstring(message);

    x64->op(PUSHQ, message_offset);
    x64->op(PUSHQ, r);
    x64->op(CALL, logref_label);
    x64->op(LEA, RSP, Address(RSP, 2 * ADDRESS_SIZE));
}

void Runtime::dump(std::string message) {
    int message_offset = x64->data.size();
    x64->data_zstring(message);

    x64->op(PUSHQ, message_offset);
    x64->op(CALL, dump_label);
    x64->op(LEA, RSP, Address(RSP, ADDRESS_SIZE));
}


void Runtime::die(std::string message) {
    // TODO: this encodes the message several times unnecessarily!
    Label message_label;
    x64->data_label(message_label);
    x64->data_zstring(message);

    x64->op(LEA, RDI, Address(message_label, 0));
    call_sysv(sysv_die_label);
    x64->op(UD2);
}


void Runtime::dies(Register r) {
    x64->op(MOVQ, RDI, r);
    call_sysv(sysv_dies_label);
    x64->op(UD2);
}
