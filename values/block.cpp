
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

        if (scope->get_scope_type() != BOGUS_TS) {
            for (auto &arg : args) {
                bool is_allowed = (arg->type == Expr::DECLARATION);

                if (!is_allowed) {
                    error = true;
                    std::cerr << "Impure statement not allowed in a pure context: " << arg->token << "!\n";
                    //continue;
                    throw TYPE_ERROR;
                }

                //try {
                    value = typize(arg.get(), scope, context);
                //} catch (Error) {
                //    error = true;
                    //std::cerr << "Continuing...\n";
                    //continue;
                //    throw;
                //}
                
                add_statement(value, false);
            }
        }
        else {
            for (unsigned i = 0; i < args.size() - 1; i++) {
                bool escape_last = (args[i]->type == Expr::DECLARATION);
                
                //try {
                    value = typize(args[i].get(), scope, &VOID_CODE_TS);
                //} catch (Error) {
                //    error = true;
                    //std::cerr << "Continuing...\n";
                    //continue;
                //    throw;
                //}
                
                value = make_code_value(value, escape_last);
                add_statement(value, false);
            }
            
            //try {
                value = typize(args.back().get(), scope, context);
                add_statement(value, true);
            //} catch (Error) {
            //    error = true;
            //    //std::cerr << "Continuing...\n";
            //    throw;
            //}
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


class CodeValue: public Value {
public:
    std::unique_ptr<Value> value;
    CodeScope *code_scope;
    Register reg;

    CodeValue(Value *v, bool escape_last)
        :Value(v->ts.rvalue()) {
        value.reset(v);
        code_scope = NULL;
        
        CodeScope *intruder = new CodeScope;
        
        if (value->marker.scope->intrude(intruder, value->marker, escape_last))
            code_scope = intruder;
        else
            delete intruder;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        throw INTERNAL_ERROR;
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = value->precompile(preferred);
        reg = preferred.get_gpr();
        return clob.add(reg);
    }

    virtual Storage compile(X64 *x64) {
        // Can't let the result be passed as a MEMORY storage, because it may
        // point to a local variable that we're about to destroy. So grab that
        // value while we can. 
        Storage s = value->compile(x64);
        
        if (s.where == MEMORY) {
            switch (value->ts.where()) {
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
        
        if (code_scope)
            code_scope->finalize_scope(Storage(MEMORY, Address(RBP, 0)), x64);
            
        return s;
    }
};


class DeclarationValue: public Value {
public:
    std::string name;
    Variable *var;
    std::unique_ptr<Value> value;
    TypeSpec *context;
    
    DeclarationValue(std::string n, TypeSpec *c = NULL)
        :Value(VOID_TS) {
        name = n;
        context = c;  // This may have a limited lifetime!
        var = NULL;
    }

    virtual std::string get_name() {
        return name;
    }
    
    virtual Variable *get_var() {
        return var;
    }

    virtual bool use(Value *v, Scope *scope) {
        value.reset(v);

        TypeSpec scope_type = scope->get_scope_type();
        
        if (scope_type == BOGUS_TS) {
            // Allow declaration by value or type
            var = value->declare_impure(name);
        
            if (var) {
                scope->add(var);
                ts = var->var_ts;
                return true;
            }
        }
        
        // Allow declaration by type or metatype
        Declaration *d = value->declare_pure(name, scope_type);
        
        if (d) {
            scope->add(d);
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

    virtual Regs precompile(Regs preferred) {
        return value->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        if (var) {
            // TODO: for references, we now need to first zero out the variable, then
            // the store will do an assignment. This could be simpler.
            Storage fn_storage(MEMORY, Address(RBP, 0));  // this must be a local variable
            Storage t = var->get_storage(fn_storage);
            
            var->var_ts.create(t, x64);

            Storage s = value->compile(x64);
            
            if (s.where != NOWHERE)
                var->var_ts.store(s, t, x64);
                
            s = var->get_storage(Storage(MEMORY, Address(RBP, 0)));
            return s;
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

