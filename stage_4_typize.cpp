
// Stage 4

class Declaration;
class Type;
class Scope;
class FunctionReturnScope;
class FunctionHeadScope;
class FunctionBodyScope;
class FunctionScope;
class Variable;
class Function;

typedef std::vector<Type *> TypeSpec;
typedef TypeSpec::iterator TypeSpecIter;

Type *type_type = NULL;
Type *lvalue_type = NULL;
Type *void_type = NULL;
Type *function_type = NULL;
Type *boolean_type = NULL;
Type *integer_type = NULL;

TypeSpec TS_VOID;

typedef std::vector<std::unique_ptr<Expr>> Args;
typedef std::map<std::string, std::unique_ptr<Expr>> Kwargs;

class Value;
class FunctionDefinitionValue;

TypeSpec get_typespec(Value *v);
Value *typize(Expr *expr, Scope *scope);
bool operator>>(TypeSpec &this_ts, TypeSpec &that_ts);
bool are_equal(TypeSpecIter &this_tsi, TypeSpecIter &that_tsi);
bool are_convertible(TypeSpecIter &this_tsi, TypeSpecIter &that_tsi);

Value *make_function_head_value(FunctionHeadScope *s);
Value *make_function_body_value(FunctionBodyScope *s);
Value *make_function_return_value(FunctionReturnScope *s, Value *v);
Value *make_variable_value(Variable *decl, Value *pivot);
Value *make_function_value(Function *decl);
Value *make_type_value(TypeSpec ts);
Value *make_block_value();
Value *make_function_definition_value(Value *ret, Value *head, Value *body, FunctionScope *fn_scope);
Value *make_declaration_value(Declaration *decl, Value *v);
Value *make_number_value(std::string text);

class X64;


// Declaration

class Declaration {
public:
    Scope *outer;
    int offset;

    virtual Value *match(std::string, Value *) {
        return NULL;
    }
    
    virtual void allocate() {
    }
};


class Scope: virtual public Declaration {
public:
    std::vector<std::unique_ptr<Declaration>> contents;
    bool is_ro;  // These flags should be subclass behavior instead
    bool is_downward;
    unsigned size;
    
    Scope(bool ro = false, bool dw = false)
        :Declaration() {
        is_ro = ro;
        is_downward = dw;
        size = 0;
    }
    
    virtual bool is_readonly() {
        return is_ro;
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
    
    virtual void allocate() {
        // TODO: this may not be correct for all kind of scopes
        for (auto &content : contents)
            content->allocate();
    }

    virtual int reserve(unsigned s) {
        unsigned old_size = size;
        size += (s + 7) & ~7;
        
        if (is_downward) {
            return -size;
        }
        else {
            return old_size;
        }
    }
    
    virtual Value *get_implicit_value() {
        if (outer)
            return outer->get_implicit_value();
        else {
            std::cerr << "No implicit scope value!\n";
            throw INTERNAL_ERROR;
        }
    }
};


class FunctionHeadScope: public Scope {
public:
    FunctionHeadScope():Scope(true, true) {};
    
    virtual Value *get_implicit_value() {
        return make_function_head_value(this);
    }
};


class FunctionBodyScope: public Scope {
public:
    FunctionBodyScope():Scope(false, true) {};
    
    virtual Value *get_implicit_value() {
        return make_function_body_value(this);
    }
};


class FunctionReturnScope: public Scope {
public:
    FunctionReturnScope():Scope(true, true) {};

    virtual Value *get_implicit_value() {
        std::cerr << "How the hell did you access a return value variable?\n";
        throw INTERNAL_ERROR;
    }
};


class FunctionScope: public Scope {
public:
    FunctionReturnScope *return_scope;
    FunctionHeadScope *head_scope;
    FunctionBodyScope *body_scope;

    FunctionScope()
        :Scope() {
        return_scope = NULL;
        head_scope = NULL;
        body_scope = NULL;
    }
    
    FunctionReturnScope *add_return_scope() {
        return_scope = new FunctionReturnScope;
        add(return_scope);
        return return_scope;
    }
    
    FunctionHeadScope *add_head_scope() {
        head_scope = new FunctionHeadScope;
        add(head_scope);
        return head_scope;
    }
    
    FunctionBodyScope *add_body_scope() {
        body_scope = new FunctionBodyScope;
        add(body_scope);
        return body_scope;
    }
    
    virtual Value *lookup(std::string name, Value *pivot) {
        if (body_scope) {
            Value *v = head_scope->lookup(name, pivot);
            if (v)
                return v;
        }
        
        if (head_scope) {
            Value *v = return_scope->lookup(name, pivot);
            if (v)
                return v;
        }
            
        return NULL;
    }
    
    virtual void allocate() {
        Scope::allocate();
        
        int offset = 8 + 8;  // From RBP upward, skipping the pushed RBP and RIP

        offset += head_scope->size;
        head_scope->offset = offset;

        offset += return_scope->size;
        return_scope->offset = offset;
        
        body_scope->offset = 0;
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
            if (!pivot) {
                // Include an implicit pivot value for local variables
                pivot = outer->get_implicit_value();
            }
            
            Value *v = make_variable_value(this, pivot);
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

    Function(std::string name, TypeSpec pts, std::vector<TypeSpec> atss, std::vector<std::string> ans, TypeSpec rts) {
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
        return *that_tsi == boolean_type || is_equal(this_tsi, that_tsi);
    }
};


class LvalueType: public Type {
public:
    LvalueType()
        :Type("<Lvalue>", 1) {
    }
    
