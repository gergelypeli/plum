
// Stage 4

class Declaration;
class Type;
class Scope;
class FunctionScope;
class Variable;
class Function;

typedef std::vector<Type *> TypeSpec;
typedef TypeSpec::iterator TypeSpecIter;

Type *type_type = NULL;
Type *void_type = NULL;
Type *function_type = NULL;
Type *integer_type = NULL;

TypeSpec TS_VOID;

typedef std::vector<std::unique_ptr<Expr>> Args;
typedef std::map<std::string, std::unique_ptr<Expr>> Kwargs;

class Value;
class FunctionDefinitionValue;

TypeSpec get_typespec(Value *v);
Value *typize(Expr *expr, Scope *scope);
bool operator>>(TypeSpec &this_ts, TypeSpec &that_ts);

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
        else {
            std::cerr << "Not the last decl to remove!\n";
            throw INTERNAL_ERROR;
        }
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
        
        if (name == this->name && pts >> pivot_ts) {
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
    std::vector<TypeSpec> arg_tss;
    std::vector<std::string> arg_names;

    Function(std::string name, TypeSpec pts, TypeSpec rts, std::vector<TypeSpec> atss, std::vector<std::string> ans) {
        this->name = name;
        this->pivot_ts = pts;
        ret_ts = rts;
        arg_tss = atss;
        arg_names = ans;
    }

    virtual Value *match(std::string name, Value *pivot) {
        TypeSpec pts = get_typespec(pivot);
        //std::cerr << "XXX Function.match " << name << " " << print_typespec(ts) << "\n";

        if (name == this->name && pts >> pivot_ts) {
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
        return arg_tss.size();
    }
    
    virtual TypeSpec get_argument_typespec(unsigned i) {
        //std::cerr << "Returning typespec for argument " << i << ".\n";
        return arg_tss[i];
    }

    virtual int get_argument_index(std::string keyword) {  // FIXME
        for (unsigned i = 0; i < arg_names.size(); i++)
            if (arg_names[i] == keyword)
                return i;
                
        return (unsigned)-1;
    }
};


bool are_equal(TypeSpecIter &this_tsi, TypeSpecIter &that_tsi);


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
    
    virtual bool is_equal(TypeSpecIter &this_tsi, TypeSpecIter &that_tsi) {
        if (*this_tsi != *that_tsi)
            return false;
            
        this_tsi++;
        that_tsi++;
        
        for (unsigned p = 0; p < parameter_count; p++) {
            if (!are_equal(this_tsi, that_tsi))
                return false;
        }
        
        return true;
    }

    virtual bool is_convertible(TypeSpecIter &this_tsi, TypeSpecIter &that_tsi) {
        return is_equal(this_tsi, that_tsi);
    }
};


bool are_equal(TypeSpecIter &this_tsi, TypeSpecIter &that_tsi) {
    return (*this_tsi)->is_equal(this_tsi, that_tsi);
}


bool are_convertible(TypeSpecIter &this_tsi, TypeSpecIter &that_tsi) {
    return (*this_tsi)->is_convertible(this_tsi, that_tsi);
}


bool operator>>(TypeSpec &this_ts, TypeSpec &that_ts) {
    TypeSpecIter this_tsi(this_ts.begin());
    TypeSpecIter that_tsi(that_ts.begin());
    
    return are_convertible(this_tsi, that_tsi);
}


std::ostream &operator<<(std::ostream &os, TypeSpec &ts) {
    os << "[";
    
    bool start = true;
    
    for (auto type : ts) {
        if (start)
            start = false;
        else
            os << ",";
            
        os << type->name;
    }
    
    os << "]";
    
    return os;
}


// Value

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


Scope *init_types() {
    Scope *root_scope = new Scope();
    
    type_type = new Type("<Type>", 1);
    root_scope->add(type_type);

    function_type = new Type("<Function>", 1);
    root_scope->add(function_type);
    
    void_type = new Type("<Void>", 0);
    root_scope->add(void_type);
    TS_VOID.push_back(void_type);

    integer_type = new Type("Integer", 0);
    root_scope->add(integer_type);
    
    TypeSpec int_ts;
    int_ts.push_back(integer_type);
    
    std::vector<TypeSpec> arg_tss;
    arg_tss.push_back(int_ts);
    std::vector<std::string> arg_names;
    arg_names.push_back("other");
    
    Declaration *integer_add = new Function("plus", int_ts, int_ts, arg_tss, arg_names);
    root_scope->add(integer_add);

    Declaration *integer_print = new Function("print", TS_VOID, TS_VOID, arg_tss, arg_names);
    root_scope->add(integer_print);

    return root_scope;
}


