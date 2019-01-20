
// Stage 1

struct Token {
    std::ustring utext;
    int file_index;
    int row;  // one-based
    int column;  // zero-based

    Token() {
        //text = "";
        file_index = -1;
        row = -1;
        column = -1;
    }
    
    Token(const std::ustring &ut, int f, int r, int c) {
        utext = ut;
        file_index = f;
        row = r;
        column = c;
    }
};


std::ustring INDENT_UTEXT = " indent";
std::ustring DEDENT_UTEXT = " dedent";
std::ustring SEPARATE_UTEXT = " separate";


std::string get_source_file_name(int index);

std::ostream &operator<<(std::ostream &os, const Token &token) {
    os << get_source_file_name(token.file_index) << ":" << token.row << ":" << token.column << ":\"" << token.utext << "\"";
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


bool is_identifier(char c) {
    return isalnum(c) || c == '_' || c == '.' || c == '$' || c == '@';
}


bool is_prefix(char c) {
    return c == ':' || c == '`' || c == '~';
}


std::vector<Token> tokenize(std::ustring buffer, int file_index) {
    std::vector<Token> tokens;
    int row_count = 1;
    int row_start = 0;
    int i = 0;
    int indent = -1;

    // Put the whole source in an indent/dedent pair
    if (buffer[0] == ' ') {
        std::cerr << "Source file can't begin with indentation!\n";
        throw TOKEN_ERROR;
    }
    else {
        tokens.push_back(Token(INDENT_UTEXT, file_index, row_count, 0));
        indent++;
    }
    
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
                tokens.push_back(Token(SEPARATE_UTEXT, file_index, row_count, 0));  // May be removed by subsequent label
                continue;
            }
            else if (ind == indent && mod == 1) {
                continue;
            }
            else if (ind == indent + 1 && mod == 0) {
                tokens.push_back(Token(INDENT_UTEXT, file_index, row_count, 0));
                indent++;
                continue;
            }
            else if (ind < indent && mod == 0) {
                while (ind < indent) {
                    tokens.push_back(Token(DEDENT_UTEXT, file_index, row_count, 0));
                    indent--;
                }
                
                tokens.push_back(Token(SEPARATE_UTEXT, file_index, row_count, 0));  // May be removed by subsequent label
                continue;
            }

            std::cerr << "Invalid indentation of " << n << " spaces (at " << indent << ")!\n";
            throw TOKEN_ERROR;
        }
        else if (c == '#') {
            do {
                i++;
                c = buffer[i];
            } while (c != '\n');
            
            continue;
        }
        else if (isdigit(c)) {  // .1 is not a valid floating point constant
            // For the sake of floating point numbers, include a plus or minus sign following
            // a letter E if that was the first letter in the token.
            int letter_count = 0;
            
            do {
                if (isalpha(c))
                    letter_count += 1;
                
                i++;
                c = buffer[i];
            } while (
                isalnum(c) || c == '_' || c == '.' ||
                ((c == '+' || c == '-') && (buffer[i - 1] == 'e' || buffer[i - 1] == 'E') && letter_count == 1)
            );
        }
        else if (is_identifier(c) || is_prefix(c)) {  // except numeric and "?=", handled above
            char prefix = (is_prefix(c) ? c : '\0');

            // Allow middle colon as a shortcut for two colons
            if (!prefix && i > 0 && buffer[i - 1] == ':') {
                start -= 1;
                prefix = ':';
            }
            
            do {
                i++;
                c = buffer[i];
            } while (is_identifier(c));

            // Implicit line continuation on labels after logical line breaks
            if (!prefix && c == ':' && tokens.back().utext == SEPARATE_UTEXT)
                tokens.pop_back();

            if (c == ':') {
                if (!prefix || prefix == ':') {
                    i++;
                    c = buffer[i];
                }
                else {
                    std::cerr << "Invalid something-label token!\n";
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
        else if (is_quote(c)) {
            char quote = c;
            
            do {
                i++;
                c = buffer[i];
            } while (c != quote);
            
            i++;
        }
        else if (ispunct(c)) {
            do {
                i++;
                c = buffer[i];
            } while (ispunct(c) && !is_prefix(c) && !is_paren(c) && !is_separator(c) && !is_quote(c));
        }
        else {
            std::cerr << "Invalid input character " << c << "!\n";
            throw TOKEN_ERROR;
        }

        tokens.push_back(Token(buffer.substr(start, i - start), file_index, row_count, start - row_start));
    }

    if (tokens.back().utext == SEPARATE_UTEXT)
        tokens.pop_back();

    // Be nice, and don't complain if the last line is not terminated properly
    while (indent > 0) {
        tokens.push_back(Token(DEDENT_UTEXT, file_index, row_count, 0));
        indent--;
    }
    
    return tokens;
}

