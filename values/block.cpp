#include "../plum.h"


CodeScopeValue::CodeScopeValue(Value *v, CodeScope *s, TypeSpec ts)
    :Value(ts) {
    value.reset(v);
    set_token(v->token);
    code_scope = s;
    code_scope->be_taken();
}

bool CodeScopeValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    throw INTERNAL_ERROR;
}

Regs CodeScopeValue::precompile(Regs preferred) {
    return value->precompile_tail() | Regs(RAX) | Regs(FPR0);
}

Storage CodeScopeValue::compile_body(X64 *x64) {
    x64->add_lineno(token.file_index, token.row);

    x64->unwind->push(this);
    Storage s = value->compile(x64);
    x64->unwind->pop(this);
    
    // NOTE: don't return a MEMORY storage, that may point to a variable we're
    // about to destroy!
    StorageWhere where = value->ts.where(AS_VALUE);
    Storage t = (
        where == NOWHERE ? Storage() :
        where == REGISTER ? Storage(REGISTER, RAX) :
        where == FPREGISTER ? Storage(FPREGISTER, FPR0) :
        Storage(STACK)
    );

    value->ts.store(s, t, x64);
    
    return t;
}

Storage CodeScopeValue::compile(X64 *x64) {
    code_scope->initialize_contents(x64);

    Storage t = compile_body(x64);

    code_scope->finalize_contents_and_unwind(x64);

    return t;
}

CodeScope *CodeScopeValue::unwind(X64 *x64) {
    return code_scope;  // stop unwinding here, and start destroying scoped variables
}

void CodeScopeValue::escape_statement_variables() {
    value->escape_statement_variables();
}



RetroScopeValue::RetroScopeValue(Value *v, CodeScope *s, TypeSpec ts)
    :CodeScopeValue(v, s, ts) {
}

void RetroScopeValue::deferred_compile(Label label, X64 *x64) {
    RetroScope *rs = ptr_cast<RetroScope>(code_scope);
    if (!rs)
        throw INTERNAL_ERROR;

    int retro_offset = rs->get_frame_offset();

    x64->code_label_local(label, "<retro>");
    x64->prologue();

    // Create an artificial stack frame at the location that RetroScope has allocated
    x64->op(POPQ, R11);  // caller RBP
    x64->op(MOVQ, R10, Address(R11, 0));  // enclosing RBP
    x64->op(POPQ, Address(R10, retro_offset + ADDRESS_SIZE));
    x64->op(MOVQ, Address(R10, retro_offset), R11);
    x64->op(LEA, RBP, Address(R10, retro_offset));

    code_scope->get_function_scope()->adjust_frame_base_offset(-retro_offset);
    code_scope->initialize_contents(x64);

    compile_body(x64);
    
    x64->op(MOVQ, RDX, NO_EXCEPTION);

    code_scope->finalize_contents(x64);
    code_scope->get_function_scope()->adjust_frame_base_offset(retro_offset);

    x64->code_label(code_scope->got_nothing_label);
    x64->code_label(code_scope->got_exception_label);
    x64->code_label(code_scope->got_yield_label);
    
    x64->op(CMPQ, RDX, NO_EXCEPTION);  // ZF => OK

    // Undo artificial frame
    x64->op(PUSHQ, Address(RBP, ADDRESS_SIZE));
    x64->op(PUSHQ, Address(RBP, 0));
    
    x64->epilogue();
    
    // Generate fixup code for the preceding ALIAS storage retro variables, they're
    // allocated in the middle of the function's stack frame in a RetroArgumentScope,
    // so there's no one else to take care of them. Fortunately they were moved into
    // this code scope by check_retros, so it's easy to find them.
    Label fixup_label;
    x64->code_label_local(fixup_label, "<retro>__fixup");
    x64->prologue();
    x64->runtime->log("Fixing retro arguments of a retro block.");
    
    x64->op(MOVQ, RBP, Address(RBP, 0));
    
    for (auto &d : code_scope->contents) {
        RetroArgumentScope *ras = ptr_cast<RetroArgumentScope>(d.get());
        
        if (!ras)
            break;
            
        // We need to fix all potential retro arguments, even the ones that were not
        // named in this invocation, because :evaluate still created them.
        TSs tss;
        ras->tuple_ts.unpack_tuple(tss);
        Storage s = ras->get_local_storage() + (-retro_offset);
        int offset = ras->tuple_ts.measure_stack();

        //std::cerr << "XXX retro fix " << ras->tuple_ts << "\n";
        
        for (auto &ts : tss) {
            StorageWhere w = ts.where(AS_ARGUMENT);
            offset -= ts.measure_where(w);
            
            if (w == ALIAS) {
                x64->runtime->fix_address(s.address + offset);
                x64->runtime->log("Fixed retro argument of " + ts.symbolize() + " of a retro block.");
                std::cerr << "Will fix retro argument of " << ts << " of a retro block.\n";
            }
        }
        
        if (offset != 0)
            throw INTERNAL_ERROR;
    }
    
    x64->epilogue();
    
    x64->runtime->add_func_info("<retro>", label, fixup_label);
}

