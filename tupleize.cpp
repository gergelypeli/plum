
// Stage 3

class Expr;
typedef GenericArgs<Expr> Args;
typedef GenericKwargs<Expr> Kwargs;

class Expr {
public:
    enum ExprType {
        TUPLE,
        UNSIGNED_INTEGER, NEGATIVE_INTEGER, STRING,
        INITIALIZER, PARTINITIALIZER, MATCHER, IDENTIFIER,
        CONTROL, EVAL, DECLARATION
    } type;
    Token token;
    std::string text;
    
    std::unique_ptr<Expr> pivot;
    Args args;
    Kwargs kwargs;
    
    Expr(ExprType t, Token k) {
        type = t;
        token = k;
    }

    Expr(ExprType t, Token k, std::string tx) {
        type = t;
        token = k;
        text = tx;
    }
    
    Expr *set_pivot(Expr *p) {
        pivot.reset(p);
        return this;
    }
    
    Expr *add_arg(Expr *a) {
        if (kwargs.size()) {
            std::cerr << "Positional params after keyword at " << a->token << "!\n";
            throw TUPLE_ERROR;
        }
            
        args.push_back(std::unique_ptr<Expr>(a));
        return this;
    }
    
    Expr *add_kwarg(std::string k, Expr *v) {
        if (kwargs[k]) {
            std::cerr << "Duplicate keyword argument " << k << " at " << token << "!\n";
            throw TUPLE_ERROR;
        }
            
        kwargs[k] = std::unique_ptr<Expr>(v);
        return this;
    }

    const char *print_type() {
        return (
            type == TUPLE ? "TUPLE" :
            type == UNSIGNED_INTEGER ? "UNSIGNED_INTEGER" :
            type == NEGATIVE_INTEGER ? "NEGATIVE_INTEGER" :
            type == STRING ? "STRING" :
            type == INITIALIZER ? "INITIALIZER" :
            type == PARTINITIALIZER ? "PARTINITIALIZER" :
            type == MATCHER ? "MATCHER" :
            type == IDENTIFIER ? "IDENTIFIER" :
            type == CONTROL ? "CONTROL" :
            type == EVAL ? "EVAL" :
            type == DECLARATION ? "DECLARATION" :
            throw TUPLE_ERROR
        );
    }
};


Expr *tupleize(std::vector<Node> &nodes, int i);


void tupleize_arguments(Expr *e, std::vector<Node> &nodes, int i) {
    Node &node = nodes[i];

    if (node.type == Node::OPEN) {
        tupleize_arguments(e, nodes, node.right);
    }
    else if (node.type == Node::CLOSE) {
        if (node.left)
            tupleize_arguments(e, nodes, node.left);
    }
    else if (node.type == Node::SEPARATOR) {
        // We allowed extra separators
        if (node.left)
            tupleize_arguments(e, nodes, node.left);
            
        if (node.right)
            tupleize_arguments(e, nodes, node.right);
    }
    else if (node.type == Node::LABEL) {
        if (node.left)
            tupleize_arguments(e, nodes, node.left);  // May be needed in a block of labels

        if (!node.right) {
            std::cerr << "Keyword argument without value: " << node.token << "!\n";
            throw TUPLE_ERROR;
        }
        
        Expr *f = tupleize(nodes, node.right);
        e->add_kwarg(node.text, f);
    }
    else {
        Expr *f = tupleize(nodes, i);
        e->add_arg(f);
    }
}


void tupleize_pivot(Expr *e, std::vector<Node> &nodes, int i) {
    Expr *p = tupleize(nodes, i);
    e->set_pivot(p);
}


