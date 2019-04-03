

class ControlValue: public Value {
public:
    std::string name;
    TypeSpec context_ts;

    ControlValue(std::string n)
        :Value(VOID_TS) {
        name = n;
        context_ts = VOID_TS;  // pivot controls will be Void
    }

    virtual void set_context_ts(TypeSpec *c) {
        if (!c)
            return;
            
        context_ts = *c;
        Type *a = context_ts[0];
        
        if (a == code_type)
            context_ts = context_ts.unprefix(code_type);
        else if (a == ovalue_type)
            context_ts = context_ts.unprefix(ovalue_type);
        else if (a == lvalue_type) {
            std::cerr << "Control :" << name << " in Lvalue context!\n";
            throw TYPE_ERROR;
        }
    }

    virtual bool check_args(Args &args, ArgInfo arg_info) {
        if (args.size() == 0) {
            if (*arg_info.context != NO_TS && *arg_info.context != VOID_TS) {
                std::cerr << "Missing :" << name << " positional argument!\n";
                return false;
            }
            
            return true;
        }

        if (args.size() > 1) {
            // Controls don't want multiple positional arguments, turn them into
            // a single code block
            Expr *arg = new Expr(Expr::TUPLE, Token());
            arg->args = std::move(args);
            args.clear();
            args.push_back(std::unique_ptr<Expr>(arg));
        }
            
        Kwargs fake_kwargs;
        return check_arguments(args, fake_kwargs, ArgInfos { arg_info });
    }

    virtual bool check_kwargs(Kwargs &kwargs, ArgInfos arg_infos) {
        Args fake_args;
        
        return check_arguments(fake_args, kwargs, arg_infos);
    }

