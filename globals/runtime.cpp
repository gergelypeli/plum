#include "../plum.h"


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


unsigned Once::type_info(TypeSpec ts, bool as_alias) {
    if (ts == NO_TS)
        throw INTERNAL_ERROR;
        
    return type_die_offsets.add(std::make_pair(ts, as_alias)).def_index;
}


void Once::for_all(Cx *cx) {
    // NOTE: once-functions may ask to once-compile other functions, so we must keep
    // on checking until no new compilations are requested.
    
    bool was_dirty = true;
    
    while (was_dirty) {
        was_dirty = false;
        
        while (function_compilers.is_dirty()) {
            auto kv = function_compilers.take();
            FunctionCompiler fc = kv.first;
            Label label = kv.second;
            
            fc(label, cx);
            was_dirty = true;
        }
        
        while (typed_function_compilers.is_dirty()) {
            auto kv = typed_function_compilers.take();
            TypedFunctionCompiler tfc = kv.first.first;
            TypeSpec ts = kv.first.second;
            Label label = kv.second;
            
            tfc(label, ts, cx);
            was_dirty = true;
        }
        
        while (deferrables.is_dirty()) {
            auto kv = deferrables.take();
            Deferrable *d = kv.first;
            Label label = kv.second;

            d->deferred_compile(label, cx);
            was_dirty = true;
        }
        
        while (import_gots.is_dirty()) {
            auto kv = import_gots.take();
            std::string name = kv.first;
            Label label = kv.second;
        
            Label shared_label;
            cx->code_label_import(shared_label, name);
        
            // symbol points to the function address in the data segment
            cx->data_label_local(label, name + "@GOT");
            cx->data_reference(shared_label);
            was_dirty = true;
        }
    }
}


