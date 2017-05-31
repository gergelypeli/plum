#include <fstream>
#include <vector>

#include <util.h>


std::string read_source(const char *filename) {
    std::ifstream source(filename, std::ios::binary);
    
    if (!source.is_open())
        return "";
    
    std::string buffer((std::istreambuf_iterator<char>(source)),
                       (std::istreambuf_iterator<char>()));
                       
    buffer.push_back('\0');
    
    return buffer;
}



bool is_paren(char c) {
    return (c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}');
}


bool is_separator(char c) {
    return (c == ';' || c == ',');
}


bool is_quote(char c) {
    return (c == '\'' || c == '"');
}


std::vector<std::string> tokenize(std::string buffer) {
    std::vector<std::string> tokens;
    int i = 0;
    int indent = 0;
    
    while (buffer[i] != '\0') {
        char c = buffer[i];
        int start = i;
        
        if (c == '\n') {
            int start = i;
            
            do {
                i++;
                c = buffer[i];
            } while (c == ' ');
            
            if (c == '\n' || c == '#')
                continue;
                
            int n = i - start - 1;
            int ind = n / 4;
            int mod = n % 4;
            
            if (ind == indent && mod == 0) {
                tokens.push_back(";");  // May be removed by subsequent label
                continue;
            }
            else if (ind == indent && mod == 1) {
                continue;
            }
            else if (ind == indent + 1 && mod == 0) {
                tokens.push_back(" indent");
                indent++;
                continue;
            }
            else if (ind < indent && mod == 0) {
                for (int j=0; j < indent - ind; j++) {
                    tokens.push_back(" dedent");
                    indent--;
                }
                
                tokens.push_back(";");  // May be removed by subsequent label
                continue;
            }

            throw Error("Invalid indentation of %d spaces!", n);
        }
        else if (c == ' ') {
            i++;
            continue;
        }
        else if (c == '#') {
            do {
                i++;
                c = buffer[i];
            } while (c != '\n');
            
            continue;
        }
        else if (isdigit(c) || c == '.') {
            do {
                i++;
                c = buffer[i];
            } while (isdigit(c) || c == '_' || c == '.');
        }
        else if (isalpha(c) || c == '_' || c == ':') {
            bool prefixed = (c == ':');
            
            do {
                i++;
                c = buffer[i];
            } while (isalnum(c) || c == '_' || c == '.');
            
            if (!prefixed && (c == ':' || c == '?')) {
                if (c == ':' && tokens.back() == ";")
                    tokens.pop_back();

                i++;
                c = buffer[i];
            }
        }
        else if (is_paren(c)) {
            i++;
        }
        else if (is_separator(c)) {
            i++;
        }
        else if (is_quote(c)) {
            char quote = c;
            
            do {
                if (c == '\\')
                    i++;
                    
                i++;
                c = buffer[i];
            } while (c != quote);
            
            i++;
        }
        else if (ispunct(c)) {
            do {
                i++;
                c = buffer[i];
            } while (ispunct(c) && !is_paren(c) && !is_separator(c) && !is_quote(c));
        }
        else {
            throw Error("Invalid input character %c!", c);
        }

        tokens.push_back(buffer.substr(start, i - start));
    }
    
    return tokens;
}


enum Precedence {
    BASE,
    SEPARATING, // comma must end statements: :if x then: y else: z, ...
    LABELING,   // must be lower than DECLARING: each: i?
    ASSIGNING,  // must be lower than DECLARING: x? Int = 8
    DECLARING,
    LOGICAL_LOW,    // must be lower than COMPARING
    LOGICAL_HIGH,
    COMPARING,
    ADDITIVE,
    MULTIPLICATIVE,
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
        p == LOGICAL_HIGH ? "LOGICAL_HIGH" :
        p == COMPARING ? "COMPARING" :
        p == ADDITIVE ? "ADDITIVE" :
        p == MULTIPLICATIVE ? "MULTIPLICATIVE" :
        p == TEXTUAL ? "TEXTUAL" :
        p == LITERAL ? "LITERAL" :
        "???"
    );
}


bool is_right_associative(Precedence precedence) {
    return false;
}


enum OpType {
    OPEN, CLOSE,
    NUMBER, STRING, INITIALIZER,
    IDENTIFIER, LABEL, STATEMENT, DECLARATION,
    SEPARATOR
};


