
class CodeScopeValue: public Value {
public:
    std::unique_ptr<Value> value;
    CodeScope *code_scope;
    Storage save_storage;

    CodeScopeValue(Value *v, CodeScope *s)
        :Value(v->ts.rvalue()) {
        value.reset(v);
        set_token(v->token);
        code_scope = s;
        code_scope->be_taken();
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        throw INTERNAL_ERROR;
    }

    virtual bool unpack(std::vector<TypeSpec> &t) {
        // Just in case we forward a multivalue out of the body
        return value->unpack(t);
    }

    virtual Regs precompile(Regs preferred) {
        return value->precompile(preferred) | Regs(RAX) | Regs(XMM0);
    }

    virtual Storage compile_body(X64 *x64) {
        x64->add_line_info(token.file_index, token.row);

        x64->unwind->push(this);
        Storage s = value->compile(x64);
        x64->unwind->pop(this);
        
        // NOTE: don't return a MEMORY storage, that may point to a variable we're
        // about to destroy!
        StorageWhere where = value->ts.where(AS_VALUE);
        Storage t = (
            where == NOWHERE ? Storage() :
            where == REGISTER ? Storage(REGISTER, RAX) :
            where == SSEREGISTER ? Storage(SSEREGISTER, XMM0) :
            Storage(STACK)
        );

        value->ts.store(s, t, x64);
        
        return t;
    }
    
    virtual Storage compile(X64 *x64) {
        Storage t = compile_body(x64);
    
        if (code_scope->is_unwindable())
            x64->op(MOVQ, RDX, NO_EXCEPTION);

        code_scope->finalize_contents(x64);

        if (code_scope->is_unwindable()) {
            x64->op(CMPQ, RDX, NO_EXCEPTION);

            Label ok;
            x64->op(JE, ok);
    
            x64->unwind->initiate(code_scope, x64);
        
            x64->code_label(ok);
        }

        return t;
    }
    
    virtual Scope *unwind(X64 *x64) {
        return code_scope;  // stop unwinding here, and start destroying scoped variables
    }
    
    virtual void escape_statement_variables() {
        value->escape_statement_variables();
    }
};


class RetroScopeValue: public CodeScopeValue, public Deferrable {
public:
    RetroScopeValue(Value *v, CodeScope *s)
        :CodeScopeValue(v, s) {
    }
    
    virtual void deferred_compile(Label label, X64 *x64) {
        RetroScope *rs = ptr_cast<RetroScope>(code_scope);
        if (!rs)
            throw INTERNAL_ERROR;

        int retro_offset = rs->get_frame_offset();

        x64->code_label(label);
        
        // Create an artificial stack frame at the location that RetroScope has allocated
        x64->op(MOVQ, R10, Address(RBP, 0));  // get our own frame back
        x64->op(POPQ, Address(R10, retro_offset + ADDRESS_SIZE));
        x64->op(MOVQ, Address(R10, retro_offset), RBP);
        x64->op(LEA, RBP, Address(R10, retro_offset));

        compile_body(x64);
        
        x64->op(MOVQ, RDX, NO_EXCEPTION);

        code_scope->finalize_contents(x64);

        x64->op(CMPQ, RDX, NO_EXCEPTION);  // ZF => OK

        x64->op(PUSHQ, Address(RBP, ADDRESS_SIZE));
        x64->op(MOVQ, RBP, Address(RBP, 0));
        
        x64->op(RET);
        
        // Generate fixup code for the preceding ALIAS storage retro variables, they're
        // allocated in the middle of the function's stack frame, so there's no one else
        // to take care of them. Fortunately they were moved into this code scope by
        // check_retros, so it's easy to find them.
        Label fix;
        x64->code_label(fix);
        x64->runtime->log("Fixing retro arguments of a retro block.");
        
        for (auto &d : code_scope->contents) {
            RetroVariable *rv = ptr_cast<RetroVariable>(d.get());
            
            if (!rv)
                break;
                
            Storage s = rv->get_local_storage();
            
            if (s.where == ALIAS) {
                x64->runtime->fix_address(s.address + (-retro_offset));
                x64->runtime->log("Fixed retro argument " + rv->name + " of a retro block.");
            }
        }
        
        x64->op(RET);
        
        x64->runtime->add_func_info("<retro>", label, fix);
    }
    
    virtual Storage compile(X64 *x64) {
        Label label = x64->once->compile(this);

        // Return a pointer to our code
        x64->op(LEA, R10, Address(label, 0));
        x64->op(PUSHQ, R10);

        return Storage(STACK);
    }
};




class DataBlockValue: public Value {
public:
    Scope *scope;  // Must work with various scopes
    std::vector<std::unique_ptr<Value>> statements;

