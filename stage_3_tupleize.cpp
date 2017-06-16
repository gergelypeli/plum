
// Stage 3


class Expr {
public:
    NodeType type;
    std::string text;
    
    std::unique_ptr<Expr> pivot;
    std::vector<std::unique_ptr<Expr>> args;
    std::map<std::string, std::unique_ptr<Expr>> kwargs;
    
    Expr(NodeType t) {
        type = t;
    }

    Expr(NodeType t, std::string tx) {
        type = t;
        text = tx;
    }
    
    Expr *set_pivot(Expr *p) {
        pivot.reset(p);
        //p->parent = this;
        return this;
    }
    
    Expr *add_arg(Expr *a) {
        if (kwargs.size())
            throw Error("Positional params after keyword!");
            
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
    std::cerr << "Fill: " << print_node_type(node.type) << " " << node.text << "\n";

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
    std::cerr << "Fill statement: " << print_node_type(node.type) << " " << node.text << "\n";

    if (node.type == OPEN) {
        // Positional argument block
        Expr *f = new Expr(OPEN);
        fill_arguments(f, nodes, node.right);
        e->set_pivot(f);
    }
    else if (node.type == CLOSE) {
        throw Error("CLOSE while filling a statement!");
    }
    else if (node.type == SEPARATOR) {
        throw Error("SEPARATOR while filling a statement!");
    }
    else if (node.type == LABEL) {
        if (node.left >= 0)
            fill_statement(e, nodes, node.left);
            
        Expr *f = new Expr(OPEN);
        fill_arguments(f, nodes, node.right);
        e->add_kwarg(node.text, f);
    }
    else {
        // Positional argument expression, enclose for consistency
        Expr *f = new Expr(OPEN);
        fill_arguments(f, nodes, i);
        e->set_pivot(f);
    }
}


Expr *tupleize(std::vector<Node> nodes, int i) {
    if (i < 0)
        throw Error("Eiii!");
        
    Node &node = nodes[i];
    std::cerr << "Tupleize: " << print_node_type(node.type) << " " << node.text << "\n";
    
    if (node.type == OPEN) {
        return tupleize(nodes, node.right);
        //Expr *e = new Expr(OPEN);
        //fill(e, nodes, node.right);
        //return e;
    }
    else if (node.type == CLOSE) {
        return tupleize(nodes, node.left);
    }
    else if (node.type == LABEL) {
        throw Error("Labeling found outside of blocks or statements!");
    }
    else if (node.type == SEPARATOR) {
        throw Error("Separator found outside of blocks!");
    }
    else if (node.type == STATEMENT) {
        Expr *e = new Expr(STATEMENT, node.text);
        
        fill_statement(e, nodes, node.right);
        
        return e;
    }
    else if (node.type == DECLARATION) {
        Expr *e = new Expr(DECLARATION, node.text);
        Expr *f = tupleize(nodes, node.right);
        e->set_pivot(f);  // Yes, the type/value will be stored in the pivot field
        return e;
    }
    else if (node.type == IDENTIFIER) {
        Expr *e = new Expr(IDENTIFIER, node.text);
    
        if (node.left >= 0) {
            Expr *l = tupleize(nodes, node.left);
            e->set_pivot(l);
        }
        
        if (node.right >= 0) {
            fill_arguments(e, nodes, node.right);
        }

        return e;
    }
    else if (node.type == NUMBER) {
        return new Expr(NUMBER, node.text);
    }
    else
        throw Error("Can't tupleize this now %d!", node.type);
}


Expr *tupleize(std::vector<Node> nodes) {
    Expr *root = new Expr(OPEN);
    
    fill_arguments(root, nodes, 0);
    
    return root;
}


void print_expr_tree(Expr *e, int indent, const char *prefix) {
    for (int j=0; j<indent; j++)
        std::cout << " ";
        
    std::cout << prefix << " " << print_node_type(e->type) << " " << e->text << "\n";
    
    if (e->pivot)
        print_expr_tree(e->pivot.get(), indent + 2, "$");
        
    for (auto &x : e->args)
        print_expr_tree(x.get(), indent + 2, "#");
            
    for (auto &kv : e->kwargs)
        print_expr_tree(kv.second.get(), indent + 2, kv.first.c_str());
}