    virtual bool is_convertible(TypeSpecIter &this_tsi, TypeSpecIter &that_tsi) {
        if (*this_tsi == *that_tsi) {
            // When an lvalue is required, only the same type suffices
            this_tsi++;
            that_tsi++;
            
            return are_equal(this_tsi, that_tsi);
        }
        else {
            // When an rvalue is required, subtypes are also fine
            this_tsi++;
            
            return are_convertible(this_tsi, that_tsi);
        }
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
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *) {
        return (args.size() == 0 && kwargs.size() == 0);
    }
    
    virtual void compile(X64 *) {
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
    
    virtual void compile(X64 *x64) {
        // FIXME: this works for a bunch of declarations, but not in general
        
        for (auto &item : items)
            item->compile(x64);
            
        for (auto &kv : kwitems)
            kv.second->compile(x64);
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
};


// The value of a :function statement
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
    
    virtual void compile(X64 *x64) {
        fn_scope->allocate();
        body->compile(x64);
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
};


class TypeValue: public Value {
public:
    TypeValue(TypeSpec ts) {
        set_ts(ts);
    }
};


class FunctionReturnValue: public Value {
public:
    FunctionReturnScope *return_scope;
    Value *value;
    
    FunctionReturnValue(FunctionReturnScope *s, Value *v) {
        return_scope = s;
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
    
    virtual void compile(X64 *x64) {
        // TODO: when declaring by value, this should be an initialization, too
        value->compile(x64);
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


Value *make_function_head_value(FunctionHeadScope *s) {
    return new FunctionHeadValue(s);
}


Value *make_function_body_value(FunctionBodyScope *s) {
    return new FunctionBodyValue(s);
}


Value *make_function_return_value(FunctionReturnScope *s, Value *v) {
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

    lvalue_type = new LvalueType();
    root_scope->add(lvalue_type);

    function_type = new Type("<Function>", 1);
    root_scope->add(function_type);
    
    void_type = new Type("<Void>", 0);
    root_scope->add(void_type);
    TS_VOID.push_back(void_type);

    boolean_type = new Type("Boolean", 0);
    root_scope->add(boolean_type);

    integer_type = new Type("Integer", 0);
    root_scope->add(integer_type);
    
    TypeSpec void_ts = { void_type };
    TypeSpec bool_ts = { boolean_type };
    TypeSpec int_ts = { integer_type };
    TypeSpec lint_ts = { lvalue_type, integer_type };
    
    std::vector<TypeSpec> int_tss = { int_ts };
    std::vector<TypeSpec> bool_tss = { bool_ts };
    
    std::vector<std::string> value_names = { "value" };

    for (auto name : { "minus", "negate" })
        root_scope->add(new Function(name, void_ts, int_tss, value_names, int_ts));

    for (auto name : { "plus", "minus", "star", "slash", "percent", "or", "xor", "and", "exponent" })
        root_scope->add(new Function(name, int_ts, int_tss, value_names, int_ts));

    for (auto name : { "equal", "not_equal", "less", "greater", "less_equal", "greater_equal", "incomparable", "compare" })
        root_scope->add(new Function(name, int_ts, int_tss, value_names, bool_ts));

    for (auto name : { "logical not", "logical and", "logical or", "logical xor" })
        root_scope->add(new Function(name, bool_ts, bool_tss, value_names, bool_ts));

    for (auto name : { "plus_assign", "minus_assign", "star_assign", "slash_assign", "percent_assign", "or_assign", "xor_assign", "and_assign" })
        root_scope->add(new Function(name, lint_ts, int_tss, value_names, int_ts));

    root_scope->add(new Function("print", void_ts, int_tss, value_names, void_ts));

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
            FunctionScope *fn_scope = new FunctionScope();
            scope->add(fn_scope);
            
            Expr *r = expr->pivot.get();
            Scope *rs = fn_scope->add_return_scope();
            Value *ret = r ? typize(r, rs) : NULL;
        
            Expr *h = expr->kwargs["from"].get();
            Scope *hs = fn_scope->add_head_scope();
            Value *head = h ? typize(h, hs) : NULL;
            
            Expr *b = expr->kwargs["as"].get();
            Scope *bs = fn_scope->add_body_scope();
            Value *body = b ? typize(b, bs) : NULL;
            
            TypeSpec fn_ts;
                
            if (ret) {
                if (ret->ts[0] != type_type) {
                    std::cerr << "Function return expression is not a type!\n";
                    throw TYPE_ERROR;
                }

                // Add anon return value variable, so funretscp can compute its size!
                TypeSpec var_ts = ret->ts;
                var_ts[0] = lvalue_type;
                Variable *decl = new Variable("<ret>", TS_VOID, var_ts);
                rs->add(decl);
                    
                fn_ts = ret->ts;
                fn_ts[0] = function_type;
            }
            else {
                fn_ts.push_back(function_type);
                fn_ts.push_back(void_type);
            }

            std::cerr << "Function ts " << fn_ts << "\n";
            
            Value *v = make_function_definition_value(ret, head, body, fn_scope)->set_token(expr->token);
            v->set_ts(fn_ts);
            
            return v;
        }
        else if (expr->text == "return") {
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
        
            Expr *r = expr->pivot.get();
            Value *ret = r ? typize(r, scope) : NULL;  // TODO: inner scope?

            Value *v = make_function_return_value(fn_scope->return_scope, ret)->set_token(expr->token);

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
            
            if (!scope->is_readonly())
                var_ts.push_back(lvalue_type);
            
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
            
            decl = new Function(name, TS_VOID, arg_tss, arg_names, ret_ts);
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
