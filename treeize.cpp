
// Stage 2

enum Precedence {
    BASE,
    SEPARATING, // comma must end controls: :if x then: y else: z, ...
    LABELING,   // must be lower than DECLARING: each: i?
    ASSIGNING,  // must be lower than DECLARING: x? Int = 8
    DECLARING,
    LOGICAL_LOW,    // must be lower than COMPARING
    LOGICAL_MED,
    LOGICAL_HIGH,
    COMPARING,
    ADDITIVE,
    MULTIPLICATIVE,
    EXPONENTIAL,
    UNARY,
    TEXTUAL,
    LITERAL
};


const char *print_precedence(Precedence p) {
    return (
        p == BASE ? "BASE" :
        p == SEPARATING ? "SEPARATING" :
        p == LABELING ? "LABELING" :
        p == ASSIGNING ? "ASSIGNING" :
        p == DECLARING ? "DECLARING" :
        p == LOGICAL_LOW ? "LOGICAL_LOW" :
        p == LOGICAL_MED ? "LOGICAL_MED" :
        p == LOGICAL_HIGH ? "LOGICAL_HIGH" :
        p == COMPARING ? "COMPARING" :
        p == ADDITIVE ? "ADDITIVE" :
        p == MULTIPLICATIVE ? "MULTIPLICATIVE" :
        p == EXPONENTIAL ? "EXPONENTIAL" :
        p == UNARY ? "UNARY" :
        p == TEXTUAL ? "TEXTUAL" :
        p == LITERAL ? "LITERAL" :
        "???"
    );
}


bool is_right_associative(Precedence) {
    return false;
}


class Node {
public:
    enum NodeType {
        OPEN, CLOSE,
        NUMBER, STRING, INITIALIZER,
        IDENTIFIER, LABEL, CONTROL, DECLARATION,
        SEPARATOR
    } type;
    std::string text;
    Precedence back, fore;
    int left;
    int right;
    int up;
    Token token;
    
    Node(NodeType type, const std::string &text, Precedence back, Precedence fore, Token token) {
        this->type = type;
        this->text = text;
        this->back = back;
        this->fore = fore;
        this->token = token;
        
        left = 0;
        right = 0;
        up = 0;
    }
    
    //std::ostream &operator<<(std::ostream &os, NodeType type) {
    const char *print_type() {
        return (
            type == OPEN ? "OPEN" :
            type == CLOSE ? "CLOSE" :
            type == NUMBER ? "NUMBER" :
            type == STRING ? "STRING" :
            type == INITIALIZER ? "INITIALIZER" :
            type == IDENTIFIER ? "IDENTIFIER" :
            type == LABEL ? "LABEL" :
            type == CONTROL ? "CONTROL" :
            type == DECLARATION ? "DECLARATION" :
            type == SEPARATOR ? "SEPARATOR" :
            throw TREE_ERROR
        );
    }
};


enum Paren {
    PAREN, BRACKET, BRACE, BLOCK, UNIT
};


struct {
    const char *token;
    const char *text;
    Precedence precedence;
} operators[] = {
    { "**",  "_exponent", EXPONENTIAL },  // TODO: do we need this?
    { "~",   "_tilde", EXPONENTIAL },
    { "<<",  "_shift_left", EXPONENTIAL },
    { ">>",  "_shift_right", EXPONENTIAL },

    { "*",   "_star", MULTIPLICATIVE },
    { "/",   "_slash", MULTIPLICATIVE },
    { "%",   "_percent", MULTIPLICATIVE },
    { "&",   "_and", MULTIPLICATIVE },

    { "+",   "_plus", ADDITIVE },
    { "-",   "_minus", ADDITIVE },
    { "|",   "_or", ADDITIVE },
    { "^",   "_xor", ADDITIVE },

    { "==",  "is_equal", COMPARING },
    { "!=",  "not_equal", COMPARING },
    { "<",   "is_less", COMPARING },
    { ">",   "is_greater", COMPARING },
    { "<=",  "not_greater", COMPARING },
    { ">=",  "not_less", COMPARING },
    //{ "<>",  "not comparable", COMPARING },
    //{ "<=>", "ordering", COMPARING },

    { "!",   "logical not", LOGICAL_HIGH },  // TODO: or make it ~~, and != into ~=?
    
    { "&&",  "logical and", LOGICAL_MED },
    
    { "||",  "logical or", LOGICAL_LOW },
    { "^^",  "logical xor", LOGICAL_LOW },
    
    { "=",   "assign", ASSIGNING },
    { "+=",  "assign_plus", ASSIGNING },
    { "-=",  "assign_minus", ASSIGNING },
    { "*=",  "assign_star", ASSIGNING },
    { "/=",  "assign_slash", ASSIGNING },
    { "%=",  "assign_percent", ASSIGNING },
    { "&=",  "assign_and", ASSIGNING },
    { "|=",  "assign_or", ASSIGNING },
    { "^=",  "assign_xor", ASSIGNING },
    { "<<=", "assign_shift_left", ASSIGNING },
    { ">>=", "assign_shift_right", ASSIGNING },
    //{ "", "",  },
};