Value *typize(Expr *expr, Scope *scope) {
    if (expr->type == OPEN) {
        if (expr->pivot) {
            std::cerr << "An OPEN had a pivot argument!\n";
            throw INTERNAL_ERROR;
        }

        Value *v = make_block_value()->set_token(expr->token);
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
                if (ret->ts[0] != type_type) {
                    std::cerr << "Function return expression is not a type!\n";
                    throw TYPE_ERROR;
                }
                    
                fn_ts = ret->ts;
                fn_ts[0] = function_type;
            }
            else {
                fn_ts.push_back(function_type);
                fn_ts.push_back(void_type);
            }

            std::cerr << "Function ts " << fn_ts << "\n";
            
            Value *v = make_function_definition_value(ret, head, body, body_scope)->set_token(expr->token);
            v->set_ts(fn_ts);
            
            return v;
        }
        else if (expr->text == "return") {
            Expr *r = expr->pivot.get();
            Value *ret = r ? typize(r, scope) : NULL;  // TODO: inner scope?

            Value *v = make_return_value(ret)->set_token(expr->token);

            return v;
        }
        else {
            std::cerr << "Unknown statement " << expr->token << "!\n";
            throw TYPE_ERROR;
        }
    }
    else if (expr->type == DECLARATION) {
        std::string name = expr->text;
        std::cerr << "Declaring " << name << ".\n";
        
        Value *d = typize(expr->pivot.get(), scope);
        
        if (d->ts.size() == 0) {
            std::cerr << "Declaration needs a type " << expr->token << "!\n";
            throw TYPE_ERROR;
        }

        Declaration *decl;

        if (d->ts[0] == type_type) {
            TypeSpec var_ts;
            for (unsigned i = 1; i < d->ts.size(); i++)
                var_ts.push_back(d->ts[i]);
        
            decl = new Variable(name, TS_VOID, var_ts);
        }
        else if (d->ts[0] == function_type) {
            FunctionDefinitionValue *fdv = dynamic_cast<FunctionDefinitionValue *>(d);

            TypeSpec ret_ts;
            for (unsigned i = 1; i < d->ts.size(); i++)
                ret_ts.push_back(d->ts[i]);

            std::cerr << "It's a function with return type " << ret_ts << ".\n";

            Scope *head_scope = fdv->fn_scope->head_scope;
            unsigned n = head_scope->get_length();

            std::vector<TypeSpec> arg_tss;
            std::vector<std::string> arg_names;
            
            for (unsigned i = 0; i < n; i++) {
                Declaration *xd = head_scope->get_declaration(i);
                Variable *vd = dynamic_cast<Variable *>(xd);
                arg_tss.push_back(vd->var_ts);
                arg_names.push_back(vd->name);
            }
            
            decl = new Function(name, TS_VOID, ret_ts, arg_tss, arg_names);
        }
            
        scope->add(decl);
    
        Value *v = make_declaration_value(decl, d)->set_token(expr->token);
        
        return v;
    }
    else if (expr->type == IDENTIFIER) {
        std::string name = expr->text;
        Value *p = expr->pivot ? typize(expr->pivot.get(), scope) : NULL;
        TypeSpec pts = p ? p->ts : TS_VOID;
        
        std::cerr << "Looking up " << pts << " " << name << "\n";

        for (Scope *s = scope; s; s = s->outer) {
            //std::cerr << "Trying a scope...\n";
            
            Value *v = s->lookup(expr->text, p);
        
            if (v) {
                v->set_token(expr->token);
                bool ok = v->check(expr->args, expr->kwargs, scope);
                
                if (!ok) {
                    std::cerr << "Argument problem for " << expr->token << "!\n";
                    throw TYPE_ERROR;
                }

                std::cerr << "Found " << pts << " " << name << " as a " << v->ts << ".\n";
                return v;
            }
        }
        
        std::cerr << "No match for " << pts << " " << name << " at " << expr->token << "!\n";
        throw TYPE_ERROR;
    }
    else if (expr->type == NUMBER) {
        return make_number_value(expr->text)->set_token(expr->token);  // TODO
    }
    else {
        std::cerr << "Can't typize this now: " << expr->token << "!\n";
        throw INTERNAL_ERROR;
    }
}
