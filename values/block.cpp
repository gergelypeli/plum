
class BlockValue: public Value {
public:
    std::vector<std::unique_ptr<Value>> statements;

    BlockValue()
        :Value(VOID_TS) {  // Will be overridden
    }

    virtual void add_statement(Value *value) {
        statements.push_back(std::unique_ptr<Value>(value));
        ts = value->ts;  // TODO: rip code_type
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() < 2) {
            std::cerr << "Weird, I thought tuples contain at least two expressions!\n";
            throw INTERNAL_ERROR;
        }

        if (kwargs.size() > 0) {
            std::cerr << "Labeled statements make no sense!\n";
            throw TYPE_ERROR;
        }
        
        for (auto &arg : args) {
            Value *value = typize(arg.get(), scope);
            
            if (!declaration_value_cast(value))
                value = make_code_value(value);
                
            add_statement(value);
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


class CodeValue: public Value {
public:
    std::unique_ptr<Value> value;
    CodeScope *code_scope;
    Register reg;

    CodeValue(Value *v)
        :Value(v->ts.rvalue().prefix(code_type)) {
        value.reset(v);
        code_scope = new CodeScope();
        value->marker.scope->intrude(value->marker.last, code_scope);
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
        
        code_scope->finalize_scope(Storage(MEMORY, Address(RBP, 0)), x64);
        return s;
    }
};


class DeclarationValue: public Value {
public:
    std::string name;
    Variable *var;
    std::unique_ptr<Value> value;
    
    DeclarationValue(std::string n)
        :Value(VOID_TS) {
        name = n;
        var = NULL;
    }

    virtual std::string get_name() {
        return name;
    }

    virtual Variable *use(Value *v, Scope *scope) {
        value.reset(v);
        
        var = value->declare(name, scope);
        
        if (var)
            ts = var->var_ts;
            
        return var;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() > 1 || kwargs.size() != 0) {
            std::cerr << "Whacky declaration!\n";
            return false;
        }
        
        if (args.size() == 0) {
            ts = UNCERTAIN_TS;
            return true;
        }
        
        Value *v = typize(args[0].get(), scope);
        use(v, scope);

        return true;
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
            //std::cerr << "XXX " << ts << " " << v->var_ts << " " << s << "\n";
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