Storage RetroScopeValue::compile(X64 *x64) {
    Label label = x64->once->compile(this);

    // Return a pointer to our code
    x64->op(LEA, R10, Address(label, 0));
    x64->op(PUSHQ, R10);

    return Storage(STACK);
}




DataBlockValue::DataBlockValue()
    :Value(VOID_TS) {
    //scope = s;
    
    //if (s->type != DATA_SCOPE && s->type != ARGUMENT_SCOPE && s->type != MODULE_SCOPE)
    //    throw INTERNAL_ERROR;
}

bool DataBlockValue::check_statement(Expr *expr, Scope *scope) {
    bool is_allowed = (expr->type == Expr::DECLARATION);

    if (!is_allowed) {
        std::cerr << "Impure statement not allowed in a pure context: " << expr->token << "!\n";
        return false;
    }

    Value *value = typize(expr, scope);

    // This is just a check for explicitly scoped declarations
    //DeclarationValue *dv = ptr_cast<DeclarationValue>(value);
    //Declaration *d = declaration_get_decl(dv);

    //if (d->outer_scope != scope)
    //    std::cerr << "Hah, a declaration wandered away!\n";

    statements.push_back(std::unique_ptr<Value>(value));
    return true;
}

bool DataBlockValue::check(Args &args, Kwargs &kwargs, Scope *scope) {      
    if (kwargs.size() > 0)
        throw INTERNAL_ERROR;
        
    for (auto &a : args) {
        if (!check_statement(a.get(), scope))
            return false;
    }
    
    return true;
}

bool DataBlockValue::check_tuple(Expr *expr, Scope *scope) {
    if (expr->type == Expr::TUPLE) {
        for (auto &a : expr->args)
            if (!check_statement(a.get(), scope))
                return false;
                
        return true;
    }
    else {
        return check_statement(expr, scope);
    }
}

bool DataBlockValue::define_data() {
    for (unsigned i = 0; i < statements.size(); i++)
        if (!statements[i]->define_data())
            return false;
            
    return true;
}

bool DataBlockValue::define_code() {
    for (unsigned i = 0; i < statements.size(); i++)
        if (!statements[i]->define_code())
            return false;
            
    return true;
}

Regs DataBlockValue::precompile(Regs preferred) {
    for (unsigned i = 0; i < statements.size(); i++)
        statements[i]->precompile_tail();
        
    return Regs();
}

Storage DataBlockValue::compile(X64 *x64) {
    for (unsigned i = 0; i < statements.size(); i++)
        statements[i]->compile(x64);
        
    return Storage();
}




CodeBlockValue::CodeBlockValue(TypeSpec lsc)
    :Value(VOID_TS) {  // May be overridden
    last_statement_context = lsc;

    // The context is always specified as a T Code, otherwise this is not invoked.
    // Function bodies are always evaluated with either {} Code (for void functions),
    // or {Whatever} Code (for nonvoid functions), but never allowing the last statement
    // to return a value. But currently we disallow any fancier context.
    if (lsc != TUPLE0_CODE_TS && lsc != WHATEVER_TUPLE1_CODE_TS)
        throw INTERNAL_ERROR;
}

