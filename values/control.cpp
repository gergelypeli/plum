
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
    
    virtual Scope *unwind(X64 *x64) {
        Label noncontinue, nonbreak;
        
        x64->op(CMPB, EXCEPTION_ADDRESS, CONTINUE_EXCEPTION);
        x64->op(JNE, noncontinue);
        x64->op(MOVB, EXCEPTION_ADDRESS, NO_EXCEPTION);
        x64->op(JMP, start);
        x64->code_label(noncontinue);
        
        x64->op(CMPB, EXCEPTION_ADDRESS, BREAK_EXCEPTION);
        x64->op(JNE, nonbreak);
        x64->op(MOVB, EXCEPTION_ADDRESS, NO_EXCEPTION);
        x64->op(JMP, end);
        x64->code_label(nonbreak);

        return NULL;
    }
};



class ForEachValue: public Value {
public:
    std::unique_ptr<Value> iterator, each, body, next;
    Variable *iterator_var;
    TryScope *next_try_scope;
    TypeSpec each_ts;
    Label start, end;
    bool performing;
    
    ForEachValue(Value *pivot, TypeMatch &match)
        :Value(VOID_TS) {
        iterator_var = NULL;
        performing = false;
        next_try_scope = NULL;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() == 1) {
            iterator.reset(typize(args[0].get(), scope));
            
            // TODO: this should be a "local variable", to be destroyed once we're done
            // instead of letting the enclosing scope destroy it.
            TypeSpec its = iterator->ts.lvalue();
            iterator_var = new Variable("<iterator>", VOID_TS, its);
            scope->add(iterator_var);

            next_try_scope = new TryScope;
            scope->add(next_try_scope);

            Expr it_expr(Expr::IDENTIFIER, Token(), "<iterator>");
            Value *it = lookup("<iterator>", NULL, &it_expr, next_try_scope);
            
            Expr next_expr(Expr::IDENTIFIER, Token(), "next");
            Value *next = lookup("next", it, &next_expr, next_try_scope);
            
            if (!next) {
                std::cerr << ":for argument has no next method!\n";
                return false;
            }
            
            this->next.reset(next);
            
            each_ts = next->ts.lvalue();
        }
        else {
            std::cerr << "Missing iterator in :for!\n";
            return false;
        }
        
        for (auto &kv : kwargs) {
            if (kv.first == "each") {
                Value *v = typize(kv.second.get(), scope, &each_ts);
                TypeMatch match;
                
                if (!typematch(each_ts, v, match)) {
                    std::cerr << "Wrong each argument!\n";
                    return false;
                }
                
                each.reset(v);
            }
            else if (kv.first == "do")
                body.reset(make_code_value(typize(kv.second.get(), scope, &VOID_CODE_TS)));
            else {
                std::cerr << "Invalid argument to :for!\n";
                return false;
            }
        }

        return true;
    }
    
    virtual Regs precompile(Regs preferred) {
        if (iterator)
            iterator->precompile(Regs::all());
            
        if (each)
            each->precompile(Regs::all());
            
        if (body)
            body->precompile(Regs::all());
            
        if (next)
            next->precompile(Regs::all());
            
        return Regs::all();  // We're Void
    }
    
    virtual Storage compile(X64 *x64) {
        Storage is = iterator->compile(x64);
        Storage fn_storage(MEMORY, Address(RBP, 0));
        Storage var_storage = iterator_var->get_storage(fn_storage);
        iterator->ts.create(is, var_storage, x64);
        
        Storage es = each->compile(x64);
        if (es.where != MEMORY || es.address.base != RBP)
            throw INTERNAL_ERROR;  // FIXME: lame temporary restriction only
        
        x64->code_label(start);

        x64->unwind->push(this);
        performing = false;
        Storage ns = next->compile(x64);
        
        next->ts.store(ns, es, x64);
        // Finalize after storing, so the return value won't be lost
        next_try_scope->finalize_contents(x64);

        performing = true;
        body->compile_and_store(x64, Storage());
        
        x64->unwind->pop(this);
        
        x64->op(JMP, start);
        
        x64->code_label(end);
        // We don't need to clean up local variables yet
        
        return Storage();
    }
    
    virtual Scope *unwind(X64 *x64) {
        if (!performing) {
            x64->op(MOVB, EXCEPTION_ADDRESS, NO_EXCEPTION);
            x64->op(JMP, end);
            return next_try_scope;
        }

        // We don't need to clean up temporary values yet
        return NULL;
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
        x64->op(MOVB, EXCEPTION_ADDRESS, BREAK_EXCEPTION);

        x64->unwind->initiate(dummy, x64);
        
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
        x64->op(MOVB, EXCEPTION_ADDRESS, CONTINUE_EXCEPTION);

        x64->unwind->initiate(dummy, x64);
        
        return Storage();
    }
};


