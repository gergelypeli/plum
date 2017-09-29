

class ControlValue: public Value {
public:
    std::string name;
    TypeSpec *context;
    
    struct Kwinfo {
        const char *label;
        Scope *scope;
        TypeSpec *context;
        std::unique_ptr<Value> *target;
    };
    
    ControlValue(std::string n)
        :Value(VOID_TS) {
        name = n;
        context = NULL;
    }

    virtual Value *set_context_ts(TypeSpec *c) {
        context = c;
        return this;
    }

    virtual Value *check_value(Args &args, Scope *scope, TypeSpec *value_context) {
        Value *v = make_code_block_value(value_context);
        Kwargs fake_kwargs;
        
        if (!v->check(args, fake_kwargs, scope))
            return NULL;
            
        if (value_context) {
            TypeMatch match;
        
            if (!typematch(*value_context, v, match)) {
                std::cerr << "Wrong :" << name << " value type!\n";
                return NULL;
            }
        }
        
        return v;
    }

    virtual bool check_kwargs(Kwargs &kwargs, std::vector<Kwinfo> kwinfos) {
        for (auto &kv : kwargs) {
            bool found = false;
            
            for (auto &info : kwinfos) {
                if (kv.first != info.label)
                    continue;
                    
                Value *v = typize(kv.second.get(), info.scope, info.context);
        
                TypeMatch match;
        
                if (!typematch(*info.context, v, match)) {
                    std::cerr << ":" << name << " keyword argument '" << info.label << "' is not " << *info.context << " but " << v->ts << "!\n";
                    return false;
                }
        
                info.target->reset(v);
                
                found = true;
                break;
            }
            
            if (!found) {
                std::cerr << "Invalid :" << name << " keyword argument " << kv.first << "!\n";
                return false;
            }
        }
        
        return true;
    }
};


class YieldableValue: public ControlValue {
public:
    std::string eval_name;
    EvalScope *eval_scope;
    Variable *yield_var;
    
    YieldableValue(std::string n, std::string en)
        :ControlValue(n) {
        eval_name = en;
        eval_scope = NULL;
        yield_var = NULL;
    }

    virtual bool setup_yieldable(Scope *scope) {
        if (context) {
            ts = *context;
            
            if (ts[0] == code_type)
                ts = ts.unprefix(code_type);
            else if (ts[0] == lvalue_type) {
                std::cerr << name << " in lvalue context!\n";
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
            yield_var = new Variable(eval_scope->get_variable_name(), VOID_TS, ts.lvalue());
            scope->add(yield_var);
            eval_scope->set_yield_var(yield_var);
        }
        
        return true;
    }
    
    virtual Storage get_yield_storage() {
        if (yield_var) {
            Storage fn_storage(MEMORY, Address(RBP, 0));
            return yield_var->get_storage(fn_storage);
        }
        else
            return Storage();
    }
};




class RepeatValue: public ControlValue {
public:
    std::unique_ptr<Value> setup, condition, step, body;
    Label start, end;
    
