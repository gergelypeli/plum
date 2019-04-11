

// Once

Label Once::compile(FunctionCompiler fc) {
    return function_compilers.add(fc);
}


Label Once::compile(TypedFunctionCompiler tfc, TypeSpec ts) {
    return typed_function_compilers.add(make_pair(tfc, ts));
}


Label Once::sysv_wrapper(SysvFunction *f) {
    return sysv_wrappers.add(f);
}


Label Once::functor_definition(Value *v) {
    return functor_definitions.add(v);
}


Label Once::import(std::string name) {
    return imports.add(name);
}


Label Once::import_got(std::string name) {
    return import_gots.add(name);
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
        
        while (sysv_wrappers.is_dirty()) {
            auto kv = sysv_wrappers.take();
            SysvFunction *f = kv.first;
            Label label = kv.second;

            f->wrap(label, x64);
            was_dirty = true;
        }
        
        while (functor_definitions.is_dirty()) {
            auto kv = functor_definitions.take();
            Value *v = kv.first;

            v->compile(x64);
            was_dirty = true;
        }
        
        while (imports.is_dirty()) {
            auto kv = imports.take();
            std::string name = kv.first;
            Label label = kv.second;

            // symbol points to the function start in the code segment
            x64->code_label_import(label, name);
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

    die_unmatched_message_label = data_heap_string(decode_utf8("Fatal unmatched value: "));

    x64->code_label_import(sysv_memalloc_label, "memalloc");
    x64->code_label_import(sysv_memfree_label, "memfree");
    x64->code_label_import(sysv_memrealloc_label, "memrealloc");
    
    x64->code_label_import(sysv_logfunc_label, "logfunc");  // bah...
    x64->code_label_import(sysv_logreffunc_label, "logreffunc");
    x64->code_label_import(sysv_dump_label, "dump");
    x64->code_label_import(sysv_die_label, "die");
    x64->code_label_import(sysv_dies_label, "dies");
    x64->code_label_import(sysv_die_uncaught_label, "die_uncaught");
    
    x64->code_label_import(sysv_sort_label, "sort");
    x64->code_label_import(sysv_string_regexp_match_label, "string_regexp_match");
    x64->code_label_import(sysv_streamify_pointer_label, "streamify_pointer");

    init_memory_management();
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
    // This is a lame solution to make sure the stack is 16-bytes aligned just before
    // calling a SysV function. Of course, it only works if all arguments are passed
    // and returned in registers. Not using this risks that the called SysV code uses
    // an SSE instruction with alignment requirement on the stack, and crashes with GP.
    // Use the callee-saved RBX to save the old RSP, leave RBP as the frame pointer.

    bool old = x64->accounting->pause();

    x64->op(PUSHQ, RBX);
    x64->op(MOVQ, RBX, RSP);
    x64->op(ANDQ, RSP, -16);
    
    x64->op(CALL, l);
    
    x64->op(MOVQ, RSP, RBX);
    x64->op(POPQ, RBX);
    
    x64->accounting->unpause(old);
    x64->adjust_stack_usage(24);
    x64->adjust_stack_usage(-24);
}

void Runtime::call_sysv_got(Label got_l) {
    // As above

    bool old = x64->accounting->pause();
    
    x64->op(PUSHQ, RBX);
    x64->op(MOVQ, RBX, RSP);
    x64->op(ANDQ, RSP, -16);
    
    x64->op(CALL, Address(got_l, 0));
    
    x64->op(MOVQ, RSP, RBX);
    x64->op(POPQ, RBX);

    x64->accounting->unpause(old);
    x64->adjust_stack_usage(24);
    x64->adjust_stack_usage(-24);
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
    call_sysv(sysv_memfree_label);  // clobbers all
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
    call_sysv(sysv_memfree_label);  // will probably clobber everything

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
    call_sysv(sysv_memalloc_label);
    
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
    call_sysv(sysv_memrealloc_label);
    
    x64->op(ADDQ, RAX, HEAP_HEADER_SIZE);
    x64->op(RET);
}

void Runtime::compile_fcb_alloc() {
    // fcb_alloc(pointer, callback, payload1, payload2)
    // Clobbers all registers

    x64->code_label_global(fcb_alloc_label, "fcb_alloc");
    const int ARGS = ARGS_4;

    x64->op(MOVQ, RDI, FCB_SIZE);
    call_sysv(sysv_memalloc_label);
    
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
    call_sysv(sysv_memfree_label);  // clobbers all

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

void Runtime::init_memory_management() {
    x64->data_align(ADDRESS_SIZE);
    x64->data_label_global(refcount_balance_label, "refcount_balance");
    x64->data_qword(0);

    x64->code_label_global(empty_function_label, "empty_function");
    x64->op(RET);

    compile_incref_decref();
    compile_finalize();
    compile_heap_alloc();
    compile_heap_realloc();
    compile_fcb_alloc();
    compile_fcb_free();
    compile_finalize_reference_array();
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


void Runtime::log(std::string message) {
    Label message_label;
    x64->data_label(message_label);
    x64->data_zstring(message);

    pusha();
    x64->op(LEA, RDI, Address(message_label, 0));
    call_sysv(sysv_logfunc_label);
    popa();
}

void Runtime::logref(std::string message, Register r) {
    Label message_label;
    x64->data_label(message_label);
    x64->data_zstring(message);

    x64->op(PUSHFQ);
    pusha();
    x64->op(MOVQ, RSI, r);
    x64->op(LEA, RDI, Address(message_label, 0));
    call_sysv(sysv_logreffunc_label);
    popa();
    x64->op(POPFQ);
}

void Runtime::dump(std::string message) {
    Label message_label;
    x64->data_label(message_label);
    x64->data_zstring(message);

    x64->op(PUSHFQ);

    for (Register r : { RAX, RBX, RCX, RDX, RSP, RBP, RSI, RDI, R8, R9, R10, R11, R12, R13, R14, R15 })
        x64->op(PUSHQ, r);
    
    x64->op(LEA, RDI, Address(message_label, 0));
    x64->op(MOVQ, RSI, RSP);
    call_sysv(sysv_dump_label);

    for (Register r : { R15, R14, R13, R12, R11, R10, R9, R8, RDI, RSI, RBP, RDX, RDX, RCX, RBX, RAX })
        x64->op(POPQ, r);
        
    x64->op(POPFQ);
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
