#include <string>
#include <iostream>


class Error {
public:
    std::string message;
    
    Error(const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
};


std::ostream &operator<<(std::ostream &os, Error const &error);
