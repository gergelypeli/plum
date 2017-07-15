
// Stage 3


class Expr {
public:
    NodeType type;
    Token token;
    std::string text;
    
    std::unique_ptr<Expr> pivot;
    std::vector<std::unique_ptr<Expr>> args;
    std::map<std::string, std::unique_ptr<Expr>> kwargs;
    
    Expr(NodeType t, Token k) {
        type = t;
        token = k;
    }

    Expr(NodeType t, Token k, std::string tx) {
        type = t;
        token = k;
        text = tx;
    }
    
    Expr *set_pivot(Expr *p) {
        pivot.reset(p);
        //p->parent = this;
        return this;
    }
    
    Expr *add_arg(Expr *a) {
        if (kwargs.size()) {
            std::cerr << "Positional params after keyword at " << a->token << "!\n";
            throw TUPLE_ERROR;
        }
            
        args.push_back(std::unique_ptr<Expr>(a));
        //a->parent = this;
        return this;
    }
    
    Expr *add_kwarg(std::string k, Expr *v) {
        kwargs.insert(decltype(kwargs)::value_type(k, v));
        //v->parent = this;
        return this;
    }
};


Expr *tupleize(std::vector<Node> nodes, int i);


void fill_arguments(Expr *e, std::vector<Node> nodes, int i) {
    Node &node = nodes[i];
    //std::cerr << "Fill: " << print_node_type(node.type) << " " << node.text << "\n";

    if (node.type == OPEN) {
        fill_arguments(e, nodes, node.right);
    }
    else if (node.type == CLOSE) {
        fill_arguments(e, nodes, node.left);
    }
    else if (node.type == SEPARATOR) {
        fill_arguments(e, nodes, node.left);
        fill_arguments(e, nodes, node.right);
    }
    else if (node.type == LABEL) {
        if (node.left >= 0)
            fill_arguments(e, nodes, node.left);  // May be needed in a block of labels
            
        Expr *f = tupleize(nodes, node.right);
        e->add_kwarg(node.text, f);
    }
    else {
        Expr *f = tupleize(nodes, i);
        e->add_arg(f);
    }
}


void fill_statement(Expr *e, std::vector<Node> nodes, int i) {
    Node &node = nodes[i];
    //std::cerr << "Fill statement: " << print_node_type(node.type) << " " << node.text << "\n";

    if (node.type == OPEN) {
        // Positional argument block
        Expr *f = new Expr(OPEN, node.token);
        fill_arguments(f, nodes, node.right);
        e->set_pivot(f);
    }
    else if (node.type == CLOSE) {
        std::cerr << "CLOSE while filling a statement!\n";
        throw INTERNAL_ERROR;
    }
    else if (node.type == SEPARATOR) {
        std::cerr << "SEPARATOR while filling a statement!\n";
        throw INTERNAL_ERROR;
    }
    else if (node.type == LABEL) {
        if (node.left >= 0)
            fill_statement(e, nodes, node.left);
            
        Expr *f = new Expr(OPEN, node.token);
        fill_arguments(f, nodes, node.right);
        e->add_kwarg(node.text, f);
    }
    else {
        // Positional argument expression, enclose for consistency
        Expr *f = new Expr(OPEN, node.token);
        fill_arguments(f, nodes, i);
        e->set_pivot(f);
    }
}


Expr *tupleize(std::vector<Node> nodes, int i) {
    if (i < 0) {
        std::cerr << "Eiii!\n";
        throw INTERNAL_ERROR;
    }
        
    Node &node = nodes[i];
    //std::cerr << "Tupleize: " << print_node_type(node.type) << " " << node.text << "\n";
    
    if (node.type == OPEN) {
        return tupleize(nodes, node.right);
    }
    else if (node.type == CLOSE) {
        return tupleize(nodes, node.left);
    }
    else if (node.type == LABEL) {
        std::cerr << "Labeling found outside of blocks or statements at " << node.token << "!\n";
        throw TUPLE_ERROR;
    }
    else if (node.type == SEPARATOR) {
        std::cerr << "Separator found outside of blocks at " << node.token << "!\n";
        throw TUPLE_ERROR;
    }
    else if (node.type == STATEMENT) {
        Expr *e = new Expr(STATEMENT, node.token, node.text);
        
        fill_statement(e, nodes, node.right);
        
        return e;
    }
    else if (node.type == DECLARATION) {
        Expr *e = new Expr(DECLARATION, node.token, node.text);
        if (node.right >= 0)
            fill_arguments(e, nodes, node.right);
        //Expr *f = tupleize(nodes, node.right);
        //e->set_pivot(f);  // Yes, the type/value will be stored in the pivot field
        return e;
    }
    else if (node.type == IDENTIFIER) {
        Expr *e = new Expr(IDENTIFIER, node.token, node.text);
    
        if (node.left >= 0) {
            Expr *l = tupleize(nodes, node.left);
            e->set_pivot(l);
        }
        
        if (node.right >= 0) {
            fill_arguments(e, nodes, node.right);
        }

        // Special handling for unary operators, move the first positional parameter to pivot
        if ((node.text == "minus" || node.text == "tilde") && node.left < 0 && node.right >= 0) {
            if (e->args.size() == 0) {
                std::cerr << "Unary operator needs a positional argument!\n";
                throw TUPLE_ERROR;
            }
            
            e->text = "unary_" + e->text;
            Expr *p = e->args[0].release();
            e->args.erase(e->args.begin());
            e->set_pivot(p);
        }

        return e;
    }
    else if (node.type == NUMBER) {
        return new Expr(NUMBER, node.token, node.text);
    }
    else if (node.type == STRING) {
        return new Expr(STRING, node.token, node.text);
    }
    else {
        std::cerr << "Can't tupleize this now " << node.token << "!\n";
        throw INTERNAL_ERROR;
    }
}


Expr *tupleize(std::vector<Node> nodes) {
    Expr *root = new Expr(OPEN, Token("", 0, 0));
    
    fill_arguments(root, nodes, 0);
    
    return root;
}


void print_expr_tree(Expr *e, int indent, const char *prefix) {
    for (int j=0; j<indent; j++)
        std::cerr << " ";
        
    std::cerr << prefix << " " << print_node_type(e->type) << " " << e->text << "\n";
    
    if (e->pivot)
        print_expr_tree(e->pivot.get(), indent + 2, "$");
        
    for (auto &x : e->args)
        print_expr_tree(x.get(), indent + 2, "#");
            
    for (auto &kv : e->kwargs)
        print_expr_tree(kv.second.get(), indent + 2, kv.first.c_str());
}