void print_node(std::vector<Node> &nodes, int i, int indent, const char *prefix) {
    if (nodes[i].left)
        print_node(nodes, nodes[i].left, indent + 2, "/ ");
    
    //std::cerr << i << "(" << nodes[i].left << "/" << nodes[i].right << ")\n";
    for (int j=0; j<indent; j++)
        std::cerr << " ";
        
    std::cerr << prefix << "[" << nodes[i].print_type() << "] " << nodes[i].text << "\n";
    
    if (nodes[i].right)
        print_node(nodes, nodes[i].right, indent + 2, "\\ ");
}


// The handling of parentheses will look like this:
//
//          CONTENTS
//                  |
//                   CLOSE
//                  /
//              OPEN
//             /
// When the matching CLOSE is found, the precedence of the OPEN
// is set to the precedence of the CLOSE.

std::vector<Node> treeize(std::vector<Token> tokens) {
    std::vector<Node> nodes;
    std::vector<Paren> parens;
    
    if (tokens.front().text != " indent") {
        std::cerr << "Onodes?\n";  // WTF?
        throw TREE_ERROR;
    }
    
    for (auto token : tokens) {
        Node::NodeType type;
        Precedence back, fore;
        std::string text;
        
        char c = token.text[0];
        
        if (isdigit(c) || c == '.') {
            type = Node::NUMBER;
            back = LITERAL;
            fore = LITERAL;
            text = token.text;
        }
        else if (c == ':') {
            type = Node::CONTROL;
            back = LITERAL;
            fore = SEPARATING;
            text = token.text.substr(1);
        }
        else if (c == '`') {
            type = Node::INITIALIZER;
            back = TEXTUAL;
            fore = TEXTUAL;
            text = token.text.substr(1);
        }
        else if (isalpha(c) || c == '_') {
            if (token.text.back() == ':') {
                type = Node::LABEL;
                back = LABELING;
                fore = LABELING;
                text = token.text.substr(0, token.text.length() - 1);
            }
            else if (token.text.back() == '?') {
                type = Node::DECLARATION;
                back = DECLARING;
                fore = DECLARING;
                text = token.text.substr(0, token.text.length() - 1);
            }
            else {
                type = Node::IDENTIFIER;
                back = TEXTUAL;
                fore = TEXTUAL;
                text = token.text;
            }
        }
        else if (is_quote(c)) {
            type = Node::STRING;
            back = LITERAL;
            fore = LITERAL;
            text = token.text.substr(1, token.text.length() - 2);
        }
        else if (c == ',') {
            type = Node::SEPARATOR;
            back = SEPARATING;
            fore = SEPARATING;
        }
        else if (is_paren(c)) {
            if (c == '(') {
                parens.push_back(PAREN);
                
                type = Node::OPEN;
                back = LITERAL;
                fore = BASE;
            }
            else if (c == ')') {
                if (parens.back() != PAREN) {
                    std::cerr << "Mismatched " << token.text << "!\n";
                    throw TREE_ERROR;
                }
                    
                parens.pop_back();
                
                type = Node::CLOSE;
                back = BASE;
                fore = LITERAL;
            }
            else if (c == '[') {
                parens.push_back(BRACKET);
                
                type = Node::IDENTIFIER;
                back = TEXTUAL;
                fore = BASE;
                text = "index";
            }
            else if (c == ']') {
                if (parens.back() != BRACKET) {
                    std::cerr << "Mismatched " << token.text << "!\n";
                    throw TREE_ERROR;
                }
                    
                parens.pop_back();
                
                type = Node::CLOSE;
                back = BASE;
                fore = LITERAL;
            }
            else if (c == '{') {
                parens.push_back(BRACE);
                
                type = Node::INITIALIZER;
                back = TEXTUAL;
                fore = BASE;
            }
            else if (c == '}') {
                if (parens.back() != BRACE) {
                    std::cerr << "Mismatched " << token.text << "!\n";
                    throw TREE_ERROR;
                }
                    
                parens.pop_back();
                
                type = Node::CLOSE;
                back = BASE;
                fore = LITERAL;
            }
        }
        else if (c == ' ') {
            if (token.text == " indent") {
                parens.push_back(BLOCK);
                
                type = Node::OPEN;
                back = LITERAL;
                fore = BASE;
            }
            else if (token.text == " dedent") {
                if (parens.back() != BLOCK) {
                    std::cerr << "Mismatched " << token.text << "!\n";
                    throw TREE_ERROR;
                }
                    
                parens.pop_back();
                
                type = Node::CLOSE;
                back = BASE;
                fore = LITERAL;
            }
            else if (token.text == " separate") {
                if (parens.back() != BLOCK && parens.back() != UNIT) {
                    std::cerr << "Unclosed parentheses " << token.text << "!\n";
                    throw TREE_ERROR;
                }
                
                type = Node::SEPARATOR;
                back = SEPARATING;
                fore = SEPARATING;
            }
            else {
                std::cerr << "Invalid internal thingy " << token.text << "!\n";
                throw INTERNAL_ERROR;
            }
        }
        else {
            for (auto op : operators) {
                if (op.token == token.text) {
                    type = Node::IDENTIFIER;
                    
                    if (nodes.back().fore < UNARY) {
                        if (op.precedence < ADDITIVE) {
                            std::cerr << "Operator " << op.text << " cannot be unary!\n";
                            throw TREE_ERROR;
                        }
                        
                        // This operator either follows another, or the first one
                        // in an expression. We will treat it as a unary prefix operator,
                        // instead of nagging the user for using unreasonable parentheses.
                        text = "unary";
                        text += op.text;
                        back = UNARY;
                        fore = UNARY;
                    }
                    else {
                        if (op.precedence < ADDITIVE)
                            text = op.text;
                        else {
                            text = "binary";
                            text += op.text;
                        }
                        
                        back = op.precedence;
                        fore = op.precedence;
                    }
                    
                    break;
                }
            }
            
            if (!type) {
                std::cerr << "No operator " << token << "!\n";
                throw TREE_ERROR;
            }
        }
        
        int n = nodes.size();
        //std::cerr << "Token " << token.text << " => " << n << "\n";

        nodes.push_back(Node(type, text, back, fore, token));
        
        for (int i = n - 1; i >= 0; i = nodes[i].up) {
            if (nodes[i].fore > back)  // || (nodes[i].precedence == back && !is_right_associative(back)))
                continue;
            else if (nodes[i].fore == back) {
                if (back != BASE)
                    continue;

                // A group is closed. Insert the CLOSE node so that it becomes the last one,
                // and the next operator won't traverse into the enclosed nodes.
                nodes[i].fore = LITERAL;
            }

            // nodes[i] will be our parent, and we'll be its right child
            int x = nodes[i].right;

            nodes[n].left = x;
            if (x)
                nodes[x].up = n;

            nodes[n].up = i;
            nodes[i].right = n;
            
            break;
        }
    }
    
    if (parens.size() != 1) {
        std::cerr << "Paren remained open at the end: ";
        for (auto p : parens)
            std::cerr << "#" << p << " ";
        std::cerr << "\n";
        throw TREE_ERROR;
    }
        
    print_node(nodes, 0, 0, "- ");
    
    return nodes;
}
