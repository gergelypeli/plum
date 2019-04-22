
class FrameNameValue: public Value, public Raiser {
public:
    std::unique_ptr<Value> value;
    
    FrameNameValue(Value *, TypeMatch)
        :Value(STRING_TS) {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!check_raise(lookup_exception_type, scope))
            return false;
            
        return check_arguments(args, kwargs, { { "up", &INTEGER_TS, scope, &value } });
    }
    
    virtual Regs precompile(Regs preferred) {
        return value->precompile(preferred) | Regs::all();
    }
    
    virtual Storage compile(X64 *x64) {
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
};


class FrameStuffValue: public Value, public Raiser {
public:
    std::unique_ptr<Value> value;
    
    FrameStuffValue(Value *, TypeMatch)
        :Value(MULTI_TS) {
    }

    virtual bool unpack(std::vector<TypeSpec> &tss) {
        tss = { STRING_TS, STRING_TS, INTEGER_TS };
        return true;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!check_raise(lookup_exception_type, scope))
            return false;
            
        return check_arguments(args, kwargs, { { "up", &INTEGER_TS, scope, &value } });
    }
    
    virtual Regs precompile(Regs preferred) {
        return value->precompile(preferred) | Regs::all();
    }
    
    virtual Storage compile(X64 *x64) {
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
};


class DoubleStackValue: public Value {
public:
    DoubleStackValue(Value *, TypeMatch)
        :Value(VOID_TS) {
    }

    virtual Regs precompile(Regs preferred) {
        return Regs::all();
    }
    
    virtual Storage compile(X64 *x64) {
        x64->op(CALL, x64->runtime->double_stack_label);

        return Storage();
    }
};
