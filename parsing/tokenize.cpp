
// Stage 1

const std::ustring INDENT_UTEXT = " indent";
const std::ustring DEDENT_UTEXT = " dedent";
const std::ustring SEPARATE_UTEXT = " separate";


class Tokenizer {
public:
    std::ustring buffer;
    int file_index;
    int row_count;
    int column_count;
    int token_start;
    int token_end;
    
    static bool is_paren(char c) {
        return (c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}');
    }

    static bool is_separator(char c) {
        return (c == ',');
    }

    static bool is_quote(char c) {
        return (c == '\'' || c == '"');
    }

    static bool is_identifier(char c) {
        return isalnum(c) || c == '_' || c == '.' || c == '$' || c == '@';
    }

    static bool is_prefix(char c) {
        return c == ':' || c == '`' || c == '~';
    }

    Token current_token() {
        return Token(buffer.substr(token_start, token_end - token_start), file_index, row_count, column_count);
    }

    Token artificial_token(std::ustring t) {
        return Token(t, file_index, row_count, 0);
    }

    Tokenizer(std::ustring b, int fi) {
        buffer = b;
        file_index = fi;
        row_count = 1;
        column_count = 0;
        token_start = 0;
        token_end = 0;
    }

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        int row_start = 0;
        int indent = -1;

        // Put the whole source in an indent/dedent pair
        if (buffer[0] == ' ') {
            std::cerr << "Source file can't begin with indentation!\n";
            throw TOKEN_ERROR;
        }
        else {
            tokens.push_back(artificial_token(INDENT_UTEXT));
            indent++;
        }
    
        while (buffer[token_end] != '\0') {
            char c = buffer[token_end];
            token_start = token_end;
            column_count = token_start - row_start;
        
            if (c == ' ') {
                token_end++;
                continue;
            }
            else if (c == '\n') {
                row_count += 1;
                row_start = token_end + 1;
            
                do {
                    token_end++;
                    c = buffer[token_end];
                } while (c == ' ');
            
                if (c == '\n' || c == '#')
                    continue;
                
                int n = token_end - row_start;
                int ind = n / 4;
                int mod = n % 4;
            
                if (ind == indent && mod == 0) {
                    tokens.push_back(artificial_token(SEPARATE_UTEXT));  // May be removed by subsequent label
                    continue;
                }
                else if (ind == indent && mod == 1) {
                    continue;
                }
                else if (ind == indent + 1 && mod == 0) {
                    tokens.push_back(artificial_token(INDENT_UTEXT));
                    indent++;
                    continue;
                }
                else if (ind < indent && mod == 0) {
                    while (ind < indent) {
                        tokens.push_back(artificial_token(DEDENT_UTEXT));
                        indent--;
                    }
                
                    tokens.push_back(artificial_token(SEPARATE_UTEXT));  // May be removed by subsequent label
                    continue;
                }

                std::cerr << "Invalid indentation of " << n << " spaces after " << indent << " at " << current_token() << "!\n";
                throw TOKEN_ERROR;
            }
            else if (c == '#') {
                do {
                    token_end++;
                    c = buffer[token_end];
                } while (c != '\n');
            
                continue;
            }
            else if (isdigit(c)) {  // .1 is not a valid floating point constant
                // For the sake of floating point numbers, accept a plus or minus sign following
                // a letter {e,E} if that was the first letter in the token.
                int letter_count = 0;
            
                do {
                    if (isalpha(c))
                        letter_count += 1;
                
                    token_end++;
                    c = buffer[token_end];
                } while (
                    isalnum(c) || c == '_' || c == '.' ||
                    ((c == '+' || c == '-') && (buffer[token_end - 1] == 'e' || buffer[token_end - 1] == 'E') && letter_count == 1)
                );
            }
            else if (c == '~' && buffer[token_end + 1] == '{') {
                // Bulk equality matcher token, handle specially
                token_end += 2;
            }
            else if (is_identifier(c) || is_prefix(c)) {  // except numeric and "?=", handled above
                char prefix = (is_prefix(c) ? c : '\0');

                // Allow middle colon as a shortcut for two colons
                if (!prefix && token_end > 0 && buffer[token_end - 1] == ':') {
                    token_start -= 1;
                    prefix = ':';
                }

                do {
                    token_end++;
                    c = buffer[token_end];
                } while (is_identifier(c));

                if (prefix && token_end == token_start + 1) {
                    std::cerr << "Missing identifier of control token at " << current_token() << "!\n";
                    throw TOKEN_ERROR;
                }

                // Implicit line continuation on labels after logical line breaks
                if (!prefix && islower(buffer[token_start]) && c == ':' && tokens.back().utext == SEPARATE_UTEXT)
                    tokens.pop_back();

                if (c == ':') {
                    if (!prefix) {
                        token_end++;
                        c = buffer[token_end];
                    }
                    else {
                        std::cerr << "Invalid something-label token at " << current_token() << "!\n";
                        throw TOKEN_ERROR;
                    }
                }
                else if (c == '?') {
                    if (!prefix) {
                        token_end++;
                        c = buffer[token_end];
                    }
                    else {
                        std::cerr << "Invalid something-declaration token at " << current_token() << "!\n";
                        throw TOKEN_ERROR;
                    }
                }
            }
            else if (is_paren(c)) {
                token_end++;
            }
            else if (is_separator(c)) {
                token_end++;
            }
            else if (is_quote(c)) {
                char quote = c;
            
                do {
                    token_end++;
                    c = buffer[token_end];
                } while (c != quote);
            
                token_end++;
            }
            else if (ispunct(c)) {
                do {
                    token_end++;
                    c = buffer[token_end];
                } while (ispunct(c) && !is_prefix(c) && !is_paren(c) && !is_separator(c) && !is_quote(c));
            }
            else {
                std::cerr << "Invalid input character " << c << " at " << current_token() << "!\n";
                throw TOKEN_ERROR;
            }

            tokens.push_back(current_token());
        }

        if (tokens.back().utext == SEPARATE_UTEXT)
            tokens.pop_back();

        // Be nice, and don't complain if the last line is not terminated properly
        while (indent > 0) {
            tokens.push_back(artificial_token(DEDENT_UTEXT));
            indent--;
        }
    
        return tokens;
    }
};


std::vector<Token> tokenize(std::ustring buffer, int file_index) {
    return Tokenizer(buffer, file_index).tokenize();
}