    DataBlockValue(Scope *s)
        :Value(VOID_TS) {
        scope = s;
        
        if (s->type != DATA_SCOPE && s->type != ARGUMENT_SCOPE && s->type != MODULE_SCOPE)
            throw INTERNAL_ERROR;
    }

    virtual bool check_statement(Expr *expr) {
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

    virtual bool define_data() {
        for (unsigned i = 0; i < statements.size(); i++)
            if (!statements[i]->define_data())
                return false;
                
        return true;
    }

    virtual bool define_code() {
        for (unsigned i = 0; i < statements.size(); i++)
            if (!statements[i]->define_code())
                return false;
                
        return true;
    }

    virtual Regs precompile(Regs preferred) {
        for (unsigned i = 0; i < statements.size(); i++)
            statements[i]->precompile();
            
        return Regs();
    }

    virtual Storage compile(X64 *x64) {
        for (unsigned i = 0; i < statements.size(); i++)
            statements[i]->compile(x64);
            
        return Storage();
    }
};


class CodeBlockValue: public Value {
public:
    std::vector<std::unique_ptr<Value>> statements;
    TypeSpec *context;

    CodeBlockValue(TypeSpec *c)
        :Value(VOID_TS) {  // May be overridden
        context = c;
        
        if (!c || (*c != VOID_TS && *c != VOID_CODE_TS && *c != WHATEVER_TS && *c != WHATEVER_CODE_TS))
            throw INTERNAL_ERROR;
    }

    virtual bool add_statement(Value *value, bool result = false) {
        statements.push_back(std::unique_ptr<Value>(value));
        
        if (result)
            ts = value->ts;  // TODO: rip code_type
            
        return true;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (kwargs.size() > 0) {
            std::cerr << "Labeled statements make no sense!\n";
            return false;
        }

        if (args.size() > 0) {
            for (unsigned i = 0; i < args.size() - 1; i++) {
                Expr *expr = args[i].get();
                std::unique_ptr<Value> v;
                
                if (!check_argument(0, expr, { { "stmt", &VOID_CODE_TS, scope, &v } })) {
                    std::cerr << "Statement error: " << expr->token << "\n";
                    return false;
                }
                
                v->escape_statement_variables();
                
                if (!add_statement(v.release(), false))
                    return false;
            }
            
            Expr *expr = args.back().get();
            std::unique_ptr<Value> v;
        
            if (!check_argument(0, expr, { { "stmt", context, scope, &v } })) {
                std::cerr << "Statement error: " << expr->token << "\n";
                return false;
            }
        
            if (!add_statement(v.release(), true))
                return false;
        }

        return true;
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob;
        
        for (unsigned i = 0; i < statements.size() - 1; i++)
            clob = clob | statements[i]->precompile();

        clob = clob | statements.back()->precompile(preferred);
            
        return clob;
    }

    virtual Storage compile(X64 *x64) {
        for (unsigned i = 0; i < statements.size() - 1; i++) {
            Token &token = statements[i]->token;
            
            // Poor man's debug info
            //Label l;
            //std::stringstream ss;
            //ss << "line." << token.row;
            //x64->code_label_local(l, ss.str());
            
            // Dwarves debug info
            x64->add_line_info(token.file_index, token.row);
            
            statements[i]->compile_and_store(x64, Storage());

            x64->op(NOP);  // For readability
            //std::cerr << "XXX statement\n";
        }

        Token &token = statements.back()->token;
        x64->add_line_info(token.file_index, token.row);
        
        return statements.back()->compile(x64);
    }
};


class DeclarationValue: public Value {
public:
    std::string name;
    Declaration *decl;
    Variable *var;
    std::unique_ptr<Value> value;
    TypeSpec *context;
    
    DeclarationValue(std::string n, TypeSpec *c = NULL)
        :Value(VOID_TS) {
        name = n;
        context = c;  // This may have a limited lifetime!
        decl = NULL;
        var = NULL;
    }

    virtual std::string get_name() {
        return name;
    }

    virtual Declaration *get_decl() {
        return decl;
    }

    virtual Variable *get_var() {
        return var;
    }

    virtual void fix_bare(TypeSpec var_ts, Scope *scope) {
        // This must be called after/instead of check
        var = new Variable(name, var_ts.lvalue());
        decl = var;
        
        scope->add(decl);
        
        ts = var->alloc_ts.reprefix(lvalue_type, uninitialized_type);
    }

