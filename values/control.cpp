#include "../plum.h"


ControlValue::ControlValue(std::string n)
    :Value(VOID_TS) {
    name = n;
    context_ts = VOID_TS;  // pivot controls will be Void
}

TypeSpec ControlValue::codify(TypeSpec ts) {
    if (ts[0] == code_type)
        return ts;
    else if (ts == VOID_TS)
        return { code_type, tuple0_type };
    else if (ts.has_meta(tuple_metatype))
        return ts.prefix(code_type);
    else
        return ts.prefix(tuple1_type).prefix(code_type);
}

void ControlValue::set_context_ts(TypeSpec *c) {
    if (!c)
        return;
        
    context_ts = *c;
    Type *a = context_ts[0];
    
    if (a == code_type)
        context_ts = (
            context_ts[1] == tuple0_type ? VOID_TS :
            context_ts[1] == tuple1_type ? context_ts.unprefix(code_type).unprefix(tuple1_type) :
            context_ts.unprefix(code_type)  // TODO: leaving Tuple on for now
        );
    else if (a == ovalue_type)
        context_ts = context_ts.unprefix(ovalue_type);
    else if (a == lvalue_type) {
        std::cerr << "Control :" << name << " in Lvalue context!\n";
        throw TYPE_ERROR;
    }
}

bool ControlValue::has_fuzzy_context_ts() {
    bool is_fuzzy = false;
    
    if (context_ts.is_tuple()) {
        TSs tss;
        context_ts.unpack_tuple(tss);
        
        for (auto &ts : tss)
            is_fuzzy = is_fuzzy || (ts == ANY_TS);
    }
    else
        is_fuzzy = (context_ts == ANY_TS);
        
    return is_fuzzy;
}

