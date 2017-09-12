
// Stage 1

struct Token {
    std::string text;
    int row;  // zero-based
    int column;  // zero-based

    Token() {
        text = "";
        row = -1;
        column = -1;
    }
    
    Token(const std::string &t, int r, int c) {
        text = t;
        row = r;
        column = c;
    }
};


std::ostream &operator<<(std::ostream &os, const Token &token) {
    os << (token.row + 1) << ":" << (token.column + 1) << ":\"" << token.text << "\"";
    return os;
}


bool is_paren(char c) {
    return (c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}');
}


bool is_separator(char c) {
    return (c == ',');
}


bool is_quote(char c) {
    return (c == '\'' || c == '"');
}


std::vector<Token> tokenize(std::string buffer) {
    std::vector<Token> tokens;
    int row_count = 0;
    int row_start = 0;
    int i = 0;
    int indent = -1;
    
    while (buffer[i] != '\0') {
        char c = buffer[i];
        int start = i;
        
        if (c == ' ') {
            i++;
            continue;
        }
        else if (c == '\n') {
            row_count += 1;
            row_start = i + 1;
            
            do {
                i++;
                c = buffer[i];
            } while (c == ' ');
            
            if (c == '\n' || c == '#')
                continue;
                
            int n = i - row_start;
            int ind = n / 4;
            int mod = n % 4;
            
            if (ind == indent && mod == 0) {
                tokens.push_back(Token(" separate", row_count, 0));  // May be removed by subsequent label
                continue;
            }
            else if (ind == indent && mod == 1) {
                continue;
            }
            else if (ind == indent + 1 && mod == 0) {
                tokens.push_back(Token(" indent", row_count, 0));
                indent++;
                continue;
            }
            else if (ind < indent && mod == 0) {
                while (ind < indent) {
                    tokens.push_back(Token(" dedent", row_count, 0));
                    indent--;
                }
                
                tokens.push_back(Token(" separate", row_count, 0));  // May be removed by subsequent label
                continue;
            }

            std::cerr << "Invalid indentation of " << n << " spaces!\n";
            throw TOKEN_ERROR;
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
            } while (isalnum(c) || c == '_' || c == '.');
        }
        else if (isalpha(c) || c == '_' || c == ':' || c == '`') {
            char prefix = (c == ':' || c == '`' ? c : '\0');

            // Allow middle colon as a shortcut for two colons
            if (!prefix && buffer[i - 1] == ':') {
                start -= 1;
                prefix = ':';
            }
            
            do {
                i++;
                c = buffer[i];
            } while (isalnum(c) || c == '_' || c == '.');

            // Implicit line continuation
            if (!prefix && c == ':' && tokens.back().text == " separate")
                tokens.pop_back();

            if (c == ':') {
                if (!prefix || prefix == ':') {
                    i++;
                    c = buffer[i];
                }
                else {
                    std::cerr << "Invalid initializer-label token!\n";
                    throw TOKEN_ERROR;
                }
            }
            else if (c == '?') {
                if (!prefix) {
                    i++;
                    c = buffer[i];
                }
                else {
                    std::cerr << "Invalid something-declaration token!\n";
                    throw TOKEN_ERROR;
                }
            }
        }
        else if (is_paren(c)) {
            i++;
        }
        else if (is_separator(c)) {
            i++;
        }
        else if (c == '$') {
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
            std::cerr << "Invalid input character " << c << "!\n";
            throw TOKEN_ERROR;
        }

        tokens.push_back(Token(buffer.substr(start, i - start), row_count, start - row_start));
    }

    if (tokens.back().text == " separate")
        tokens.pop_back();

    // Be nice, and don't complain if the last line is not terminated properly
    while (indent > 0) {
        tokens.push_back(Token(" dedent", row_count, 0));
        indent--;
    }
    
    return tokens;
}

