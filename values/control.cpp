
Kwargs fake_kwargs;


class YieldableValue: public Value {
public:
    std::string eval_name;
    EvalScope *eval_scope;
    Variable *value_var;
    TypeSpec *context;
    
    YieldableValue(std::string en)
        :Value(VOID_TS) {  // may be overridden
        eval_name = en;
        eval_scope = NULL;
        value_var = NULL;
        context = NULL;
    }

    virtual Value *set_context_ts(TypeSpec *c) {
        context = c;
        return this;
    }

    virtual bool check_eval(Scope *scope) {
        if (context) {
            ts = *context;
            
            if (ts[0] == code_type)
                ts = ts.unprefix(code_type);
            else if (ts[0] == lvalue_type) {
                std::cerr << "Yieldable in lvalue context!\n";
                return false;
            }
        }
            
        eval_scope = new EvalScope(ts, eval_name);
        scope->add(eval_scope);
        
        if (eval_name.size())
            eval_scope->add(new Yield(":" + eval_name, eval_scope));
        
        if (ts != VOID_TS) {
            // Add the variable after the EvalScope, so it can survive the finalization
            // of the scope, and can be left uninitialized until the successful completion.
            value_var = new Variable(eval_scope->get_variable_name(), VOID_TS, ts.lvalue());
            scope->add(value_var);
            eval_scope->set_value_var(value_var);
        }
        
        return true;
    }
    
    virtual Storage get_yielded_storage() {
        if (value_var) {
            Storage fn_storage(MEMORY, Address(RBP, 0));
            return value_var->get_storage(fn_storage);
        }
        else
            return Storage();
    }
};




class RepeatValue: public Value {
public:
    std::unique_ptr<Value> setup, condition, step, body;
    Label start, end;
    
    RepeatValue(Value *pivot, TypeMatch &match)
        :Value(VOID_TS) {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        Value *v = make_code_block_value(NULL);

        if (!v->check(args, fake_kwargs, scope))
            return false;
        
        setup.reset(v);
        
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
        
        body->compile_and_store(x64, Storage());
        
        if (step)
            step->compile_and_store(x64, Storage());
        
        x64->op(JMP, start);
        
        x64->code_label(end);
        return Storage();
    }
};



class ForEachValue: public Value {
public:
    std::unique_ptr<Value> iterator, each, body, next;
    Variable *iterator_var;
    TryScope *next_try_scope;
    TypeSpec each_ts;
    Label start, end;
    
    ForEachValue(Value *pivot, TypeMatch &match)
        :Value(VOID_TS) {
        iterator_var = NULL;
        next_try_scope = NULL;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        Value *v = make_code_block_value(NULL);
        
        if (!v->check(args, fake_kwargs, scope))
            return false;

        if (v->ts == VOID_TS) {
            std::cerr << "Missing iterator in :for!\n";
            return false;
        }
            
        iterator.reset(v);
        
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
        Storage ns = next->compile(x64);
        x64->unwind->pop(this);
        
        next->ts.store(ns, es, x64);
        // Finalize after storing, so the return value won't be lost
        next_try_scope->finalize_contents(x64);

        body->compile_and_store(x64, Storage());
        
        x64->op(JMP, start);
        
        x64->code_label(end);
        // We don't need to clean up local variables yet
        
        return Storage();
    }
    
    virtual Scope *unwind(X64 *x64) {
        // May be called only while executing next
        x64->op(MOVB, EXCEPTION_ADDRESS, NO_EXCEPTION);
        x64->op(JMP, end);
        return next_try_scope;
    }
};




class SwitchValue: public YieldableValue {
public:
    std::unique_ptr<Value> value, body;
    SwitchScope *switch_scope;
    Variable *switch_var;
    
    SwitchValue(Value *v, TypeMatch &m)
        :YieldableValue("yield") {
        switch_scope = NULL;
        switch_var = NULL;
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        Value *v = make_code_block_value(NULL);
        
        if (!v->check(args, fake_kwargs, scope))
            return false;
            
        if (v->ts == VOID_TS) {
            std::cerr << "Whacky :switch!\n";
            return false;
        }
        
        value.reset(v);
    
        // Insert variable before the body to keep the finalization order
        if (!check_eval(scope))
            return false;
            
        switch_scope = new SwitchScope;
        eval_scope->add(switch_scope);
        
        switch_var = new Variable(switch_scope->get_variable_name(), VOID_TS, value->ts.lvalue());
        switch_scope->add(switch_var);
            
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
        Regs clob = value->precompile(Regs::all());
            
        if (body)
            clob = clob | body->precompile(Regs::all());
            
        return clob;
    }
    
