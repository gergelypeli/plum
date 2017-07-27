
enum Error {
    TOKEN_ERROR, TREE_ERROR, TUPLE_ERROR, TYPE_ERROR,
    INTERNAL_ERROR, X64_ERROR
};

// From https://stackoverflow.com/questions/874134/find-if-string-ends-with-another-string-in-c
inline bool ends_with(std::string const & value, std::string const & ending)
{
    if (ending.size() > value.size()) return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}


unsigned stack_size(unsigned size) {
    return (size + 7) & ~7;
}


unsigned item_size(unsigned size) {
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
    COMPLEMENT, NEGATE,
    ADD, SUBTRACT, MULTIPLY, DIVIDE, MODULO, EXPONENT,
    OR, XOR, AND, SHIFT_LEFT, SHIFT_RIGHT, 
    EQUAL, NOT_EQUAL, LESS, GREATER, LESS_EQUAL, GREATER_EQUAL, INCOMPARABLE,
    ASSIGN, ASSIGN_ADD, ASSIGN_SUBTRACT, ASSIGN_MULTIPLY, ASSIGN_DIVIDE, ASSIGN_MODULO, ASSIGN_EXPONENT,
    ASSIGN_OR, ASSIGN_XOR, ASSIGN_AND, ASSIGN_SHIFT_LEFT, ASSIGN_SHIFT_RIGHT
};

bool is_unary(OperationType o) {
    return o == COMPLEMENT || o == NEGATE;
}

bool is_comparison(OperationType o) {
    return o >= EQUAL && o <= INCOMPARABLE;
}

bool is_assignment(OperationType o) {
    return o >= ASSIGN;
}
