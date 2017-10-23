
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
        return clob.add(reg);
    }

    virtual Storage compile(X64 *x64) {
        x64->unwind->push(this);
        Storage s = value->compile(x64);
        x64->unwind->pop(this);
        
        // Can't let the result be passed as a MEMORY storage, because it may
        // point to a local variable that we're about to destroy. So grab that
        // value while we can. 
        if (s.where == MEMORY) {
            switch (value->ts.rvalue().where(false)) {
            case REGISTER:
                value->ts.store(s, Storage(REGISTER, reg), x64);
                s = Storage(REGISTER, reg);
                break;
            case STACK:
                value->ts.store(s, Storage(STACK), x64);
                s = Storage(STACK);
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
};


class DataBlockValue: public Value {
public:
    DataScope *scope;
    std::vector<std::unique_ptr<Value>> statements;

    DataBlockValue(DataScope *s)
        :Value(VOID_TS) {
        scope = s;
        
        if (!s->is_pure())
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
            //Value *value;
            
            for (unsigned i = 0; i < args.size() - 1; i++) {
                std::unique_ptr<Value> v;
                
                if (!check_argument(0, args[i].get(), { { "stmt", &VOID_CODE_TS, scope, &v } }))
                    return false;
                
                //value = typize(args[i].get(), scope, &VOID_CODE_TS);
            
                CodeScopeValue *csv = dynamic_cast<CodeScopeValue *>(v.get());
                if (!csv)
                    throw INTERNAL_ERROR;

                Value *st = csv->value.get();
                
                st = peek_void_conversion_value(st);
                
                DeclarationValue *dv = declaration_value_cast(st);
            
                if (dv) {
                    Declaration *decl = declaration_get_decl(dv);
                    decl->outer_scope->remove(decl);
                    scope->add(decl);
                    
                    Identifier *id = dynamic_cast<Identifier *>(decl);
                    if (id)
                        std::cerr << "XXX Escaped " << id->name << ".\n";
                    else
                        std::cerr << "XXX Escaped something.\n";
                }
            
                // This matters, because makes the expression Void before putting it
                // in CodeValue, which would be sensitive about MEMORY return values,
                // and push them unnecessarily.
                // Using typematch would be nicer, but we can't pass the escape_last
                // flag to CodeValue...
            
                //value = make_void_conversion_value(value);
                //value = make_code_scope_value(value, escape);
                add_statement(v.release(), false);
            }
        
            std::unique_ptr<Value> v;
        
            if (!check_argument(0, args.back().get(), { { "stmt", context, scope, &v } }))
                return false;
        
            //value = typize(args.back().get(), scope, context);
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
    
    virtual Value *lookup_inner(std::string name, TypeMatch &match) {
        // TODO: this is not nice!
        if (statements.size() > 0)
            return statements.back()->lookup_inner(name, match);
        else
            return NULL;
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

    virtual bool use(Value *v, Scope *scope) {
        value.reset(v);

        if (!scope->is_pure()) {
            // Allow declaration by value or type
            var = value->declare_impure(name, scope);
        
            if (var) {
                decl = var;
                scope->add(decl);
                ts = var->var_ts;
                return true;
            }
        }
        
        // Allow declaration by type or metatype
        decl = value->declare_pure(name, scope);
        
        if (decl) {
            scope->add(decl);
            return true;
        }
        
        std::cerr << "Impure declaration not allowed in a pure context: " << token << "!\n";
        return false;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() > 1 || kwargs.size() != 0) {
            std::cerr << "Whacky declaration!\n";
            return false;
        }

        std::cerr << "Trying to declare " << name << "\n";
        auto pos = name.find(".");
        
        if (pos != std::string::npos) {
            std::string scope_name = name.substr(0, pos);
            name = name.substr(pos + 1);
            Scope *inner_scope = NULL;
            
            for (auto &d : scope->contents) {
                ImplementationType *it = dynamic_cast<ImplementationType *>(d.get());
                
                if (it && it->name == scope_name) {
                    inner_scope = it->inner_scope;
                    break;
                }
            }
            
            if (!inner_scope) {
                std::cerr << "Invalid scope name: " << scope_name << "!\n";
                return false;
            }
            
            scope = inner_scope;
        }

        Value *v;
        
        if (args.size() == 0) {
            if (!context) {
                std::cerr << "Can't declare implicitly without type context!\n";
                return false;
            }
            
            if ((*context)[0] != lvalue_type) {
                std::cerr << "Can't declare implicitly without lvalue type context!\n";
                return false;
            }
            
            v = make_type_value((*context).rvalue().prefix(type_type));
        }
        else
            v = typize(args[0].get(), scope, context);  // This is why arg shouldn't be a pivot
        
        return use(v, scope);
    }

    virtual bool complete_definition() {
        return value->complete_definition();
    }

    virtual Regs precompile(Regs preferred) {
        return value->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        if (var) {
            Storage s = value->compile(x64);  // may be NOWHERE, then we'll clear initialize
            Storage fn_storage(MEMORY, Address(RBP, 0));  // this must be a local variable
            Storage t = var->get_storage(fn_storage);

            // Use the value to initialize the variable, then return the variable
            var->var_ts.create(s, t, x64);
            
            return t;
        }
        else {
            value->compile_and_store(x64, Storage());
            return Storage();
        }
    }
    
};


DeclarationValue *declaration_value_cast(Value *value) {
    return dynamic_cast<DeclarationValue *>(value);
}


std::string declaration_get_name(DeclarationValue *dv) {
    return dv->get_name();
}


Declaration *declaration_get_decl(DeclarationValue *dv) {
    return dv->get_decl();
}


Variable *declaration_get_var(DeclarationValue *dv) {
    return dv->get_var();
}