    virtual bool associate(Declaration *decl, std::string name) {
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

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        Expr *expr = NULL;
        
        ExprInfos eis = {
            { "", &expr }
        };
        
        if (!check_exprs(args, kwargs, eis)) {
            std::cerr << "Whacky declaration!\n";
            return false;
        }

        std::cerr << "Trying to declare " << name << "\n";

        if (!expr) {
            if (!context) {
                // Must call fix_bare later to add the real declaration
                ts = WHATEVER_UNINITIALIZED_TS;  // dummy type, just to be Uninitialized
                return true;
            }

            if ((*context)[0] == dvalue_type) {
                ts = *context;
                TypeSpec var_ts = ts.unprefix(dvalue_type);
                var = new RetroVariable(name, var_ts);
            
                decl = var;
                scope->add(decl);
            
                return true;
            }

            std::cerr << "Bare declaration is not allowed in this context!\n";
            return false;
        }

        Value *v = typize(expr, scope, context);  // This is why arg shouldn't be a pivot
        
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

    virtual bool define_data() {
        if (!decl) {
            std::cerr << "Bare declaration was not fixed!\n";
            throw INTERNAL_ERROR;
        }
            
        if (value)
            return value->define_data();
        else
            return true;
    }

    virtual bool define_code() {
        if (value)
            return value->define_code();
        else
            return true;
    }

    virtual Regs precompile(Regs preferred) {
        if (value)
            return value->precompile(preferred);
        else
            return Regs();
    }
    
    virtual Storage compile(X64 *x64) {
        // value may be unset for retro variables
        Storage s = (value ? value->compile(x64) : Storage());
        
        // just to be sure we don't have something nasty here
        if (s.where != NOWHERE)
            throw INTERNAL_ERROR;
            
        // var is unset for anything nonvariable
        return var ? var->get_local_storage() : Storage();
    }
    
    virtual void escape_statement_variables() {
        Scope *s = decl->outer_scope;
        s->enter();
        s->remove(decl);
        s->leave();
        s->outer_scope->add(decl);
        //std::cerr << "Escaped variable " << name << " from " << s << " to " << s->outer_scope << "\n";
    }
};




class CreateValue: public GenericOperationValue {
public:
    CreateValue(Value *l, TypeMatch &tm)
        :GenericOperationValue(CREATE, tm[1], tm[1].prefix(lvalue_type), l) {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        // CreateValue may operate on uninitialized member variables, or newly
        // declared local variables. The latter case needs some extra care.
        // Since the declared variable will be initialized in the final step, we
        // should make it the last declaration in this scope, despite the syntax
        // which puts it at the beginning of the initialization expression.
        // This way it can also be taken to its parent scope if necessary.
        
        DeclarationValue *dv = ptr_cast<DeclarationValue>(left.get());
        Declaration *d = NULL;
        
        if (dv) {
            d = declaration_get_decl(dv);
            
            if (d)
                scope->remove(d);  // care
            else if (dv->ts == WHATEVER_UNINITIALIZED_TS)
                arg_ts = ANY_TS;  // bare
            else
                throw INTERNAL_ERROR;
        }
        
        if (!GenericOperationValue::check(args, kwargs, scope))
            return false;
            
        if (dv) {
            if (d)
                scope->add(d);  // care
            else if (dv->ts == WHATEVER_UNINITIALIZED_TS) {
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

    virtual void use(Value *r) {
        DeclarationValue *dv = ptr_cast<DeclarationValue>(left.get());
        
        if (!declaration_get_decl(dv))  // make sure no unfixed bare declarations
            throw INTERNAL_ERROR;
            
        right.reset(r);
    }
    
    virtual Declaration *get_decl() {
        DeclarationValue *dv = ptr_cast<DeclarationValue>(left.get());

        return (dv ? declaration_get_decl(dv) : NULL);
    }
    /*
    virtual Regs precompile(Regs preferred) {
        return left->precompile(preferred) | right->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        Storage ls = left->compile(x64);
        
        // TODO: check that it can't be clobbered!
        if (ls.where != MEMORY)
            throw INTERNAL_ERROR;
            
        Storage rs = right->compile(x64);
        
        //Identifier *i = ptr_cast<Identifier>(get_decl());
        //std::cerr << "XXX CreateValue " << (i ? i->name : "?") << " from " << rs << "\n";
        
        arg_ts.create(rs, ls, x64);

        DeclarationValue *dv = ptr_cast<DeclarationValue>(left.get());
        if (!dv)
            std::cerr << "XXX member init " << arg_ts << " from " << rs << " to " << ls << "\n";
        
        return ls;
    }
    */
    virtual void escape_statement_variables() {
        DeclarationValue *dv = ptr_cast<DeclarationValue>(left.get());
        
        if (dv)
            left->escape_statement_variables();
    }
};

