
class CodeScopeValue: public Value {
public:
    std::unique_ptr<Value> value;
    CodeScope *code_scope;
    Register reg;
    bool may_be_aborted;

    CodeScopeValue(Value *v, CodeScope *s)
        :Value(v->ts.rvalue()) {
        value.reset(v);
        code_scope = s;
        code_scope->taken();
        may_be_aborted = false;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        throw INTERNAL_ERROR;
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = value->precompile(preferred);
        reg = preferred.get_any();
        return clob | reg;
    }

    virtual Storage compile(X64 *x64) {
        x64->unwind->push(this);
        Storage s = value->compile(x64);
        x64->unwind->pop(this);
        
        // Can't let the result be passed as a MEMORY storage, because it may
        // point to a local variable that we're about to destroy. So grab that
        // value while we can. 
        if (s.where == MEMORY) {
            switch (value->ts.where(AS_VALUE)) {
            case REGISTER:
                s = value->ts.store(s, Storage(REGISTER, reg), x64);
                break;
            case STACK:
                s = value->ts.store(s, Storage(STACK), x64);
                break;
            default:
                throw INTERNAL_ERROR;
            }
        }
        
        code_scope->finalize_contents(x64);

        if (may_be_aborted) {
            Label ok;
            x64->op(CMPB, EXCEPTION_ADDRESS, NO_EXCEPTION);
            x64->op(JE, ok);
    
            x64->unwind->initiate(code_scope, x64);

            x64->code_label(ok);
        }
            
        return s;
    }
    
    virtual Scope *unwind(X64 *x64) {
        may_be_aborted = true;
        return code_scope;  // stop unwinding here, and start destroying scoped variables
    }
    
    virtual void escape_statement_variables() {
        value->escape_statement_variables();
    }
};


class DataBlockValue: public Value {
public:
    Scope *scope;  // Must work with both DataScope and ArgumentScope
    std::vector<std::unique_ptr<Value>> statements;

    DataBlockValue(Scope *s)
        :Value(VOID_TS) {
        scope = s;
        
        if (s->type != DATA_SCOPE && s->type != ARGUMENT_SCOPE)
            throw INTERNAL_ERROR;
    }

    virtual bool check_statement(Expr *expr) {
        bool is_allowed = (expr->type == Expr::DECLARATION);

        if (!is_allowed) {
            std::cerr << "Impure statement not allowed in a pure context: " << expr->token << "!\n";
            return false;
        }

        Value *value = typize(expr, scope);
        
        statements.push_back(std::unique_ptr<Value>(value));
        return true;
    }

    virtual bool complete_definition() {
        for (unsigned i = 0; i < statements.size(); i++)
            if (!statements[i]->complete_definition())
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
        :Value(VOID_TS) {  // Will be overridden
        context = c;
    }

    virtual void add_statement(Value *value, bool result = false) {
        statements.push_back(std::unique_ptr<Value>(value));
        
        value->complete_definition();
        
        if (result)
            ts = value->ts;  // TODO: rip code_type
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        //if (args.size() < 2) {
        //    std::cerr << "Weird, I thought tuples contain at least two expressions!\n";
        //    throw INTERNAL_ERROR;
        //}

        if (kwargs.size() > 0) {
            std::cerr << "Labeled statements make no sense!\n";
            return false;
        }

        if (args.size() > 0) {
            for (unsigned i = 0; i < args.size() - 1; i++) {
                std::unique_ptr<Value> v;
                
                if (!check_argument(0, args[i].get(), { { "stmt", &VOID_CODE_TS, scope, &v } })) {
                    std::cerr << "Statement error: " << args[i]->token << "\n";
                    return false;
                }
                
                /*
                CodeScopeValue *csv = ptr_cast<CodeScopeValue>(v.get());
                if (!csv)
                    throw INTERNAL_ERROR;

                Value *st = csv->value.get();
                
                st = peek_void_conversion_value(st);
                
                CreateValue *cv = ptr_cast<CreateValue>(st);
                
                DeclarationValue *dv = ptr_cast<DeclarationValue>(cv ? cv->left.get() : st);
                
                if (dv) {
                    Declaration *decl = declaration_get_decl(dv);
                    decl->outer_scope->remove(decl);
                    scope->add(decl);
                }
                */
                
                v->escape_statement_variables();
                
                add_statement(v.release(), false);
            }
        
            std::unique_ptr<Value> v;
        
            if (!check_argument(0, args.back().get(), { { "stmt", context, scope, &v } })) {
                std::cerr << "Statement error: " << args.back()->token << "\n";
                return false;
            }
        
            add_statement(v.release(), true);
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
            statements[i]->compile_and_store(x64, Storage());
            x64->op(NOP);  // For readability
        }
        
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
        var = new Variable(name, scope->pivot_type_hint(), var_ts.lvalue());
        decl = var;
        
        scope->add(decl);
        
        ts = var->alloc_ts.reprefix(lvalue_type, uninitialized_type);
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() > 1 || kwargs.size() != 0) {
            std::cerr << "Whacky declaration!\n";
            return false;
        }

        std::cerr << "Trying to declare " << name << "\n";

        if (args.size() == 0) {
            if (!context) {
                // Must call fix_bare later to add the real declaration
                ts = UNIT_UNINITIALIZED_TS;  // dummy type, just to be Uninitialized
                return true;
            }

            if ((*context)[0] == dvalue_type) {
                ts = *context;
                TypeSpec var_ts = ts.reprefix(dvalue_type, lvalue_type);
                var = new RetroVariable(name, NO_TS, var_ts);
            
                decl = var;
                scope->add(decl);
            
                return true;
            }

            std::cerr << "Bare declaration is not allowed in this context!\n";
            return false;
        }

        if (!descend_into_explicit_scope(name, scope))  // Modifies both arguments
            return false;

        Value *v = typize(args[0].get(), scope, context);  // This is why arg shouldn't be a pivot
        
        if (!v->ts.is_meta() && !v->ts.is_hyper()) {
            std::cerr << "Not a type used for declaration, but a value of " << v->ts << "!\n";
            return false;
        }
        
        value.reset(v);

        decl = value->declare(name, scope->type);
        
        if (!decl) {
            std::cerr << "Invalid declaration: " << token << "!\n";
            return false;
        }

        scope->add(decl);
        
        if (scope->type == CODE_SCOPE) {
            var = ptr_cast<Variable>(decl);
            
            if (var)
                ts = var->alloc_ts.reprefix(lvalue_type, uninitialized_type);
        }

        return true;
    }

    virtual bool complete_definition() {
        if (!decl) {
            std::cerr << "Bare declaration was not fixed!\n";
            throw INTERNAL_ERROR;
        }
            
        if (value)
            return value->complete_definition();
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
        s->remove(decl);
        s->outer_scope->add(decl);
    }
};




class CreateValue: public GenericValue {
public:
    CreateValue(Value *l, TypeMatch &tm)
        :GenericValue(tm[1], tm[1].prefix(lvalue_type), l) {
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
            else if (dv->ts == UNIT_UNINITIALIZED_TS)
                arg_ts = ANY_TS;  // bare
            else
                throw INTERNAL_ERROR;
        }
        
        if (!GenericValue::check(args, kwargs, scope))
            return false;
            
        if (dv) {
            if (d)
                scope->add(d);  // care
            else if (dv->ts == UNIT_UNINITIALIZED_TS) {
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
        
        return ls;
    }
    
    virtual void escape_statement_variables() {
        DeclarationValue *dv = ptr_cast<DeclarationValue>(left.get());
        
        if (dv)
            left->escape_statement_variables();
    }
};

