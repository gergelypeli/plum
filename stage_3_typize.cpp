
// Stage 3

class Type;
class Declaration;
typedef std::vector<Type *> TypeSpec;
//typedef TypeSpec TypeList;

Type *type_type = NULL;
Type *void_type = NULL;
Type *function_type = NULL;
Type *integer_type = NULL;

TypeSpec TS_VOID;
std::string print_typespec(TypeSpec &ts);


struct Match {
    Declaration *decl;
    std::vector<TypeSpec> params;
    
    Match() {
        decl = NULL;
    }

    Match(Declaration *d) {
        decl = d;
    }
    
    Match(Declaration *d, std::vector<TypeSpec> p) {
        decl = d;
        params = p;
    }
};


class Declaration {
public:
    virtual Match match(std::string name, TypeSpec ts) {
        return Match();
    }
    
    virtual TypeSpec get_return_ts(Match m) {
        throw Error("No return type!");
    }
    
    virtual TypeSpec get_argument_ts(Match m) {
        throw Error("No argument types!");
    }
};


class Variable: public Declaration {
public:
    std::string name;
    TypeSpec ts;
    
    Variable(std::string name, TypeSpec ts) {
        this->name = name;
        this->ts = ts;
    }
    
    virtual Match match(std::string name, TypeSpec ts) {
        if (name == this->name && ts == TS_VOID)
            return Match(this);
        else
            return Match();
    }
    
    virtual TypeSpec get_return_ts(Match m) {
        return ts;
    }
    
    virtual TypeSpec get_argument_ts(Match m) {
        return TS_VOID;
    }
};


class Function: public Declaration {
public:
    std::string name;
    TypeSpec ts;
    TypeSpec pivot_ts;
    TypeSpec argument_ts;
    
    Function(std::string name, TypeSpec ts, TypeSpec pivot_ts, TypeSpec argument_ts) {
        this->name = name;
        this->ts = ts;
        this->pivot_ts = pivot_ts;
        this->argument_ts = argument_ts;
    }
    
    virtual Match match(std::string name, TypeSpec ts) {
        std::cout << "XXX Function.match " << name << " " << print_typespec(ts) << "\n";
        if (name == this->name && ts == pivot_ts)  // TODO: convert
            return Match(this);
        else
            return Match();
    }
    
    virtual TypeSpec get_return_ts(Match m) {
        return ts;
    }
    
    virtual TypeSpec get_argument_ts(Match m) {
        return argument_ts;
    }
};


class Scope: virtual public Declaration {
public:
    std::vector<std::unique_ptr<Declaration>> contents;
    Scope *outer;
    
    Scope(Scope *outer = NULL) {
        this->outer = outer;
    }
    
    virtual void add(Declaration *decl) {
        contents.push_back(std::unique_ptr<Declaration>(decl));
    }
    
    virtual void remove(Declaration *decl) {
        if (contents.back().get() == decl) {
            contents.back().release();
            contents.pop_back();
        }
        else
            throw Error("Not the last decl to remove!");
    }
    
    virtual unsigned get_length() {
        return contents.size();
    }
    
    virtual Match lookup(std::string name, TypeSpec ts) {
        for (auto &decl : contents) {
            Match m = decl->match(name, ts);
            
            if (m.decl)
                return m;
        }

        return Match();
    }
    
    virtual TypeSpec get_return_ts(Match m) {
        //TypeSpec ts;
        //ts.push_back();
        return TS_VOID;  // TODO
    }
};


class FunctionScope: public Scope {
public:
    FunctionScope(Scope *o)
        :Scope(o) {
    }
    
