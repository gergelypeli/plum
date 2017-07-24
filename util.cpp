
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