    RepeatValue(Value *pivot, TypeMatch &match)
        :ControlValue("repeat") {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        setup.reset(check_value(args, scope, NULL));
        if (!setup)
            return false;
    
        std::vector<Kwinfo> infos = {
            { "do", scope, &VOID_CODE_TS, &body },
            { "on", scope, &BOOLEAN_CODE_TS, &condition },
            { "by", scope, &VOID_CODE_TS, &step }
        };
        
        if (!check_kwargs(kwargs, infos))
            return false;
            
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



class ForEachValue: public ControlValue {
public:
    std::unique_ptr<Value> iterator, each, body, next;
    Variable *iterator_var;
    TryScope *next_try_scope;
    TypeSpec each_ts;
    Label start, end;
    
    ForEachValue(Value *pivot, TypeMatch &match)
        :ControlValue("for") {
        iterator_var = NULL;
        next_try_scope = NULL;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        iterator.reset(check_value(args, scope, NULL));
        if (!iterator)
            return false;
            
        if (iterator->ts == VOID_TS) {
            std::cerr << "Missing iterator in :for!\n";
            return false;
        }
            
        // TODO: this should be a "local variable", to be destroyed once we're done
        // instead of letting the enclosing scope destroy it.
        TypeSpec its = iterator->ts.lvalue();
        iterator_var = new Variable("<iterator>", VOID_TS, its);
        scope->add(iterator_var);

        next_try_scope = new TryScope;
        scope->add(next_try_scope);

        Expr it_expr(Expr::IDENTIFIER, Token(), "<iterator>");
        Value *it = lookup("<iterator>", NULL, &it_expr, next_try_scope);
        
        TypeMatch match;
        
        if (!typematch(ANY_ITERATOR_TS, it, match)) {
            std::cerr << "Not an Interator in :for control, but: " << it->ts << "!\n";
            return false;
        }

        each_ts = match[1].lvalue();
        
        Expr next_expr(Expr::IDENTIFIER, Token(), "next");
        Value *next = lookup("next", it, &next_expr, next_try_scope);
        
        if (!next) {
            std::cerr << "Iterator didn't implement the next method!\n";
            throw INTERNAL_ERROR;
        }
        
        this->next.reset(next);

        std::vector<Kwinfo> infos = {
            { "each", scope, &each_ts, &each },
            { "do", scope, &VOID_CODE_TS, &body }
        };
        
        if (!check_kwargs(kwargs, infos))
            return false;

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
        :YieldableValue("switch", "yield") {
        switch_scope = NULL;
        switch_var = NULL;
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        value.reset(check_value(args, scope, NULL));
        if (!value)
            return false;
            
        if (value->ts == VOID_TS) {
            std::cerr << "Whacky :switch!\n";
            return false;
        }
        
        // Insert variable before the body to keep the finalization order
        if (!setup_yieldable(scope))
            return false;
            
        switch_scope = new SwitchScope;
        eval_scope->add(switch_scope);
        
        switch_var = new Variable(switch_scope->get_variable_name(), VOID_TS, value->ts.lvalue());
        switch_scope->add(switch_var);
        
        std::vector<Kwinfo> infos = {
            { "do", switch_scope, &VOID_CODE_TS, &body }
        };
        
        if (!check_kwargs(kwargs, infos))
            return false;

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

        return get_yield_storage();
    }
    
    virtual Scope *unwind(X64 *x64) {
        return switch_scope;  // Start finalizing the variable
    }
};


class WhenValue: public ControlValue {
public:
    std::unique_ptr<Value> cover, body;
    Label end;
    
    WhenValue(Value *v, TypeMatch &m)
        :ControlValue("when") {
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

        // Process the value
        Value *v = check_value(args, scope, &switch_ts);
        if (!v)
            return false;
        
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
    
        std::vector<Kwinfo> infos = {
            { "then", scope, &VOID_CODE_TS, &body },
        };
        
        if (!check_kwargs(kwargs, infos))
            return false;

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


class RaiseValue: public ControlValue {
public:
    Declaration *dummy;
    std::unique_ptr<Value> value;
    int exception_value;
    
    RaiseValue(Value *v, TypeMatch &m)
        :ControlValue("raise") {
        dummy = NULL;
        exception_value = 0;
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
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
        
        value.reset(check_value(args, scope, &ets));
        if (!value)
            return false;
        
        dummy = new Declaration;
        scope->add(dummy);

        std::vector<Kwinfo> infos = {
        };
        
        if (!check_kwargs(kwargs, infos))
            return false;
        
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
        :YieldableValue("try", "yield") {
        try_scope = NULL;
        switch_var = NULL;
        switch_scope = NULL;
        
        handling = false;
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        try_scope = new TryScope;
        scope->add(try_scope);
        
        body.reset(check_value(args, try_scope, context));
        if (!body)
            return false;

        if (!setup_yieldable(scope))
            return false;

        switch_scope = new SwitchScope;
        eval_scope->add(switch_scope);

        Type *et = try_scope->get_exception_type();
        
        if (et) {
            TypeSpec ets = { lvalue_type, et };
            switch_var = new Variable(switch_scope->get_variable_name(), VOID_TS, ets);
            switch_scope->add(switch_var);
        }

        std::vector<Kwinfo> infos = {
            { "or", switch_scope, &VOID_CODE_TS, &handler }
        };
        
        if (!check_kwargs(kwargs, infos))
            return false;

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
        
        Storage var_storage = get_yield_storage();

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
        
        return get_yield_storage();
    }
    
    virtual Scope *unwind(X64 *x64) {
        return handling ? (Scope *)switch_scope : (Scope *)try_scope;
    }
};


class EvalValue: public YieldableValue {
public:
    std::unique_ptr<Value> body;
    
    EvalValue(std::string en)
        :YieldableValue(en, en) {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!setup_yieldable(scope))
            return false;
        
        body.reset(check_value(args, eval_scope, context));
        if (!body)
            return false;
        
        std::vector<Kwinfo> infos = {
        };
        
        if (!check_kwargs(kwargs, infos))
            return false;
        
        return true;
    }
    
    virtual Regs precompile(Regs preferred) {
        return body->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        x64->unwind->push(this);
        Storage s = body->compile(x64);
        x64->unwind->pop(this);
        
        Storage yield_storage = get_yield_storage();
        
        if (yield_storage.where != NOWHERE)
            body->ts.create(s, yield_storage, x64);
        
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
        
        return yield_storage;
    }
    
    virtual Scope *unwind(X64 *x64) {
        return eval_scope;
    }
};


class YieldValue: public ControlValue {
public:
    Declaration *dummy;
    std::unique_ptr<Value> value;
    EvalScope *eval_scope;

    YieldValue(EvalScope *es)
        :ControlValue(es->get_label()) {
        dummy = NULL;
        eval_scope = es;
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        TypeSpec arg_ts = eval_scope->get_ts();
        
        if (arg_ts == VOID_TS) {
            if (args.size() != 0) {
                std::cerr << ":" << name << " with arguments!\n";
                return false;
            }
        }
        else {
            if (args.size() == 0) {
                std::cerr << ":" << name << " without arguments!\n";
                return false;
            }

            value.reset(check_value(args, scope, &arg_ts));
            if (!value)
                return false;
        }

        dummy = new Declaration;
        scope->add(dummy);

        std::vector<Kwinfo> infos = {
        };
        
        if (!check_kwargs(kwargs, infos))
            return false;
        
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
            Storage var_storage = eval_scope->get_yield_var()->get_storage(fn_storage);
            value->ts.create(s, var_storage, x64);
        }
        
        x64->op(MOVB, EXCEPTION_ADDRESS, eval_scope->get_exception_value());
        x64->unwind->initiate(dummy, x64);
        return Storage();
    }
};
