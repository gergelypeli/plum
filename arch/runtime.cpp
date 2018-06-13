
//#define EXCEPTION_ADDRESS Address(RBP, -8)
//#define RESULT_ALIAS_ADDRESS Address(RBP, -8)


class Runtime {
public:
    X64 *x64;
    
    Label zero_label, float_minus_zero_label;
    Label alloc_RAX_RBX_label, realloc_RAX_RBX_label;
    Label empty_function_label, empty_array_label;
    Label alloc_fcb_label, free_fcb_label;
    Label finalize_label, finalize_reference_array_label, weak_finalized_die_label;
    std::vector<Label> incref_labels, decref_labels;

    Label sysv_memalloc_label, sysv_memfree_label, sysv_memrealloc_label;
    Label sysv_logfunc_label, sysv_dump_label, sysv_die_label, sysv_dies_label;
    Label sysv_sort_label, sysv_string_regexp_match_label;
    
    Runtime(X64 *x) {
        x64 = x;
        
        x64->absolute_label(zero_label, 0);

        // Float constants are aligned to 16 bytes so SSE packed instructions can use it
        x64->data_align(16);
        x64->data_label(float_minus_zero_label);
        x64->data_double(-0.0);
        x64->data_double(0.0);

        data_heap_header();
        x64->data_label(empty_array_label);
        x64->data_qword(0);  // reservation
        x64->data_qword(0);  // length

        x64->code_label_import(sysv_memalloc_label, "memalloc");
        x64->code_label_import(sysv_memfree_label, "memfree");
        x64->code_label_import(sysv_memrealloc_label, "memrealloc");
        
        x64->code_label_import(sysv_logfunc_label, "logfunc");  // bah...
        x64->code_label_import(sysv_dump_label, "dump");  // bah...
        x64->code_label_import(sysv_die_label, "die");
        x64->code_label_import(sysv_dies_label, "dies");
        
        x64->code_label_import(sysv_sort_label, "sort");
        x64->code_label_import(sysv_string_regexp_match_label, "string_regexp_match");

        init_memory_management();
    }

    void data_heap_header() {
        if (HEAP_HEADER_SIZE != 32 || HEAP_REFCOUNT_OFFSET != -16 || HEAP_WEAKREFCOUNT_OFFSET != -8 || HEAP_FINALIZER_OFFSET != -24)
            throw X64_ERROR;
    
        x64->data_align(8);
        x64->data_qword(0);  // next
        x64->data_reference(empty_function_label);  // finalizer
        x64->data_qword(1);  // artificial reference to prevent freeing
        x64->data_qword(0);  // weakrefcount
    }

    Label data_heap_string(std::vector<unsigned short> characters) {
        if (ARRAY_HEADER_SIZE != 16 || ARRAY_RESERVATION_OFFSET != 0 || ARRAY_LENGTH_OFFSET != 8)
            throw X64_ERROR;
        
        Label l;
    
        data_heap_header();
        x64->data_label(l);
        x64->data_qword(characters.size());
        x64->data_qword(characters.size());

        for (unsigned short &c : characters)
            x64->data_word(c);

        return l;
    }

    void call_sysv(Label l) {
        // This is a lame solution to make sure the stack is 16-bytes aligned just before
        // calling a SysV function. Of course, it only works if all arguments are passed
        // and returned in registers. Not using this risks that the called SysV code uses
        // an SSE instruction with alignment requirement on the stack, and crashes with GP.
    
        x64->op(PUSHQ, RBP);
        x64->op(MOVQ, RBP, RSP);
        x64->op(ANDQ, RSP, -16);
        
        x64->op(CALL, l);
        
        x64->op(MOVQ, RSP, RBP);
        x64->op(POPQ, RBP);
    }

    void call_sysv_got(Label got_l) {
        x64->op(PUSHQ, RBP);
        x64->op(MOVQ, RBP, RSP);
        x64->op(ANDQ, RSP, -16);
        
        x64->op(CALL, Address(got_l, 0));
        
        x64->op(MOVQ, RSP, RBP);
        x64->op(POPQ, RBP);
    }

