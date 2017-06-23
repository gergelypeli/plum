
// Values

class Value {
public:
    TypeSpec ts;
    Token token;
    
    Value() {
    }

    virtual void set_ts(TypeSpec ts) {
        this->ts = ts;
    }
    
    virtual Value *set_token(Token t) {
        token = t;
        return this;
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *) {
        return (args.size() == 0 && kwargs.size() == 0);
    }
    
    virtual Storage compile(X64 *) {
        std::cerr << "This Value shouldn't have been compiled!\n";
        throw INTERNAL_ERROR;
    }
};


class BlockValue: public Value {
public:
    // FIXME: must keep kwarg order!
    std::vector<std::unique_ptr<Value>> items;
    std::map<std::string, std::unique_ptr<Value>> kwitems;

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        for (auto &arg : args)
            items.push_back(std::unique_ptr<Value>(typize(arg.get(), scope)));
            
        for (auto &kv : kwargs)
            kwitems.insert(decltype(kwitems)::value_type(kv.first, typize(kv.second.get(), scope)));
            
        if (items.size() == 1 && kwitems.size() == 0)
            set_ts(items[0]->ts);

        return true;
    }
    
    virtual Storage compile(X64 *x64) {
        // FIXME: this works for a bunch of declarations, but not in general
        
        for (auto &item : items)
            item->compile(x64);
            
        for (auto &kv : kwitems)
            kv.second->compile(x64);
            
        return Storage();
    }
};


// Represents the argument list of a function
class FunctionHeadValue: public Value {
public:
    FunctionHeadScope *head_scope;
    
    FunctionHeadValue(FunctionHeadScope *s) {
        head_scope = s;
    }
};


// Represents the local variable list of a function
class FunctionBodyValue: public Value {
public:
    FunctionBodyScope *body_scope;
    
    FunctionBodyValue(FunctionBodyScope *s) {
        body_scope = s;
    }
};


class VariableValue: public Value {
public:
    Variable *variable;
    Value *pivot;
    
    VariableValue(Variable *v, Value *p) {
        variable = v;
        pivot = p;
        set_ts(v->var_ts);
    }
    
    virtual Storage compile(X64 *x64) {
        Storage s = pivot->compile(x64);
        
        switch (s.where) {
        case FRAME: return Storage(FRAME, s.frame_offset + variable->offset);
        default: throw INTERNAL_ERROR;
        }
    }
};


// The value of a :function statement
class FunctionDefinitionValue: public Value {
public:
    Value *ret;
    Value *head;
    Value *body;
    FunctionScope *fn_scope;
    
    Function *function;  // If declared with a name, which is always, for now
        
    FunctionDefinitionValue(Value *r, Value *h, Value *b, FunctionScope *f) {
        ret = r;
        head = h;
        body = b;
        fn_scope = f;
    }
    
    void set_function(Function *f) {
        function = f;
    }
    
    virtual Storage compile(X64 *x64) {
        fn_scope->allocate(x64);
        
        unsigned frame_size = fn_scope->body_scope->size;

        x64->code_label(function->x64_label, 0);
        x64->op(PUSHQ, RBP);
        x64->op(MOVQ, RBP, RSP);
        x64->op(SUBQ, RSP, frame_size);
        
        body->compile(x64);
        
        // TODO: destructors
        x64->code_label(fn_scope->body_scope->get_rollback_label());
        x64->op(ADDQ, RSP, frame_size);
        x64->op(POPQ, RBP);
        x64->op(RET);
        
        return Storage();
    }
};


// The value of calling a function
class FunctionValue: public Value {
public:
    Function *function;
    std::vector<std::unique_ptr<Value>> items;  // FIXME
    
    FunctionValue(Function *f) {
        function = f;

        set_ts(function->get_return_typespec());
        
        for (unsigned i = 0; i < function->get_argument_count(); i++)
            items.push_back(NULL);
    }

    virtual bool check_arg(unsigned i, Value *v) {
        if (i >= items.size()) {
            std::cerr << "Too many arguments!\n";
            return false;
        }
    
        if (items[i]) {
            std::cerr << "Argument " << i << " already supplied!\n";
            return false;
        }
            
        TypeSpec var_ts = function->get_argument_typespec(i);
        
        if (!(v->ts >> var_ts)) {
            std::cerr << "Argument type mismatch, " << v->ts << " is not a " << var_ts << "!\n";
            return false;
        }
        
        items[i] = std::unique_ptr<Value>(v);
        return true;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        for (unsigned i = 0; i < args.size(); i++) {
            Expr *e = args[i].get();
            
            if (!check_arg(i, typize(e, scope)))
                return false;
        }
                
        for (auto &kv : kwargs) {
            unsigned i = function->get_argument_index(kv.first);
            if (i == (unsigned)-1) {
                std::cerr << "No argument named " << kv.first << "!\n";
                return false;
            }
            
            Expr *e = kv.second.get();
            
            if (!check_arg(i, typize(e, scope)))
                return false;
        }

        for (auto &item : items) {
            if (!item) {
                std::cerr << "Not all arguments supplied!\n";
                return false;
            }
        }
        
        return true;
    }
    
