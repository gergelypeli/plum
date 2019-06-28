#include "../plum.h"


// Weakref

void compile_nosyref_callback(Label label, Cx *cx) {
    cx->code_label_local(label, "nosyref_callback");
    cx->prologue();
    cx->runtime->log("Nosyref callback.");

    cx->op(MOVQ, RCX, Address(RSP, RIP_SIZE + ADDRESS_SIZE * 2));  // payload1 arg (nosyref address)
    cx->op(MOVQ, Address(RCX, NOSYREF_MEMBER_OFFSET + NOSYVALUE_RAW_OFFSET), 0);
        
    cx->epilogue();
}


void compile_nosyref_finalizer(Label label, Cx *cx) {
    cx->code_label_local(label, "nosyref_finalizer");
    cx->prologue();
    cx->runtime->log("Nosyref finalized.");

    cx->op(MOVQ, RAX, Address(RSP, ADDRESS_SIZE + RIP_SIZE));  // pointer arg
    
    Label callback_label = cx->once->compile(compile_nosyref_callback);
    Label ok;

    // This object may have died already
    cx->op(MOVQ, R10, Address(RAX, NOSYREF_MEMBER_OFFSET + NOSYVALUE_RAW_OFFSET));
    cx->op(CMPQ, R10, 0);
    cx->op(JE, ok);
    
    cx->op(PUSHQ, R10);  // object
    cx->op(LEA, R10, Address(callback_label, 0));
    cx->op(PUSHQ, R10);  // callback
    cx->op(PUSHQ, RAX);  // payload1
    cx->op(PUSHQ, 0);    // payload2
    
    cx->op(CALL, cx->runtime->fcb_free_label);  // clobbers all

    cx->op(ADDQ, RSP, 4 * ADDRESS_SIZE);
    
    cx->code_label(ok);

    // Member needs no actual finalization

    cx->epilogue();
}


void alloc_nosyref(Cx *cx) {
    Label finalizer_label = cx->once->compile(compile_nosyref_finalizer);
    
    cx->op(PUSHQ, NOSYVALUE_SIZE);
    cx->op(LEA, R10, Address(finalizer_label, 0));
    cx->op(PUSHQ, R10);
    cx->runtime->heap_alloc();  // clobbers all
    cx->op(ADDQ, RSP, 2 * ADDRESS_SIZE);

    // Ref to Nosyref in RAX
}




WeakrefToValue::WeakrefToValue(TypeSpec hts)
    :Value(hts.prefix(weakref_type)) {
    heap_ts = hts;
}

bool WeakrefToValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    TypeSpec arg_ts = heap_ts.prefix(ptr_type);
    
    return check_arguments(args, kwargs, {
        { "target", &arg_ts, scope, &target }
    });
}

Regs WeakrefToValue::precompile(Regs preferred) {
    target->precompile_tail();
    return Regs::all();
}

Storage WeakrefToValue::compile(Cx *cx) {
    Label callback_label = cx->once->compile(compile_nosyref_callback);
    
    target->compile_and_store(cx, Storage(STACK));  // object address
    
    cx->op(LEA, R10, Address(callback_label, 0));
    cx->op(PUSHQ, R10);  // callback address

    alloc_nosyref(cx);
    cx->op(PUSHQ, RAX);  // nosy container address as payload1
    
    cx->op(PUSHQ, 0);  // payload2
    
    cx->op(CALL, cx->runtime->fcb_alloc_label);  // clobbers all, returns nothing
    
    cx->op(POPQ, RBX);
    cx->op(POPQ, RBX);  // nosyref address
    cx->op(POPQ, RCX);
    cx->op(POPQ, RCX);  // object address

    cx->op(MOVQ, Address(RBX, NOSYREF_MEMBER_OFFSET + NOSYVALUE_RAW_OFFSET), RCX);

    heap_ts.decref(RCX, cx);
    
    return Storage(REGISTER, RBX);
}




WeakrefDeadMatcherValue::WeakrefDeadMatcherValue(Value *p, TypeMatch &match)
    :GenericValue(NO_TS, VOID_TS, p) {
    param_heap_ts = match[1];
}

bool WeakrefDeadMatcherValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    if (!check_raise(match_unmatched_exception_type, scope))
        return false;
    
    return GenericValue::check(args, kwargs, scope);
}

Regs WeakrefDeadMatcherValue::precompile(Regs preferred) {
    return left->precompile_tail();
}

Storage WeakrefDeadMatcherValue::compile(Cx *cx) {
    TypeSpec nosy_heap_ts = param_heap_ts.prefix(nosyvalue_type).prefix(nosyref_type);
    Label ok;
    left->compile_and_store(cx, Storage(STACK));
    
    cx->op(POPQ, R10);
    cx->op(MOVQ, R11, Address(R10, NOSYREF_MEMBER_OFFSET + NOSYVALUE_RAW_OFFSET));
    nosy_heap_ts.decref(R10, cx);
    cx->op(CMPQ, R11, 0);
    cx->op(JE, ok);

    // popped
    raise("UNMATCHED", cx);
            
    cx->code_label(ok);
    return Storage();
}




WeakrefLiveMatcherValue::WeakrefLiveMatcherValue(Value *p, TypeMatch &match)
    :GenericValue(NO_TS, match[1].prefix(ptr_type), p) {
    param_heap_ts = match[1];
}

bool WeakrefLiveMatcherValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    if (!check_raise(match_unmatched_exception_type, scope))
        return false;
    
    return GenericValue::check(args, kwargs, scope);
}

Regs WeakrefLiveMatcherValue::precompile(Regs preferred) {
    return left->precompile_tail();
}

Storage WeakrefLiveMatcherValue::compile(Cx *cx) {
    TypeSpec nosy_heap_ts = param_heap_ts.prefix(nosyvalue_type).prefix(nosyref_type);
    Label ok;
    left->compile_and_store(cx, Storage(STACK));
    
    cx->op(POPQ, R10);
    cx->op(MOVQ, R11, Address(R10, NOSYREF_MEMBER_OFFSET + NOSYVALUE_RAW_OFFSET));
    nosy_heap_ts.decref(R10, cx);
    cx->op(CMPQ, R11, 0);
    cx->op(JNE, ok);
    
    // popped
    raise("UNMATCHED", cx);
            
    cx->code_label(ok);
    param_heap_ts.incref(R11, cx);
    cx->op(PUSHQ, R11);
    
    return Storage(STACK);
}
