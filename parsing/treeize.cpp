
// Stage 2

enum Precedence {
    OPENING,
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
    REGULAR,
    CLOSING
};


const char *print_precedence(Precedence p) {
    return (
        p == OPENING ? "OPENING" :
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
        p == REGULAR ? "REGULAR" :
        p == CLOSING ? "CLOSING" :
        "???"
    );
}


bool is_right_associative(Precedence) {
    return false;
}


class Node {
public:
    enum NodeType {
        NONE, OPEN, CLOSE,
        UNSIGNED_NUMBER, STRING,
        INITIALIZER, MATCHER,
        IDENTIFIER, LABEL, CONTROL, EVAL, DECLARATION,
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
            type == UNSIGNED_NUMBER ? "UNSIGNED_NUMBER" :
            type == STRING ? "STRING" :
            type == INITIALIZER ? "INITIALIZER" :
            type == MATCHER ? "MATCHER" :
            type == IDENTIFIER ? "IDENTIFIER" :
            type == LABEL ? "LABEL" :
            type == CONTROL ? "CONTROL" :
            type == EVAL ? "EVAL" :
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
    std::ustring token;
    std::string text;
    Precedence precedence;
} operators[] = {
    { "**",  "_exponent", EXPONENTIAL },  // TODO: do we need this?
    { "!",   "_not", EXPONENTIAL },
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
    { "<=>", "compare", COMPARING },

    { "!!",   "logical not", LOGICAL_HIGH },  // TODO: or make it ~~, and != into ~=?
    
    { "&&",  "logical and", LOGICAL_MED },
    
    { "||",  "logical or", LOGICAL_LOW },
    { "^^",  "logical xor", LOGICAL_LOW },
    
    { "=",   "assign other", ASSIGNING },
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
    
    if (tokens.front().utext != INDENT_UTEXT) {
        std::cerr << "Onodes?\n";  // WTF?
        throw TREE_ERROR;
    }
    
    for (auto token : tokens) {
        Node::NodeType type;
        Precedence back, fore;
        std::string text;  // Non-ASCII characters were only accepted in string literals
        
        unsigned16 c = token.utext[0];
        
        if (isdigit(c)) {
            type = Node::UNSIGNED_NUMBER;
            back = REGULAR;
            fore = REGULAR;
            text = encode_ascii(token.utext);
        }
        else if (c == ':') {
            type = Node::CONTROL;
            back = REGULAR;
            fore = SEPARATING;
            text = encode_ascii(token.utext.substr(1));
        }
        else if (c == '`') {
            type = Node::INITIALIZER;
            back = REGULAR;
            fore = REGULAR;
            text = encode_ascii(token.utext.substr(1));
        }
        else if (c == '~') {
            if (token.utext[1] == '{') {
                parens.push_back(BRACE);
                
                type = Node::MATCHER;
                back = COMPARING;
                fore = OPENING;
                text = "{";
            }
            else {
                type = Node::MATCHER;
                back = COMPARING;
                fore = COMPARING;
                text = encode_ascii(token.utext.substr(1));
            }
        }
        else if (c == '?') {
            type = Node::DECLARATION;
            back = DECLARING;
            fore = DECLARING;
            text = "";
        }
        else if (isalpha(c) || c == '_' || c == '$' || c == '@' || c == '.') {
            if (token.utext.back() == ':') {
                if (islower(c)) {
                    type = Node::LABEL;
                    back = LABELING;
                    fore = LABELING;
                    text = encode_ascii(token.utext.substr(0, token.utext.length() - 1));
                }
                else {
                    type = Node::EVAL;
                    back = REGULAR;
                    fore = SEPARATING;
                    text = encode_ascii(token.utext.substr(0, token.utext.size() - 1));
                }
            }
            else if (token.utext.back() == '?') {
                type = Node::DECLARATION;
                back = DECLARING;
                fore = DECLARING;
                text = encode_ascii(token.utext.substr(0, token.utext.length() - 1));
            }
            else {
                type = Node::IDENTIFIER;
                back = REGULAR;
                fore = REGULAR;
                text = encode_ascii(token.utext);
            }
        }
        else if (Tokenizer::is_quote(c)) {
            type = Node::STRING;
            back = REGULAR;
            fore = REGULAR;
            text = "";  // token.utext.substr(1, token.utext.length() - 2);
        }
        else if (c == ',') {
            type = Node::SEPARATOR;
            back = SEPARATING;
            fore = SEPARATING;
        }
        else if (Tokenizer::is_paren(c)) {
            if (c == '(') {
                parens.push_back(PAREN);
                
                type = Node::OPEN;
                back = CLOSING;
                fore = OPENING;
            }
            else if (c == ')') {
                if (parens.back() != PAREN) {
                    std::cerr << "Mismatched paren: " << token << "\n";
                    throw TREE_ERROR;
                }
                    
                parens.pop_back();
                
                type = Node::CLOSE;
                back = OPENING;
                fore = CLOSING;
            }
            else if (c == '[') {
                parens.push_back(BRACKET);
                
                type = Node::IDENTIFIER;
                back = REGULAR;
                fore = OPENING;
                text = "index";
            }
            else if (c == ']') {
                if (parens.back() != BRACKET) {
                    std::cerr << "Mismatched bracket: " << token << "!\n";
                    throw TREE_ERROR;
                }
                    
                parens.pop_back();
                
                type = Node::CLOSE;
                back = OPENING;
                fore = CLOSING;
            }
            else if (c == '{') {
                parens.push_back(BRACE);
                
                type = Node::INITIALIZER;
                back = REGULAR;
                fore = OPENING;
            }
            else if (c == '}') {
                if (parens.back() != BRACE) {
                    std::cerr << "Mismatched brace: " << token << "!\n";
                    throw TREE_ERROR;
                }
                    
                parens.pop_back();
                
                type = Node::CLOSE;
                back = OPENING;
                fore = CLOSING;
            }
        }
        else if (c == ' ') {
            if (token.utext == INDENT_UTEXT) {
                parens.push_back(BLOCK);
                
                type = Node::OPEN;
                back = CLOSING;
                fore = OPENING;
            }
            else if (token.utext == DEDENT_UTEXT) {
                if (parens.back() != BLOCK) {
                    std::cerr << "Unclosed grouping: " << token << "!\n";
                    throw TREE_ERROR;
                }
                    
                parens.pop_back();
                
                type = Node::CLOSE;
                back = OPENING;
                fore = CLOSING;
            }
            else if (token.utext == SEPARATE_UTEXT) {
                if (parens.back() != BLOCK && parens.back() != UNIT) {
                    std::cerr << "Unclosed grouping: " << token << "!\n";
                    throw TREE_ERROR;
                }
                
                type = Node::SEPARATOR;
                back = SEPARATING;
                fore = SEPARATING;
            }
            else {
                std::cerr << "Invalid internal thingy " << token << "!\n";
                throw INTERNAL_ERROR;
            }
        }
        else {
            type = Node::NONE;
            
            for (auto op : operators) {
                if (op.token == token.utext) {
                    type = Node::IDENTIFIER;
                    bool dual = (op.precedence == ADDITIVE || op.precedence == MULTIPLICATIVE || op.precedence == EXPONENTIAL);
                    
                    if (nodes.back().fore < UNARY && !(nodes.back().fore == DECLARING && op.precedence == ASSIGNING)) {
                        // This operator either follows another, or the first one
                        // in an expression. We will treat it as a unary prefix operator,
                        // instead of nagging the user for using unreasonable parentheses.
                        
                        if (op.precedence == LOGICAL_HIGH) {
                            text = op.text;
                            back = op.precedence;
                            fore = op.precedence;
                        }
                        else if (dual) {
                            text = "unary";
                            text += op.text;
                            back = UNARY;
                            fore = UNARY;
                        }
                        else {
                            std::cerr << "Operator " << op.text << " cannot be unary: " << token << "\n";
                            throw TREE_ERROR;
                        }
                    }
                    else {
                        if (dual) {
                            text = "binary";
                            text += op.text;
                        }
                        else
                            text = op.text;
                        
                        back = op.precedence;
                        fore = op.precedence;
                    }
                    
                    break;
                }
            }
            
            if (type == Node::NONE) {
                std::cerr << "No operator " << token << "!\n";
                throw TREE_ERROR;
            }
        }
        
        int n = nodes.size();
        //std::cerr << "Token " << token.utext << " => " << n << "\n";

        nodes.push_back(Node(type, text, back, fore, token));
        
        for (int i = n - 1; i >= 0; i = nodes[i].up) {
            if (nodes[i].fore > back)  // || (nodes[i].precedence == back && !is_right_associative(back)))
                continue;
            else if (nodes[i].fore == back) {
                if (back != OPENING)
                    continue;

                // A group is closed. Insert the CLOSE node so that it becomes the last one,
                // and the next operator won't traverse into the enclosed nodes.
                nodes[i].fore = CLOSING;
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
