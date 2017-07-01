
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

/*
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

    Error() {
    }
    
    Error(const char *fmt, ...) __attribute__ ((format (printf, 2, 3))) {
        va_list ap;
        va_start(ap, fmt);
        message = vformat(fmt, ap);
        va_end(ap);
        
        std::cerr << "Oops... " << message << "\n";
    }
};


std::ostream &operator<<(std::ostream &os, Error const &error) {
    os << error.message;
    return os;
}
*/
