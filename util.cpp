
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


unsigned round_up(unsigned size) {
    return (size + 7) & ~7;
}