Expr *tupleize(std::vector<Node> &nodes, int i) {
    if (!i) {
        std::cerr << "Eiii!\n";
        throw INTERNAL_ERROR;
    }
        
    Node &node = nodes[i];
    
    if (node.type == Node::OPEN) {
        // Grouping operators are ignored from now on
        return tupleize(nodes, node.right);
    }
    else if (node.type == Node::CLOSE) {
        // Grouping operators are ignored from now on
        if (node.left)
            return tupleize(nodes, node.left);
        else {
            std::cerr << "Enclosing nothing is only forgiven in argument positions!\n";
            throw TUPLE_ERROR;
        }
    }
    else if (node.type == Node::LABEL) {
        Expr *e = new Expr(Expr::TUPLE, node.token);
        tupleize_arguments(e, nodes, i);
        return e;
    }
    else if (node.type == Node::SEPARATOR) {
        Expr *e = new Expr(Expr::TUPLE, node.token);
        tupleize_arguments(e, nodes, i);
        return e;
    }
    else if (node.type == Node::IDENTIFIER) {
        // Special handling for negating numeric literals, so they can be type correct.
        // Applying a second negation is not handled specially.
        if (node.text == "unary_minus" && node.right && nodes[node.right].type == Node::UNSIGNED_INTEGER) {
            return new Expr(Expr::NEGATIVE_INTEGER, nodes[node.right].token, nodes[node.right].text);
        }
        
        Expr *e = new Expr(Expr::IDENTIFIER, node.token, node.text);
    
        if (node.fore == UNARY || node.fore == LOGICAL_HIGH) {
            if (!node.right) {
                std::cerr << "Unary operator without argument: " << node.token << "!\n";
                throw TUPLE_ERROR;
            }
            
            tupleize_pivot(e, nodes, node.right);
        }
        else {
            if (node.left)
                tupleize_pivot(e, nodes, node.left);
        
            if (node.right)
                tupleize_arguments(e, nodes, node.right);
        }

        return e;
    }
    else if (node.type == Node::UNSIGNED_INTEGER) {
        return new Expr(Expr::UNSIGNED_INTEGER, node.token, node.text);
    }
    else if (node.type == Node::STRING) {
        return new Expr(Expr::STRING, node.token, node.text);
    }
    else {
        Expr::ExprType et = (
            node.type == Node::CONTROL ? Expr::CONTROL :
            node.type == Node::EVAL ? Expr::EVAL :
            node.type == Node::DECLARATION ? Expr::DECLARATION :
            node.type == Node::INITIALIZER ? Expr::INITIALIZER :
            node.type == Node::PARTINITIALIZER ? Expr::PARTINITIALIZER :
            node.type == Node::MATCHER ? Expr::MATCHER :
            throw INTERNAL_ERROR
        );

        Expr *e = new Expr(et, node.token, node.text);
    
        if (node.left)
            tupleize_pivot(e, nodes, node.left);
    
        if (node.right)
            tupleize_arguments(e, nodes, node.right);

        return e;
    }
}


Expr *tupleize(std::vector<Node> nodes) {
    Expr *root = new Expr(Expr::TUPLE, Token("", 0, 0));
    
    tupleize_arguments(root, nodes, 0);
    
    return root;
}


void print_expr_tree(Expr *e, int indent, const char *prefix) {
    for (int j=0; j<indent; j++)
        std::cerr << " ";
        
    std::cerr << prefix << " " << e->print_type() << " " << e->text << "\n";
    
    if (e->pivot)
        print_expr_tree(e->pivot.get(), indent + 2, "$");
        
    for (auto &x : e->args)
        print_expr_tree(x.get(), indent + 2, "#");
            
    for (auto &kv : e->kwargs)
        print_expr_tree(kv.second.get(), indent + 2, kv.first.c_str());
}


// Further helper functions

Expr *mkexpr(std::string name, Expr *left = NULL, Expr *right = NULL) {
    Expr *e = new Expr(Expr::IDENTIFIER, Token(), name);
    
    if (left)
        e->set_pivot(left);
        
    if (right)
        e->add_arg(right);
        
    return e;
}


Expr *mkctrl(std::string name, Expr *arg0 = NULL,
    const char *kw1 = NULL, Expr *arg1 = NULL,
    const char *kw2 = NULL, Expr *arg2 = NULL,
    const char *kw3 = NULL, Expr *arg3 = NULL,
    const char *kw4 = NULL, Expr *arg4 = NULL
) {
    Expr *e = new Expr(Expr::CONTROL, Token(), name);
    
    if (arg0)
        e->add_arg(arg0);
    
    if (kw1 && arg1)
        e->add_kwarg(kw1, arg1);

    if (kw2 && arg2)
        e->add_kwarg(kw2, arg2);

    if (kw3 && arg3)
        e->add_kwarg(kw3, arg3);

    if (kw4 && arg4)
        e->add_kwarg(kw4, arg4);
        
    return e;
}


Expr *mktupl(Expr *arg0 = NULL,
    Expr *arg1 = NULL,
    Expr *arg2 = NULL,
    Expr *arg3 = NULL
) {
    Expr *e = new Expr(Expr::TUPLE, Token());
    
    if (arg0)
        e->add_arg(arg0);
    
    if (arg1)
        e->add_arg(arg1);
    
    if (arg2)
        e->add_arg(arg2);
    
    if (arg3)
        e->add_arg(arg3);
        
    return e;
}


Expr *mkdecl(std::string name, Expr *arg) {
    Expr *e = new Expr(Expr::DECLARATION, Token(), name);
    
    e->add_arg(arg);
        
    return e;
}


Expr *mkinit(std::string name, Expr *arg = NULL) {
    Expr *e = new Expr(Expr::INITIALIZER, Token(), name);
    
    if (arg)
        e->add_arg(arg);
        
    return e;
}