bool CodeBlockValue::add_statement(Value *value, bool result) {
    statements.push_back(std::unique_ptr<Value>(value));
    
    if (result)
        ts = value->ts;  // TODO: rip code_type
        
    return true;
}

bool CodeBlockValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    if (kwargs.size() > 0) {
        std::cerr << "Labeled statements make no sense!\n";
        return false;
    }

    if (args.size() > 0) {
        for (unsigned i = 0; i < args.size() - 1; i++) {
            Expr *expr = args[i].get();
            std::unique_ptr<Value> v;
            
            if (!check_argument(0, expr, { { "stmt", &TUPLE0_CODE_TS, scope, &v } })) {
                std::cerr << "Statement error: " << expr->token << "\n";
                return false;
            }
            
            v->escape_statement_variables();
            
            if (!add_statement(v.release(), false))
                return false;
        }
        
        Expr *expr = args.back().get();
        std::unique_ptr<Value> v;
    
        if (!check_argument(0, expr, { { "stmt", &last_statement_context, scope, &v } })) {
            std::cerr << "Statement error: " << expr->token << "\n";
            return false;
        }
    
        if (!add_statement(v.release(), true))
            return false;
    }

    return true;
}

Regs CodeBlockValue::precompile(Regs preferred) {
    Regs clob;
    
    for (unsigned i = 0; i < statements.size() - 1; i++)
        clob = clob | statements[i]->precompile_tail();

    clob = clob | statements.back()->precompile(preferred);
        
    return clob;
}

Storage CodeBlockValue::compile(X64 *x64) {
    int stack_usage = x64->accounting->mark();

    for (unsigned i = 0; i < statements.size() - 1; i++) {
        Token &token = statements[i]->token;
        
        // Dwarves debug info
        x64->add_lineno(token.file_index, token.row);
        
        statements[i]->compile_and_store(x64, Storage());

        x64->op(NOP);  // For readability
        
        if (x64->accounting->mark() != stack_usage) {
            std::cerr << "Statement stack usage weirdness!\n";
            throw INTERNAL_ERROR;
        }
    }

    Token &token = statements.back()->token;
    x64->add_lineno(token.file_index, token.row);
    
    return statements.back()->compile(x64);
}




TupleBlockValue::TupleBlockValue(TypeSpec tuple_ts)
    :Value(tuple_ts) {
    tuple_ts.unpack_tuple(context_tss);
}

bool TupleBlockValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    if (kwargs.size() > 0) {
        std::cerr << "Sorry, labelled tuples are not yet supported!\n";
        return false;
    }

    if (args.size() != context_tss.size()) {
        std::cerr << "Wrong number of tuple elements!\n";
        return false;
    }

    for (unsigned i = 0; i < args.size(); i++) {
        Expr *expr = args[i].get();
        std::unique_ptr<Value> v;
        
        if (!check_argument(0, expr, { { "stmt", &context_tss[i], scope, &v } })) {
            std::cerr << "Tuple element error: " << expr->token << "\n";
            return false;
        }

        statements.push_back(std::move(v));
    }
    
    return true;
}

Regs TupleBlockValue::precompile(Regs preferred) {
    Regs clob;
    
    for (unsigned i = 0; i < statements.size(); i++)
        clob = clob | statements[i]->precompile_tail();

    return clob;
}

Storage TupleBlockValue::compile(X64 *x64) {
    x64->unwind->push(this);

    for (unsigned i = 0; i < statements.size(); i++) {
        StorageWhere where = context_tss[i].where(AS_ARGUMENT);
        Storage s = Storage(stacked(where));
        
        statements[i]->compile_and_store(x64, s);
        
        pushed_storages.push_back(s);
    }

    x64->unwind->pop(this);
    
    return Storage(STACK);
}

CodeScope *TupleBlockValue::unwind(X64 *x64) {
    for (int i = pushed_storages.size() - 1; i >= 0; i--) {
        context_tss[i].store(pushed_storages[i], Storage(), x64);
    }

    return NULL;
}