bool ControlValue::check_args(Args &args, ArgInfo arg_info) {
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

bool ControlValue::check_kwargs(Kwargs &kwargs, ArgInfos arg_infos) {
    Args fake_args;
    
    return check_arguments(fake_args, kwargs, arg_infos);
}

Storage ControlValue::get_context_storage() {
    if (context_ts == ANY_TS)
        throw INTERNAL_ERROR;
        
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

Storage ControlValue::nonsense_result(X64 *x64) {
    // NOTE: this is not very nice. Even if a control is known not to return, it needs to
    // fake it does, because we'll generate code to process the result. This mean
    // returning some believable Storage, and the corresponding stack accounting.
    // However, this is an extreme case that nobody will use in real life,
    // eg: 100 + :return
    
    if (context_ts == VOID_TS || context_ts == WHATEVER_TS) {
        return Storage();
    }
    else {
        x64->op(SUBQ, RSP, context_ts.measure_stack());
        return Storage(STACK);
    }
}




IfValue::IfValue(OperationType o, Value *pivot, TypeMatch &match)
    :ControlValue("if") {
}

bool IfValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    if (has_fuzzy_context_ts()) {
        std::cerr << ":if in fuzzy context!\n";
        return false;
    }
    
    ts = context_ts;
    
    TypeSpec arg_ts = codify(ts);
        
    ArgInfos infos = {
        { "condition", &BOOLEAN_TS, scope, &condition },
        { "then", &arg_ts, scope, &then_branch },
        { "else", &arg_ts, scope, &else_branch }
    };
    
    if (!check_arguments(args, kwargs, infos))
        return false;

    return true;
}

Regs IfValue::precompile(Regs preferred) {
    Regs clobbered = Regs();
    
    clobbered = clobbered | condition->precompile_tail();
    
    if (then_branch)
        clobbered = clobbered | then_branch->precompile(preferred);
                   
    if (else_branch)
        clobbered = clobbered | else_branch->precompile(preferred);
    
    // This won't be bothered by either branches
    reg = preferred.has_gpr() ? preferred.get_gpr() : RAX;
    
    return clobbered | reg;
}

Storage IfValue::compile(X64 *x64) {
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




YieldableValue::YieldableValue(std::string n, std::string yn)
    :ControlValue(n) {
    yield_name = yn;
    eval_scope = NULL;
    yield_var = NULL;
}

std::string YieldableValue::get_yield_label() {
    return yield_name;
}

bool YieldableValue::setup_yieldable(Scope *scope) {
    if (has_fuzzy_context_ts()) {
        std::cerr << "Yieldable in fuzzy context!\n";
        return false;
    }
    
    ts = context_ts;

    eval_scope = new EvalScope;
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

Storage YieldableValue::get_yield_storage() {
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

void YieldableValue::store_yield(Storage s, X64 *x64) {
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




RepeatValue::RepeatValue(Value *pivot, TypeMatch &match)
    :ControlValue("repeat") {
}

bool RepeatValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    if (!check_args(args, { "init", &VOID_TS, scope, &setup }))
        return false;

    ArgInfos infos = {
        { "do", &TUPLE0_CODE_TS, scope, &body },
        { "on", &BOOLEAN_TUPLE1_CODE_TS, scope, &condition },
        { "by", &TUPLE0_CODE_TS, scope, &step }
    };
    
    return check_kwargs(kwargs, infos);
}

Regs RepeatValue::precompile(Regs preferred) {
    if (setup)
        setup->precompile_tail();
        
    if (condition)
        condition->precompile_tail();
        
    if (step)
        step->precompile_tail();
        
    if (body)
        body->precompile_tail();
        
    return Regs::all();  // We're Void
}

Storage RepeatValue::compile(X64 *x64) {
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




ForEachValue::ForEachValue(Value *pivot, TypeMatch &match)
    :ControlValue("for") {
    iterator_var = NULL;
    next_try_scope = NULL;
}

bool ForEachValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    std::unique_ptr<Value> ib;

    if (!check_args(args, { "", &ANYTUPLE_ITERABLE_TS, scope, &ib}))
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
    if (!typematch(ANYTUPLE_ITERATOR_TS, it, match)) {
        std::cerr << "Iterable iter didn't return an Iterator, but: " << it->ts << "!\n";
        return false;
    }

    TypeSpec tuple_ts = match[1];
    each_ts = tuple_ts.prefix(dvalue_type);  //lvalue();
    
    Value *next = lookup_fake("next", it, next_try_scope, token, NULL);
    
    if (!next) {
        std::cerr << "Iterator didn't implement the next method!\n";
        throw INTERNAL_ERROR;
    }
    
    if (next->ts != tuple_ts) {
        std::cerr << "Misimplemented " << tuple_ts << " iterator next returns " << next->ts << "!\n";
        throw INTERNAL_ERROR;
    }
    
    this->next.reset(next);

    next_try_scope->be_taken();
    next_try_scope->leave();

    ArgInfos infos = {
        { "each", &each_ts, scope, &each },
        { "do", &TUPLE0_CODE_TS, scope, &body }
    };
    
    if (!check_kwargs(kwargs, infos))
        return false;

    return true;
}

Regs ForEachValue::precompile(Regs preferred) {
    if (iterator)
        iterator->precompile_tail();
        
    if (each)
        each->precompile_tail();
        
    if (body)
        body->precompile_tail();
        
    if (next)
        next->precompile_tail();
        
    return Regs::all();  // We're Void
}

Storage ForEachValue::compile(X64 *x64) {
    Storage is = iterator->compile(x64);
    Storage var_storage = iterator_var->get_local_storage();
    iterator->ts.create(is, var_storage, x64);
    
    Storage es = each->compile(x64);
    if (es.where != MEMORY || es.address.base != RBP)
        throw INTERNAL_ERROR;  // FIXME: lame temporary restriction only
    
    Label start, end;
    x64->code_label(start);

    next_try_scope->initialize_contents(x64);

    x64->unwind->push(this);
    next->compile_and_store(x64, Storage(STACK));
    x64->unwind->pop(this);
    
    // Finalize after storing, so the return value won't be lost
    next->ts.create(Storage(STACK), es, x64);  // create the each variable

    // On exception we jump here, so the each variable won't be created
    next_try_scope->finalize_contents_and_unwind(x64);

    x64->code_label(next_try_scope->got_nothing_label);
    
    body->compile_and_store(x64, Storage());
    
    next->ts.destroy(es, x64);  // destroy the each variable
    
    x64->op(JMP, start);
    
    x64->code_label(next_try_scope->got_exception_label);
    x64->code_label(end);
    // We don't need to clean up local variables yet (iterator_var is not in our scopes)
    
    return Storage();
}

CodeScope *ForEachValue::unwind(X64 *x64) {
    // May be called only while executing next
    return next_try_scope;
}




SwitchValue::SwitchValue(Value *v, TypeMatch &m)
    :ControlValue("switch") {
    switch_scope = NULL;
    switch_var = NULL;
}

bool SwitchValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    if (!check_args(args, { "value", &ANY_TS, scope, &value }))
        return false;
        
    if (value->ts == VOID_TS) {  // TODO: is this unnecessary fot ANY_TS?
        std::cerr << "Whacky :switch!\n";
        return false;
    }
    
    switch_scope = new SwitchScope;
    scope->add(switch_scope);
    switch_scope->enter();

    switch_var = new Variable("<switch>", value->ts);
    switch_scope->set_switch_variable(switch_var);
    
    ArgInfos infos = {
        { "do", &TUPLE0_CODE_TS, switch_scope, &body }
    };
    
    if (!check_kwargs(kwargs, infos))
        return false;

    switch_scope->be_taken();
    switch_scope->leave();

    return true;
}

Regs SwitchValue::precompile(Regs preferred) {
    Regs clob = value->precompile_tail();
        
    if (body)
        clob = clob | body->precompile_tail();
        
    return clob;
}

Storage SwitchValue::compile(X64 *x64) {
    Storage vs = value->compile(x64);

    switch_scope->initialize_contents(x64);

    Storage switch_storage = switch_var->get_local_storage();
    value->ts.create(vs, switch_storage, x64);

    x64->unwind->push(this);
    
    if (body)
        body->compile_and_store(x64, Storage());
    
    x64->unwind->pop(this);

    switch_scope->finalize_contents_and_unwind(x64);

    return Storage();
}

CodeScope *SwitchValue::unwind(X64 *x64) {
    return switch_scope;  // Start finalizing the variable
}




IsValue::IsValue(Value *v, TypeMatch &m)
    :ControlValue("is") {
    matching = false;
}

bool IsValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    then_scope = new CodeScope;
    scope->add(then_scope);
    then_scope->enter();

    match_try_scope = new TransparentTryScope;
    then_scope->add(match_try_scope);
    match_try_scope->enter();

    TypeSpec match_ts = VOID_TS;
    //SwitchScope *switch_scope = scope->get_switch_scope();

    //if (switch_scope) {
    //    Variable *switch_var = switch_scope->get_variable();
    //    match_ts = switch_var->alloc_ts.rvalue().prefix(equalitymatcher_type);
    //}
    
    if (!check_args(args, { "match", &match_ts, match_try_scope, &match }))
        return false;

    Type *et = match_try_scope->get_exception_type();
    /*
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
    */
    if (et && et != match_unmatched_exception_type) {
        std::cerr << "This :is match raises " << et->name << " exception!\n";
        return false;
    }

    // Now that we handled the implicit equality matching case, voidize the match value
    if (match->ts != VOID_TS)
        match.reset(make<VoidConversionValue>(match.release()));

    // This is a TransparentTryScope with no inner declarations, needs no finalization
    match_try_scope->leave();

    if (has_fuzzy_context_ts()) {
        std::cerr << ":is in fuzzy context!\n";
        return false;
    }

    ts = context_ts;
    TypeSpec arg_ts = codify(ts);

    ArgInfos infos = {
        { "then", &arg_ts, then_scope, &then_branch },
        { "else", &arg_ts, scope, &else_branch },
    };
    
    if (!check_kwargs(kwargs, infos))
        return false;

    then_scope->be_taken();
    then_scope->leave();

    return true;
}

Regs IsValue::precompile(Regs preferred) {
    match->precompile_tail();
        
    if (then_branch)
        then_branch->precompile_tail();

    if (else_branch)
        else_branch->precompile_tail();
        
    return Regs::all();  // We're Void
}

Storage IsValue::compile(X64 *x64) {
    Label else_label, end;

    then_scope->initialize_contents(x64);
    x64->unwind->push(this);
    
    match_try_scope->initialize_contents(x64);
    
    matching = true;
    match->compile_and_store(x64, Storage());
    matching = false;
    
    match_try_scope->finalize_contents_and_unwind(x64);

    x64->code_label(match_try_scope->got_nothing_label);
    
    Storage t = get_context_storage();

    if (then_branch)
        then_branch->compile_and_store(x64, t);
    
    x64->unwind->pop(this);
    
    then_scope->finalize_contents_and_unwind(x64);
    
    x64->op(JMP, end);
    
    x64->code_label(match_try_scope->got_exception_label);
    x64->code_label(else_label);
    
    if (else_branch)
        else_branch->compile_and_store(x64, t);
    else if (match_try_scope->has_implicit_matcher()) {
        // If an implicit matcher was used (within a :switch or :try), then die
        SwitchScope *ss = match_try_scope->get_switch_scope();
        Variable *sv = ss->get_variable();
        Label clone_label = x64->once->compile(compile_array_clone, CHARACTER_TS);

        std::stringstream msg;
        msg << "Fatal unmatched value at " << token << ": ";

        Label message_label = x64->runtime->data_heap_string(decode_utf8(msg.str()));

        x64->op(LEA, RAX, Address(message_label, 0));
        x64->runtime->incref(RAX);
        x64->op(CALL, clone_label);
        x64->op(PUSHQ, RAX);  // Pseudo-variable serving as the message stream

        sv->alloc_ts.store(sv->get_local_storage(), Storage(STACK), x64);
        x64->op(LEA, R11, Address(RSP, sv->alloc_ts.measure_stack()));
        x64->op(PUSHQ, R11);
        
        sv->alloc_ts.streamify(x64);
        
        x64->op(POPQ, R11);
        sv->alloc_ts.store(Storage(STACK), Storage(), x64);
        
        x64->op(POPQ, RAX);
        x64->runtime->dies(RAX);
    }
        
    x64->code_label(end);
    
    return t;
}

CodeScope *IsValue::unwind(X64 *x64) {
    if (matching)
        return match_try_scope;
    else
        return then_scope;
}




RaiseValue::RaiseValue(Value *v, TypeMatch &m)
    :ControlValue("raise") {
    dummy = NULL;
    exception_value = 0;
    ts = WHATEVER_TS;
}

bool RaiseValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
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
        
    dummy = new RaisingDummy(EXCEPTION_UNWOUND);
    scope->add(dummy);

    ArgInfos infos = {
    };
    
    if (!check_kwargs(kwargs, infos))
        return false;
    
    return true;
}

Regs RaiseValue::precompile(Regs preferred) {
    value->precompile_tail();
        
    return Regs::all();  // We're Void
}

Storage RaiseValue::compile(X64 *x64) {
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

    return nonsense_result(x64);
}




TryValue::TryValue(Value *v, TypeMatch &m)
    :ControlValue("try") {
    try_scope = NULL;
    switch_var = NULL;
    switch_scope = NULL;
    
    handling = false;
}

bool TryValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    try_scope = new TryScope;
    scope->add(try_scope);
    try_scope->enter();

    // Let the body have our context, even if it is fuzzy
    TypeSpec arg_ts = codify(context_ts);
    
    if (!check_args(args, { "body", &arg_ts, try_scope, &body }))
        return false;
    
    // Allow a :try without type context, but with a concrete body type return the
    // same type, and also expect the same from the fix branch.
    // But if the body is a multivalue, then just disallow the fix branch, we don't
    // want to allow multivalues from different branches.
    if (has_fuzzy_context_ts()) {
        context_ts = body->ts.rvalue();
        arg_ts = codify(context_ts);  //(context_ts.is_tuple() ? NO_TS : codify(context_ts));
    }

    ts = context_ts;

    try_scope->be_taken();
    try_scope->leave();
    
    switch_scope = new SwitchScope;
    scope->add(switch_scope);
    switch_scope->enter();

    Type *et = try_scope->get_exception_type();
    
    if (et) {
        TypeSpec ets = { et };
        switch_var = new Variable("<raised>", ets);
        switch_scope->set_switch_variable(switch_var);
    }

    if (kwargs.size() > 0) {
        // Let the fix branch be optional, even if an concrete type is expected from it,
        // because the default handler dies anyway.
        
        ArgInfos infos = {
            { "fix", &arg_ts, switch_scope, &handler }
        };
    
        if (!check_kwargs(kwargs, infos))
            return false;
    }

    switch_scope->be_taken();
    switch_scope->leave();

    return true;
}