    virtual Storage compile(X64 *x64) {
        // TODO: can't handle pivot yet
        TypeSpec ret_ts = function->get_return_typespec();
        unsigned ret_size = measure(ret_ts);
        x64->op(SUBQ, RSP, ret_size);
        
        for (auto &item : items) {
            Storage s = item->compile(x64);
            store(item->ts, s, Storage(STACK), x64);
        }
        
        x64->op(CALL, function->x64_label);
        
        for (int i = items.size() - 1; i >= 0; i--)
            store(items[i]->ts, STACK, Storage(), x64);
            
        return Storage(STACK);
    }
};


class TypeValue: public Value {
public:
    TypeValue(TypeSpec ts) {
        set_ts(ts);
    }
};


class FunctionReturnValue: public Value {
public:
    Scope *scope;
    Declaration *rollback_declaration;
    FunctionReturnScope *return_scope;
    Value *value;
    
    FunctionReturnValue(Scope *s, Value *v) {
        scope = s;
        value = v;
        
        // This must be saved now, becaus it changes during the typization!
        rollback_declaration = scope->get_rollback_declaration();
        
        FunctionScope *fn_scope = NULL;
        
        // TODO: should be solved by some form of HardScope base class instead of looping
        for (Scope *s = scope; s; s = s->outer) {
            fn_scope = dynamic_cast<FunctionScope *>(s);
            if (fn_scope)
                break;
        }
                
        if (!fn_scope) {
            std::cerr << "Return statement outside of a function!\n";
            throw TYPE_ERROR;
        }
    
        return_scope = fn_scope->return_scope;
    }
    
    virtual Storage compile(X64 *x64) {
        // TODO: destructors
        x64->op(JMP, rollback_declaration->get_rollback_label());
        return Storage();
    }
};


class DeclarationValue: public Value {
public:
    std::string name;
    Declaration *decl;
    Value *value;
    
    DeclarationValue(std::string n) {
        name = n;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() != 1 || kwargs.size() != 0) {
            std::cerr << "Whacky declaration!\n";
            throw TYPE_ERROR;
        }
        
        value = typize(args[0].get(), scope);
        
        if (value->ts.size() == 0) {
            std::cerr << "Declaration needs a type " << args[0]->token << "!\n";
            throw TYPE_ERROR;
        }
        else if (value->ts[0] == type_type) {
            TypeSpec var_ts;
            
            if (!scope->is_readonly())
                var_ts.push_back(lvalue_type);
            
            for (unsigned i = 1; i < value->ts.size(); i++)
                var_ts.push_back(value->ts[i]);
        
            Variable *variable = new Variable(name, TS_VOID, var_ts);
            decl = variable;
        }
        else if (value->ts[0] == function_type) {
            FunctionDefinitionValue *fdv = dynamic_cast<FunctionDefinitionValue *>(value);
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

            Function *function = new Function(name, TS_VOID, arg_tss, arg_names, ret_ts);
            fdv->set_function(function);
            decl = function;
        }
        else {
            std::cerr << "Now what is this?\n";
            throw TYPE_ERROR;
        }
            
        scope->add(decl);
        return true;
    }
    
    virtual Storage compile(X64 *x64) {
        value->compile(x64);
        
        // TODO: eventually this must manage the rollback labels, too.
        return Storage();
    }
};


class NumberValue: public Value {
public:
    int number;
    
    NumberValue(std::string t) {
        number = std::stoi(t);
        ts.push_back(integer_type);
    }
    
    virtual Storage compile(X64 *x64) {
        // TODO: this can only handle 32-bit immediates!
        x64->op(MOVQ, RAX, number);
        
        return Storage(REGISTER);
    }
};


TypeSpec get_typespec(Value *v) {
    return v ? v->ts : TS_VOID;
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


Value *make_function_value(Function *decl) {
    return new FunctionValue(decl);
}


Value *make_type_value(TypeSpec ts) {
    return new TypeValue(ts);
}


Value *make_block_value() {
    return new BlockValue();
}


Value *make_function_definition_value(Value *ret, Value *head, Value *body, FunctionScope *fn_scope) {
    return new FunctionDefinitionValue(ret, head, body, fn_scope);
}


Value *make_declaration_value(std::string name) {
    return new DeclarationValue(name);
}


Value *make_number_value(std::string text) {
    return new NumberValue(text);
}

