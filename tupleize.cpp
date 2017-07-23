
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


Expr *tupleize(std::vector<Node> &nodes, int i);


void tupleize_into(Expr *e, std::vector<Node> &nodes, int i) {
    Node &node = nodes[i];
    //std::cerr << "Fill: " << print_node_type(node.type) << " " << node.text << "\n";

    if (node.type == Node::OPEN) {
        tupleize_into(e, nodes, node.right);
    }
    else if (node.type == Node::CLOSE) {
        tupleize_into(e, nodes, node.left);
    }
    else if (node.type == Node::SEPARATOR) {
        tupleize_into(e, nodes, node.left);
        tupleize_into(e, nodes, node.right);
    }
    else if (node.type == Node::LABEL) {
        if (node.left)
            tupleize_into(e, nodes, node.left);  // May be needed in a block of labels
            
        Expr *f = tupleize(nodes, node.right);
        e->add_kwarg(node.text, f);
    }
    else {
        Expr *f = tupleize(nodes, i);
        e->add_arg(f);
    }
}


Expr *tupleize(std::vector<Node> &nodes, int i) {
    if (i < 0) {
        std::cerr << "Eiii!\n";
        throw INTERNAL_ERROR;
    }
        
    Node &node = nodes[i];
    //std::cerr << "Tupleize: " << print_node_type(node.type) << " " << node.text << "\n";
    
    if (node.type == Node::OPEN) {
        // Grouping operators are ignored from now on
        return tupleize(nodes, node.right);
    }
    else if (node.type == Node::CLOSE) {
        // Grouping operators are ignored from now on
        return tupleize(nodes, node.left);
    }
    else if (node.type == Node::LABEL) {
        Expr *e = new Expr(Expr::TUPLE, node.token);
        
        if (node.left)
            tupleize_into(e, nodes, node.left);
            
        Expr *f = tupleize(nodes, node.right);
        e->add_kwarg(node.text, f);
        return e;
    }
    else if (node.type == Node::SEPARATOR) {
        Expr *e = new Expr(Expr::TUPLE, node.token);
        
        tupleize_into(e, nodes, node.left);
        tupleize_into(e, nodes, node.right);
        
        return e;
    }
    else if (node.type == Node::CONTROL) {
        Expr *e = new Expr(Expr::CONTROL, node.token, node.text);
        
        if (node.left)
            throw TUPLE_ERROR;
            
        if (node.right) {
            tupleize_into(e, nodes, node.right);
            
            if (e->args.size() > 1)
                throw INTERNAL_ERROR;  // This should be impossible by the precedence rules
            else if (e->args.size() == 1) {
                e->set_pivot(e->args.back().release());
                e->args.pop_back();
            }
        }
        
        return e;
    }
    else if (node.type == Node::DECLARATION) {
        Expr *e = new Expr(Expr::DECLARATION, node.token, node.text);
        
        if (node.left)
            throw TUPLE_ERROR;
        
        if (node.right)
            tupleize_into(e, nodes, node.right);
            
        return e;
    }
    else if (node.type == Node::IDENTIFIER) {
        // Special handling for some operators
        if (node.fore != TEXTUAL) {
            bool swap = false;

            if (node.text == "logical and")
                swap = true;
                
            if ((node.text == "minus" || node.text == "tilde") && !node.left && !node.right) {
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
    
        if (node.left) {
            Expr *l = tupleize(nodes, node.left);
            e->set_pivot(l);
        }
        
        if (node.right) {
            tupleize_into(e, nodes, node.right);
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
    
    tupleize_into(root, nodes, 0);
    
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
