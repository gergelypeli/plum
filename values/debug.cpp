
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
