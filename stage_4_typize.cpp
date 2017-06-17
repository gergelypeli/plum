
// Stage 4

class Declaration;
class Type;
class Scope;
class FunctionScope;
class Variable;
class Function;

typedef std::vector<Type *> TypeSpec;

Type *type_type = NULL;
Type *void_type = NULL;
Type *function_type = NULL;
Type *integer_type = NULL;

TypeSpec TS_VOID;
std::string print_typespec(TypeSpec &ts);

typedef std::vector<std::unique_ptr<Expr>> Args;
typedef std::map<std::string, std::unique_ptr<Expr>> Kwargs;

class Value;
class FunctionDefinitionValue;

TypeSpec get_typespec(Value *v);
Value *typize(Expr *expr, Scope *scope);

Value *make_variable_value(Variable *decl);
Value *make_function_value(Function *decl);
Value *make_type_value(TypeSpec ts);
Value *make_block_value();
Value *make_function_definition_value(Value *ret, Value *head, Value *body, FunctionScope *fn_scope);
Value *make_return_value(Value *v);
Value *make_declaration_value(Declaration *decl, Value *v);
Value *make_number_value(std::string text);


// Declaration

class Declaration {
public:
    Scope *outer;

    virtual Value *match(std::string name, Value *pivot) {
        return NULL;
    }
};


class Scope: virtual public Declaration {
public:
    std::vector<std::unique_ptr<Declaration>> contents;
    
    Scope()
        :Declaration() {
    }
    
    virtual void add(Declaration *decl) {
        contents.push_back(std::unique_ptr<Declaration>(decl));
        decl->outer = this;
    }
    
    virtual void remove(Declaration *decl) {
        if (contents.back().get() == decl) {
            contents.back().release();
            contents.pop_back();
            decl->outer = NULL;
        }
        else
            throw Error("Not the last decl to remove!");
    }
    
    virtual unsigned get_length() {
        return contents.size();
    }
    
    virtual Declaration *get_declaration(unsigned i) {
        return contents[i].get();
    }
    
    virtual Value *lookup(std::string name, Value *pivot) {
        for (auto &decl : contents) {
            Value *v = decl->match(name, pivot);
            
            if (v)
                return v;
        }

        return NULL;
    }
};


class FunctionScope: public Scope {
public:
    Scope *ret_scope;
    Scope *head_scope;

    FunctionScope(Scope *rs, Scope *hs)
        :Scope() {
        ret_scope = rs;
        head_scope = hs;
    }
    
    virtual Value *lookup(std::string name, Value *pivot) {
        Value *v = Scope::lookup(name, pivot);
        if (v)
            return v;
        
        v = head_scope->lookup(name, pivot);
        if (v)
            return v;
            
        v = ret_scope->lookup(name, pivot);
        if (v)
            return v;
            
        return NULL;
    }
};


class Variable: public Declaration {
public:
    std::string name;
    TypeSpec pivot_ts;
    TypeSpec var_ts;
    
    Variable(std::string name, TypeSpec pts, TypeSpec vts) {
        this->name = name;
        this->pivot_ts = pts;
        this->var_ts = vts;
    }
    
    virtual Value *match(std::string name, Value *pivot) {
        TypeSpec pts = get_typespec(pivot);
        
        if (name == this->name && pts == pivot_ts) {
            Value *v = make_variable_value(this);
            return v;
        }
        else
            return NULL;
    }
};


class Function: public Declaration {
public:
    std::string name;
    TypeSpec pivot_ts;
    TypeSpec ret_ts;
    std::vector<TypeSpec> arg_ts;

    Function(std::string name, TypeSpec pts, TypeSpec rts, std::vector<TypeSpec> ats) {
        this->name = name;
        this->pivot_ts = pts;
        ret_ts = rts;
        arg_ts = ats;
    }

    virtual Value *match(std::string name, Value *pivot) {
        TypeSpec pts = get_typespec(pivot);
        //std::cout << "XXX Function.match " << name << " " << print_typespec(ts) << "\n";

        if (name == this->name && pts == pivot_ts) {  // TODO: convert
            Value *v = make_function_value(this);
            return v;
        }
        else
            return NULL;
    }

    virtual TypeSpec get_return_typespec() {
        return ret_ts;
    }
    
    virtual unsigned get_argument_count() {
        return arg_ts.size();
    }
    
    virtual TypeSpec get_argument_typespec(unsigned i) {
        std::cout << "Returning typespec for argument " << i << ".\n";
        return arg_ts[i];
    }

