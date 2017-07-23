
// Stage 3


class Expr {
public:
    enum ExprType {
        TUPLE, NUMBER, STRING, INITIALIZER, IDENTIFIER, CONTROL, DECLARATION
    } type;
    //NodeType type;
    Token token;
    std::string text;
    
    std::unique_ptr<Expr> pivot;
    std::vector<std::unique_ptr<Expr>> args;
    std::map<std::string, std::unique_ptr<Expr>> kwargs;
    
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

    const char *print_type() {
        return (
            type == TUPLE ? "TUPLE" :
            type == NUMBER ? "NUMBER" :
            type == STRING ? "STRING" :
            type == INITIALIZER ? "INITIALIZER" :
            type == IDENTIFIER ? "IDENTIFIER" :
            type == CONTROL ? "CONTROL" :
            type == DECLARATION ? "DECLARATION" :
            throw TUPLE_ERROR
        );
    }
};


Expr *tupleize(std::vector<Node> nodes, int i);


void fill_arguments(Expr *e, std::vector<Node> nodes, int i) {
    Node &node = nodes[i];
    //std::cerr << "Fill: " << print_node_type(node.type) << " " << node.text << "\n";

    if (node.type == Node::OPEN) {
        fill_arguments(e, nodes, node.right);
    }
    else if (node.type == Node::CLOSE) {
        fill_arguments(e, nodes, node.left);
    }
    else if (node.type == Node::SEPARATOR) {
        fill_arguments(e, nodes, node.left);
        fill_arguments(e, nodes, node.right);
    }
    else if (node.type == Node::LABEL) {
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


void fill_control(Expr *e, std::vector<Node> nodes, int i) {
    Node &node = nodes[i];
    //std::cerr << "Fill control: " << print_node_type(node.type) << " " << node.text << "\n";

    if (node.type == Node::OPEN) {
        // Positional argument block
        Expr *f = new Expr(Expr::TUPLE, node.token);
        fill_arguments(f, nodes, node.right);
        e->set_pivot(f);
    }
    else if (node.type == Node::CLOSE) {
        std::cerr << "CLOSE while filling a control!\n";
        throw INTERNAL_ERROR;
    }
    else if (node.type == Node::SEPARATOR) {
        std::cerr << "SEPARATOR while filling a control!\n";
        throw INTERNAL_ERROR;
    }
    else if (node.type == Node::LABEL) {
        if (node.left >= 0)
            fill_control(e, nodes, node.left);
            
        Expr *f = new Expr(Expr::TUPLE, node.token);
        fill_arguments(f, nodes, node.right);
        e->add_kwarg(node.text, f);
    }
    else {
        // Positional argument expression, enclose for consistency
        Expr *f = new Expr(Expr::TUPLE, node.token);
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
    
    if (node.type == Node::OPEN) {
        return tupleize(nodes, node.right);
    }
    else if (node.type == Node::CLOSE) {
        return tupleize(nodes, node.left);
    }
    else if (node.type == Node::LABEL) {
        std::cerr << "Labeling found outside of blocks or controls at " << node.token << "!\n";
        throw TUPLE_ERROR;
    }
    else if (node.type == Node::SEPARATOR) {
        std::cerr << "Separator found outside of blocks at " << node.token << "!\n";
        throw TUPLE_ERROR;
    }
    else if (node.type == Node::CONTROL) {
        Expr *e = new Expr(Expr::CONTROL, node.token, node.text);
        
        fill_control(e, nodes, node.right);
        
        return e;
    }
    else if (node.type == Node::DECLARATION) {
        Expr *e = new Expr(Expr::DECLARATION, node.token, node.text);
        if (node.right >= 0)
            fill_arguments(e, nodes, node.right);
        //Expr *f = tupleize(nodes, node.right);
        //e->set_pivot(f);  // Yes, the type/value will be stored in the pivot field
        return e;
    }
    else if (node.type == Node::IDENTIFIER) {
        // Special handling for some operators
        if (node.fore != TEXTUAL) {
            bool swap = false;

            if (node.text == "logical and")
                swap = true;
                
            if ((node.text == "minus" || node.text == "tilde") && node.left < 0 && node.right >= 0) {
                node.text = "unary_" + node.text;
                swap = true;
            }
            
            if (swap) {
                int x = node.left;
                node.left = node.right;
                node.right = x;
            }
        }

        Expr *e = new Expr(Expr::IDENTIFIER, node.token, node.text);
    
        if (node.left >= 0) {
            Expr *l = tupleize(nodes, node.left);
            e->set_pivot(l);
        }
        
        if (node.right >= 0) {
            fill_arguments(e, nodes, node.right);
        }

        return e;
    }
    else if (node.type == Node::NUMBER) {
        return new Expr(Expr::NUMBER, node.token, node.text);
    }
    else if (node.type == Node::STRING) {
        return new Expr(Expr::STRING, node.token, node.text);
    }
    else {
        std::cerr << "Can't tupleize this now " << node.token << "!\n";
        throw INTERNAL_ERROR;
    }
}


Expr *tupleize(std::vector<Node> nodes) {
    Expr *root = new Expr(Expr::TUPLE, Token("", 0, 0));
    
    fill_arguments(root, nodes, 0);
    
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
