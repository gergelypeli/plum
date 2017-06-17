
// Stage 4

class Declaration;
class Type;
class Scope;
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
Value *typize(Expr *expr, Scope *scope);

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
        //set_ts(v->ts);  // TODO: circular reference...
    }
};


class FunctionValue: public Value {
public:
    Function *function;
    
    FunctionValue(Function *f) {
        function = f;
        //set_ts(f->ts);  // TODO: circular reference...
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        return true;  // FIXME
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


class FunctionDefinitionValue: public Value {
public:
    Value *ret;
    Value *head;
    Value *body;
    
    FunctionDefinitionValue(Value *r, Value *h, Value *b) {
        ret = r;
        head = h;
        body = b;
    }
};


class DeclarationValue: public Value {
public:
    Declaration *decl;
    
    DeclarationValue(Declaration *d) {
        decl = d;
    }
};


class NumberValue: public Value {
public:
    std::string text;
    
    NumberValue(std::string t) {
        text = t;
    }
};


// Declaration

class Declaration {
public:
    Scope *outer;

    virtual Value *match(std::string name, Value *pivot) {
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
        TypeSpec pts = pivot ? pivot->ts : TS_VOID;
        
        if (name == this->name && pts == pivot_ts) {
            Value *v = new VariableValue(this);
            v->set_ts(var_ts);
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
    //TypeSpec argument_ts;
    
    Function(std::string name, TypeSpec pts, TypeSpec rts) {
        this->name = name;
        this->pivot_ts = pts;
        this->ret_ts = rts;
        //this->argument_ts = argument_ts;
    }
    
    virtual Value *match(std::string name, Value *pivot) {
        TypeSpec pts = pivot ? pivot->ts : TS_VOID;
        //std::cout << "XXX Function.match " << name << " " << print_typespec(ts) << "\n";

        if (name == this->name && pts == pivot_ts) {  // TODO: convert
            Value *v = new FunctionValue(this);
            v->set_ts(ret_ts);
            return v;
        }
        else
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
    Scope *head_scope;

    FunctionScope(Scope *hs)
        :Scope() {
        head_scope = hs;
    }
    
    virtual Value *lookup(std::string name, Value *pivot) {
        Value *v = Scope::lookup(name, pivot);
        if (v)
            return v;
        
        v = head_scope->lookup(name, pivot);
        if (v)
            return v;
            
        return NULL;
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
            
            return new TypeValue(ts);
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


Value *typize(Expr *expr, Scope *scope) {
    if (expr->type == OPEN) {
        //Scope *inner = new Scope(scope);
        //scope->add(inner);
        
        //Match m(inner);
        if (expr->pivot)
            throw Error("An OPEN had a pivot argument!");

        BlockValue *v = new BlockValue();
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
            
            Scope *body_scope = new FunctionScope(head_scope);
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
            
            Value *v = new FunctionDefinitionValue(ret, head, body);
            v->set_ts(fn_ts);
            
            return v;
        }
        else if (expr->text == "return") {
            Expr *r = expr->pivot.get();
            Value *ret = r ? typize(r, scope) : NULL;  // TODO: inner scope?

            Value *v = new ReturnValue(ret);

            return v;
        }
        else
            throw Error("Unknown statement!");
    }
    else if (expr->type == DECLARATION) {
        std::string name = expr->text;
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
            TypeSpec ts;
            for (unsigned i = 1; i < d->ts.size(); i++)
                ts.push_back(d->ts[i]);

            decl = new Function(name, TS_VOID, ts);  // TODO: store the actual definition!
        }
            
        scope->add(decl);
    
        Value *v = new DeclarationValue(decl);
        
        return v;
    }
    else if (expr->type == IDENTIFIER) {
        std::string name = expr->text;
        Value *p = expr->pivot ? typize(expr->pivot.get(), scope) : NULL;
        TypeSpec pts = p ? p->ts : TS_VOID;
        
        std::cout << "Looking up " << name << " with pivot type " << print_typespec(pts) << "\n";

        for (Scope *s = scope; s; s = s->outer) {
            Value *v = s->lookup(expr->text, p);
        
            if (v) {
                bool ok = v->check(expr->args, expr->kwargs, scope);
                
                if (!ok)
                    throw Error("Argument problem!");
                    
                return v;
            }
        }
        
        throw Error("No match for %s!", name.c_str());
    }
    else if (expr->type == NUMBER) {
        return new NumberValue(expr->text);  // TODO
    }
    else
        throw Error("Can't typize this now %d!", expr->type);
}
