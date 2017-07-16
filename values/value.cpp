
// Values

class Value {
public:
    TypeSpec ts;
    Token token;
    
    Value(TypeSpec t)
        :ts(t) {
    }

    virtual Value *set_token(Token t) {
        token = t;
        return this;
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *) {
        return (args.size() == 0 && kwargs.size() == 0);
    }
    
    virtual Regs precompile(Regs) {
        std::cerr << "This Value shouldn't have been precompiled!\n";
        throw INTERNAL_ERROR;
    }

    virtual Storage compile(X64 *) {
        std::cerr << "This Value shouldn't have been compiled!\n";
        throw INTERNAL_ERROR;
    }
    
    virtual void compile_and_store(X64 *x64, Storage t) {
        Storage s = compile(x64);
        //std::cerr << "Compiled and storing a " << ts << " from " << s << " to " << t << ".\n";
        ts.store(s, t, x64);
    }
};


class TypeValue: public Value {
public:
    TypeValue(TypeSpec ts)
        :Value(ts) {
    }
    
    virtual Regs precompile(Regs regs) {
        return regs;
    }
    
    virtual Storage compile(X64 *) {
        return Storage();
    }
};


class ConvertedValue: public Value {
public:
    std::unique_ptr<Value> orig;
    
    ConvertedValue(TypeSpec t, Value *o)
        :Value(t) {
        orig.reset(o);
    }
    
    virtual Regs precompile(Regs regs) {
        return regs;  // TODO: allow the conversion to allocate registers?
    }
    
    virtual Storage compile(X64 *x64) {
        Storage s = orig->compile(x64);
        Storage t = orig->ts.convert(ts, s, x64);
        return t;
    }
};


class VariableValue: public Value {
public:
    Variable *variable;
    std::unique_ptr<Value> pivot;
    
    VariableValue(Variable *v, Value *p)
        :Value(v->var_ts) {
        variable = v;
        pivot.reset(p);
    }
    
    virtual Regs precompile(Regs regs) {
        return regs;
    }
    
    virtual Storage compile(X64 *x64) {
        Storage s;
        
        if (pivot)
            s = pivot->compile(x64);
        else
            s = variable->outer->get_storage();
        
        return variable->get_storage(s);
    }
};


class BlockValue: public Value {
public:
    // FIXME: must keep kwarg order!
    std::vector<std::unique_ptr<Value>> items;
    std::map<std::string, std::unique_ptr<Value>> kwitems;

    BlockValue()
        :Value(VOID_TS) {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        for (auto &arg : args)
            items.push_back(std::unique_ptr<Value>(typize(arg.get(), scope)));
            
        for (auto &kv : kwargs)
            kwitems.insert(decltype(kwitems)::value_type(kv.first, typize(kv.second.get(), scope)));
            
        if (items.size() == 1 && kwitems.size() == 0)
            ts = items[0]->ts;  // maybe we should get this in the constructor instead?

        std::cerr << "BlockValue " << token << " ts: " << ts << "\n";
        return true;
    }

    virtual Regs precompile(Regs regs) {
        if (items.size() == 1 && kwitems.size() == 0)
            return items[0]->precompile(regs);
            
        for (auto &item : items)
            item->precompile(regs);
            
        for (auto &kv : kwitems)
            kv.second->precompile(regs);
            
        return regs;
        
    }

    virtual Storage compile(X64 *x64) {
        // FIXME: this works for a bunch of declarations, but not in general

        if (items.size() == 1 && kwitems.size() == 0)
            return items[0]->compile(x64);
            
        for (auto &item : items)
            item->compile_and_store(x64, Storage());
            
        for (auto &kv : kwitems)
            kv.second->compile_and_store(x64, Storage());
            
        return Storage();
    }
};


#include "function.cpp"
#include "literal.cpp"
#include "integer.cpp"
#include "boolean.cpp"


class DeclarationValue: public Value {
public:
    std::string name;
    Declaration *decl;
    std::unique_ptr<Value> value;
    
    DeclarationValue(std::string n)
        :Value(VOID_TS) {
        name = n;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() != 1 || kwargs.size() != 0) {
            std::cerr << "Whacky declaration!\n";
            return false;
        }
        
        value.reset(typize(args[0].get(), scope));
        
