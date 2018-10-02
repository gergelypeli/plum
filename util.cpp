
// From https://stackoverflow.com/questions/874134/find-if-string-ends-with-another-string-in-c
inline bool desuffix(std::string &value, std::string const &ending)
{
    if (ending.size() > value.size())
        return false;
        
    if (!std::equal(ending.rbegin(), ending.rend(), value.rbegin()))
        return false;
        
    value = value.substr(0, value.size() - ending.size());
    return true;
}


inline bool deprefix(std::string &value, std::string const &beginning)
{
    if (beginning.size() > value.size())
        return false;
        
    if (!std::equal(beginning.begin(), beginning.end(), value.begin()))
        return false;
        
    value = value.substr(beginning.size());
    //std::cerr << "Deprefixed " << beginning << " into " << value << "\n";
    return true;
}


inline bool has_prefix(std::string &value, std::string const &beginning)
{
    if (beginning.size() > value.size())
        return false;
        
    if (!std::equal(beginning.begin(), beginning.end(), value.begin()))
        return false;
        
    return true;
}


std::string get_working_path() {
   char temp[4096];  // No fucking standard, also see http://insanecoding.blogspot.com/2007/11/pathmax-simply-isnt.html
   return ( getcwd(temp, 4096) ? std::string( temp ) : std::string("") );
}


unsigned stack_size(unsigned size) {
    return (size + 7) & ~7;
}


unsigned elem_size(unsigned size) {
    return size <= 2 ? size : size <= 4 ? 4 : size <= 8 ? 8 : stack_size(size);
}


std::vector<std::string> brace_split(std::string s) {
    std::vector<std::string> fragments;
    unsigned p = 0;
    bool inside = false;
    
    for (unsigned i = 0; i < s.size(); i++) {
        if (s[i] != (inside ? '}' : '{'))
            continue;
            
        fragments.push_back(s.substr(p, i - p));
        p = i + 1;
        inside = !inside;
    }
    
    fragments.push_back(s.substr(p, s.size() - p));
    
    return fragments;
}


enum OperationType {
    TWEAK,
    COMPLEMENT, NEGATE, GENERIC_UNARY,
    ADD, SUBTRACT, MULTIPLY, DIVIDE, MODULO, EXPONENT,
    OR, XOR, AND, SHIFT_LEFT, SHIFT_RIGHT, 
    EQUAL, NOT_EQUAL, LESS, GREATER, LESS_EQUAL, GREATER_EQUAL, COMPARE,
    ASSIGN, ASSIGN_ADD, ASSIGN_SUBTRACT, ASSIGN_MULTIPLY, ASSIGN_DIVIDE, ASSIGN_MODULO, ASSIGN_EXPONENT,
    ASSIGN_OR, ASSIGN_XOR, ASSIGN_AND, ASSIGN_SHIFT_LEFT, ASSIGN_SHIFT_RIGHT,
    CREATE
};

bool is_unary(OperationType o) {
    return o == COMPLEMENT || o == NEGATE || o == GENERIC_UNARY;
}

bool is_comparison(OperationType o) {
    return o >= EQUAL && o <= COMPARE;
}

bool is_assignment(OperationType o) {
    return o >= ASSIGN;
}


std::vector<unsigned16> decode_utf8(std::string text) {
    int bytelen = text.size();
    std::vector<unsigned16> characters;
    characters.resize(bytelen);  // upper limit
    
    int64 character_count, byte_count;
    decode_utf8_buffer(text.data(), bytelen, characters.data(), characters.size(), &byte_count, &character_count);
    
    characters.resize(character_count);
    return characters;
}


unsigned64 parse_unsigned_integer(std::string text) {
    unsigned base = 10;
    unsigned n = text.size();
    unsigned start = 0;
    
    if (text[0] == '0' && n > 1) {
        if (n < 2) {
            std::cerr << "Integer literal with invalid prefix: " << text << "!\n";
            throw TYPE_ERROR;
        }
        
        switch (text[1]) {
        case 'x':
        case 'X':
            base = 16;
            break;
        case 'o':
        case 'O':
            base = 8;
            break;
        case 'b':
        case 'B':
            base = 2;
            break;
        default:
            std::cerr << "Integer literal with invalid base: " << text << "!\n";
            throw TYPE_ERROR;
        }
        
        start = 2;
    }
    
    const unsigned64 limit_value = (0UL - 1) / base;
    const unsigned64 limit_digit = (0UL - 1) % base;
    unsigned64 value = 0;
    
    for (unsigned i = start; i < n; i++) {
        char c = text[i];
        
        if (c == '_')
            continue;

        unsigned64 digit = (
            c >= '0' && c <= '9' ? c - '0' :
            c >= 'a' && c <= 'f' ? c - 'a' + 10 :
            c >= 'A' && c <= 'F' ? c - 'A' + 10 :
            16
        );
        
        if (digit >= base) {
            std::cerr << "Integer literal with invalid digit: " << text << "!\n";
            throw TYPE_ERROR;
        }
        
        if (value > limit_value || (value == limit_value && digit > limit_digit)) {
            std::cerr << "Integer literal overflow: " << text << "!\n";
            throw TYPE_ERROR;
        }
            
        value = value * base + digit;
    }
    
    return value;
}


double parse_float(std::string text) {
    double value = 0;
    int exponent = 0;
    bool seen_dot = false;
    unsigned n = text.size();
    
    for (unsigned i = 0; i < n; i++) {
        char c = text[i];
        
        if (c == '_')
            continue;
        else if (c == '.') {
            if (seen_dot) {
                std::cerr << "Float literal with multiple dots: " << text << "!\n";
                throw TYPE_ERROR;
            }
            
            seen_dot = true;
            continue;
        }
        else if (c >= '0' && c <= '9') {
            value = 10 * value + (c - '0');
            
            if (seen_dot)
                exponent -= 1;
                
            continue;
        }
        else if (c == 'e' || c == 'E') {
            i += 1;
            
            if (i == n) {
                std::cerr << "Float literal with missing exponent: " << text << "!\n";
                throw TYPE_ERROR;
            }
            
            bool is_negative;
            
            if (text[i] == '-')
                is_negative = true;
            else if (text[i] == '+')
                is_negative = false;
            else {
                std::cerr << "Float literal with unsigned exponent: " << text << "!\n";
                throw TYPE_ERROR;
            }
            
            i += 1;
            
            if (i == n) {
                std::cerr << "Float literal with truncated exponent: " << text << "!\n";
                throw TYPE_ERROR;
            }
            
            unsigned e = 0;
            
            while (i < n) {
                c = text[i];
                
                if (c >= '0' && c <= '9')
                    e = e * 10 + c - '0';
                else {
                    std::cerr << "Float literal with invalid exponent: " << text << "!\n";
                    throw TYPE_ERROR;
                }
                
                if (e > 308) {
                    std::cerr << "Float literal with overflowing exponent: " << text << "!\n";
                    throw TYPE_ERROR;
                }
                
                i += 1;
            }
            
            if (is_negative)
                exponent -= e;
            else
                exponent += e;
        }
        else {
            std::cerr << "Float literal with invalid digit: " << text << "!\n";
            throw TYPE_ERROR;
        }
    }
    
    return value * pow(10.0, exponent);
}


