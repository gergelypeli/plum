
namespace std {
    // We really can't work with a type system that cannot even define char16_t properly.
    class ustring: public std::basic_string<unsigned16> {
    public:
        using basic_string::basic_string;
        
        ustring(const ustring &x)
            :basic_string(x) {
        }

        ustring(const basic_string &x)
            :basic_string(x) {
        }
        
        ustring(const char *x) {
            for (unsigned i = 0; x[i]; i++)
                push_back(x[i]);
        }
    };
};


template <typename T>
class devector {
public:
    std::deque<T> deque;
    int center;
    
    devector() {
        center = 0;
    }
    
    bool empty() const {
        return deque.empty();
    }
    
    int low() const {
        return -center;
    }
    
    int high() const {
        return deque.size() - center;
    }

    int append(const T& x) {
        deque.push_back(x);
        return high() - 1;
    }
    
    int prepend(const T& x) {
        deque.push_front(x);
        center += 1;
        return low();
    }
    
    int extend(const devector<T> &o) {
        deque.insert(deque.end(), o.deque.begin(), o.deque.end());
            
        return high() - o.high();
    }

    int prextend(const devector<T> &o) {
        deque.insert(deque.begin(), o.deque.begin(), o.deque.end());
        center += o.deque.size();
            
        return low() - o.low();
    }
    
    T get(int i) {
        return deque.at(i + center);
    }

    void set(int i, const T &x) {
        deque.at(i + center) = x;
    }
};


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


std::vector<std::ustring> brace_split(std::ustring s) {
    std::vector<std::ustring> fragments;
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


template <typename T>
std::ostream &operator<<(std::ostream &os, const std::set<T> &x) {
    os << "{";
    bool first = true;
    
    for (auto &y : x) {
        if (!first)
            os << ", ";
            
        first = false;
        os << y;
    }
    
    os << "}";
    return os;
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


// These must be uppercase for character interpolation
struct {
    std::string name;
    int code;
} character_codes[] = {
    { "NUL", 0 },
    { "SOH", 1 },
    { "STX", 2 },
    { "ETX", 3 },
    { "EOT", 4 },
    { "ENQ", 5 },
    { "ACK", 6 },
    { "BEL", 7 },
    { "BS", 8 },
    { "HT", 9 },
    { "LF", 10 },
    { "VT", 11 },
    { "FF", 12 },
    { "CR", 13 },
    { "SO", 14 },
    { "SI", 15 },
    { "DLE", 16 },
    { "DC1", 17 },
    { "DC2", 18 },
    { "DC3", 19 },
    { "DC4", 20 },
    { "NAK", 21 },
    { "SYN", 22 },
    { "ETB", 23 },
    { "CAN", 24 },
    { "EM", 25 },
    { "SUB", 26 },
    { "ESC", 27 },
    { "FS", 28 },
    { "GS", 29 },
    { "RS", 30 },
    { "US", 31 },

    { "QU", 34 },
    { "LB", 123 },
    { "RB", 125 },
    { "DEL", 127 },
};


int character_code(std::string name) {
    for (unsigned i = 0; i < sizeof(character_codes) / sizeof(character_codes[0]); i++)
        if (character_codes[i].name == name)
            return character_codes[i].code;
            
    return -1;
}


std::string character_name(int code) {
    for (unsigned i = 0; i < sizeof(character_codes) / sizeof(character_codes[0]); i++)
        if (character_codes[i].code == code)
            return character_codes[i].name;
            
    throw INTERNAL_ERROR;
}


int uni_code(std::string name) {
    if (name.size() != 5)
        return -1;
        
    if (name[0] != 'U')
        return -1;
        
    int code = 0;
    
    for (char c : { name[1], name[2], name[3], name[4] }) {
        c = tolower(c);
        int x = (c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1);
        
        if (x < 0)
            return -1;
        
        code = code * 16 + x;
    }
    
    return code;
}


std::ustring decode_utf8(std::string text) {
    int bytelen = text.size();
    unsigned16 characters[bytelen];
    
    int64 character_count, byte_count;
    decode_utf8_buffer(text.data(), bytelen, characters, bytelen, &byte_count, &character_count);
    
    return std::ustring(characters, character_count);
}


std::string encode_utf8(std::ustring text) {
    int charlen = text.size();
    char bytes[charlen * 3];
    
    int64 character_count, byte_count;
    encode_utf8_buffer(text.data(), charlen, bytes, charlen * 3, &character_count, &byte_count);
    
    return std::string(bytes, byte_count);
}


std::ostream &operator<<(std::ostream &os, const std::ustring &x) {
    int charlen = x.size();
    char bytes[charlen * 3];
    
    int64 character_count, byte_count;
    encode_utf8_buffer(x.data(), charlen, bytes, charlen * 3, &character_count, &byte_count);

    os.write(bytes, byte_count);
    return os;
}


std::string encode_ascii(std::ustring text) {
    std::string out;
    
    for (unsigned16 c : text) {
        if (c < 32 || c > 126)
            return std::string();
        
        out.push_back(c);
    }
    
    return out;
}


std::vector<std::ustring> interpolate_characters(std::vector<std::ustring> in) {
    std::vector<std::ustring> out;
    bool is_literal = true;
    
    for (auto &f : in) {
        if (is_literal) {
            if (out.size() % 2 == 0) {
                // Literal fragments are at even indexes only
                out.push_back(std::ustring());
            }
            
            // May append to existing literal fragment
            for (auto c : f)
                out.back().push_back(c);
        }
        else {
            if (f.size() > 0 && isupper(f[0])) {
                // NOTE: currently all lookup returns an ASCII result
                std::string kw = encode_ascii(f);
                
                if (kw.empty()) {
                    std::cerr << "Interpolation keyword not ASCII: " << f << "!\n";
                    throw TYPE_ERROR;
                }
                
                int c = character_code(kw);
                
                if (c < 0)
                    c = uni_code(kw);
                
                if (c < 0) {
                    std::cerr << "Unknown interpolated character " << f << "!\n";
                    throw TYPE_ERROR;
                }
                
                // Append to previous literal fragment
                out.back().push_back(c);
            }
            else {
                // Create identifier fragment
                out.push_back(f);
            }
        }
        
        is_literal = !is_literal;
    }

    return out;
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