        if (value->ts.size() == 0) {
            std::cerr << "Declaration needs something " << args[0]->token << "!\n";
            return false;
        }
        else if (value->ts[0] == function_type) {
            FunctionDefinitionValue *fdv = dynamic_cast<FunctionDefinitionValue *>(value.get());
            TypeSpec ret_ts;
            std::vector<TypeSpec> arg_tss;
            std::vector<std::string> arg_names;

            for (unsigned i = 1; i < fdv->ts.size(); i++)
                ret_ts.push_back(fdv->ts[i]);

            std::cerr << "It's a function with return type " << ret_ts << ".\n";

            Scope *head_scope = fdv->fn_scope->head_scope;
            unsigned n = head_scope->get_length();

            for (unsigned i = 0; i < n; i++) {
                Declaration *xd = head_scope->get_declaration(i);
                Variable *vd = dynamic_cast<Variable *>(xd);
                arg_tss.push_back(vd->var_ts);
                arg_names.push_back(vd->name);
            }

            Function *function = new Function(name, VOID_TS, arg_tss, arg_names, ret_ts);
            fdv->set_function(function);
            decl = function;
        }
        else if (value->ts[0] == type_type) {
            TypeSpec var_ts;
            
            if (!scope->is_readonly())
                var_ts.push_back(lvalue_type);
            
            for (unsigned i = 1; i < value->ts.size(); i++)
                var_ts.push_back(value->ts[i]);
        
            Variable *variable = new Variable(name, VOID_TS, var_ts);
            decl = variable;
        }
        else if (value->ts[0] != void_type) {
            TypeSpec var_ts = scope->is_readonly() ? value->ts : value->ts.lvalue();
            
            Variable *variable = new Variable(name, VOID_TS, var_ts);
            decl = variable;
        }
        else {
            std::cerr << "Now what is this?\n";
            return false;
        }
            
        scope->add(decl);
        return true;
    }
    
    virtual Regs precompile(Regs regs) {
        return value->precompile(regs);
    }
    
    virtual Storage compile(X64 *x64) {
        Variable *v = dynamic_cast<Variable *>(decl);
        
        if (v) {
            Storage s = value->compile(x64);
            Storage t = v->get_storage(v->outer->get_storage());  // local variable
            
            if (s.where == NOWHERE)
                v->var_ts.create(t, x64);
            else
                v->var_ts.store(s, t, x64);
                
            v->outer->set_rollback_declaration(v);
        }
        else
            value->compile_and_store(x64, Storage());
        
        // TODO: eventually this must manage the rollback labels, too.
        return Storage();
    }
};


Value *convertible(TypeSpec to, Value *value) {
    return value ? value->ts.convertible(to, value) : NULL;
}


Value *make_function_head_value(FunctionHeadScope *s) {
    return new FunctionHeadValue(s);
}


Value *make_function_body_value(FunctionBodyScope *s) {
    return new FunctionBodyValue(s);
}


Value *make_function_return_value(Scope *s, Value *v) {
    return new FunctionReturnValue(s, v);
}


Value *make_variable_value(Variable *decl, Value *pivot) {
    return new VariableValue(decl, pivot);
}


Value *make_function_value(Function *decl, Value *pivot) {
    return new FunctionValue(decl, pivot);
}


Value *make_type_value(TypeSpec ts) {
    return new TypeValue(ts);
}


Value *make_block_value() {
    return new BlockValue();
}


Value *make_function_definition_value(TypeSpec fn_ts, Value *ret, Value *head, Value *body, FunctionScope *fn_scope) {
    return new FunctionDefinitionValue(fn_ts, ret, head, body, fn_scope);
}


Value *make_declaration_value(std::string name) {
    return new DeclarationValue(name);
}


Value *make_number_value(std::string text) {
    return new NumberValue(text);
}


Value *make_string_value(std::string text) {
    return new StringValue(text);
}


Value *make_integer_operation_value(NumericOperation o, TypeSpec t, Value *pivot) {
    return new IntegerOperationValue(o, t, pivot);
}


Value *make_boolean_operation_value(NumericOperation o, Value *pivot) {
    return new BooleanOperationValue(o, pivot);
}


Value *make_boolean_if_value(Value *pivot) {
    return new BooleanIfValue(pivot);
}


Value *make_converted_value(TypeSpec ts, Value *orig) {
    return new ConvertedValue(ts, orig);
}
