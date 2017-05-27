#include <stdarg.h>

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>

/*
class Error {
protected:
    std::vector<std::stringstream> items;
    
public:
    Error() {
    }
    
    std::ostream &log() {
        items.push_back(std::stringstream());
        return items.back();
    }
}


std::ostream &operator<<(std::ostream &os, Error const &error) {
    os << "Error details:\n";
    
    for (item : error.items)
        os << " " << item.str() << "\n";
}
*/
/*
class Error: public std::stringstream {
public:
    Error() {
        *this << "Error details:";
    }

    std::ostream &log() {
        *this << "\n";
        *this << " ";
        return *this;
    }
};


std::ostream &operator<<(std::ostream &os, Error const &error) {
    os << " " << error.str() << "\n";
}
*/

static std::string vformat(const char *f, va_list ap) {
    char buffer[65536];
    
    unsigned expected = vsnprintf(buffer, sizeof(buffer), f, ap);

    if (expected >= sizeof(buffer))
        std::cerr << "vformat buffer overflow!\n";
    
    return std::string(buffer);
}


class Error {
public:
    std::string message;
    
    Error(const char *fmt, ...) __attribute__ ((format (printf, 2, 3))) {
        va_list ap;
        va_start(ap, fmt);
        message = vformat(fmt, ap);
        va_end(ap);
    }
};


std::ostream &operator<<(std::ostream &os, Error const &error) {
    os << error.message;
    return os;
}


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
            throw Error("Invalid input character %c!", c);
        }

        tokens.push_back(buffer.substr(start, i - start));
    }
    
    return tokens;
}


int main(int argc, char **argv) {
    try {
        std::string buffer = read_source(argv[1]);
        std::vector<std::string> tokens = tokenize(buffer);
        
        for (auto token : tokens)
            std::cout << "" << token << "\n";
    }
    catch (Error &e) {
        std::cerr << e;
        return 1;
    }
    
    return 0;
}