    virtual Storage compile(X64 *x64) {
        Storage vs = value->compile(x64);
        
        Storage fn_storage(MEMORY, Address(RBP, 0));
        Storage switch_storage = switch_var->get_storage(fn_storage);
        value->ts.create(vs, switch_storage, x64);
        
        x64->unwind->push(this);
        
        if (body)
            body->compile_and_store(x64, Storage());
        
        x64->unwind->pop(this);

        x64->die("Switch assertion failed!");

        switch_scope->finalize_contents(x64);
        eval_scope->finalize_contents(x64);
        
        Label live;
        
        x64->op(CMPB, EXCEPTION_ADDRESS, eval_scope->get_exception_value());
        x64->op(JE, live);

        x64->unwind->initiate(eval_scope, x64);

        x64->code_label(live);
        x64->op(MOVB, EXCEPTION_ADDRESS, NO_EXCEPTION);

        return get_yielded_storage();
    }
    
    virtual Scope *unwind(X64 *x64) {
        return switch_scope;  // Start finalizing the variable
    }
};


class WhenValue: public Value {
public:
    std::unique_ptr<Value> cover, body;
    Label end;
    
    WhenValue(Value *v, TypeMatch &m)
        :Value(VOID_TS) {
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        SwitchScope *switch_scope = scope->get_switch_scope();
        
        if (!switch_scope) {
            std::cerr << "Not in :switch!\n";
            return false;
        }
        
        Variable *switch_var = variable_cast(switch_scope->contents[0].get());
        
        if (!switch_var)
            throw INTERNAL_ERROR;
            
        TypeSpec switch_ts = switch_var->var_ts.rvalue();
    
        Value *v = make_code_block_value(&switch_ts);
        
        if (!v->check(args, fake_kwargs, scope))
            return false;
    
        if (v->ts == VOID_TS) {
            std::cerr << "Whacky :when!\n";
            return false;
        }
        
        Expr cover_expr(Expr::IDENTIFIER, Token(), "cover");
        cover_expr.add_arg(new Expr(Expr::IDENTIFIER, Token(), switch_scope->get_variable_name()));
        Value *cover = lookup("cover", v, &cover_expr, scope);
        
        if (!cover) {
            std::cerr << "Cannot :switch with uncoverable " << v->ts << "!\n";
            return false;
        }

        if (cover->ts != BOOLEAN_TS) {
            std::cerr << "Cannot :switch with nonboolean cover!\n";
            return false;
        }
        
        this->cover.reset(cover);
    
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

        if (body)
            body->compile_and_store(x64, Storage());
            
        x64->code_label(end);
        
        return Storage();
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
        if (kwargs.size() != 0) {
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
        
        Value *v = make_code_block_value(&ets);
        
        if (!v->check(args, fake_kwargs, scope))
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


class TryValue: public YieldableValue {
public:
    std::unique_ptr<Value> body, handler;
    TryScope *try_scope;
    Variable *switch_var;
    SwitchScope *switch_scope;
    bool handling;
    
    TryValue(Value *v, TypeMatch &m)
        :YieldableValue("yield") {
        try_scope = NULL;
        switch_var = NULL;
        switch_scope = NULL;
        
        handling = false;
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        try_scope = new TryScope;
        scope->add(try_scope);
        
        Value *v = make_code_block_value(context);
        
        if (!v->check(args, fake_kwargs, try_scope))
            return false;
        
        body.reset(v);

        if (!check_eval(scope))
            return false;

        switch_scope = new SwitchScope;
        eval_scope->add(switch_scope);

        for (auto &kv : kwargs) {
            if (kv.first == "or") {
                Type *et = try_scope->get_exception_type();
                
                if (et) {
                    TypeSpec ets = { lvalue_type, et };
                    switch_var = new Variable(switch_scope->get_variable_name(), VOID_TS, ets);
                    switch_scope->add(switch_var);
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
        Regs clob = body->precompile(preferred);
            
        if (handler)
            clob = clob | handler->precompile(preferred);
            
        return clob;
    }
    
    virtual Storage compile(X64 *x64) {
        x64->unwind->push(this);
        handling = false;
        Storage s = body->compile(x64);
        x64->unwind->pop(this);
        
        Storage var_storage = get_yielded_storage();

        if (var_storage.where != NOWHERE)
            body->ts.create(s, var_storage, x64);

        try_scope->finalize_contents(x64);
        
        // The body may throw an exception
        Label die, live;
        x64->op(CMPB, EXCEPTION_ADDRESS, NO_EXCEPTION);
        x64->op(JLE, live);
        
        // User exception, prepare for handling
        if (switch_var) {
            Storage fn_storage(MEMORY, Address(RBP, 0));
            Storage switch_storage = switch_var->get_storage(fn_storage);
            x64->op(MOVB, BL, EXCEPTION_ADDRESS);
            x64->op(MOVB, switch_storage.address, BL);
        }

        x64->op(MOVB, EXCEPTION_ADDRESS, NO_EXCEPTION);
    
        x64->unwind->push(this);
        handling = true;
        if (handler)
            handler->compile_and_store(x64, Storage());
        x64->unwind->pop(this);

        x64->die("Try assertion failed!");

        switch_scope->finalize_contents(x64);
        eval_scope->finalize_contents(x64);
        
        Label caught;
    
        x64->op(CMPB, EXCEPTION_ADDRESS, eval_scope->get_exception_value());
        x64->op(JE, caught);

        x64->unwind->initiate(eval_scope, x64);

        x64->code_label(caught);
        x64->op(MOVB, EXCEPTION_ADDRESS, NO_EXCEPTION);
    
        x64->code_label(live);
        
        return get_yielded_storage();
    }
    
    virtual Scope *unwind(X64 *x64) {
        return handling ? (Scope *)switch_scope : (Scope *)try_scope;
    }
};


class EvalValue: public YieldableValue {
public:
    std::unique_ptr<Value> body;
    
    //EvalValue(Value *pivot, TypeMatch &match)
    //    :YieldableValue("") {
    //}

    EvalValue(std::string en)
        :YieldableValue(en) {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (kwargs.size() != 0) {
            std::cerr << "Whacky :eval!\n";
            return false;
        }

        if (!check_eval(scope))
            return false;
        
        Value *v = make_code_block_value(context);
        
        if (!v->check(args, fake_kwargs, eval_scope))
            return false;
        
        if (ts != VOID_TS) {
            TypeMatch match;
            
            if (!typematch(ts, v, match)) {
                std::cerr << "Wrong :eval result type!\n";
                return false;
            }
        }
        
        body.reset(v);
        
        return true;
    }
    
    virtual Regs precompile(Regs preferred) {
        return body->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        x64->unwind->push(this);
        Storage s = body->compile(x64);
        x64->unwind->pop(this);
        
        Storage yielded_storage = get_yielded_storage();
        
        if (yielded_storage.where != NOWHERE)
            body->ts.create(s, yielded_storage, x64);
        
        eval_scope->finalize_contents(x64);

        Label ok, caught;
        
        x64->op(CMPB, EXCEPTION_ADDRESS, NO_EXCEPTION);
        x64->op(JE, ok);
        
        x64->op(CMPB, EXCEPTION_ADDRESS, eval_scope->get_exception_value());
        x64->op(JE, caught);
        
        x64->unwind->initiate(eval_scope, x64);
        
        x64->code_label(caught);
        x64->op(MOVB, EXCEPTION_ADDRESS, NO_EXCEPTION);
        
        x64->code_label(ok);
        
        return yielded_storage;
    }
    
    virtual Scope *unwind(X64 *x64) {
        return eval_scope;
    }
};


class YieldValue: public Value {
public:
    Declaration *dummy;
    std::unique_ptr<Value> value;
    EvalScope *eval_scope;

    YieldValue(EvalScope *es)
        :Value(VOID_TS) {
        dummy = NULL;
        eval_scope = es;
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (kwargs.size() != 0) {
            std::cerr << "Whacky :yield!\n";
            return false;
        }
        
        TypeSpec arg_ts = eval_scope->get_ts();
        
        if (arg_ts == VOID_TS) {
            if (args.size() != 0) {
                std::cerr << ":yield with arguments!\n";
                return false;
            }
        }
        else {
            if (args.size() != 1) {
                std::cerr << ":yield without arguments!\n";
                return false;
            }

            Value *v = make_code_block_value(&arg_ts);
            
            if (!v->check(args, fake_kwargs, scope))
                return false;
                
            TypeMatch match;
            
            if (!typematch(arg_ts, v, match)) {
                std::cerr << "Wrong :yield result type!\n";
                return false;
            }
            
            value.reset(v);
        }

        dummy = new Declaration;
        scope->add(dummy);
        
        return true;
    }
    
    virtual Regs precompile(Regs preferred) {
        if (value)
            value->precompile(preferred);
            
        return Regs::all();  // We're Void
    }
    
    virtual Storage compile(X64 *x64) {
        if (value) {
            Storage s = value->compile(x64);

            Storage fn_storage(MEMORY, Address(RBP, 0));
            Storage var_storage = eval_scope->get_value_var()->get_storage(fn_storage);
            value->ts.create(s, var_storage, x64);
        }
        
        x64->op(MOVB, EXCEPTION_ADDRESS, eval_scope->get_exception_value());
        x64->unwind->initiate(dummy, x64);
        return Storage();
    }
};