Regs TryValue::precompile(Regs preferred) {
    Regs clob = body->precompile(preferred);
        
    if (handler)
        clob = clob | handler->precompile(preferred);
        
    return clob;
}

Storage TryValue::compile(X64 *x64) {
    try_scope->initialize_contents(x64);

    x64->unwind->push(this);
    handling = false;
    Storage s = body->compile(x64);
    x64->unwind->pop(this);

    // FIXME:
    Storage t = get_context_storage();
    ts.store(s, t, x64);
    //XXX store_yield(s, x64);

    try_scope->finalize_contents_and_unwind(x64);  // exceptions from body jump here
    
    // Caught exception, prepare for handling
    x64->code_label(try_scope->got_exception_label);

    switch_scope->initialize_contents(x64);
    
    if (switch_var) {
        Storage switch_storage = switch_var->get_local_storage();
        x64->op(MOVQ, switch_storage.address, RDX);
    }
    // dropped RDX

    x64->unwind->push(this);
    handling = true;
    
    if (handler) {
        handler->compile_and_store(x64, t);
    }
    else {
        // Normal execution will die here
        TreenumerationType *et = try_scope->get_exception_type();
        
        if (et) {
            auto arg_regs = x64->abi_arg_regs();
            
            x64->op(MOVQ, R10, switch_var->get_local_storage().address);
            x64->op(LEA, R11, Address(et->get_stringifications_label(x64), 0));
            x64->op(MOVQ, arg_regs[0], Address(R11, R10, Address::SCALE_8, 0));  // treenum name
            x64->op(MOVQ, arg_regs[1], token.row);
            x64->runtime->call_sysv(x64->runtime->sysv_die_uncaught_label);
            x64->op(UD2);
        }
    }

    x64->unwind->pop(this);

    switch_scope->finalize_contents_and_unwind(x64);

    x64->code_label(try_scope->got_nothing_label);
    
    return t;
}

