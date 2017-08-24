
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


class SwitchValue: public Value {
public:
    std::unique_ptr<Value> value, body;
    Variable *value_var;
    Label end;
    
    SwitchValue(Value *v, TypeMatch &m)
        :Value(VOID_TS) {
        value_var = NULL;
        // TODO: return something?
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() != 1) {
            std::cerr << "Whacky :switch!\n";
            return false;
        }
        
        value.reset(typize(args[0].get(), scope));
    
        // Insert variable before the body to keep the finalization order
        value_var = new Variable("<switched>", VOID_TS, value->ts.lvalue());
        scope->add(value_var);
        
        for (auto &kv : kwargs) {
            if (kv.first == "do")
                body.reset(make_code_value(typize(kv.second.get(), scope, &VOID_CODE_TS)));
            else {
                std::cerr << "Invalid argument to :switch!\n";
                return false;
            }
        }

        return true;
    }

    virtual Regs precompile(Regs preferred) {
        value->precompile(Regs::all());
            
        if (body)
            body->precompile(Regs::all());
            
        return Regs::all();  // We're Void
    }
    
    virtual Storage compile(X64 *x64) {
        Storage vs = value->compile(x64);

        Storage fn_storage(MEMORY, Address(RBP, 0));
        Storage var_storage = value_var->get_storage(fn_storage);
        value->ts.create(vs, var_storage, x64);
        
        x64->unwind->push(this);
        
        if (body)
            body->compile_and_store(x64, Storage());
        
        x64->unwind->pop(this);

        x64->code_label(end);
        
        return Storage();
    }
    
    virtual bool unwind(X64 *x64) {
        Label nonunswitch;
        
        x64->op(CMPQ, x64->exception_label, UNSWITCH_EXCEPTION);
        x64->op(JNE, nonunswitch);
        x64->op(MOVQ, x64->exception_label, NO_EXCEPTION);
        x64->op(JMP, end);
        x64->code_label(nonunswitch);
        
        return false;
    }
};


class WhenValue: public Value {
public:
    std::unique_ptr<Value> cover, body;
    //Variable *value_var;
    Declaration *dummy;
    
    WhenValue(Value *v, TypeMatch &m)
        :Value(VOID_TS) {
        dummy = NULL;
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        //SwitchScope *switch_scope = scope->get_switch_scope();
        
        //if (!switch_scope) {
        //    std::cerr << "Not in :switch!\n";
        //    return false;
        //}
        
        //value_var = variable_cast(switch_scope->contents[0].get());
        
        //if (!value_var)
        //    throw INTERNAL_ERROR;
    
        if (args.size() != 1) {
            std::cerr << "Whacky :when!\n";
            return false;
        }
        
        Value *pivot = typize(args[0].get(), scope);

        Expr cover_expr(Expr::IDENTIFIER, Token(), "cover");
        cover_expr.add_arg(new Expr(Expr::IDENTIFIER, Token(), "<switched>"));
        Value *cover = lookup("cover", pivot, &cover_expr, scope);
        
        if (!cover) {
            std::cerr << "Cannot :switch with uncoverable " << pivot->ts << "!\n";
            return false;
        }

        if (cover->ts != BOOLEAN_TS) {
            std::cerr << "Cannot :switch with nonboolean cover!\n";
            return false;
        }
        
        this->cover.reset(cover);
    
        dummy = new Declaration;
        scope->add(dummy);
    
        for (auto &kv : kwargs) {
            if (kv.first == "then")
                body.reset(make_code_value(typize(kv.second.get(), scope, &VOID_CODE_TS)));
            else {
                std::cerr << "Invalid argument to :when!\n";
                return false;
            }
        }

        return true;
    }

    virtual Regs precompile(Regs preferred) {
        cover->precompile(Regs::all());
            
        if (body)
            body->precompile(Regs::all());
            
        return Regs::all();  // We're Void
    }
    
    virtual Storage compile(X64 *x64) {
        Label end;
        Storage cs = cover->compile(x64);

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

        // Need no unwinding, because we don't store temporary values, and
        // the variable will be destroyed separately.
        
        if (body)
            body->compile_and_store(x64, Storage());
        
        x64->op(MOVQ, x64->exception_label, UNSWITCH_EXCEPTION);
        x64->unwind->unwind(dummy->outer_scope, dummy->previous_declaration, x64);
        
        x64->code_label(end);
        
        return Storage();
    }
};
