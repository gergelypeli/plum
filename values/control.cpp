
class RepeatValue: public Value {
public:
    std::unique_ptr<Value> setup, condition, step, body;
    Label start, end;
    
    RepeatValue(Value *pivot, TypeMatch &match)
        :Value(VOID_TS) {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() > 1) {
            Value *s = make_code_block_value(NULL);
            Kwargs fake_kwargs;
        
            if (!s->check(args, fake_kwargs, scope))
                return false;
            
            setup.reset(s);
        }
        else if (args.size() == 1) {
            setup.reset(typize(args[0].get(), scope));
        }
        
        for (auto &kv : kwargs) {
            if (kv.first == "do")
                body.reset(make_code_value(typize(kv.second.get(), scope, &VOID_CODE_TS)));
            else if (kv.first == "on")
                condition.reset(make_code_value(typize(kv.second.get(), scope, &BOOLEAN_CODE_TS)));
            else if (kv.first == "by")
                step.reset(make_code_value(typize(kv.second.get(), scope, &VOID_CODE_TS)));
            else {
                std::cerr << "Invalid argument to :repeat!\n";
                return false;
            }
        }

        return true;
    }
    
    virtual Regs precompile(Regs preferred) {
        if (setup)
            setup->precompile(Regs::all());
            
        if (condition)
            condition->precompile(Regs::all());
            
        if (step)
            step->precompile(Regs::all());
            
        if (body)
            body->precompile(Regs::all());
            
        return Regs::all();  // We're Void
    }
    
    virtual Storage compile(X64 *x64) {
        // Exceptions are only caught from the loop body
        if (setup)
            setup->compile_and_store(x64, Storage());
    
        x64->code_label(start);

        if (condition) {
            Storage cs = condition->compile(x64);
            
            switch (cs.where) {
            case CONSTANT:
                if (!cs.value)
                    x64->op(JMP, end);
                break;
            case FLAGS:
                x64->op(branchize(negate(cs.bitset)), end);
                break;
            case REGISTER:
                x64->op(CMPB, cs.reg, 0);
                x64->op(JE, end);
                break;
            case MEMORY:
                x64->op(CMPB, cs.address, 0);
                x64->op(JE, end);
                break;
            default:
                throw INTERNAL_ERROR;
            }
        }
        
        x64->unwind->push(this);
        body->compile_and_store(x64, Storage());
        x64->unwind->pop(this);
        
        if (step)
            step->compile_and_store(x64, Storage());
        
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

