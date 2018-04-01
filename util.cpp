
enum Error {
    TOKEN_ERROR, TREE_ERROR, TUPLE_ERROR, TYPE_ERROR,
    INTERNAL_ERROR, X64_ERROR
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
    ASSIGN_OR, ASSIGN_XOR, ASSIGN_AND, ASSIGN_SHIFT_LEFT, ASSIGN_SHIFT_RIGHT
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


template <typename T>
class GenericArgs: public std::vector<std::unique_ptr<T>> {
};


template <typename T>
class GenericKwargs: public std::vector<std::pair<std::string, std::unique_ptr<T>>> {
public:
    std::unique_ptr<T> &operator[](std::string s) {
        for (unsigned i = 0; i < this->size(); i++)
            if (this->at(i).first == s)
                return this->at(i).second;
            
        this->push_back(std::make_pair(s, std::unique_ptr<T>()));
        return this->back().second;
    }
    
    std::unique_ptr<T> &operator[](unsigned i) {
        return this->at(i).second;
    }
};


std::vector<unsigned short> decode_utf8(std::string text) {
    int bytelen = text.size();
    std::vector<unsigned short> characters;
    characters.resize(bytelen);
    int charlen = decode_utf8_buffer(text.data(), bytelen, characters.data());
    characters.resize(charlen);
    return characters;
}


unsigned long parse_unsigned_integer(std::string text) {
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
    
    const unsigned long limit_value = (0UL - 1) / base;
    const unsigned long limit_digit = (0UL - 1) % base;
    unsigned long value = 0;
    
    for (unsigned i = start; i < n; i++) {
        char c = text[i];
        
        if (c == '_')
            continue;

        unsigned long digit = (
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