    virtual Match lookup(std::string name, TypeSpec ts) {
        if (contents.size() > 1) {
            Scope *s = dynamic_cast<Scope *>(contents[0].get());
            return s->lookup(name, ts);  // Look up function arguments
        }
        else
            return Match();
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
    
    virtual Match match(std::string name, TypeSpec ts) {
        if (name == this->name && ts == TS_VOID && parameter_count == 0)
            return Match(this);
        else
            return Match();
    }
    
    virtual TypeSpec get_return_ts(Match m) {
        if (m.params.size() != parameter_count)
            throw Error("Mismatching type parameter count!");
            
        TypeSpec ts;
        ts.push_back(type_type);
        ts.push_back(this);
        
        for (auto &param : m.params)
            for (auto type : param)
                ts.push_back(type);
        
        return ts;
    }
    
    virtual TypeSpec get_argument_ts(Match m) {
        return TS_VOID;  // FIXME
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


// FIXME: this is not this simple. For method calls and statements it's the first
// parameter, but for types it's all parameters.
TypeSpec get_pivot_typespec(TypeSpec &ts) {
    if (ts[0]->get_length() == 1)
        return ts;
        
    int start = 1;
    int end = step_typespec(ts, start);
    
    TypeSpec t;
    
    for (int i = start; i < end; i++)
        t.push_back(ts[i]);
        
    return t;
}


class LabelType: public Type {
public:
    std::string label;
    
    LabelType(std::string l)
        :Type("<Label>", 1) {
        label = l;
    }
    
    virtual std::string get_label() {
        return label;
    }
};


Type *make_label_type(std::string label) {
    return new LabelType(label);  // FIXME: cache!
}


class TupleType: public Type {
public:
    TupleType(int pc)
        :Type("<Tuple>", pc) {
        if (pc < 2)
            throw Error("A tuple type needs at least 2 parameters!");
    }
    
    virtual int get_length() {
        return parameter_count;
    }
};


Type *make_tuple_type(int pc) {
    return new TupleType(pc);  // FIXME: cache!
}


TypeSpec make_tuple_typespec(TypeSpec a, TypeSpec b) {
    if (a == TS_VOID)
        return b;
        
    if (b == TS_VOID)
        return a;
    
    int a_length = a[0]->get_length();
    int b_length = b[0]->get_length();
    
    TypeSpec ts;
    Type *t = make_tuple_type(a_length + b_length);
    ts.push_back(t);
    
    for (unsigned i = (a_length >= 2 ? 1 : 0); i < a.size(); i++)
        ts.push_back(a[i]);
        
    for (unsigned i = (b_length >= 2 ? 1 : 0); i < b.size(); i++)
        ts.push_back(b[i]);
    
    return ts;
}


class Expr {
public:
    Match match;
    TypeSpec ts;
    std::unique_ptr<Expr> left, right;
    Expr *parent;
    
    Expr(TypeSpec t, Expr *l, Expr *r)
        :left(l), right(r) {
        ts = t;
        
        if (l)
            l->parent = this;
            
        if (r)
            r->parent = this;
    }

    Expr(Match m, Expr *l, Expr *r)
        :left(l), right(r) {
        match = m;
        ts = m.decl->get_return_ts(m);

        if (l)
            l->parent = this;
            
        if (r)
            r->parent = this;
    }

};


Expr *resolve(std::vector<Op> ops, int i, Scope *scope) {
    if (i < 0)
        throw Error("Eiii!");
        
    Op &op = ops[i];
    
    if (op.type == OPEN) {
        Scope *inner = new Scope(scope);
        scope->add(inner);
        
        Expr *r = resolve(ops, op.right, inner);
        
        if (inner->get_length() == 0) {
            scope->remove(inner);
            return r;
        }
        else {
            //TypeSpec ts;
            //ts.push_back(inner);
            Match m(inner);
            
            Expr *e = new Expr(m, NULL, r);
            return e;
        }
    }
    else if (op.type == CLOSE) {
        return resolve(ops, op.left, scope);
    }
    else if (op.type == SEPARATOR) {
        Expr *l = resolve(ops, op.left, scope);
        Expr *r = resolve(ops, op.right, scope);
        Expr *e = new Expr(TS_VOID, l, r);  // TODO
        return e;
    }
    else if (op.type == STATEMENT) {
        if (op.text == "function") {
            Scope *inner = new FunctionScope(scope);
            scope->add(inner);

            Expr *r = resolve(ops, op.right, inner);

            //Expr *from = r->left->right;
            //Scope *args = dynamic_cast<Scope *>(from->match.decl);
            //std::cout << "XXX: " << print_typespec(r->ts) << "\n";
            TypeSpec ret_ts;
            
            if (r->left && r->left->left)
                ret_ts = r->left->left->ts;
            else {
                ret_ts.push_back(type_type);
                ret_ts.push_back(void_type);
            }
            
            std::cout << "Function ret_ts " << print_typespec(ret_ts) << "\n";
            //TypeSpec ts;
            
            //for (unsigned i = 1; i < ret->ts.size(); i++)
            //    ts.push_back(ret->ts[i]);
                
            Expr *e = new Expr(ret_ts, NULL, r);  // FIXME, a lot
            return e;
        }
        else {
            Expr *r = resolve(ops, op.right, scope);
            Expr *e = new Expr(TS_VOID, NULL, r);  // TODO
            return e;
        }
    }
    else if (op.type == DECLARATION) {
        Expr *r = resolve(ops, op.right, scope);
        
        if (r->ts.size() == 0)
            throw Error("Declaration needs a type!");

        Declaration *decl;

        if (r->ts[0] == type_type) {
            TypeSpec ts;
            for (unsigned i = 1; i < r->ts.size(); i++)
                ts.push_back(r->ts[i]);
        
            decl = new Variable(op.text, ts);
        }
        else if (r->ts[0] == function_type) {
            TypeSpec ts;
            for (unsigned i = 1; i < r->ts.size(); i++)
                ts.push_back(r->ts[i]);

            decl = new Function(op.text, ts, TS_VOID, TS_VOID);
        }
            
        scope->add(decl);
    
        Expr *e = new Expr(TS_VOID, NULL, r);  // TODO
        return e;
    }
    else if (op.type == LABEL) {
        // TODO: check if already labeled!
        Expr *l = op.left >= 0 ? resolve(ops, op.left, scope) : NULL;
        Expr *r = resolve(ops, op.right, scope);
        Type *lt = make_label_type(op.text);
        
        TypeSpec ts(r->ts);
        ts.insert(ts.begin(), lt);
        
        if (l)
            ts = make_tuple_typespec(l->ts, ts);
        
        Expr *e = new Expr(ts, l, r);  // TODO
        return e;
    }
    else if (op.type == IDENTIFIER) {
        Expr *l = op.left >= 0 ? resolve(ops, op.left, scope) : NULL;
        Expr *r = op.right >= 0 ? resolve(ops, op.right, scope) : NULL;
        
        TypeSpec lts = l ? l->ts : TS_VOID;
        TypeSpec rts = r ? r->ts : TS_VOID;
        TypeSpec ts = make_tuple_typespec(lts, rts);
        TypeSpec pts = get_pivot_typespec(ts);

        std::cout << "Looking up " << op.text << " with pivot type " << print_typespec(pts) << "\n";

        for (Scope *s = scope; s; s = s->outer) {
            Match m = s->lookup(op.text, pts);
        
            if (m.decl)
                return new Expr(m, l, r);
        }
        
        throw Error("No match for %s!", op.text.c_str());
    }
    else if (op.type == NUMBER) {
        TypeSpec ts;
        ts.push_back(integer_type);
        
        return new Expr(ts, NULL, NULL);  // TODO
    }
    else
        throw Error("Can't resolve this now %d!", op.type);
}