DeclarationValue::DeclarationValue(std::string n, TypeSpec *c)
    :Value(VOID_TS) {
    name = n;
    context = c;  // This may have a limited lifetime!
    decl = NULL;
    var = NULL;
}

std::string DeclarationValue::get_name() {
    return name;
}

Declaration *DeclarationValue::get_decl() {
    return decl;
}

Variable *DeclarationValue::get_var() {
    return var;
}

void DeclarationValue::fix_bare(TypeSpec var_ts, Scope *scope) {
    // This must be called after/instead of check
    var = new Variable(name, var_ts.lvalue());
    decl = var;
    
    scope->add(decl);
    
    ts = var->alloc_ts.reprefix(lvalue_type, uninitialized_type);
}

void DeclarationValue::fix_bare_retro(TypeSpec var_ts, Scope *scope) {
    // This must be called after/instead of check
    var = new Variable(name, var_ts);
    decl = var;
    
    scope->add(decl);
}

bool DeclarationValue::associate(Declaration *decl, std::string name) {
    if (!decl || !decl->outer_scope)
        throw INTERNAL_ERROR;
        
    for (auto &d : decl->outer_scope->contents) {
        Associable *able = ptr_cast<Associable>(d.get());
        
        if (able) {
            Associable *a = able->lookup_associable(name);
            
            if (a)
                return a->check_associated(decl);
        }
    }
        
    std::cerr << "Invalid association qualification for declaration!\n";
    return false;
}

bool DeclarationValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    std::unique_ptr<Expr> expr;
    
    ExprInfos eis = {
        { "", &expr }
    };
    
    if (!check_exprs(args, kwargs, eis)) {
        std::cerr << "Whacky declaration!\n";
        return false;
    }

    std::cerr << "Trying to declare " << name << "\n";

    if (!expr) {
        if (!context || (*context)[0] == dvalue_type) {
            // Must call fix_bare later to add the real declaration
            ts = BARE_UNINITIALIZED_TS;  // dummy type
            return true;
        }

        std::cerr << "Bare declaration is not allowed in this context!\n";
        return false;
    }

    Value *v = typize(expr.get(), scope, context);  // This is why arg shouldn't be a pivot
    
    if (!v->ts.is_meta() && !v->ts.is_hyper()) {
        std::cerr << "Not a type used for declaration, but a value of " << v->ts << "!\n";
        return false;
    }
    
    value.reset(v);

    decl = value->declare(name, scope);
    
    if (!decl) {
        std::cerr << "Invalid declaration: " << token << "!\n";
        return false;
    }
    
    auto i = name.rfind('.');
    
    if (i != std::string::npos) {
        if (!associate(decl, name.substr(0, i)))
            return false;
    }
    
    if (scope->type == CODE_SCOPE) {
        var = ptr_cast<Variable>(decl);
        
        if (var)
            ts = var->alloc_ts.reprefix(lvalue_type, uninitialized_type);
    }

    return true;
}

bool DeclarationValue::define_data() {
    if (!decl) {
        std::cerr << "Bare declaration was not fixed!\n";
        throw INTERNAL_ERROR;
    }
        
    if (value)
        return value->define_data();
    else
        return true;
}

bool DeclarationValue::define_code() {
    if (value)
        return value->define_code();
    else
        return true;
}

Regs DeclarationValue::precompile(Regs preferred) {
    if (value)
        return value->precompile(preferred);
    else
        return Regs();
}

Storage DeclarationValue::compile(X64 *x64) {
    // value may be unset for retro variables
    Storage s = (value ? value->compile(x64) : Storage());
    
    // just to be sure we don't have something nasty here
    if (s.where != NOWHERE)
        throw INTERNAL_ERROR;
        
    // var is unset for anything nonvariable
    // FIXME: handle lvalue result properly!
    return var ? var->get_local_storage() : Storage();
}

void DeclarationValue::escape_statement_variables() {
    Scope *s = decl->outer_scope;
    s->enter();
    s->remove(decl);
    s->leave();
    s->outer_scope->add(decl);
    //std::cerr << "Escaped variable " << name << " from " << s << " to " << s->outer_scope << "\n";
}




