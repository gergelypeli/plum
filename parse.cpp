#include <iostream>
#include <fstream>
#include <string>
#include <vector>
 
 
std::string read_source(const char *filename) {
    std::ifstream source(filename, std::ios::binary);
    
    if (!source.is_open())
        return "";
    
    std::string buffer((std::istreambuf_iterator<char>(source)),
                       (std::istreambuf_iterator<char>()));
                       
    buffer.push_back('\0');
    
    return buffer;
}


bool is_solo(char c) {
    return (c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}' || c == ';' || c == ',');
}


bool is_quote(char c) {
    return (c == '\'' || c == '"');
}

 
int main(int argc, char **argv) {
    std::string buffer = read_source(argv[1]);
    
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
                tokens.push_back(";;");
                continue;
            }
            else if (ind == indent && mod == 1) {
                continue;
            }
            else if (ind == indent + 1 && mod == 0) {
                tokens.push_back("{{");
                indent++;
                continue;
            }
            else if (ind < indent && mod == 0) {
                for (int j=0; j < indent - ind; j++) {
                    tokens.push_back("}}");
                    indent--;
                }
                
                continue;
            }

            std::cerr << "Invalid indentation of " << n << " spaces!\n";
            return 3;
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
                i++;
                c = buffer[i];
            }
        }
        else if (is_solo(c)) {
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
            } while (ispunct(c) && !is_solo(c) && !is_quote(c));
        }
        else {
            std::cerr << "Invalid input character: " << c << "!\n";
            return 2;
        }

        tokens.push_back(buffer.substr(start, i - start));
    }
    
    for (auto token : tokens)
        std::cout << "" << token << "\n";

    return 0;
}
