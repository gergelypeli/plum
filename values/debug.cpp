

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

Storage FrameNameValue::compile(X64 *x64) {
    Label ok;
    
    value->compile_and_store(x64, Storage(STACK));

    x64->op(CALL, x64->runtime->caller_frame_info_label);

    x64->op(ADDQ, RSP, INTEGER_SIZE);
    
    x64->op(CMPQ, RAX, 0);
    x64->op(JNE, ok);
    
    raise("NOT_FOUND", x64);
    
    x64->code_label(ok);
    x64->op(MOVQ, RAX, Address(RAX, FRAME_INFO_NAME_OFFSET));
    x64->runtime->incref(RAX);
    
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

Storage FrameStuffValue::compile(X64 *x64) {
    Label ok, no_call_info;
    
    value->compile_and_store(x64, Storage(STACK));

    x64->op(CALL, x64->runtime->caller_frame_info_label);
    x64->runtime->add_call_info(token.file_index, token.row);  // this is kinda funny

    x64->op(ADDQ, RSP, INTEGER_SIZE);
    
    x64->op(CMPQ, RAX, 0);
    x64->op(JNE, ok);
    
    raise("NOT_FOUND", x64);
    
    x64->code_label(ok);
    
    x64->op(MOVQ, R10, Address(RAX, FRAME_INFO_NAME_OFFSET));
    x64->runtime->incref(R10);
    x64->op(PUSHQ, R10);  // function name String
    
    // Must please the stack accounting here
    x64->op(MOVQ, RSI, 0);
    x64->op(MOVQ, RDI, 0);
    
    x64->op(CMPQ, RBX, 0);
    x64->op(JE, no_call_info);
    
    x64->op(MOVZXWQ, RSI, Address(RBX, 4));
    x64->op(MOVZXWQ, RDI, Address(RBX, 6));
    
    x64->code_label(no_call_info);
    x64->op(PUSHQ, RSI);
    x64->op(CALL, x64->runtime->lookup_source_info_label);
    x64->op(POPQ, RSI);
    
    x64->op(PUSHQ, RAX);  // source file name String
    x64->op(PUSHQ, RDI);  // line number Integer
    
    return Storage(STACK);
}



DoubleStackValue::DoubleStackValue(Value *, TypeMatch)
    :Value(VOID_TS) {
}

Regs DoubleStackValue::precompile(Regs preferred) {
    return Regs::all();
}

Storage DoubleStackValue::compile(X64 *x64) {
    x64->op(CALL, x64->runtime->double_stack_label);

    return Storage();
}



DieValue::DieValue(Value *p, TypeMatch tm)
    :Value(WHATEVER_TS) {
}

Regs DieValue::precompile(Regs preferred) {
    return Regs();
}

Storage DieValue::compile(X64 *x64) {
    x64->runtime->die("As expected.");
    return Storage();
}