RetroArgumentValue::RetroArgumentValue(Value *v, RetroArgumentScope *ras)
    :Value(ras->tuple_ts.prefix(dvalue_type)) {
    value.reset(v);  // potentially NULL
    retro_argument_scope = ras;
}

bool RetroArgumentValue::check() {
    // We must have zero or more bare declarations inside, must tell them
    // what types they are actually.
    
    TSs tss;
    retro_argument_scope->tuple_ts.unpack_tuple(tss);
    
    std::vector<Value *> vs;
    DataBlockValue *dbv = ptr_cast<DataBlockValue>(value.get());
    
    if (dbv) {
        for (auto &s : dbv->statements)
            vs.push_back(s.get());
    }
    else if (value)
        vs.push_back(value.get());
        
    if (vs.size() > tss.size()) {
        std::cerr << "Too many retro variables!\n";
        return false;
    }
    
    for (unsigned i = 0; i < vs.size(); i++) {
        DeclarationValue *dv = ptr_cast<DeclarationValue>(vs[i]);
        
        if (!dv) {
            std::cerr << "Not a declaration in a retro argument!\n";
            return false;
        }
        
        if (dv->ts != BARE_UNINITIALIZED_TS) {
            std::cerr << "Not a bare declaration in a retro argument!\n";
            return false;
        }
        
        dv->fix_bare_retro(tss[i], retro_argument_scope);
        std::cerr << "Fixed bare retro variable " << dv->name << " to be " << tss[i] << "\n";
    }
    
    return true;
}

Regs RetroArgumentValue::precompile(Regs preferred) {
    return Regs();
}

Storage RetroArgumentValue::compile(X64 *x64) {
    // A MEMORY that points to the beginning of the tuple values.
    // Will be passed as ALISTACK, though.
    return retro_argument_scope->get_local_storage();
}




CreateValue::CreateValue(Value *l, TypeMatch &tm)
    :GenericOperationValue(CREATE, tm[1], tm[1].prefix(lvalue_type), l) {
}

bool CreateValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    // CreateValue may operate on uninitialized member variables, or newly
    // declared local variables. The latter case needs some extra care.
    // Since the declared variable will be initialized in the final step, we
    // should make it the last declaration in this scope, despite the syntax
    // which puts it at the beginning of the initialization expression.
    // This way it can also be taken to its parent scope if necessary.
    
    DeclarationValue *dv = ptr_cast<DeclarationValue>(left.get());
    Declaration *d = NULL;
    
    if (dv) {
        d = dv->get_decl();
        
        if (d)
            scope->remove(d);  // care
        else if (dv->ts == BARE_UNINITIALIZED_TS)
            arg_ts = ANY_TS;  // bare
        else
            throw INTERNAL_ERROR;
    }
    
    if (!GenericOperationValue::check(args, kwargs, scope))
        return false;
        
    if (dv) {
        if (d)
            scope->add(d);  // care
        else if (dv->ts == BARE_UNINITIALIZED_TS) {
            // bare
            TypeSpec implicit_ts = right->ts.rvalue();
            std::cerr << "Fixing bare declaration with " << implicit_ts << ".\n";
            dv->fix_bare(implicit_ts, scope);
        
            arg_ts = left->ts.unprefix(uninitialized_type);
            ts = left->ts.reprefix(uninitialized_type, lvalue_type);
        }
        else
            throw INTERNAL_ERROR;
    }
    
    return true;
}

void CreateValue::use(Value *r) {
    DeclarationValue *dv = ptr_cast<DeclarationValue>(left.get());
    
    if (!dv->get_decl())  // make sure no unfixed bare declarations
        throw INTERNAL_ERROR;
        
    right.reset(r);
}

Declaration *CreateValue::get_decl() {
    DeclarationValue *dv = ptr_cast<DeclarationValue>(left.get());

    return (dv ? dv->get_decl() : NULL);
}

void CreateValue::escape_statement_variables() {
    DeclarationValue *dv = ptr_cast<DeclarationValue>(left.get());
    
    if (dv)
        left->escape_statement_variables();
}
