
// Stage 2

enum Precedence {
    BASE,
    SEPARATING, // comma must end statements: :if x then: y else: z, ...
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
        p == TEXTUAL ? "TEXTUAL" :
        p == LITERAL ? "LITERAL" :
        "???"
    );
}


bool is_right_associative(Precedence) {
    return false;
}


enum NodeType {
    OPEN, CLOSE,
    NUMBER, STRING, INITIALIZER,
    IDENTIFIER, LABEL, STATEMENT, DECLARATION,
    SEPARATOR
};


const char *print_node_type(NodeType type) {
    return (
        type == OPEN ? "OPEN" :
        type == CLOSE ? "CLOSE" :
        type == NUMBER ? "NUMBER" :
        type == STRING ? "STRING" :
        type == INITIALIZER ? "INITIALIZER" :
        type == IDENTIFIER ? "IDENTIFIER" :
        type == LABEL ? "LABEL" :
        type == STATEMENT ? "STATEMENT" :
        type == DECLARATION ? "DECLARATION" :
        type == SEPARATOR ? "SEPARATOR" :
        "???"
    );
}


class Node {
public:
    NodeType type;
    std::string text;
    Precedence back, fore;
    int left;
    int right;
    Token token;
    
    Node(NodeType type, const std::string &text, Precedence back, Precedence fore, Token token) {
        this->type = type;
        this->text = text;
        this->back = back;
        this->fore = fore;
        this->token = token;
        
        left = -1;
        right = -1;
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
    { "**",  "exponent", EXPONENTIAL },  // TODO: do we need this?
    { "~",   "tilde", EXPONENTIAL },  // Special handling
    { "<<",  "shift_left", EXPONENTIAL },
    { ">>",  "shift_right", EXPONENTIAL },

    { "*",   "star", MULTIPLICATIVE },
    { "/",   "slash", MULTIPLICATIVE },
    { "%",   "percent", MULTIPLICATIVE },
    { "&",   "and", MULTIPLICATIVE },

    { "+",   "plus", ADDITIVE },
    { "-",   "minus", ADDITIVE },  // Special handling
    { "|",   "or", ADDITIVE },
    { "^",   "xor", ADDITIVE },

    { "==",  "equal", COMPARING },
    { "!=",  "not_equal", COMPARING },
    { "<",   "less", COMPARING },
    { ">",   "greater", COMPARING },
    { "<=",  "less_equal", COMPARING },
    { ">=",  "greater_equal", COMPARING },
    { "<>",  "incomparable", COMPARING },
    { "<=>", "compare", COMPARING },

    { "!",   "logical not", LOGICAL_HIGH },  // TODO: or make it ~~, and != into ~=?
    
    { "&&",  "logical and", LOGICAL_MED },
    
    { "||",  "logical or", LOGICAL_LOW },
    { "^^",  "logical xor", LOGICAL_LOW },
    
    { "=",   "assign", ASSIGNING },
    { "+=",  "plus_assign", ASSIGNING },
    { "-=",  "minus_assign", ASSIGNING },
    { "*=",  "star_assign", ASSIGNING },
    { "/=",  "slash_assign", ASSIGNING },
    { "%=",  "percent_assign", ASSIGNING },
    { "&=",  "and_assign", ASSIGNING },
    { "|=",  "or_assign", ASSIGNING },
    { "^=",  "xor_assign", ASSIGNING },
    { "<<=",  "shift_left_assign", ASSIGNING },
    { ">>=",  "shift_right_assign", ASSIGNING },
    //{ "", "",  },
};


void print_node(std::vector<Node> &nodes, int i, int indent, const char *prefix) {
    if (nodes[i].left >= 0)
        print_node(nodes, nodes[i].left, indent + 2, "/ ");
    
    //std::cerr << i << "(" << nodes[i].left << "/" << nodes[i].right << ")\n";
    for (int j=0; j<indent; j++)
        std::cerr << " ";
        
    std::cerr << prefix << "[" << print_node_type(nodes[i].type) << "] " << nodes[i].text << "\n";
    
    if (nodes[i].right >= 0)
        print_node(nodes, nodes[i].right, indent + 2, "\\ ");
}


std::vector<Node> treeize(std::vector<Token> tokens) {
    std::vector<Node> nodes;
    std::vector<Paren> parens;
    
    if (tokens.front().text != " indent") {
        std::cerr << "Onodes?\n";  // WTF?
        throw TREE_ERROR;
    }
    
    for (auto token : tokens) {
        NodeType type;
        Precedence back, fore;
        std::string text;
        
        char c = token.text[0];
        
        if (isdigit(c) || c == '.') {
            type = NUMBER;
            back = fore = LITERAL;
            text = token.text;
        }
        else if (c == ':') {
            type = STATEMENT;
            back = LITERAL;
            fore = SEPARATING;
            text = token.text.substr(1);
        }
        else if (isalpha(c) || c == '_') {
            if (token.text.back() == ':') {
                type = LABEL;
                back = LABELING;
                fore = LABELING;
                text = token.text.substr(0, token.text.length() - 1);
            }
            else if (token.text.back() == '?') {
                type = DECLARATION;
                back = DECLARING;
                fore = DECLARING;
                text = token.text.substr(0, token.text.length() - 1);
            }
            else {
                type = IDENTIFIER;
                back = TEXTUAL;
                fore = TEXTUAL;
                text = token.text;
            }
        }
        else if (is_quote(c)) {
            type = STRING;
            back = LITERAL;
            fore = LITERAL;
            text = token.text.substr(1, token.text.length() - 1);
        }
        else if (c == ',') {
            type = SEPARATOR;
            back = SEPARATING;
            fore = SEPARATING;
        }
        else if (is_paren(c)) {
            if (c == '(') {
                parens.push_back(PAREN);
                
                type = OPEN;
                back = LITERAL;
                fore = BASE;
            }
            else if (c == ')') {
                if (parens.back() != PAREN) {
                    std::cerr << "Mismatched " << token.text << "!\n";
                    throw TREE_ERROR;
                }
                    
                parens.pop_back();
                
                type = CLOSE;
                back = BASE;
                fore = LITERAL;
            }
            else if (c == '[') {
                parens.push_back(BRACKET);
                
                type = IDENTIFIER;
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
                
                type = CLOSE;
                back = BASE;
                fore = LITERAL;
            }
            else if (c == '{') {
                parens.push_back(BRACE);
                
                type = INITIALIZER;
                back = LITERAL;
                fore = BASE;
            }
            else if (c == '}') {
                if (parens.back() != BRACE) {
                    std::cerr << "Mismatched " << token.text << "!\n";
                    throw TREE_ERROR;
                }
                    
                parens.pop_back();
                
                type = CLOSE;
                back = BASE;
                fore = LITERAL;
            }
        }
        else if (c == ' ') {
            if (token.text == " indent") {
                parens.push_back(BLOCK);
                
                type = OPEN;
                back = LITERAL;
                fore = BASE;
            }
            else if (token.text == " dedent") {
                if (parens.back() != BLOCK) {
                    std::cerr << "Mismatched " << token.text << "!\n";
                    throw TREE_ERROR;
                }
                    
                parens.pop_back();
                
                type = CLOSE;
                back = BASE;
                fore = LITERAL;
            }
            else if (token.text == " separate") {
                if (parens.back() != BLOCK && parens.back() != UNIT) {
                    std::cerr << "Unclosed parentheses " << token.text << "!\n";
                    throw TREE_ERROR;
                }
                
                type = SEPARATOR;
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
                    type = IDENTIFIER;
                    back = op.precedence;
                    fore = op.precedence;
                    text = op.text;
                    break;
                }
            }
            
            if (!type) {
                std::cerr << "No operator " << token << "!\n";
                throw TREE_ERROR;
            }
        }
        
        int n = nodes.size();
        std::cerr << "Token " << token.text << " => " << n << "\n";

        nodes.push_back(Node(type, text, back, fore, token));
        int r = -1;
        
        for (int i = n - 1; i >= 0; i--) {
            if (nodes[i].right != r)
                continue;
                
            r = i;

            if (nodes[i].fore > back)  // || (nodes[i].precedence == back && !is_right_associative(back)))
                continue;
            else if (nodes[i].fore == back) {
                if (back == LITERAL) {
                    std::cerr << "Literals can't follow each other at " << token << "!\n";
                    throw TREE_ERROR;
                }
                else if (back == BASE)
                    nodes[i].fore = nodes[i].back;
                else
                    continue;
            }

            // nodes[i] will be our parent, and we'll be its right child
            nodes[n].left = nodes[i].right;
            nodes[i].right = n;
            break;
        }
    }
    
    if (parens.size() != 1) {
        std::cerr << "Parens remained open at the end!\n";
        throw TREE_ERROR;
    }
        
    print_node(nodes, 0, 0, "- ");
    
    return nodes;
}