void Once::for_debug(Cx *cx) {
    while (type_die_offsets.is_dirty()) {
        auto kv = type_die_offsets.take();
        TypeSpecTuple tst = kv.first;
        TypeSpec ts = tst.first;
        bool as_alias = tst.second;
        Label label = kv.second;
        unsigned index = label.def_index;
        
        // NOTE: our Ref and Ptr types are represented as Dwarf pointer types, but our
        // ALIAS storages are represented as Dwarf reference types.

        cx->dwarf->info_def(index);
        
        if (as_alias) {
            unsigned ts_index = type_info(ts);
            cx->dwarf->reference_type_info(ts.symbolize("alias"), ts_index);
        }
        else {
            //std::cerr << "XXX type_info " << ts << "\n";
            ts.type_info(cx);
        }
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


void Unwind::initiate(Declaration *last, Cx *cx) {
    // Sanity check for debugging
    //Label ok;
    //cx->op(CMPQ, RDX, NO_EXCEPTION);
    //cx->op(JNE, ok);
    //cx->runtime->die("Raised nothing!");
    //cx->code_label(ok);

    int old_stack_usage = cx->accounting->mark();

    for (int i = stack.size() - 1; i >= 0; i--) {
        CodeScope *cs = stack[i]->unwind(cx);
        
        if (cs) {
            cs->jump_to_content_finalization(last, cx);
            cx->accounting->rewind(old_stack_usage);
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

Runtime::Runtime(Cx *x, unsigned application_size, std::vector<std::string> source_file_names) {
    cx = x;
    
    cx->code_label_global(code_start_label, "code_start");
    cx->data_label_global(data_start_label, "data_start");
    
    cx->absolute_label(zero_label, 0);

    // Float constants are aligned to 16 bytes so SIMD packed instructions can use it
    cx->data_align(16);
    cx->data_label(float_zero_label);
    cx->data_double(0.0);
    cx->data_double(0.0);

    cx->data_align(16);
    cx->data_label(float_minus_zero_label);
    cx->data_double(-0.0);
    cx->data_double(0.0);

    data_heap_header();
    cx->data_label(empty_array_label);
    cx->data_qword(0);  // reservation
    cx->data_qword(0);  // length

    cx->data_label(start_frame_label);
    cx->data_qword(0);

    cx->data_label(task_frame_label);
    cx->data_qword(0);
    
    cx->data_label(task_stack_address_label);
    cx->data_qword(0);

    cx->data_label(task_stack_size_label);
    cx->data_qword(0);

    cx->data_label_global(allocation_count_label, "allocation_count");
    cx->data_qword(0);

    cx->data_label_global(refcount_balance_label, "refcount_balance");
    cx->data_qword(0);

    cx->code_label_global(empty_function_label, "empty_function");
    cx->prologue();
    cx->epilogue();

    die_unmatched_message_label = data_heap_string(decode_utf8("Fatal unmatched value: "));

    cx->code_label_import(sysv_malloc_label, "C__malloc");
    cx->code_label_import(sysv_aligned_alloc_label, "C__aligned_alloc");
    cx->code_label_import(sysv_free_label, "C__free");
    cx->code_label_import(sysv_realloc_label, "C__realloc");
    cx->code_label_import(sysv_mprotect_label, "C__mprotect");
    cx->code_label_import(sysv_memcpy_label, "C__memcpy");
    cx->code_label_import(sysv_memmove_label, "C__memmove");
    
    cx->code_label_import(sysv_log_label, "C__log");
    cx->code_label_import(sysv_logref_label, "C__logref");
    cx->code_label_import(sysv_dump_label, "C__dump");
    cx->code_label_import(sysv_die_label, "C__die");
    cx->code_label_import(sysv_dies_label, "C__dies");
    cx->code_label_import(sysv_die_uncaught_label, "C__die_uncaught");
    
    cx->code_label_import(sysv_sort_label, "C__sort");
    cx->code_label_import(sysv_string_regexp_match_label, "C__string_regexp_match");
    
    cx->code_label_import(sysv_streamify_integer_label, "C__streamify_integer");
    cx->code_label_import(sysv_streamify_unteger_label, "C__streamify_unteger");
    cx->code_label_import(sysv_streamify_boolean_label, "C__streamify_boolean");
    cx->code_label_import(sysv_streamify_float_label, "C__streamify_float");
    cx->code_label_import(sysv_streamify_pointer_label, "C__streamify_pointer");

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
    compile_application_data(application_size);
    compile_source_infos(source_file_names);
}

void Runtime::data_heap_header() {
    if (HEAP_HEADER_SIZE != 32 || HEAP_REFCOUNT_OFFSET != -16 || HEAP_FINALIZER_OFFSET != -24)
        throw ASM_ERROR;

    cx->data_align(8);
    cx->data_qword(0);  // next
    cx->data_reference(empty_function_label);  // finalizer
    cx->data_qword(1);  // artificial reference to prevent freeing
    cx->data_qword(0);  // padding, used to be weakrefcount
}

Label Runtime::data_heap_string(std::ustring characters) {
    if (LINEARRAY_HEADER_SIZE != 16 || LINEARRAY_RESERVATION_OFFSET != 0 || LINEARRAY_LENGTH_OFFSET != 8)
        throw ASM_ERROR;
    
    Label l;

    data_heap_header();
    cx->data_label(l);
    cx->data_qword(characters.size());
    cx->data_qword(characters.size());

    for (unsigned16 &c : characters)
        cx->data_word(c);

    return l;
}

void Runtime::call_sysv(Label l) {
    cx->op(LEA, R10, Address(l, 0));
    cx->op(CALL, call_sysv_label);
}

void Runtime::call_sysv_got(Label got_l) {
    cx->op(MOVQ, R10, Address(got_l, 0));
    cx->op(CALL, call_sysv_label);
}

void Runtime::callback_prologue() {
    // We're returning to the task stack temporarily. As RSP will be clobbered,
    // we must save it on the task stack. But since the return address may be saved on
    // the system stack, have the stack frame there. This means that the callback function
    // must not use local variables, and must get all arguments in registers.
    
    // Create stack frame, let RBP point to the system stack
    cx->welcome();
    
    // Switch to the task stack
    cx->op(MOVQ, RSP, Address(task_frame_label, 0));
}

void Runtime::callback_epilogue() {
    // This may have changed by recursive invocations
    cx->op(MOVQ, Address(task_frame_label, 0), RSP);
    
    // Return to the system stack
    cx->op(MOVQ, RSP, RBP);
    
    cx->goodbye();
}

void Runtime::compile_source_infos(std::vector<std::string> source_file_names) {
    cx->data_label(source_infos_length_label);
    cx->data_qword(source_file_names.size());
    
    std::vector<Label> labels;

    for (auto &source_file_name : source_file_names) {
        labels.push_back(data_heap_string(decode_utf8(source_file_name)));
    }
    
    cx->data_align(8);
    cx->data_label_global(source_infos_label, "source_infos");
    
    for (auto &label : labels) {
        cx->data_reference(label);
    }
}

void Runtime::add_func_info(std::string name, Label start, Label end) {
    func_infos.push_back({ name, start, end, Label() });
}

void Runtime::compile_func_infos() {
    cx->data_label(func_infos_length_label);
    cx->data_qword(func_infos.size());
    
    for (auto &fi : func_infos) {
        fi.name_label = cx->runtime->data_heap_string(decode_utf8(fi.name));
    }
    
    cx->data_align(8);
    cx->data_label_global(func_infos_label, "func_infos");
    
    for (auto &fi : func_infos) {
        cx->data_reference(fi.start_label);
        cx->data_reference(fi.end_label);
        cx->data_reference(fi.name_label);
    }
}

void Runtime::add_call_info(int file_index, int line_number) {
    call_infos.push_back(LineInfo { cx->get_pc(), file_index, line_number });
}

void Runtime::compile_call_infos() {
    cx->data_label(call_infos_length_label);
    cx->data_qword(call_infos.size());
    
    cx->data_align(8);
    cx->data_label_global(call_infos_label, "call_infos");
    
    for (auto &ci : call_infos) {
        cx->data_dword(ci.address);
        cx->data_word(ci.file_index);
        cx->data_word(ci.line_number);
    }
}

void Runtime::compile_application_data(unsigned application_size) {
    std::cerr << "Application size is " << application_size << " bytes.\n";
    
    cx->data_align(16);
    cx->data_label(application_label);
    
    for (unsigned i = 0; i < application_size; i++)
        cx->data_byte(0);
}

int Runtime::pusha(bool except_rax) {
    // RBX and the last 4 are preserved by the System V ABI
    // But we save RBX, because it's annoying to handle it separately
    // 10 registers total, including RAX

    if (!except_rax)
        cx->op(PUSHQ, RAX);
    
    cx->op(PUSHQ, RBX);
    cx->op(PUSHQ, RCX);
    cx->op(PUSHQ, RDX);
    cx->op(PUSHQ, RSI);
    cx->op(PUSHQ, RDI);
    cx->op(PUSHQ, R8);
    cx->op(PUSHQ, R9);
    cx->op(PUSHQ, R10);
    cx->op(PUSHQ, R11);

    return except_rax ? 72 : 80;
}

void Runtime::popa(bool except_rax) {
    cx->op(POPQ, R11);
    cx->op(POPQ, R10);
    cx->op(POPQ, R9);
    cx->op(POPQ, R8);
    cx->op(POPQ, RDI);
    cx->op(POPQ, RSI);
    cx->op(POPQ, RDX);
    cx->op(POPQ, RCX);
    cx->op(POPQ, RBX);

    if (!except_rax)
        cx->op(POPQ, RAX);
}

void Runtime::compile_finalize() {
    // finalize(pointer)
    // Preserves all registers, including the scratch ones
    auto arg_regs = cx->abi_arg_regs();

    cx->code_label_global(finalize_label, "finalize");
    cx->prologue();

    const int ARGS = pusha() + ARGS_1;
    Label fcb_loop, fcb_cond;
    
    //log("Finalizing heap object.");
    cx->op(MOVQ, RAX, Address(RSP, ARGS + ARG_1));  // pointer
    cx->op(MOVQ, RAX, Address(RAX, HEAP_NEXT_OFFSET));  // FCB
    cx->op(JMP, fcb_cond);

    cx->code_label(fcb_loop);
    log("Triggering finalizer callback.");
    cx->op(PUSHQ, RAX);

    // Invoke FCB    
    cx->op(PUSHQ, Address(RAX, FCB_PAYLOAD1_OFFSET));
    cx->op(PUSHQ, Address(RAX, FCB_PAYLOAD2_OFFSET));
    cx->op(CALL, Address(RAX, FCB_CALLBACK_OFFSET));  // clobbers all
    cx->op(ADDQ, RSP, 2 * ADDRESS_SIZE);

    // Free FCB
    cx->op(POPQ, arg_regs[0]);
    cx->op(PUSHQ, Address(arg_regs[0], FCB_NEXT_OFFSET));  // advance before free
    call_sysv(sysv_free_label);  // clobbers all
    cx->op(POPQ, RAX);

    cx->code_label(fcb_cond);
    cx->op(CMPQ, RAX, FCB_NIL);
    cx->op(JNE, fcb_loop);

    // Invoke finalizer
    //dump("finalizing");
    cx->op(MOVQ, RAX, Address(RSP, ARGS + ARG_1));  // pointer
    cx->op(PUSHQ, RAX);
    cx->op(CALL, Address(RAX, HEAP_FINALIZER_OFFSET));  // finalizers may clobber everything

    cx->op(POPQ, arg_regs[0]);
    cx->op(SUBQ, arg_regs[0], HEAP_HEADER_SIZE);
    call_sysv(sysv_free_label);  // will probably clobber everything

    popa();
    cx->epilogue();
}

void Runtime::compile_heap_alloc() {
    // heap_alloc(size, finalizer)
    // Clobbers all registers
    auto arg_regs = cx->abi_arg_regs();
    auto res_regs = cx->abi_res_regs();
    
    cx->code_label_global(heap_alloc_label, "heap_alloc");
    const int ARGS = ARGS_2;
    
    cx->prologue();
    
    cx->op(MOVQ, arg_regs[0], Address(RSP, ARGS + ARG_1));  // size arg
    cx->op(ADDQ, arg_regs[0], HEAP_HEADER_SIZE);
    call_sysv(sysv_malloc_label);
    cx->op(MOVQ, RAX, res_regs[0]);
    
    cx->op(ADDQ, RAX, HEAP_HEADER_SIZE);
    cx->op(MOVQ, RBX, Address(RSP, ARGS + ARG_2));  // finalizer arg
    //dump("heap_alloc");
    
    cx->op(MOVQ, Address(RAX, HEAP_NEXT_OFFSET), 0);
    cx->op(MOVQ, Address(RAX, HEAP_FINALIZER_OFFSET), RBX);
    cx->op(MOVQ, Address(RAX, HEAP_REFCOUNT_OFFSET), 1);  // start from 1
    
    cx->op(INCQ, Address(refcount_balance_label, 0));  // necessary to keep the balance
    
    cx->epilogue();
}

void Runtime::compile_heap_realloc() {
    // heap_realloc(pointer, new_size)
    // Clobbers all registers
    auto arg_regs = cx->abi_arg_regs();
    auto res_regs = cx->abi_res_regs();

    cx->code_label_global(heap_realloc_label, "heap_realloc");
    const int ARGS = ARGS_2;
    Label realloc_ok;
    
    cx->prologue();
    
    cx->op(MOVQ, arg_regs[0], Address(RSP, ARGS + ARG_1));  // pointer arg
    check_unshared(arg_regs[0]);
    cx->op(JE, realloc_ok);
    
    die("Realloc of shared array!");

    cx->code_label(realloc_ok);
    cx->op(SUBQ, arg_regs[0], HEAP_HEADER_SIZE);
    cx->op(MOVQ, arg_regs[1], Address(RSP, ARGS + ARG_2));  // new_size arg
    cx->op(ADDQ, arg_regs[1], HEAP_HEADER_SIZE);
    call_sysv(sysv_realloc_label);

    cx->op(LEA, RAX, Address(res_regs[0], HEAP_HEADER_SIZE));
    
    cx->epilogue();
}

void Runtime::compile_fcb_alloc() {
    // fcb_alloc(pointer, callback, payload1, payload2)
    // Clobbers all registers
    auto arg_regs = cx->abi_arg_regs();
    auto res_regs = cx->abi_res_regs();

    cx->code_label_global(fcb_alloc_label, "fcb_alloc");
    const int ARGS = ARGS_4;

    cx->prologue();

    cx->op(MOVQ, arg_regs[0], FCB_SIZE);
    call_sysv(sysv_malloc_label);
    cx->op(MOVQ, RAX, res_regs[0]);
    
    cx->op(MOVQ, RBX, Address(RSP, ARGS + ARG_2));  // callback arg
    cx->op(MOVQ, Address(RAX, FCB_CALLBACK_OFFSET), RBX);
    cx->op(MOVQ, RBX, Address(RSP, ARGS + ARG_3));  // payload1 arg
    cx->op(MOVQ, Address(RAX, FCB_PAYLOAD1_OFFSET), RBX);
    cx->op(MOVQ, RBX, Address(RSP, ARGS + ARG_4));  // payload2 arg
    cx->op(MOVQ, Address(RAX, FCB_PAYLOAD2_OFFSET), RBX);
    
    cx->op(MOVQ, RBX, Address(RSP, ARGS + ARG_1));  // pointer arg
    cx->op(MOVQ, RCX, Address(RBX, HEAP_NEXT_OFFSET));
    cx->op(MOVQ, Address(RAX, FCB_NEXT_OFFSET), RCX);
    cx->op(MOVQ, Address(RBX, HEAP_NEXT_OFFSET), RAX);
    cx->op(ADDQ, RBX, HEAP_HEADER_OFFSET);
    cx->op(MOVQ, Address(RAX, FCB_PREV_OFFSET), RBX);
    //dump("alloc_fcb RAX=fcb, RBX=prev, RCX=next");

    Label no_next;
    cx->op(CMPQ, RCX, FCB_NIL);
    cx->op(JE, no_next);
    cx->op(MOVQ, Address(RCX, FCB_PREV_OFFSET), RAX);
    cx->code_label(no_next);

    cx->epilogue();
}

void Runtime::compile_fcb_free() {
    // fcb_free(pointer, callback, payload1, payload2)
    // Clobbers all registers
    auto arg_regs = cx->abi_arg_regs();
    
    cx->code_label_global(fcb_free_label, "fcb_free");
    Label check, loop, nope, no_next;
    const int ARGS = ARGS_4;

    cx->prologue();

    cx->op(MOVQ, RAX, Address(RSP, ARGS + ARG_1));  // object
    cx->op(MOVQ, RBX, Address(RSP, ARGS + ARG_2));  // callback
    cx->op(MOVQ, RCX, Address(RSP, ARGS + ARG_3));  // payload1
    cx->op(MOVQ, RDX, Address(RSP, ARGS + ARG_4));  // payload2
    
    cx->op(MOVQ, RAX, Address(RAX, HEAP_NEXT_OFFSET));  // FCB
    cx->op(JMP, check);

    cx->code_label(loop);
    cx->op(CMPQ, Address(RAX, FCB_CALLBACK_OFFSET), RBX);
    cx->op(JNE, nope);
    cx->op(CMPQ, Address(RAX, FCB_PAYLOAD1_OFFSET), RCX);
    cx->op(JNE, nope);
    cx->op(CMPQ, Address(RAX, FCB_PAYLOAD2_OFFSET), RDX);
    cx->op(JNE, nope);
    
    // Found the FCB
    cx->op(MOVQ, RBX, Address(RAX, FCB_PREV_OFFSET));  // always valid
    cx->op(MOVQ, RCX, Address(RAX, FCB_NEXT_OFFSET));
    cx->op(MOVQ, Address(RBX, FCB_NEXT_OFFSET), RCX);
    cx->op(CMPQ, RCX, FCB_NIL);
    cx->op(JE, no_next);
    cx->op(MOVQ, Address(RCX, FCB_PREV_OFFSET), RBX);
    cx->code_label(no_next);

    cx->op(MOVQ, arg_regs[0], RAX);
    call_sysv(sysv_free_label);  // clobbers all

    cx->epilogue();
    
    cx->code_label(nope);
    cx->op(MOVQ, RAX, Address(RAX, FCB_NEXT_OFFSET));
    
    cx->code_label(check);
    cx->op(CMPQ, RAX, FCB_NIL);
    cx->op(JNE, loop);
    
    // Not found
    die("FCB not found!");
}

void Runtime::compile_finalize_reference_array() {
    // finalize_reference_array(pointer)
    // Clobbers all registers

    cx->code_label_global(finalize_reference_array_label, "finalize_reference_array");
    Label fra_cond, fra_loop;
    const int ARGS = ARGS_1;
    
    cx->prologue();
    
    cx->op(MOVQ, RAX, Address(RSP, ARGS + ARG_1));
    cx->op(MOVQ, RCX, 0);
    cx->op(JMP, fra_cond);

    cx->code_label(fra_loop);
    cx->op(MOVQ, RBX, Address(RAX, RCX, Address::SCALE_8, LINEARRAY_ELEMS_OFFSET));
    decref(RBX);
    cx->op(INCQ, RCX);

    cx->code_label(fra_cond);
    cx->op(CMPQ, RCX, Address(RAX, LINEARRAY_LENGTH_OFFSET));
    cx->op(JB, fra_loop);

    cx->epilogue();
}

void Runtime::compile_incref_decref() {
    incref_labels.resize(REGISTER_COUNT);
    decref_labels.resize(REGISTER_COUNT);

    for (Register reg : { RAX, RBX, RCX, RDX, RSI, RDI, R8, R9, R10, R11, R12, R13, R14, R15 }) {
        Label il;
        // We use a standard stack frame only for debugging, should be cleaned up later
    
        // NOTE: preserves all registers, including the scratch ones
        cx->code_label_global(incref_labels[reg], std::string("incref_") + register_name(reg));
        cx->prologue();

        cx->op(INCQ, Address(reg, HEAP_REFCOUNT_OFFSET));
        //logref(std::string("INCREF ") + REGISTER_NAMES[reg], reg);

        cx->op(INCQ, Address(refcount_balance_label, 0));
        cx->epilogue();
    
        Label dl, dl2;
    
        // NOTE: preserves all registers, including the scratch ones
        cx->code_label_global(decref_labels[reg], std::string("decref_") + register_name(reg));
        cx->prologue();

        cx->op(DECQ, Address(reg, HEAP_REFCOUNT_OFFSET));
        //logref(std::string("DECREF ") + REGISTER_NAMES[reg], reg);
        cx->op(JNE, dl);

        cx->op(PUSHQ, reg);
        cx->op(CALL, finalize_label);
        cx->op(POPQ, reg);

        cx->code_label(dl);
        cx->op(DECQ, Address(refcount_balance_label, 0));
        cx->epilogue();
    }
}

void Runtime::compile_lookup_source_info() {
    // lookup_source_info(source_file_index)
    // result - RAX = String name
    // clobbers RAX, RBX, RCX
    
    cx->code_label_global(lookup_source_info_label, "lookup_source_info");
    cx->prologue();
    Label found;

    cx->op(MOVQ, RBX, Address(RSP, 2 * ADDRESS_SIZE));  // index
    cx->op(LEA, RAX, Address(source_infos_label, 0));
    cx->op(MOVQ, RCX, Address(source_infos_length_label, 0));
    
    cx->op(CMPD, RBX, RCX);
    cx->op(JB, found);
    
    cx->op(MOVQ, RBX, 0);
    
    cx->code_label(found);
    cx->op(MOVQ, RAX, Address(RAX, RBX, Address::SCALE_8, 0));
    incref(RAX);

    cx->epilogue();
}

void Runtime::compile_lookup_call_info() {
    // lookup_call_info(rip)
    // result - RAX = pointer to the call info structure, or NULL
    // clobbers RAX, RBX, RCX
    
    cx->code_label_global(lookup_call_info_label, "lookup_call_info");
    cx->prologue();
    Label loop, skip, end;

    cx->op(LEA, RBX, Address(code_start_label, 0));
    cx->op(NEGQ, RBX);
    cx->op(ADDQ, RBX, Address(RSP, 2 * ADDRESS_SIZE));  // relative RIP within the code segment
    cx->op(LEA, RAX, Address(call_infos_label, 0));
    cx->op(MOVQ, RCX, Address(call_infos_length_label, 0));
    
    cx->code_label(loop);
    cx->op(CMPD, EBX, Address(RAX, 0));
    cx->op(JE, end);
    
    cx->op(ADDQ, RAX, 8);
    cx->op(DECQ, RCX);
    cx->op(JNE, loop);
    
    // Not found
    cx->op(MOVQ, RAX, 0);
    
    cx->code_label(end);
    cx->epilogue();
}

void Runtime::compile_lookup_frame_info() {
    // lookup_frame_info(rip)
    // result - RAX = pointer to the frame info structure, or NULL
    // clobbers RAX, RBX, RCX
    
    cx->code_label_global(lookup_frame_info_label, "lookup_frame_info");
    cx->prologue();
    Label loop, skip, end;

    cx->op(MOVQ, RBX, Address(RSP, 2 * ADDRESS_SIZE));
    cx->op(LEA, RAX, Address(func_infos_label, 0));
    cx->op(MOVQ, RCX, Address(func_infos_length_label, 0));
    
    cx->code_label(loop);
    cx->op(CMPQ, RBX, Address(RAX, FRAME_INFO_START_OFFSET));
    cx->op(JB, skip);
    cx->op(CMPQ, RBX, Address(RAX, FRAME_INFO_END_OFFSET));
    cx->op(JAE, skip);
    
    // Found the frame
    cx->op(JMP, end);
    
    cx->code_label(skip);
    cx->op(ADDQ, RAX, FRAME_INFO_SIZE);
    cx->op(DECQ, RCX);
    cx->op(JNE, loop);
    
    // Not found
    cx->op(MOVQ, RAX, 0);
    
    cx->code_label(end);
    cx->epilogue();
}

void Runtime::compile_caller_frame_info() {
    // caller_frame_info(up_count)
    // result - RAX = pointer to the frame info structure, or NULL
    //          RBX = pointer to the call info structure, or NULL
    // clobbers RAX, RBX, RCX
    
    Label up, lookup, nope, end;
    
    cx->code_label_global(caller_frame_info_label, "caller_frame_info");
    cx->prologue();
    
    cx->op(MOVQ, RCX, Address(RBP, 2 * ADDRESS_SIZE));
    
    cx->op(MOVQ, RBX, RBP);
    cx->op(CMPQ, RCX, 0);
    cx->op(JE, lookup);
    
    cx->code_label(up);
    cx->op(MOVQ, RBX, Address(RBX, 0));
    cx->op(CMPQ, RBX, Address(start_frame_label, 0));  // Hit the top frame
    cx->op(JE, nope);
    cx->op(DECQ, RCX);
    cx->op(JNE, up);
    
    cx->code_label(lookup);
    cx->op(PUSHQ, 0);
    cx->op(PUSHQ, Address(RBX, ADDRESS_SIZE));  // caller RIP

    cx->op(CALL, lookup_call_info_label);
    cx->op(MOVQ, Address(RSP, ADDRESS_SIZE), RAX);
    cx->op(CALL, lookup_frame_info_label);
    
    cx->op(ADDQ, RSP, ADDRESS_SIZE);
    cx->op(POPQ, RBX);
    cx->op(JMP, end);
    
    cx->code_label(nope);
    cx->op(MOVQ, RAX, 0);
    cx->op(MOVQ, RBX, 0);
    
    cx->code_label(end);
    cx->epilogue();
}

void Runtime::fix_address(Address address) {
    Label skip;
    
    cx->op(CMPQ, address, RSI);
    cx->op(JB, skip);
    cx->op(CMPQ, address, RDI);
    cx->op(JAE, skip);
    cx->op(ADDQ, address, RDX);
    
    cx->code_label(skip);
}

void Runtime::compile_fix_stack() {
    // fix_stack(old_bottom, old_top, relocation)
    // clobbers all registers
    // must be run on the new stack, with relocated RBP and RSP
    // must be called from a helper function with a proper frame, so the current [RBP + 8]
    // points to the first app function that needs fixing.
    
    cx->code_label_global(fix_stack_label, "fix_stack");
    cx->prologue();
    
    cx->op(MOVQ, RSI, Address(RBP, 4 * ADDRESS_SIZE));  // old_bottom
    cx->op(MOVQ, RDI, Address(RBP, 3 * ADDRESS_SIZE));  // old_top
    cx->op(MOVQ, RDX, Address(RBP, 2 * ADDRESS_SIZE));  // relocation
    cx->op(MOVQ, RBP, Address(RBP, 0));  // caller helper function frame

    Label loop, end;
    cx->code_label(loop);
    
    // find caller info
    cx->op(PUSHQ, Address(RBP, ADDRESS_SIZE));
    cx->op(CALL, lookup_frame_info_label);  // RAX - info record
    cx->op(ADDQ, RSP, ADDRESS_SIZE);
    
    // up to caller frame
    cx->op(MOVQ, RBX, Address(RBP, 0));
    cx->op(CMPQ, RBX, Address(start_frame_label, 0));
    cx->op(JE, end);
    
    cx->op(ADDQ, RBX, RDX);
    cx->op(MOVQ, Address(RBP, 0), RBX);
    cx->op(MOVQ, RBP, RBX);
    
    // invoke fix
    cx->op(CALL, Address(RAX, FRAME_INFO_END_OFFSET));
    
    cx->op(JMP, loop);
    
    cx->code_label(end);
    cx->epilogue();
}

void Runtime::compile_double_stack() {
    // This function is invoked on the task stack
    // clobbers all registers
    // must use a stack frame for the stack fix code
    auto arg_regs = cx->abi_arg_regs();
    auto res_regs = cx->abi_res_regs();
    
    cx->code_label_global(double_stack_label, "double_stack");
    cx->prologue();

    // Allocate new stack    
    cx->op(MOVQ, arg_regs[0], PAGE_SIZE);
    cx->op(MOVQ, arg_regs[1], Address(task_stack_size_label, 0));
    cx->op(SHLQ, arg_regs[1], 2);
    call_sysv(sysv_aligned_alloc_label);
    cx->op(MOVQ, RAX, res_regs[0]);
    
    // Compute relocation offset of the stack tops.
    // We can't count on architecture-independent callee-saved registers.
    cx->op(MOVQ, RBX, RAX);
    cx->op(ADDQ, RBX, Address(task_stack_size_label, 0));
    cx->op(SUBQ, RBX, Address(task_stack_address_label, 0));
    cx->op(PUSHQ, RBX);  // save diff
    
    cx->op(MOVQ, arg_regs[0], RAX);
    cx->op(MOVQ, arg_regs[1], PAGE_SIZE);
    cx->op(MOVQ, arg_regs[2], PROT_NONE);
    call_sysv(sysv_mprotect_label);

    // Copy stack contents to the upper half of the new stack (skipping the guard page)
    cx->op(MOVQ, arg_regs[0], Address(task_stack_address_label, 0));
    cx->op(ADDQ, arg_regs[0], Address(RSP, 0));
    cx->op(ADDQ, arg_regs[0], PAGE_SIZE);
    cx->op(MOVQ, arg_regs[1], Address(task_stack_address_label, 0));
    cx->op(ADDQ, arg_regs[1], PAGE_SIZE);
    cx->op(MOVQ, arg_regs[2], Address(task_stack_size_label, 0));
    cx->op(SUBQ, arg_regs[2], PAGE_SIZE);
    call_sysv(sysv_memcpy_label);

    // Switch stacks
    cx->op(POPQ, RBX);
    cx->op(ADDQ, RSP, RBX);
    cx->op(ADDQ, RBP, RBX);
    cx->op(PUSHQ, RBX);

    // Drop old stack
    cx->op(MOVQ, arg_regs[0], Address(task_stack_address_label, 0));
    cx->op(MOVQ, arg_regs[1], PAGE_SIZE);
    cx->op(MOVQ, arg_regs[2], PROT_RW);
    call_sysv(sysv_mprotect_label);
    
    cx->op(MOVQ, arg_regs[0], Address(task_stack_address_label, 0));
    call_sysv(sysv_free_label);

    // Fix new stack
    cx->op(POPQ, RBX);
    cx->op(MOVQ, R10, Address(task_stack_address_label, 0));
    cx->op(PUSHQ, R10);  // old_bottom
    cx->op(ADDQ, R10, Address(task_stack_size_label, 0));
    cx->op(PUSHQ, R10);  // old_top
    cx->op(PUSHQ, RBX);  // relocation offset
    cx->op(CALL, fix_stack_label);  // also uses our RBP to traverse the stack frames
    cx->op(POPQ, RBX);
    cx->op(ADDQ, RSP, 2 * ADDRESS_SIZE);
    
    // Update info
    cx->op(SUBQ, RBX, Address(task_stack_size_label, 0));
    cx->op(ADDQ, Address(task_stack_address_label, 0), RBX);
    cx->op(SHLQ, Address(task_stack_size_label, 0), 1);
    
    // That should be it
    cx->epilogue();
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
    
    cx->code_label_local(call_sysv_label, "__morestack");
    cx->prologue();

    // The only architecture-independent way to save our RSP is to store it globally
    cx->op(MOVQ, Address(task_frame_label, 0), RSP);
    
    cx->op(MOVQ, RSP, Address(start_frame_label, 0));
    cx->op(CALL, R10);
    cx->op(MOVQ, RSP, Address(task_frame_label, 0));
    
    cx->epilogue();
}


void Runtime::compile_start(Storage main_storage, std::vector<Label> initializer_labels, std::vector<Label> finalizer_labels) {
    Label start;
    auto arg_regs = cx->abi_arg_regs();
    
    cx->code_label_global(start, "start");

    // Be nice to debuggers and set up a stack frame.
    // NOTE: the RBP must point at its older value and next to the return address,
    // so it has to be on the system stack. We won't need one on the task stack.
    // NOTE: Don't use Runtime::call_sysv, that works on task stacks only! And
    // this frame is guaranteed to be aligned.
    cx->welcome();

    // This may be clobbered on A64 during SysV calls, save it first
    cx->op(MOVQ, Address(start_frame_label, 0), RBP);  // aligned

    // Create the initial task stack with a guard page at the bottom
    cx->op(MOVQ, arg_regs[0], PAGE_SIZE);
    cx->op(MOVQ, arg_regs[1], INITIAL_STACK_SIZE);
    cx->op(CALL, sysv_aligned_alloc_label);
    
    cx->op(MOVQ, Address(task_stack_address_label, 0), RAX);
    cx->op(MOVQ, Address(task_stack_size_label, 0), INITIAL_STACK_SIZE);
    
    cx->op(MOVQ, arg_regs[0], RAX);
    cx->op(MOVQ, arg_regs[1], PAGE_SIZE);
    cx->op(MOVQ, arg_regs[2], PROT_NONE);
    cx->op(CALL, sysv_mprotect_label);
    
    // Switch to the new stack
    cx->op(MOVQ, RBX, Address(task_stack_address_label, 0));
    cx->op(ADDQ, RBX, Address(task_stack_size_label, 0));
    cx->op(MOVQ, RSP, RBX);  // should be a single step
    cx->op(MOVQ, RBP, Address(start_frame_label, 0));  // for proper frame linking

    // Invoke global initializers
    for (Label l : initializer_labels)
        cx->op(CALL, l);

    // Into the new world
    cx->op(MOVQ, R10, main_storage.address);
    cx->op(MOVQ, R11, Address(R10, CLASS_VT_OFFSET));
    cx->op(PUSHQ, R10);
    cx->op(CALL, Address(R11, -1 * ADDRESS_SIZE));
    cx->op(ADDQ, RSP, ADDRESS_SIZE);

    // Invoke global finalizers
    for (unsigned i = finalizer_labels.size(); i--;)
        cx->op(CALL, finalizer_labels[i]);
    
    // Switch back
    cx->op(MOVQ, RSP, Address(start_frame_label, 0));  // should be a single step
    
    // Drop the task stack
    cx->op(MOVQ, arg_regs[0], Address(task_stack_address_label, 0));
    cx->op(MOVQ, arg_regs[1], PAGE_SIZE);
    cx->op(MOVQ, arg_regs[2], PROT_RW);
    cx->op(CALL, sysv_mprotect_label);
    
    cx->op(MOVQ, arg_regs[0], Address(task_stack_address_label, 0));
    cx->op(CALL, sysv_free_label);

    // For proper goodbye
    cx->op(MOVQ, RBP, Address(start_frame_label, 0));  // should be a single step
    
    cx->goodbye();
}

void Runtime::compile_logging() {
    // We want to preserve all registers, but also keep calling these functions the simplest.
    // Since we can't push the string address directly onto the stack, we push its
    // relative location in the data segment, and convert it to absolute address here.
    auto arg_regs = cx->abi_arg_regs();
    
    cx->code_label_local(log_label, "R__log");
    cx->prologue();
    
    cx->op(PUSHFQ);
    int offset = pusha();
    
    cx->op(LEA, arg_regs[0], Address(data_start_label, 0));
    cx->op(ADDQ, arg_regs[0], Address(RSP, offset + 2 * ADDRESS_SIZE + RIP_SIZE));
    
    call_sysv(sysv_log_label);
    
    popa();
    cx->op(POPFQ);
    
    cx->epilogue();

    // As above
    cx->code_label_local(logref_label, "R__logref");
    cx->prologue();
    
    cx->op(PUSHFQ);
    offset = pusha();
    
    cx->op(LEA, arg_regs[0], Address(data_start_label, 0));
    cx->op(ADDQ, arg_regs[0], Address(RSP, offset + 3 * ADDRESS_SIZE + RIP_SIZE));
    cx->op(MOVQ, arg_regs[1], Address(RSP, offset + 2 * ADDRESS_SIZE + RIP_SIZE));
    
    call_sysv(sysv_logref_label);
    
    popa();
    cx->op(POPFQ);
    
    cx->epilogue();
    
    // As above
    cx->code_label_local(dump_label, "R__dump");
    cx->prologue();  // clobbers RBP
    
    cx->op(PUSHFQ);

    // NOTE: we'll adjust the saved RSP and RBP to their original value
    for (Register r : { RAX, RBX, RCX, RDX, RSP, RBP, RSI, RDI, R8, R9, R10, R11, R12, R13, R14, R15 })
        cx->op(PUSHQ, r);

    // Adjust the passed RSP
    cx->op(ADDQ, Address(RSP, 11 * ADDRESS_SIZE), 5 * ADDRESS_SIZE + RIP_SIZE + 2 * ADDRESS_SIZE);

    // Restore the passed RBP
    cx->op(MOVQ, R10, Address(RSP, 17 * ADDRESS_SIZE));
    cx->op(MOVQ, Address(RSP, 10 * ADDRESS_SIZE), R10);
    
    // Pass the string and the register data area address as arguments
    cx->op(LEA, arg_regs[0], Address(data_start_label, 0));
    cx->op(ADDQ, arg_regs[0], Address(RSP, 17 * ADDRESS_SIZE + RIP_SIZE + ADDRESS_SIZE));
    cx->op(MOVQ, arg_regs[1], RSP);
    
    call_sysv(sysv_dump_label);

    // NOTE: we don't pop the adjusted RSP
    for (Register r : { R15, R14, R13, R12, R11, R10, R9, R8, RDI, RSI, RBP, RDX, RDX, RCX, RBX, RAX })
        cx->op(POPQ, r);
        
    cx->op(POPFQ);
    
    cx->epilogue();
}

void Runtime::incref(Register reg) {
    if (reg == RSP || reg == RBP || reg == NOREG)
        throw ASM_ERROR;
    
    cx->op(CALL, incref_labels[reg]);
}

void Runtime::decref(Register reg) {
    if (reg == RSP || reg == RBP || reg == NOREG)
        throw ASM_ERROR;

    cx->op(CALL, decref_labels[reg]);
}

void Runtime::oneref(Register reg) {
    if (reg == RSP || reg == RBP || reg == NOREG)
        throw ASM_ERROR;

    cx->op(CMPQ, Address(reg, HEAP_REFCOUNT_OFFSET), 1);
}

void Runtime::heap_alloc() {
    cx->op(CALL, heap_alloc_label);
}

void Runtime::heap_realloc() {
    cx->op(CALL, heap_realloc_label);
}

void Runtime::check_unshared(Register r) {
    cx->op(CMPQ, Address(r, HEAP_REFCOUNT_OFFSET), 1);
}

void Runtime::r10bcompar(bool is_unsigned) {
    if (is_unsigned) {
        cx->op(SETB, R11B);
        cx->op(SETA, R10B);
        cx->op(SUBB, R10B, R11B);
    }
    else {
        cx->op(SETL, R11B);
        cx->op(SETG, R10B);
        cx->op(SUBB, R10B, R11B);
    }
    
    // R10B finally contains -1 iff below/less, +1 iff above/greater, 0 iff equal.
    // The flags are also set accordingly, now independently of the signedness.
}


void Runtime::copy(Address s, Address t, int size) {
    for (int i = 0; i < size / 8; i++) {
        cx->op(MOVQ, R10, s + i * 8);
        cx->op(MOVQ, t + i * 8, R10);
    }
    
    if (size & 4) {
        cx->op(MOVD, R10D, s + (size & ~7));
        cx->op(MOVD, t + (size & ~7), R10D);
    }
    
    if (size & 2) {
        cx->op(MOVW, R10W, s + (size & ~3));
        cx->op(MOVW, t + (size & ~3), R10W);
    }

    if (size & 1) {
        cx->op(MOVB, R10B, s + (size & ~1));
        cx->op(MOVB, t + (size & ~1), R10B);
    }
}


void Runtime::push(Address s, int size) {
    for (int i = (size + 7) / 8 - 1; i >= 0; i--)
        cx->op(PUSHQ, s + i * 8);
}


void Runtime::load_lvalue(Register reg, Register tmp, Storage ref_storage, int offset) {
    if (ref_storage.where == MEMORY) {
        cx->op(MOVQ, reg, ref_storage.address + offset);
    }
    else if (ref_storage.where == ALIAS) {
        cx->op(MOVQ, tmp, ref_storage.address);
        cx->op(MOVQ, reg, Address(tmp, ref_storage.value + offset));
    }
    else
        throw INTERNAL_ERROR;
}


void Runtime::store_lvalue(Register reg, Register tmp, Storage ref_storage, int offset) {
    if (ref_storage.where == MEMORY) {
        cx->op(MOVQ, ref_storage.address + offset, reg);
    }
    else if (ref_storage.where == ALIAS) {
        cx->op(MOVQ, tmp, ref_storage.address);
        cx->op(MOVQ, Address(tmp, ref_storage.value + offset), reg);
    }
    else
        throw INTERNAL_ERROR;
}


void Runtime::exchange_lvalue(Register reg, Register tmp, Storage ref_storage, int offset) {
    if (ref_storage.where == MEMORY) {
        cx->op(XCHGQ, ref_storage.address + offset, reg);
    }
    else if (ref_storage.where == ALIAS) {
        cx->op(MOVQ, tmp, ref_storage.address);
        cx->op(XCHGQ, Address(tmp, ref_storage.value + offset), reg);
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
        cx->op(IMUL3Q, index, index, scale);
        return Address(base, index, offset);
    }
}


void Runtime::log(std::string message) {
    int message_offset = cx->get_dc();
    cx->data_zstring(message);

    cx->op(PUSHQ, message_offset);
    cx->op(CALL, log_label);
    cx->op(LEA, RSP, Address(RSP, ADDRESS_SIZE));
}

void Runtime::logref(std::string message, Register r) {
    int message_offset = cx->get_dc();
    cx->data_zstring(message);

    cx->op(PUSHQ, message_offset);
    cx->op(PUSHQ, r);
    cx->op(CALL, logref_label);
    cx->op(LEA, RSP, Address(RSP, 2 * ADDRESS_SIZE));
}

void Runtime::dump(std::string message) {
    int message_offset = cx->get_dc();
    cx->data_zstring(message);

    cx->op(PUSHQ, message_offset);
    cx->op(CALL, dump_label);
    cx->op(LEA, RSP, Address(RSP, ADDRESS_SIZE));
}


void Runtime::die(std::string message) {
    // TODO: this encodes the message several times unnecessarily!
    auto arg_regs = cx->abi_arg_regs();

    Label message_label;
    cx->data_label(message_label);
    cx->data_zstring(message);

    cx->op(LEA, arg_regs[0], Address(message_label, 0));
    call_sysv(sysv_die_label);
    cx->op(UD2);
}


void Runtime::dies(Register r) {
    auto arg_regs = cx->abi_arg_regs();

    cx->op(MOVQ, arg_regs[0], r);
    call_sysv(sysv_dies_label);
    cx->op(UD2);
}