CodeScope *TryValue::unwind(X64 *x64) {
    return handling ? (CodeScope *)switch_scope : (CodeScope *)try_scope;
}


// The Eval-Yield pairs use a different numeric exception range from normal exceptions,
// so their unwind paths are always distinct.

EvalValue::EvalValue(std::string en)
    :YieldableValue(en, en) {
}

bool EvalValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    if (!setup_yieldable(scope))
        return false;
        
    //std::cerr << "Put :eval variable into " << scope << "\n";
    
    TypeSpec *ctx = (context_ts == VOID_TS ? &TUPLE0_CODE_TS : &WHATEVER_TUPLE1_CODE_TS);
    
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

Regs EvalValue::precompile(Regs preferred) {
    return body->precompile_tail();
}

Storage EvalValue::compile(X64 *x64) {
    eval_scope->initialize_contents(x64);

    x64->unwind->push(this);
    body->compile_and_store(x64, Storage());
    x64->unwind->pop(this);

    eval_scope->finalize_contents_and_unwind(x64);  // exceptions from body jump here

    return get_yield_storage();
}

CodeScope *EvalValue::unwind(X64 *x64) {
    return eval_scope;
}




YieldValue::YieldValue(YieldableValue *yv)
    :ControlValue(yv->get_yield_label()) {
    dummy = NULL;
    yieldable_value = yv;
    ts = WHATEVER_TS;
}

bool YieldValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    TypeSpec arg_ts = yieldable_value->ts;
    
    if (arg_ts == VOID_TS)
        arg_ts = NO_TS;
    
    if (!check_args(args, { "value", &arg_ts, scope, &value }))
        return false;

    dummy = new RaisingDummy(YIELD_UNWOUND);
    scope->add(dummy);

    ArgInfos infos = {
    };
    
    if (!check_kwargs(kwargs, infos))
        return false;

    return true;
}

Regs YieldValue::precompile(Regs preferred) {
    if (value)
        value->precompile_tail();
        
    return Regs::all();
}

Storage YieldValue::compile(X64 *x64) {
    if (value) {
        Storage s = value->compile(x64);
        yieldable_value->store_yield(s, x64);
    }
    
    x64->op(MOVQ, RDX, yieldable_value->eval_scope->get_yield_value());
    x64->unwind->initiate(dummy, x64);

    return nonsense_result(x64);
}
