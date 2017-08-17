
class DataValue: public Value {
public:
    DataScope *scope;
    std::vector<std::unique_ptr<Value>> statements;

    DataValue(DataScope *s)
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


class BlockValue: public Value {
public:
    std::vector<std::unique_ptr<Value>> statements;
    TypeSpec *context;

    BlockValue(TypeSpec *c)
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
        if (args.size() < 2) {
            std::cerr << "Weird, I thought tuples contain at least two expressions!\n";
            throw INTERNAL_ERROR;
        }

        if (kwargs.size() > 0) {
            std::cerr << "Labeled statements make no sense!\n";
            return false;
        }
        
        Value *value;
        bool error = false;

        if (scope->is_pure()) {
            throw INTERNAL_ERROR;
        }
        else {
            for (unsigned i = 0; i < args.size() - 1; i++) {
                value = typize(args[i].get(), scope, &VOID_CODE_TS);
                
                DeclarationValue *dv = declaration_value_cast(value);
                Declaration *escape = NULL;
                
                if (dv) {
                    escape = declaration_get_decl(dv);
                }
                
                // This matters, because makes the expression Void before putting it
                // in CodeValue, which would be sensitive about MEMORY return values,
                // and push them unnecessarily.
                // Using typematch would be nicer, but we can't pass the escape_last
                // flag to CodeValue...
                
                value = make_void_conversion_value(value);
                value = make_code_value(value, escape);
                add_statement(value, false);
            }
            
            value = typize(args.back().get(), scope, context);
            add_statement(value, true);
        }

        return !error;
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

/*
class CodeUnwind: public Unwind {
public:
    CodeScope *scope;
    
    CodeUnwind(CodeScope *cs)
        :Unwind() {
        scope = cs;
    }
    
    bool compile(Marker marker, X64 *x64) {
        scope->jump_to_content_finalization(marker, x64);
        return true;
    }
};
*/

class CodeValue: public Value {
public:
    std::unique_ptr<Value> value;
    CodeScope *code_scope;
    Register reg;

    CodeValue(Value *v, CodeScope *s)
        :Value(v->ts.rvalue()) {
        value.reset(v);
        code_scope = s;
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
        
        bool may_be_aborted = code_scope->finalize_contents(x64);

        if (may_be_aborted) {
            Label ok;
            x64->op(CMPQ, x64->exception_label, 0);
            x64->op(JE, ok);
    
            x64->unwind->unwind(code_scope->outer_scope, code_scope->previous_declaration, x64);

            x64->code_label(ok);
        }
            
        return s;
    }
    
    virtual bool unwind(X64 *x64) {
        return true;  // stop unwinding here, and start destroying scoped variables
    }
};


class DeclarationValue: public Value {
public:
    std::string name;
    Declaration *decl;
    Variable *var;
    bool var_needs_initialization;
    std::unique_ptr<Value> value;
    TypeSpec *context;
    
    DeclarationValue(std::string n, TypeSpec *c = NULL)
        :Value(VOID_TS) {
        name = n;
        context = c;  // This may have a limited lifetime!
        decl = NULL;
        var = NULL;
        var_needs_initialization = false;
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
            var = value->declare_dirty(name, scope);
            
            if (var) {
                // This is an already added automatic variable used in a constructor.
                // We just need it so it can be escaped from the enclosing CodeScope.
                decl = var;
                ts = var->var_ts;
                var_needs_initialization = false;
                return true;
            }
        
            // Allow declaration by value or type
            var = value->declare_impure(name, scope);
        
            if (var) {
                decl = var;
                scope->add(decl);
                ts = var->var_ts;
                var_needs_initialization = true;
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
        
        Value *v;
        
        if (args.size() == 0) {
            if (!context) {
                std::cerr << "Can't declare implicitly without context!\n";
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

            if (var_needs_initialization) {
                // Use the value to initialize the variable, then return the variable
                var->var_ts.create(s, t, x64);
            }
            else {
                // Drop the value, just return the variable, it's already initialized
                value->ts.store(s, Storage(), x64);
            }
            
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
