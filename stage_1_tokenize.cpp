
// Stage 1

bool is_paren(char c) {
    return (c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}');
}


bool is_separator(char c) {
    return (c == ',');
}


bool is_quote(char c) {
    return (c == '\'' || c == '"');
}


std::vector<std::string> tokenize(std::string buffer) {
    std::vector<std::string> tokens;
    int i = 0;
    int indent = -1;
    
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
                tokens.push_back(" separate");  // May be removed by subsequent label
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
                
                tokens.push_back(" separate");  // May be removed by subsequent label
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
                if (c == ':' && tokens.back() == " separate")
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

    if (tokens.back() == " separate")
        tokens.pop_back();
    
    return tokens;
}