    virtual int get_argument_index(std::string keyword) {  // FIXME
        return -1;
    }
};


class Type: virtual public Declaration {
public:
    std::string name;
    unsigned parameter_count;
    
    Type(std::string n, unsigned pc) {
        name = n;
        parameter_count = pc;
    }
    
    virtual unsigned get_parameter_count() {
        return parameter_count;
    }
    
    virtual int get_length() {
        return 1;
    }
    
    virtual std::string get_label() {
        return "";
    }
    
    virtual Value *match(std::string name, Value *pivot) {
        if (name == this->name && pivot == NULL && parameter_count == 0) {
            TypeSpec ts;
            ts.push_back(type_type);
            ts.push_back(this);
            
            return make_type_value(ts);
        }
        else
            return NULL;
    }
};


int step_typespec(TypeSpec &ts, int i) {
    Type *t = ts[i];
    i++;
    
    for (unsigned j = 0; j < t->get_parameter_count(); j++)
        i = step_typespec(ts, i);
        
    return i;
}


std::string print_typespec(TypeSpec &ts) {
    std::stringstream s;
    s << "[";
    
    bool start = true;
    
    for (auto type : ts) {
        if (start)
            start = false;
        else
            s << ",";
            
        s << type->name;
    }
    
    s << "]";
    
    return s.str();
}


// Value

class Value {
public:
    TypeSpec ts;
    
    Value() {
    }

    virtual void set_ts(TypeSpec ts) {
        this->ts = ts;
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        return (args.size() == 0 && kwargs.size() == 0);
    }
};


class BlockValue: public Value {
public:
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
};


class VariableValue: public Value {
public:
    Variable *variable;
    
    VariableValue(Variable *v) {
        variable = v;
        set_ts(v->var_ts);
    }
};


class FunctionDefinitionValue: public Value {
public:
    Value *ret;
    Value *head;
    Value *body;
    FunctionScope *fn_scope;
    
    FunctionDefinitionValue(Value *r, Value *h, Value *b, FunctionScope *f) {
        ret = r;
        head = h;
        body = b;
        fn_scope = f;
    }
};


class FunctionValue: public Value {
public:
    Function *function;
    std::vector<std::unique_ptr<Value>> items;  // FIXME
    
    FunctionValue(Function *f) {
        function = f;

        set_ts(function->get_return_typespec());
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        unsigned n = function->get_argument_count();
        unsigned supplied = 0;
        
        for (unsigned i = 0; i < args.size(); i++) {
            if (i >= n)
                return false;
                
            Expr *e = args[i].get();
            Value *v = typize(e, scope);
            TypeSpec var_ts = function->get_argument_typespec(i);
            
            if (v->ts != var_ts) {
                std::string vts = print_typespec(v->ts);
                std::string varts = print_typespec(var_ts);
                throw Error("Argument type mismatch, %s is not %s!", vts.c_str(), varts.c_str());
            }
            
            supplied |= 1 << i;
            items.push_back(std::unique_ptr<Value>(v));
        }
        
        if (kwargs.size() > 0)
            return false;
        
        return true;
    }
};


class TypeValue: public Value {
public:
    TypeValue(TypeSpec ts) {
        set_ts(ts);
    }
};


class ReturnValue: public Value {
public:
    Value *value;
    
    ReturnValue(Value *v) {
        value = v;
    }
};


class DeclarationValue: public Value {
public:
    Declaration *decl;
    Value *value;
    
    DeclarationValue(Declaration *d, Value *v) {
        decl = d;
        value = v;
    }
};


class NumberValue: public Value {
public:
    std::string text;
    
    NumberValue(std::string t) {
        text = t;
        ts.push_back(integer_type);
    }
};


TypeSpec get_typespec(Value *v) {
    return v ? v->ts : TS_VOID;
}