class SwitchValue: public Value {
public:
    std::unique_ptr<Value> value, body;
    Variable *value_var;
    SwitchScope *switch_scope;
    bool may_be_aborted;
    
    SwitchValue(Value *v, TypeMatch &m)
        :Value(VOID_TS) {
        value_var = NULL;
        switch_scope = NULL;
        may_be_aborted = false;
        // TODO: return something?
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() != 1) {
            std::cerr << "Whacky :switch!\n";
            return false;
        }
        
        value.reset(typize(args[0].get(), scope));
    
        // Insert variable before the body to keep the finalization order
        switch_scope = new SwitchScope;
        scope->add(switch_scope);
        value_var = new Variable(switch_scope->get_variable_name(), VOID_TS, value->ts.lvalue());
        switch_scope->add(value_var);
        
        for (auto &kv : kwargs) {
            if (kv.first == "do")
                body.reset(make_code_value(typize(kv.second.get(), switch_scope, &VOID_CODE_TS)));
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

        // Doing mostly what CodeValue does with CodeScope
        switch_scope->finalize_contents(x64);
        Label die, live;

        if (may_be_aborted) {
            Label nonbreak;
            
            x64->op(CMPB, EXCEPTION_ADDRESS, NO_EXCEPTION);
            x64->op(JE, die);

            x64->op(CMPB, EXCEPTION_ADDRESS, BREAK_EXCEPTION);
            x64->op(JNE, nonbreak);
            x64->op(MOVB, EXCEPTION_ADDRESS, NO_EXCEPTION);
            x64->op(JMP, live);
            x64->code_label(nonbreak);
    
            x64->unwind->initiate(switch_scope, x64);
        }
        
        x64->code_label(die);
        x64->die("Switch assertion failed!");
        x64->code_label(live);
        
        return Storage();
    }
    
    virtual Scope *unwind(X64 *x64) {
        may_be_aborted = true;
        return switch_scope;  // Start finalizing the variable
    }
};


class WhenValue: public Value {
public:
    std::unique_ptr<Value> cover, body;
    //Variable *value_var;
    Declaration *dummy;
    Label end;
    
    WhenValue(Value *v, TypeMatch &m)
        :Value(VOID_TS) {
        dummy = NULL;
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        SwitchScope *switch_scope = scope->get_switch_scope();
        
        if (!switch_scope) {
            std::cerr << "Not in :switch!\n";
            return false;
        }
        
        Variable *value_var = variable_cast(switch_scope->contents[0].get());
        
        if (!value_var)
            throw INTERNAL_ERROR;
            
        TypeSpec val_ts = value_var->var_ts.rvalue();
    
        if (args.size() != 1) {
            std::cerr << "Whacky :when!\n";
            return false;
        }
        
        Value *pivot = typize(args[0].get(), scope, &val_ts);

        Expr cover_expr(Expr::IDENTIFIER, Token(), "cover");
        cover_expr.add_arg(new Expr(Expr::IDENTIFIER, Token(), switch_scope->get_variable_name()));
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

        x64->unwind->push(this);
        
        if (body)
            body->compile_and_store(x64, Storage());
            
        x64->unwind->pop(this);
        
        x64->op(MOVB, EXCEPTION_ADDRESS, BREAK_EXCEPTION);
        x64->unwind->initiate(dummy, x64);
        
        x64->code_label(end);
        
        return Storage();
    }
    
    Scope *unwind(X64 *x64) {
        Label noncontinue;
        
        x64->op(CMPB, EXCEPTION_ADDRESS, CONTINUE_EXCEPTION);
        x64->op(JNE, noncontinue);
        x64->op(MOVB, EXCEPTION_ADDRESS, NO_EXCEPTION);
        x64->op(JMP, end);
        x64->code_label(noncontinue);
        
        return NULL;
    }
};


class RaiseValue: public Value {
public:
    Declaration *dummy;
    std::unique_ptr<Value> value;
    int exception_value;
    
    RaiseValue(Value *v, TypeMatch &m)
        :Value(VOID_TS) {
        dummy = NULL;
        exception_value = 0;
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() != 1 || kwargs.size() != 0) {
            std::cerr << "Whacky :raise!\n";
            return false;
        }
        
        FunctionScope *fn_scope = scope->get_function_scope();
        if (!fn_scope) {
            std::cerr << ":raise not in function!\n";
            return false;
        }
        
        Type *et = fn_scope->get_exception_type();
        if (!et) {
            std::cerr << ":raise in a function not raising exceptions!\n";
            return false;
        }
        