const char *print_op_type(OpType type) {
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


class Op {
public:
    OpType type;
    std::string text;
    Precedence back, fore;
    int left;
    int right;
    
    Op(OpType type, const std::string &text, Precedence back, Precedence fore) {
        this->type = type;
        this->text = text;
        this->back = back;
        this->fore = fore;
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
    { "+", "plus", ADDITIVE },
    { "-", "minus", ADDITIVE },
    { "*", "star", MULTIPLICATIVE },
    { "/", "slash", MULTIPLICATIVE },
    { "%", "percent", MULTIPLICATIVE },
    { "=", "assign", ASSIGNING },
    { "+=", "plus_assign", ASSIGNING },
    { "-=", "minus_assign", ASSIGNING },
    { "*=", "star_assign", ASSIGNING },
    { "/=", "slash_assign", ASSIGNING },
    { "%=", "percent_assign", ASSIGNING },
    { "==", "equal", COMPARING },
    { "!=", "not_equal", COMPARING },
    { "<", "less", COMPARING },
    { ">", "greater", COMPARING },
    { "<=", "less_equal", COMPARING },
    { ">=", "greater_equal", COMPARING },
    { "<>", "incomparable", COMPARING },
    { "!", "not", LOGICAL_HIGH },
    { "&", "and", LOGICAL_HIGH },
    { "|", "or", LOGICAL_LOW },
    { "^", "xor", LOGICAL_LOW }
    //{ "", "",  },
};


void print_op_node(std::vector<Op> &ops, int i, int indent, const char *prefix) {
    if (ops[i].left >= 0)
        print_op_node(ops, ops[i].left, indent + 2, "/ ");
    
    //std::cout << i << "(" << ops[i].left << "/" << ops[i].right << ")\n";
    for (int j=0; j<indent; j++)
        std::cout << " ";
        
    std::cout << prefix << "[" << print_op_type(ops[i].type) << "] " << ops[i].text << "\n";
    
    if (ops[i].right >= 0)
        print_op_node(ops, ops[i].right, indent + 2, "\\ ");
}


std::vector<Op> operate(std::vector<std::string> tokens) {
    std::vector<Op> ops;
    std::vector<Paren> parens;
    
    ops.push_back(Op(OPEN, "", BASE, BASE));
    parens.push_back(UNIT);
    
    for (auto token : tokens) {
        OpType type;
        Precedence back, fore;
        std::string text;
        
        char c = token[0];
        
        if (isdigit(c) || c == '.') {
            type = NUMBER;
            back = fore = LITERAL;
            text = token;
        }
        else if (c == ':') {
            type = STATEMENT;
            back = LITERAL;
            fore = SEPARATING;
            text = token.substr(1);
        }
        else if (isalpha(c) || c == '_') {
            if (token.back() == ':') {
                type = LABEL;
                back = LABELING;
                fore = LABELING;
                text = token.substr(0, token.length() - 1);
            }
            else if (token.back() == '?') {
                type = DECLARATION;
                back = DECLARING;
                fore = DECLARING;
                text = token.substr(0, token.length() - 1);
            }
            else {
                type = IDENTIFIER;
                back = TEXTUAL;
                fore = TEXTUAL;
                text = token;
            }
        }
        else if (is_quote(c)) {
            type = STRING;
            back = LITERAL;
            fore = LITERAL;
            text = token.substr(1, token.length() - 1);
        }
        else if (c == ',') {
            type = SEPARATOR;
            back = SEPARATING;
            fore = SEPARATING;
        }
        else if (c == ';') {
            if (parens.back() != BLOCK && parens.back() != UNIT)
                throw Error("Unclosed parentheses %s!", token.c_str());
                
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
                if (parens.back() != PAREN)
                    throw Error("Mismatched %s!", token.c_str());
                    
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
                if (parens.back() != BRACKET)
                    throw Error("Mismatched %s!", token.c_str());
                    
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
                if (parens.back() != BRACE)
                    throw Error("Mismatched %s!", token.c_str());
                    
                parens.pop_back();
                
                type = CLOSE;
                back = BASE;
                fore = LITERAL;
            }
        }
        else if (c == ' ') {
            if (token == " indent") {
                parens.push_back(BLOCK);
                
                type = OPEN;
                back = LITERAL;
                fore = BASE;
            }
            else if (token == " dedent") {
                if (parens.back() != BLOCK)
                    throw Error("Mismatched %s!", token.c_str());
                    
                parens.pop_back();
                
                type = CLOSE;
                back = BASE;
                fore = LITERAL;
            }
            else
                throw Error("Invalid paren thingy %s!", token.c_str());
        }
        else {
            for (auto op : operators) {
                if (op.token == token) {
                    type = IDENTIFIER;
                    back = op.precedence;
                    fore = op.precedence;
                    text = op.text;
                    break;
                }
            }
            
            if (!type)
                throw Error("No operator %s!", token.c_str());
        }
        
        int n = ops.size();
        std::cout << "Token " << token << " => " << n << "\n";
        
        ops.push_back(Op(type, text, back, fore));
        int r = -1;
        
        for (int i = n - 1; i >= 0; i--) {
            if (ops[i].right != r)
                continue;
                
            r = i;

            if (ops[i].fore > back)  // || (ops[i].precedence == back && !is_right_associative(back)))
                continue;
            else if (ops[i].fore == back) {
                if (back == LITERAL)
                    throw Error("Literals can't follow each other!");
                else if (back == BASE)
                    ops[i].fore = ops[i].back;
                else
                    continue;
            }

            // ops[i] will be our parent, and we'll be its right child
            ops[n].left = ops[i].right;
            ops[i].right = n;
            break;
        }
    }
    
    if (parens.size() != 1)
        throw Error("Parens remained at the end!");
        
    print_op_node(ops, 0, 0, "- ");
    
    return ops;
}


int main(int argc, char **argv) {
    try {
        std::string buffer = read_source(argv[1]);
        std::vector<std::string> tokens = tokenize(buffer);
        std::vector<Op> ops = operate(tokens);
        
        //for (auto token : tokens)
        //    std::cout << "" << token << "\n";
    }
    catch (Error &e) {
        std::cerr << e;
        return 1;
    }
    
    return 0;
}