    virtual Storage get_context_storage() {
        switch (context_ts.where(AS_VALUE)) {
        case NOWHERE:
            return Storage();
        case REGISTER:
            return Storage(REGISTER, RAX);
        case SSEREGISTER:
            return Storage(SSEREGISTER, XMM0);
        case STACK:
            return Storage(STACK);
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class IfValue: public ControlValue {
public:
    std::unique_ptr<Value> condition;
    std::unique_ptr<Value> then_branch;
    std::unique_ptr<Value> else_branch;
    Register reg;
    
    IfValue(OperationType o, Value *pivot, TypeMatch &match)
        :ControlValue("if") {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        ts = context_ts;
            
        TypeSpec arg_ts = ts.prefix(code_type);
            
        ArgInfos infos = {
            { "condition", &BOOLEAN_TS, scope, &condition },
            { "then", &arg_ts, scope, &then_branch },
            { "else", &arg_ts, scope, &else_branch }
        };
        
        if (!check_arguments(args, kwargs, infos))
            return false;

        return true;
    }
    
    virtual Regs precompile(Regs preferred) {
        Regs clobbered = Regs();
        
        clobbered = clobbered | condition->precompile();
        
        if (then_branch)
            clobbered = clobbered | then_branch->precompile(preferred);
                       
        if (else_branch)
            clobbered = clobbered | else_branch->precompile(preferred);
        
        // This won't be bothered by either branches
        reg = preferred.get_any();
        
        return clobbered | reg;
    }
    
    virtual Storage compile(X64 *x64) {
        Label then_end;
        Label else_end;
        
        Storage cs = condition->compile(x64);
        
        switch (cs.where) {
        case CONSTANT:
            if (cs.value) {
                if (then_branch) {
                    ;
                }
                else if (else_branch)
                    x64->op(JMP, else_end);
            }
            else {
                if (then_branch)
                    x64->op(JMP, then_end);
                else if (else_branch) {
                    ;
                }
            }
                
            break;
        case FLAGS:
            if (then_branch) {
                BranchOp opcode = branch(negated(cs.cc));
                x64->op(opcode, then_end);
            }
            else if (else_branch) {
                BranchOp opcode = branch(cs.cc);
                x64->op(opcode, else_end);
            }
            break;
        case REGISTER:
            x64->op(CMPB, cs.reg, 0);
            
            if (then_branch)
                x64->op(JE, then_end);
            else if (else_branch)
                x64->op(JNE, else_end);
                
            break;
        case STACK:
            x64->op(POPQ, reg);
            x64->op(CMPB, RAX, 0);
            
            if (then_branch)
                x64->op(JE, then_end);
            else if (else_branch)
                x64->op(JNE, else_end);
                
            break;
        case MEMORY:
            x64->op(CMPB, cs.address, 0);

            if (then_branch)
                x64->op(JE, then_end);
            else if (else_branch)
                x64->op(JNE, else_end);
            
            break;
        default:
            throw INTERNAL_ERROR;
        }

        // FIXME: we need a function to get the recommended storage for this type!
        Storage s = get_context_storage();
        //if (ts != VOID_TS && ts != WHATEVER_TS)
        //    s = Storage(REGISTER, reg);

        if (then_branch) {
            then_branch->compile_and_store(x64, s);
            
            if (else_branch)
                x64->op(JMP, else_end);

            x64->code_label(then_end);
        }
        
        if (else_branch) {
            else_branch->compile_and_store(x64, s);
            x64->code_label(else_end);
        }
    
        return s;
    }
};




class YieldableValue: public ControlValue {
public:
    std::string yield_name;
    EvalScope *eval_scope;
    Variable *yield_var;
    bool actually_yielded;
    
    YieldableValue(std::string n, std::string yn)
        :ControlValue(n) {
        yield_name = yn;
        eval_scope = NULL;
        yield_var = NULL;
        actually_yielded = false;
    }

    virtual std::string get_yield_label() {
        return yield_name;
    }

    virtual bool setup_yieldable(Scope *scope) {
        ts = context_ts;

        eval_scope = new EvalScope(this);
        scope->add(eval_scope);
        eval_scope->enter();
        
        if (yield_name.size()) {
            ExportScope *es = new ExportScope(colon_scope);
            eval_scope->add(es);
            es->enter();
            es->add(new Yield(yield_name, this));
            es->leave();
        }
        
        if (ts.where(AS_VALUE) == STACK) {
            // Add the variable after the EvalScope, so it can survive the finalization
            // of the scope, and can be left uninitialized until the successful completion.
            yield_var = new Variable("<" + yield_name + ">", ts);
            scope->add(yield_var);
        }
        
        return true;
    }

    virtual int get_yield_exception_value() {
        EvalScope *es = eval_scope->outer_scope->get_eval_scope();
        
        return (es ? es->get_yieldable_value()->get_yield_exception_value() : RETURN_EXCEPTION) - 1;
    }

    virtual void actually_yield() {
        actually_yielded = true;
    }

    virtual Storage get_yield_storage() {
        switch (ts.where(AS_VALUE)) {
        case NOWHERE:
            return Storage();
        case REGISTER:
            return Storage(REGISTER, RAX);
        case SSEREGISTER:
            return Storage(SSEREGISTER, XMM0);
        case STACK:
            return yield_var->get_local_storage();
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual void store_yield(Storage s, X64 *x64) {
        Storage t = get_yield_storage();
        //std::cerr << "Storing " << eval_name << " yield into " << t << "\n";
        
        switch (t.where) {
        case NOWHERE:
            break;
        case REGISTER:
        case SSEREGISTER:
            ts.store(s, t, x64);
            break;
        case MEMORY:
            ts.create(s, t, x64);
            break;
        default:
            throw INTERNAL_ERROR;
        };
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
        if (!check_args(args, { "init", &VOID_TS, scope, &setup }))
            return false;

        ArgInfos infos = {
            { "do", &VOID_CODE_TS, scope, &body },
            { "on", &BOOLEAN_CODE_TS, scope, &condition },
            { "by", &VOID_CODE_TS, scope, &step }
        };
        
        return check_kwargs(kwargs, infos);
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
                x64->op(branch(negated(cs.cc)), end);
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
    
    ForEachValue(Value *pivot, TypeMatch &match)
        :ControlValue("for") {
        iterator_var = NULL;
        next_try_scope = NULL;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        std::unique_ptr<Value> ib;
    
        if (!check_args(args, { "", &ANY_ITERABLE_TS, scope, &ib}))
            return false;
    
        //std::cerr << "XXX :foreach iterable is " << ib->ts << "\n";
        
        Value *ib2 = lookup_fake("iter", ib.release(), scope, token, NULL);
        
        if (!ib2) {
            std::cerr << "Iterable didn't implement the iter method!\n";
            throw INTERNAL_ERROR;
        }

        //std::cerr << "XXX iterator is " << ib2->ts << "\n";

        iterator.reset(ib2);
            
        // TODO: this should be a "local variable", to be destroyed once we're done
        // instead of letting the enclosing scope destroy it.
        TypeSpec its = iterator->ts.lvalue();
        iterator_var = new Variable("<iterator>", its);
        scope->add(iterator_var);

        next_try_scope = new TryScope;
        scope->add(next_try_scope);
        next_try_scope->enter();

        TypeMatch imatch;
        Value *it = iterator_var->matched(NULL, scope, imatch);
        
        TypeMatch match;
        if (!typematch(ANY_ITERATOR_TS, it, match)) {
            std::cerr << "Iterable iter didn't return an Iterator, but: " << it->ts << "!\n";
            return false;
        }

        TypeSpec elem_ts = match[1];
        each_ts = elem_ts.prefix(dvalue_type);  //lvalue();
        
        Value *next = lookup_fake("next", it, next_try_scope, token, NULL);
        
        if (!next) {
            std::cerr << "Iterator didn't implement the next method!\n";
            throw INTERNAL_ERROR;
        }
        
        if (next->ts != elem_ts) {
            std::cerr << "Misimplemented " << elem_ts << " iterator next returns " << next->ts << "!\n";
            throw INTERNAL_ERROR;
        }
        
        this->next.reset(next);

        next_try_scope->be_taken();
        next_try_scope->leave();

        ArgInfos infos = {
            { "each", &each_ts, scope, &each },
            { "do", &VOID_CODE_TS, scope, &body }
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
        Storage var_storage = iterator_var->get_local_storage();
        iterator->ts.create(is, var_storage, x64);
        
        Storage es = each->compile(x64);
        if (es.where != MEMORY || es.address.base != RBP)
            throw INTERNAL_ERROR;  // FIXME: lame temporary restriction only
        
        Label start, end;
        x64->code_label(start);

        x64->unwind->push(this);
        Storage ns = next->compile(x64);
        x64->unwind->pop(this);
        
        next->ts.create(ns, es, x64);  // create the each variable

        // Finalize after storing, so the return value won't be lost
        // On exception we jump here, so the each variable won't be created
        x64->op(MOVQ, RDX, NO_EXCEPTION);
        next_try_scope->finalize_contents(x64);
        
        x64->op(CMPQ, RDX, NO_EXCEPTION);
        x64->op(JNE, end);  // dropped

        body->compile_and_store(x64, Storage());
        
        next->ts.destroy(es, x64);  // destroy the each variable
        
        x64->op(JMP, start);
        
        x64->code_label(end);
        // We don't need to clean up local variables yet
        
        return Storage();
    }
    
    virtual Scope *unwind(X64 *x64) {
        // May be called only while executing next
        return next_try_scope;
    }
};




class SwitchValue: public ControlValue {
public:
    std::unique_ptr<Value> value, body;
    SwitchScope *switch_scope;
    Variable *switch_var;
    
    SwitchValue(Value *v, TypeMatch &m)
        :ControlValue("switch") {
        switch_scope = NULL;
        switch_var = NULL;
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!check_args(args, { "value", &ANY_TS, scope, &value }))
            return false;
            
        if (value->ts == VOID_TS) {  // TODO: is this unnecessary fot ANY_TS?
            std::cerr << "Whacky :switch!\n";
            return false;
        }
        
        // Insert variable before the body to keep the finalization order
        //if (!setup_yieldable(scope))
        //    return false;
            
        switch_scope = new SwitchScope;
        scope->add(switch_scope);
        switch_scope->enter();
        
        switch_var = new Variable(switch_scope->get_variable_name(), value->ts);
        switch_scope->add(switch_var);
        
        ArgInfos infos = {
            { "do", &VOID_CODE_TS, switch_scope, &body }
        };
        
        if (!check_kwargs(kwargs, infos))
            return false;

        switch_scope->be_taken();
        switch_scope->leave();
        //eval_scope->be_taken();
        //eval_scope->leave();

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
        
        Storage switch_storage = switch_var->get_local_storage();
        value->ts.create(vs, switch_storage, x64);
        
        x64->unwind->push(this);
        
        if (body)
            body->compile_and_store(x64, Storage());
        
        x64->unwind->pop(this);

        // Normal control flow dies here
        //x64->runtime->die("Switch assertion failed!");

        switch_scope->finalize_contents(x64);
        //eval_scope->finalize_contents(x64);
        
        Label live;

        x64->op(CMPQ, RDX, NO_EXCEPTION);
        x64->op(JE, live);
        
        //if (actually_yielded) {
        //    x64->op(CMPQ, RDX, get_yield_exception_value());
        //    x64->op(JE, live);  // dropped
        //}

        // Otherwise must have raised something
        x64->unwind->initiate(switch_scope, x64);

        x64->code_label(live);

        return Storage();  // get_yield_storage();
    }
    
    virtual Scope *unwind(X64 *x64) {
        return switch_scope;  // Start finalizing the variable
    }
};




class IsValue: public ControlValue {
public:
    std::unique_ptr<Value> match, then_branch, else_branch;
    //Variable *matched_var;
    CodeScope *then_scope;
    TryScope *match_try_scope;
    Label end;
    bool matching;
    const int CAUGHT_UNMATCHED_EXCEPTION = -255;  // hopefully outside of yield range
    
    IsValue(Value *v, TypeMatch &m)
        :ControlValue("is") {
        matching = false;
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (context_ts != VOID_TS && context_ts != WHATEVER_TS) {
            std::cerr << "Sorry, :is currently can't yield values: " << context_ts << "!\n";
            return false;
        }

        then_scope = new CodeScope;
        scope->add(then_scope);
        then_scope->enter();

        match_try_scope = new TransparentTryScope;
        then_scope->add(match_try_scope);
        match_try_scope->enter();

        TypeSpec match_ts = VOID_TS;
        SwitchScope *switch_scope = scope->get_switch_scope();
    
        if (switch_scope) {
            Variable *switch_var = switch_scope->get_variable();
            match_ts = switch_var->alloc_ts.rvalue().prefix(equalitymatcher_type);
        }
        
        if (!check_args(args, { "match", &match_ts, match_try_scope, &match }))
            return false;

        Type *et = match_try_scope->get_exception_type();
        
        if (!et) {
            // Treat match as a value, and do an implicit equality matching
            // FIXME: this is stupid
            
            if (!switch_scope) {
                std::cerr << "Implicit equality matching can only be used in :is inside :switch!\n";
                return false;
            }
            
            match.reset(make<ImplicitEqualityMatcherValue>(match.release()));
            
            Args fake_args;
            Kwargs fake_kwargs;
            
            if (!match->check(fake_args, fake_kwargs, match_try_scope))
                return false;
                
            et = match_try_scope->get_exception_type();
        }
        
        if (et != match_unmatched_exception_type) {
            std::cerr << "This :is match raises " << et->name << " exception!\n";
            return false;
        }

        // Now that we handled the implicit equality matching case, voidize the match value
        if (match->ts != VOID_TS)
            match.reset(make<VoidConversionValue>(match.release()));

        // This is a TransparentTryScope with no inner declarations, needs no finalization
        match_try_scope->leave();

        ArgInfos infos = {
            { "then", &VOID_CODE_TS, then_scope, &then_branch },
            { "else", &VOID_CODE_TS, scope, &else_branch },
        };
        
        if (!check_kwargs(kwargs, infos))
            return false;

        then_scope->be_taken();
        then_scope->leave();

        return true;
    }

    virtual Regs precompile(Regs preferred) {
        match->precompile(Regs::all());
            
        if (then_branch)
            then_branch->precompile(Regs::all());

        if (else_branch)
            else_branch->precompile(Regs::all());
            
        return Regs::all();  // We're Void
    }
    
    virtual Storage compile(X64 *x64) {
        Label else_label, end;
        //Address matched_addr = matched_var->get_local_storage().address;

        x64->unwind->push(this);
        
        //x64->op(MOVQ, matched_addr, 0);
        matching = true;
        match->compile_and_store(x64, Storage());
        matching = false;

        //x64->op(MOVQ, matched_addr, 1);

        if (then_branch)
            then_branch->compile_and_store(x64, Storage());
        
        x64->unwind->pop(this);
        
        x64->op(MOVQ, RDX, NO_EXCEPTION);
        then_scope->finalize_contents(x64);
        
        // Take care of the unmatched case
        x64->op(CMPQ, RDX, CAUGHT_UNMATCHED_EXCEPTION);
        x64->op(JE, else_label);  // dropped
        
        // Take care of the matched but raised case
        x64->op(CMPQ, RDX, NO_EXCEPTION);
        x64->op(JE, end);  // proceed
        
        // The then branch raised something, unwind
        x64->unwind->initiate(then_scope, x64);
        
        x64->code_label(else_label);
        
        if (else_branch)
            else_branch->compile_and_store(x64, Storage());
            
        x64->code_label(end);
        
        return Storage();
    }
    
    virtual Scope *unwind(X64 *x64) {
        if (matching)
            x64->op(MOVQ, RDX, CAUGHT_UNMATCHED_EXCEPTION);  // replace UNMATCHED from the match

        return then_scope;
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
        ts = WHATEVER_TS;
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
        
        if (scope->get_try_scope()) {
            // :raise gets it type from the enclosing function's exception type, and
            // is supposed to terminate this function. But a :try could catch these
            // exceptions, as outgoing and incoming exceptions use the same numeric range.
            // So a :try surrounding a :raise is not allowed.
            std::cerr << ":raise in a :try scope!\n";
            return false;
        }
        
        TypeSpec ets = { et };
        
        if (!check_args(args, { "value", &ets, scope, &value }))
            return false;
            
        dummy = new RaisingDummy;
        scope->add(dummy);

        ArgInfos infos = {
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
            x64->op(MOVQ, RDX, s.value);
            break;
        case REGISTER:
            x64->op(MOVZXBQ, RDX, s.reg);
            break;
        default:
            throw INTERNAL_ERROR;
        }
        
        x64->unwind->initiate(dummy, x64);

        // Since we're Whatever, must deceive our parent with a believable Storage.
        return (context_ts != VOID_TS ? Storage(STACK) : Storage());
    }
};


class TryValue: public ControlValue {
public:
    std::unique_ptr<Value> body, handler;
    TryScope *try_scope;
    Variable *switch_var;
    SwitchScope *switch_scope;
    bool handling;
    
    TryValue(Value *v, TypeMatch &m)
        :ControlValue("try") {
        try_scope = NULL;
        switch_var = NULL;
        switch_scope = NULL;
        
        handling = false;
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (context_ts != VOID_TS && context_ts != WHATEVER_TS && kwargs.size() > 0) {
            std::cerr << "Sorry, :try currently can't yield values!\n";
            return false;
        }
        
        try_scope = new TryScope;
        scope->add(try_scope);
        try_scope->enter();

        // Allow a :try without error handling to return the body type even without
        // the context explicitly set, to make :try in a declaration simpler.
        if (context_ts == ANY_TS && kwargs.size() > 0) {
            std::cerr << "A :try in Any context can't have multiple tails: " << token << "\n";
            return false;
        }
        
        TypeSpec ctx_ts = (context_ts[0] == code_type ? context_ts : context_ts.prefix(code_type));
        
        if (!check_args(args, { "body", &ctx_ts, try_scope, &body }))
            return false;
        
        if (kwargs.size() == 0)
            context_ts = body->ts.rvalue();

        ts = context_ts;

        try_scope->be_taken();
        try_scope->leave();
        
        //if (!setup_yieldable(scope))
        //    return false;

        switch_scope = new SwitchScope;
        scope->add(switch_scope);
        switch_scope->enter();

        Type *et = try_scope->get_exception_type();
        
        if (et) {
            TypeSpec ets = { et };
            switch_var = new Variable(switch_scope->get_variable_name(), ets);
            switch_scope->add(switch_var);
        }

        ArgInfos infos = {
            { "or", &VOID_CODE_TS, switch_scope, &handler }
        };
        
        if (!check_kwargs(kwargs, infos))
            return false;

        switch_scope->be_taken();
        switch_scope->leave();
        //eval_scope->be_taken();
        //eval_scope->leave();

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

        // FIXME:
        Storage t = get_context_storage();
        ts.store(s, t, x64);
        //XXX store_yield(s, x64);

        x64->op(MOVQ, RDX, NO_EXCEPTION);
        try_scope->finalize_contents(x64);  // exceptions from body jump here
        
        // The body may throw an exception
        Label live, unwind;
        x64->op(CMPQ, RDX, NO_EXCEPTION);
        x64->op(JE, live);
        x64->op(JL, unwind);  // reraise yields from body
        
        // Caught exception, prepare for handling
        if (switch_var) {
            Storage switch_storage = switch_var->get_local_storage();
            x64->op(MOVQ, switch_storage.address, RDX);
        }

        // dropped RDX

        x64->unwind->push(this);
        handling = true;
        if (handler)
            handler->compile_and_store(x64, Storage());
        x64->unwind->pop(this);

        switch_scope->finalize_contents(x64);
        //eval_scope->finalize_contents(x64);
        
        x64->op(CMPQ, RDX, NO_EXCEPTION);
        x64->op(JE, live);  // dropped

        // reraise exceptions from the handler, or yields from anywhere
        x64->code_label(unwind);
        x64->unwind->initiate(switch_scope, x64);

        x64->code_label(live);
        
        return t;
        //return get_yield_storage();
    }
    
    virtual Scope *unwind(X64 *x64) {
        return handling ? (Scope *)switch_scope : (Scope *)try_scope;
    }
};


// The Eval-Yield pairs use a different numeric exception range from normal exceptions,
// so their unwind paths are always distinct.

class EvalValue: public YieldableValue {
public:
    std::unique_ptr<Value> body;
    
    EvalValue(std::string en)
        :YieldableValue(en, en) {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!setup_yieldable(scope))
            return false;
            
        //std::cerr << "Put :eval variable into " << scope << "\n";
        
        TypeSpec *ctx = (context_ts == VOID_TS ? &VOID_CODE_TS : &WHATEVER_CODE_TS);
        
        if (!check_args(args, { "body", ctx, eval_scope, &body }))
            return false;
        
        ArgInfos infos = {
        };
        
        if (!check_kwargs(kwargs, infos))
            return false;
        
        eval_scope->be_taken();
        eval_scope->leave();
        
        return true;
    }
    
    virtual Regs precompile(Regs preferred) {
        return body->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        x64->unwind->push(this);
        body->compile_and_store(x64, Storage());
        x64->unwind->pop(this);

        x64->op(MOVQ, RDX, NO_EXCEPTION);
        eval_scope->finalize_contents(x64);  // exceptions from body jump here

        Label ok;
        
        x64->op(CMPQ, RDX, NO_EXCEPTION);
        x64->op(JE, ok);
        
        if (actually_yielded) {
            x64->op(CMPQ, RDX, get_yield_exception_value());
            x64->op(JE, ok);  // dropped
        }

        // reraise other exceptions        
        x64->unwind->initiate(eval_scope, x64);
        
        x64->code_label(ok);
        
        return get_yield_storage();
    }
    
    virtual Scope *unwind(X64 *x64) {
        return eval_scope;
    }
};


class YieldValue: public ControlValue {
public:
    Declaration *dummy;
    std::unique_ptr<Value> value;
    YieldableValue *yieldable_value;

    YieldValue(YieldableValue *yv)
        :ControlValue(yv->get_yield_label()) {
        dummy = NULL;
        yieldable_value = yv;
        ts = WHATEVER_TS;
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        TypeSpec arg_ts = yieldable_value->ts;
        
        if (arg_ts == VOID_TS)
            arg_ts = NO_TS;
        
        if (!check_args(args, { "value", &arg_ts, scope, &value }))
            return false;

        dummy = new RaisingDummy;
        scope->add(dummy);

        ArgInfos infos = {
        };
        
        if (!check_kwargs(kwargs, infos))
            return false;
        
        return true;
    }
    
    virtual Regs precompile(Regs preferred) {
        if (value)
            value->precompile(preferred);
            
        return Regs::all();
    }
    
    virtual Storage compile(X64 *x64) {
        if (value) {
            Storage s = value->compile(x64);
            yieldable_value->store_yield(s, x64);
        }
        
        yieldable_value->actually_yield();  // disallow optimizing handling out
        x64->op(MOVQ, RDX, yieldable_value->get_yield_exception_value());
        x64->unwind->initiate(dummy, x64);
        
        // Since we're Whatever, must deceive our parent with a believable Storage.
        return (context_ts != VOID_TS ? Storage(STACK) : Storage());
    }
};