        TypeSpec ets = { et };
        Value *v = typize(args[0].get(), scope, &ets);
        
        if (!v)
            return false;
            
        value.reset(v);
        
        dummy = new Declaration;
        scope->add(dummy);
        
        return true;
    }
    
    virtual Regs precompile(Regs preferred) {
        value->precompile(Regs::all());
            
        return Regs::all();  // We're Void
    }
    
    virtual Storage compile(X64 *x64) {
        Storage s = value->compile(x64);
        
        switch (s.where) {
        case CONSTANT:
            x64->op(MOVB, EXCEPTION_ADDRESS, s.value);
            break;
        case REGISTER:
            x64->op(MOVB, EXCEPTION_ADDRESS, s.reg);
            break;
        default:
            throw INTERNAL_ERROR;
        }
        
        x64->unwind->initiate(dummy, x64);
        return Storage();
    }
};


class TryValue: public Value {
public:
    std::unique_ptr<Value> body, handler;
    Variable *exception_var;
    TryScope *try_scope;
    SwitchScope *switch_scope;
    bool handling;
    bool may_be_aborted;
    
    TryValue(Value *v, TypeMatch &m)
        :Value(VOID_TS) {
        exception_var = NULL;
        try_scope = NULL;
        switch_scope = NULL;
        
        handling = false;
        may_be_aborted = false;
        // TODO: return something?
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() != 1) {
            std::cerr << "Whacky :try!\n";
            return false;
        }

        try_scope = new TryScope;
        scope->add(try_scope);
        
        body.reset(typize(args[0].get(), try_scope));

        switch_scope = new SwitchScope;
        scope->add(switch_scope);

        for (auto &kv : kwargs) {
            if (kv.first == "or") {
                Type *et = try_scope->get_exception_type();
                
                if (et) {
                    TypeSpec ets = { lvalue_type, et };
                    exception_var = new Variable(switch_scope->get_variable_name(), VOID_TS, ets);
                    switch_scope->add(exception_var);
                }
                
                handler.reset(make_code_value(typize(kv.second.get(), switch_scope, &VOID_CODE_TS)));
            }
            else {
                std::cerr << "Invalid argument to :try!\n";
                return false;
            }
        }

        return true;
    }

    virtual Regs precompile(Regs preferred) {
        body->precompile(Regs::all());
            
        if (handler)
            handler->precompile(Regs::all());
            
        return Regs::all();  // We're Void TODO: or not
    }
    
    virtual Storage compile(X64 *x64) {
        x64->unwind->push(this);
        handling = false;
        body->compile_and_store(x64, Storage());  // TODO
        x64->unwind->pop(this);

        try_scope->finalize_contents(x64);  // for the sake of consistency only
        
        if (may_be_aborted) {
            // The body may throw an exception
            Label die, live;
            x64->op(CMPB, EXCEPTION_ADDRESS, NO_EXCEPTION);
            x64->op(JLE, live);
            
            // User exception, prepare for handling
            if (exception_var) {
                // TODO: we can't initialize a variable from the global exception value,
                // because its address is a label, and Address can't handle it in a MEMORY Storage.
                // So do this directly for now. The global variable should go away eventually.
                //value->ts.create(, var_storage, x64);
                Storage fn_storage(MEMORY, Address(RBP, 0));
                Storage var_storage = exception_var->get_storage(fn_storage);
                x64->op(MOVB, BL, EXCEPTION_ADDRESS);
                x64->op(MOVB, var_storage.address, BL);
            }

            x64->op(MOVB, EXCEPTION_ADDRESS, NO_EXCEPTION);
        
            x64->unwind->push(this);
            handling = true;
            may_be_aborted = false;
            if (handler)
                handler->compile_and_store(x64, Storage());
            x64->unwind->pop(this);

            switch_scope->finalize_contents(x64);
            
            if (may_be_aborted) {
                // The handling may throw an exception
                Label nonbreak;
            
                x64->op(CMPB, EXCEPTION_ADDRESS, NO_EXCEPTION);
                x64->op(JE, die);

                x64->op(CMPB, EXCEPTION_ADDRESS, BREAK_EXCEPTION);
                x64->op(JNE, nonbreak);
                x64->op(MOVB, EXCEPTION_ADDRESS, NO_EXCEPTION);
                x64->op(JMP, live);
                x64->code_label(nonbreak);
    
                x64->unwind->initiate(switch_scope, x64);
            }
        
            x64->code_label(die);
            x64->die("Try assertion failed!");
            x64->code_label(live);
        }
        
        return Storage();
    }
    
    virtual Scope *unwind(X64 *x64) {
        may_be_aborted = true;
        return handling ? (Scope *)switch_scope : (Scope *)try_scope;
    }
};