    int pusha(bool except_rax = false) {
        // RBX and the last 4 are preserved by the System V ABI
        // But we save RBX, because it's annoying to handle it separately
        // 10 registers, including RAX
    
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

    void popa(bool except_rax = false) {
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

    void init_memory_management() {
        incref_labels.resize(REGISTER_COUNT);
        decref_labels.resize(REGISTER_COUNT);
    
        for (Register reg : { RAX, RBX, RCX, RDX, RSI, RDI, R8, R9, R10, R11, R12, R13, R14, R15 }) {
            Label il;
        
            // NOTE: preserves all registers, including RBX
            x64->code_label_global(incref_labels[reg], std::string("incref_") + REGISTER_NAMES[reg]);
            x64->op(INCQ, Address(reg, HEAP_REFCOUNT_OFFSET));
            x64->op(RET);
        
            Label dl, dl2;
        
            // NOTE: preserves all registers, including RBX
            x64->code_label_global(decref_labels[reg], std::string("decref_") + REGISTER_NAMES[reg]);
            x64->op(DECQ, Address(reg, HEAP_REFCOUNT_OFFSET));
            x64->op(JNE, dl);

            x64->op(PUSHQ, reg);
            x64->op(CALL, finalize_label);
            x64->op(ADDQ, RSP, 8);

            x64->code_label(dl);
            x64->op(RET);
        }

        // Stack - reference to finalize
        // Preserves all registers, including RBX
        x64->code_label_global(finalize_label, "finalize");
        const int ARG_OFFSET = pusha() + 8;
    
        Label fcb_loop, fcb_cond, no_weakrefs;
        x64->op(JMP, fcb_cond);

        x64->code_label(fcb_loop);
        log("Triggering prefinalizer callback.");
        x64->op(MOVQ, RCX, Address(RAX, FCB_PAYLOAD1_OFFSET));
        x64->op(MOVQ, RDX, Address(RAX, FCB_PAYLOAD2_OFFSET));
    
        // RAX - fcb, RCX, RDX - payloads, may clobber everything
        // Must free this fcb, and potentially more
        x64->op(CALL, Address(RAX, FCB_CALLBACK_OFFSET));

        x64->code_label(fcb_cond);
        x64->op(MOVQ, RBX, Address(RSP, ARG_OFFSET));  // reference
        x64->op(MOVQ, RAX, Address(RBX, HEAP_NEXT_OFFSET));
        x64->op(CMPQ, RAX, FCB_NIL);
        x64->op(JNE, fcb_loop);

        x64->op(MOVQ, RAX, Address(RSP, ARG_OFFSET));
        x64->op(CALL, Address(RAX, HEAP_FINALIZER_OFFSET));  // may clobber everything

        x64->op(MOVQ, RAX, Address(RSP, ARG_OFFSET));
        x64->op(CMPQ, Address(RAX, HEAP_WEAKREFCOUNT_OFFSET), 0);
        x64->op(JE, no_weakrefs);
        dump("RAX=weakrefcount");
        die("Weakly referenced object finalized!");
    
        x64->code_label(no_weakrefs);
        x64->op(LEA, RDI, Address(RAX, HEAP_HEADER_OFFSET));
        call_sysv(sysv_memfree_label);  // will probably clobber everything

        popa();
        x64->op(RET);

        x64->code_label_global(alloc_RAX_RBX_label, "alloc_RAX_RBX");
        pusha(true);
        x64->op(LEA, RDI, Address(RAX, HEAP_HEADER_SIZE));
        call_sysv(sysv_memalloc_label);
        x64->op(LEA, RAX, Address(RAX, -HEAP_HEADER_OFFSET));
        x64->op(MOVQ, Address(RAX, HEAP_NEXT_OFFSET), 0);
        x64->op(MOVQ, Address(RAX, HEAP_FINALIZER_OFFSET), RBX);  // object finalizer
        x64->op(MOVQ, Address(RAX, HEAP_REFCOUNT_OFFSET), 1);  // start from 1
        x64->op(MOVQ, Address(RAX, HEAP_WEAKREFCOUNT_OFFSET), 0);  // start from 0
        popa(true);
        x64->op(RET);
    
        x64->code_label_global(realloc_RAX_RBX_label, "realloc_RAX_RBX");
        Label realloc_ok;
        lock(RAX, realloc_ok);
        die("Realloc of shared array!");
    
        x64->code_label(realloc_ok);
        pusha(true);
        x64->op(LEA, RDI, Address(RAX, HEAP_HEADER_OFFSET));
        x64->op(LEA, RSI, Address(RBX, HEAP_HEADER_SIZE));
        call_sysv(sysv_memrealloc_label);
        x64->op(LEA, RAX, Address(RAX, -HEAP_HEADER_OFFSET));
        popa(true);
        x64->op(RET);
    
        x64->code_label_global(empty_function_label, "empty_function");
        x64->op(RET);
    
        x64->code_label_global(weak_finalized_die_label, "weak_finalized_die");
        die("Weakly referenced object finalized!");
    
        // Clobbers RAX, RX, RCX, RDX, returns FCB pointer in RAX
        x64->code_label_global(alloc_fcb_label, "alloc_fcb");

        pusha();
        //op(PUSHQ, RAX);  // heap
        //op(PUSHQ, RBX);  // callback
        //op(PUSHQ, RCX);  // payload1
        //op(PUSHQ, RDX);  // payload2
        x64->op(MOVQ, RDI, FCB_SIZE);
        call_sysv(sysv_memalloc_label);
        popa(true);  // leave RAX on the stack
    
        x64->op(MOVQ, Address(RAX, FCB_CALLBACK_OFFSET), RBX);
        x64->op(MOVQ, Address(RAX, FCB_PAYLOAD1_OFFSET), RCX);
        x64->op(MOVQ, Address(RAX, FCB_PAYLOAD2_OFFSET), RDX);
        x64->op(POPQ, RBX);  // object

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
    
        // RAX - fcb
        x64->code_label_global(free_fcb_label, "free_fcb");
        pusha();

        x64->op(MOVQ, RBX, Address(RAX, FCB_PREV_OFFSET));  // always valid
        x64->op(MOVQ, RCX, Address(RAX, FCB_NEXT_OFFSET));
        x64->op(MOVQ, Address(RBX, FCB_NEXT_OFFSET), RCX);
        //dump("free_fcb RAX=fcb, RBX=prev, RCX=next");
    
        Label no_next2;
        x64->op(CMPQ, RCX, FCB_NIL);
        x64->op(JE, no_next2);
        x64->op(MOVQ, Address(RCX, FCB_PREV_OFFSET), RBX);
        x64->code_label(no_next2);
    
        x64->op(MOVQ, RDI, RAX);
        call_sysv(sysv_memfree_label);

        popa();
        x64->op(RET);
    
        x64->code_label_global(finalize_reference_array_label, "finalize_reference_array");
        Label fra_cond, fra_loop;
        x64->op(MOVQ, RCX, 0);
        x64->op(JMP, fra_cond);
    
        x64->code_label(fra_loop);
        x64->op(MOVQ, RBX, Address(RAX, RCX, 8, ARRAY_ELEMS_OFFSET));
        decref(RBX);
        x64->op(INCQ, RCX);
    
        x64->code_label(fra_cond);
        x64->op(CMPQ, RCX, Address(RAX, ARRAY_LENGTH_OFFSET));
        x64->op(JB, fra_loop);
    
        x64->op(RET);
    }

    void incref(Register reg) {
        if (reg == RSP || reg == RBP || reg == NOREG)
            throw X64_ERROR;
        
        x64->op(CALL, incref_labels[reg]);
    }

    void decref(Register reg) {
        if (reg == RSP || reg == RBP || reg == NOREG)
            throw X64_ERROR;

        x64->op(CALL, decref_labels[reg]);
    }

    void incweakref(Register reg) {
        if (reg == RSP || reg == RBP || reg == NOREG)
            throw X64_ERROR;

        x64->op(INCQ, Address(reg, HEAP_WEAKREFCOUNT_OFFSET));
    }

    void decweakref(Register reg) {
        if (reg == RSP || reg == RBP || reg == NOREG)
            throw X64_ERROR;

        x64->op(DECQ, Address(reg, HEAP_WEAKREFCOUNT_OFFSET));
    }

    void memfree(Register reg) {
        pusha();
        x64->op(LEA, RDI, Address(reg, HEAP_HEADER_OFFSET));
        call_sysv(sysv_memfree_label);
        popa();
    }

    void alloc_RAX_RBX() {
        x64->op(CALL, alloc_RAX_RBX_label);
    }

    void realloc_RAX_RBX() {
        x64->op(CALL, realloc_RAX_RBX_label);
    }

    void lock(Register r, Label ok) {
        x64->op(CMPQ, Address(r, HEAP_REFCOUNT_OFFSET), 1);
        x64->op(JE, ok);
    }

    void log(std::string message) {
        Label message_label;
        x64->data_label(message_label);
        x64->data_zstring(message);

        pusha();
        x64->op(LEA, RDI, Address(message_label, 0));
        call_sysv(sysv_logfunc_label);
        popa();
    }


    void dump(std::string message) {
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


    void die(std::string message) {
        // TODO: this encodes the message several times unnecessarily!
        Label message_label;
        x64->data_label(message_label);
        x64->data_zstring(message);
    
        x64->op(LEA, RDI, Address(message_label, 0));
        call_sysv(sysv_die_label);
        x64->op(UD2);
    }


    void dies(Register r) {
        x64->op(MOVQ, RDI, r);
        call_sysv(sysv_dies_label);
        x64->op(UD2);
    }
};
