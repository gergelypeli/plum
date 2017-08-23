
class RepeatValue: public Value {
public:
    std::unique_ptr<Value> body;
    Label start, end;
    
    RepeatValue(Value *pivot, TypeMatch &match)
        :Value(VOID_TS) {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() != 0) {
            std::cerr << "Positional argument to :repeat!\n";
            return false;
        }
        
        for (auto &kv : kwargs) {
            if (kv.first == "do")
                body.reset(make_code_value(typize(kv.second.get(), scope, &VOID_CODE_TS)));
            else {
                std::cerr << "Invalid argument to :repeat!\n";
                return false;
            }
        }

        return true;
    }
    
    virtual Regs precompile(Regs preferred) {
        if (body)
            body->precompile(preferred);
            
        return Regs::all();  // We're Void
    }
    
    virtual Storage compile(X64 *x64) {
        x64->code_label(start);
        
        x64->unwind->push(this);
        body->compile_and_store(x64, Storage());
        x64->unwind->pop(this);
        
        x64->op(JMP, start);
        
        x64->code_label(end);
        return Storage();
    }
    
    virtual bool unwind(X64 *x64) {
        Label noncontinue, nonbreak;
        
        x64->op(CMPQ, x64->exception_label, CONTINUE_EXCEPTION);
        x64->op(JNE, noncontinue);
        x64->op(MOVQ, x64->exception_label, NO_EXCEPTION);
        x64->op(JMP, start);
        x64->code_label(noncontinue);
        
        x64->op(CMPQ, x64->exception_label, BREAK_EXCEPTION);
        x64->op(JNE, nonbreak);
        x64->op(MOVQ, x64->exception_label, NO_EXCEPTION);
        x64->op(JMP, end);
        x64->code_label(nonbreak);

        return false;
    }
};


// TOOD: these two classes are almost the same

class BreakValue: public Value {
public:
    Declaration *dummy;
    
    BreakValue(Value *v, TypeMatch &m)
        :Value(VOID_TS) {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() != 0 || kwargs.size() != 0) {
            std::cerr << "Whacky :break!\n";
            return false;
        }

        dummy = new Declaration;
        scope->add(dummy);

        return true;
    }

    virtual Regs precompile(Regs) {
        return Regs();  // We won't return
    }

    virtual Storage compile(X64 *x64) {
        x64->op(MOVQ, x64->exception_label, BREAK_EXCEPTION);

        x64->unwind->unwind(dummy->outer_scope, dummy->previous_declaration, x64);
        
        return Storage();
    }
};


class ContinueValue: public Value {
public:
    Declaration *dummy;
    
    ContinueValue(Value *v, TypeMatch &m)
        :Value(VOID_TS) {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() != 0 || kwargs.size() != 0) {
            std::cerr << "Whacky :continue!\n";
            return false;
        }

        dummy = new Declaration;
        scope->add(dummy);

        return true;
    }

    virtual Regs precompile(Regs) {
        return Regs();  // We won't return
    }

    virtual Storage compile(X64 *x64) {
        x64->op(MOVQ, x64->exception_label, CONTINUE_EXCEPTION);

        x64->unwind->unwind(dummy->outer_scope, dummy->previous_declaration, x64);
        
        return Storage();
    }
};