Value *make_variable_value(Variable *decl) {
    return new VariableValue(decl);
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


Value *make_return_value(Value *v) {
    return new ReturnValue(v);
}


Value *make_declaration_value(Declaration *d, Value *v) {
    return new DeclarationValue(d, v);
}


Value *make_number_value(std::string text) {
    return new NumberValue(text);
}




Value *typize(Expr *expr, Scope *scope) {
    if (expr->type == OPEN) {
        //Scope *inner = new Scope(scope);
        //scope->add(inner);
        
        //Match m(inner);
        if (expr->pivot)
            throw Error("An OPEN had a pivot argument!");

        Value *v = make_block_value();
        v->check(expr->args, expr->kwargs, scope);
            
        return v;
    }
    else if (expr->type == STATEMENT) {
        if (expr->text == "function") {
            Scope *ret_scope = new Scope();
            scope->add(ret_scope);
            Expr *r = expr->pivot.get();
            Value *ret = r ? typize(r, ret_scope) : NULL;
        
            Scope *head_scope = new Scope();
            scope->add(head_scope);
            Expr *h = expr->kwargs["from"].get();
            Value *head = h ? typize(h, head_scope) : NULL;
            
            FunctionScope *body_scope = new FunctionScope(ret_scope, head_scope);
            scope->add(body_scope);
            Expr *b = expr->kwargs["as"].get();
            Value *body = typize(b, body_scope);
            
            TypeSpec fn_ts;
                
            if (ret) {
                if (ret->ts[0] != type_type)
                    throw Error("Function return type is not a type!");
                    
                fn_ts = ret->ts;
                fn_ts[0] = function_type;
            }
            else {
                fn_ts.push_back(function_type);
                fn_ts.push_back(void_type);
            }

            std::cout << "Function ts " << print_typespec(fn_ts) << "\n";
            
            Value *v = make_function_definition_value(ret, head, body, body_scope);
            v->set_ts(fn_ts);
            
            return v;
        }
        else if (expr->text == "return") {
            Expr *r = expr->pivot.get();
            Value *ret = r ? typize(r, scope) : NULL;  // TODO: inner scope?

            Value *v = make_return_value(ret);

            return v;
        }
        else
            throw Error("Unknown statement!");
    }
    else if (expr->type == DECLARATION) {
        std::string name = expr->text;
        std::cout << "Declaring " << name << ".\n";
        
        Value *d = typize(expr->pivot.get(), scope);
        
        if (d->ts.size() == 0)
            throw Error("Declaration needs a type!");

        Declaration *decl;

        if (d->ts[0] == type_type) {
            TypeSpec ts;
            for (unsigned i = 1; i < d->ts.size(); i++)
                ts.push_back(d->ts[i]);
        
            decl = new Variable(name, TS_VOID, ts);
        }
        else if (d->ts[0] == function_type) {
            //TypeSpec ts;
            //for (unsigned i = 1; i < d->ts.size(); i++)
            //    ts.push_back(d->ts[i]);

            FunctionDefinitionValue *fdv = dynamic_cast<FunctionDefinitionValue *>(d);

            TypeSpec ret_ts;
            if (fdv->ret) {
                for (unsigned i = 1; i < fdv->ret->ts.size(); i++)
                    ret_ts.push_back(fdv->ret->ts[i]);
            }
            else
                ret_ts.push_back(void_type);
                
            std::cout << "It's a function with return type " << print_typespec(ret_ts) << ".\n";

            Scope *head_scope = fdv->fn_scope->head_scope;
            unsigned n = head_scope->get_length();

            std::vector<TypeSpec> arg_ts;
            for (unsigned i = 0; i < n; i++) {
                Declaration *xd = head_scope->get_declaration(i);
                Variable *vd = dynamic_cast<Variable *>(xd);
                arg_ts.push_back(vd->var_ts);
            }
            
            //decl = new UserFunction(name, TS_VOID, fdv);
            decl = new Function(name, TS_VOID, ret_ts, arg_ts);
        }
            
        scope->add(decl);
    
        Value *v = make_declaration_value(decl, d);
        
        return v;
    }
    else if (expr->type == IDENTIFIER) {
        std::string name = expr->text;
        Value *p = expr->pivot ? typize(expr->pivot.get(), scope) : NULL;
        TypeSpec pts = p ? p->ts : TS_VOID;
        
        std::cout << "Looking up " << name << " with pivot type " << print_typespec(pts) << "\n";

        for (Scope *s = scope; s; s = s->outer) {
            std::cout << "Trying a scope...\n";
            
            Value *v = s->lookup(expr->text, p);
        
            if (v) {
                bool ok = v->check(expr->args, expr->kwargs, scope);
                
                if (!ok)
                    throw Error("Argument problem!");

                std::cout << "Found as a " << print_typespec(v->ts) << ".\n";
                return v;
            }
        }
        
        throw Error("No match for %s!", name.c_str());
    }
    else if (expr->type == NUMBER) {
        return make_number_value(expr->text);  // TODO
    }
    else
        throw Error("Can't typize this now %d!", expr->type);
}
