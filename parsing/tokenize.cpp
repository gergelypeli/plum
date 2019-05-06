
// Stage 1

std::ustring INDENT_UTEXT = " indent";
std::ustring DEDENT_UTEXT = " dedent";
std::ustring SEPARATE_UTEXT = " separate";


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
        int column_count = start - row_start;
        
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

            Token t(buffer.substr(start, i - start), file_index, row_count, column_count);
            std::cerr << "Invalid indentation of " << n << " spaces after " << indent << " at " << t << "!\n";
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
            // For the sake of floating point numbers, accept a plus or minus sign following
            // a letter {e,E} if that was the first letter in the token.
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
        else if (c == '~' && buffer[i + 1] == '{') {
            // Bulk equality matcher token, handle specially
            i += 2;
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

            if (prefix && i == start + 1) {
                Token t(buffer.substr(start, i - start), file_index, row_count, column_count);
                std::cerr << "Missing identifier of control token at " << t << "!\n";
                throw TOKEN_ERROR;
            }

            // Implicit line continuation on labels after logical line breaks
            if (!prefix && islower(buffer[start]) && c == ':' && tokens.back().utext == SEPARATE_UTEXT)
                tokens.pop_back();

            if (c == ':') {
                if (!prefix) {
                    i++;
                    c = buffer[i];
                }
                else {
                    Token t(buffer.substr(start, i - start), file_index, row_count, column_count);
                    std::cerr << "Invalid something-label token at " << t << "!\n";
                    throw TOKEN_ERROR;
                }
            }
            else if (c == '?') {
                if (!prefix) {
                    i++;
                    c = buffer[i];
                }
                else {
                    Token t(buffer.substr(start, i - start), file_index, row_count, column_count);
                    std::cerr << "Invalid something-declaration token at " << t << "!\n";
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
            Token t(buffer.substr(start, i - start), file_index, row_count, column_count);
            std::cerr << "Invalid input character " << c << " at " << t << "!\n";
            throw TOKEN_ERROR;
        }

        Token t(buffer.substr(start, i - start), file_index, row_count, column_count);
        tokens.push_back(t);
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

