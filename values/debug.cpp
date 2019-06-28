#include "../plum.h"


FrameNameValue::FrameNameValue(Value *, TypeMatch)
    :Value(STRING_TS) {
}

bool FrameNameValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    if (!check_raise(lookup_exception_type, scope))
        return false;
        
    return check_arguments(args, kwargs, { { "up", &INTEGER_TS, scope, &value } });
}

Regs FrameNameValue::precompile(Regs preferred) {
    return value->precompile_tail() | Regs::all();
}

Storage FrameNameValue::compile(Cx *cx) {
    Label ok;
    
    value->compile_and_store(cx, Storage(STACK));

    cx->op(CALL, cx->runtime->caller_frame_info_label);

    cx->op(ADDQ, RSP, INTEGER_SIZE);
    
    cx->op(CMPQ, RAX, 0);
    cx->op(JNE, ok);
    
    raise("NOT_FOUND", cx);
    
    cx->code_label(ok);
    cx->op(MOVQ, RAX, Address(RAX, FRAME_INFO_NAME_OFFSET));
    cx->runtime->incref(RAX);
    
    return Storage(REGISTER, RAX);
}




FrameStuffValue::FrameStuffValue(Value *, TypeMatch)
    :Value({ tuple3_type, string_type, string_type, integer_type }) {
}

bool FrameStuffValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    if (!check_raise(lookup_exception_type, scope))
        return false;
        
    return check_arguments(args, kwargs, { { "up", &INTEGER_TS, scope, &value } });
}

Regs FrameStuffValue::precompile(Regs preferred) {
    return value->precompile_tail() | Regs::all();
}

Storage FrameStuffValue::compile(Cx *cx) {
    Label ok, no_call_info;
    
    value->compile_and_store(cx, Storage(STACK));

    cx->op(CALL, cx->runtime->caller_frame_info_label);
    cx->runtime->add_call_info(token.file_index, token.row);  // this is kinda funny

    cx->op(ADDQ, RSP, INTEGER_SIZE);
    
    cx->op(CMPQ, RAX, 0);
    cx->op(JNE, ok);
    
    raise("NOT_FOUND", cx);
    
    cx->code_label(ok);
    
    cx->op(MOVQ, R10, Address(RAX, FRAME_INFO_NAME_OFFSET));
    cx->runtime->incref(R10);
    cx->op(PUSHQ, R10);  // function name String
    
    // Must please the stack accounting here
    cx->op(MOVQ, RSI, 0);
    cx->op(MOVQ, RDI, 0);
    
    cx->op(CMPQ, RBX, 0);
    cx->op(JE, no_call_info);
    
    cx->op(MOVZXWQ, RSI, Address(RBX, 4));
    cx->op(MOVZXWQ, RDI, Address(RBX, 6));
    
    cx->code_label(no_call_info);
    cx->op(PUSHQ, RSI);
    cx->op(CALL, cx->runtime->lookup_source_info_label);
    cx->op(POPQ, RSI);
    
    cx->op(PUSHQ, RAX);  // source file name String
    cx->op(PUSHQ, RDI);  // line number Integer
    
    return Storage(STACK);
}



DoubleStackValue::DoubleStackValue(Value *, TypeMatch)
    :Value(VOID_TS) {
}

Regs DoubleStackValue::precompile(Regs preferred) {
    return Regs::all();
}

Storage DoubleStackValue::compile(Cx *cx) {
    cx->op(CALL, cx->runtime->double_stack_label);

    return Storage();
}



DieValue::DieValue(Value *p, TypeMatch tm)
    :Value(WHATEVER_TS) {
}

Regs DieValue::precompile(Regs preferred) {
    return Regs();
}

Storage DieValue::compile(Cx *cx) {
    cx->runtime->die("As expected.");
    return Storage();
}
