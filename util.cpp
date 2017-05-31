#include <util.h>
#include <stdarg.h>


static std::string vformat(const char *f, va_list ap) {
    char buffer[65536];
    
    unsigned expected = vsnprintf(buffer, sizeof(buffer), f, ap);

    if (expected >= sizeof(buffer))
        std::cerr << "vformat buffer overflow!\n";
    
    return std::string(buffer);
}


Error::Error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    message = vformat(fmt, ap);
    va_end(ap);
}


std::ostream &operator<<(std::ostream &os, Error const &error) {
    os << error.message;
    return os;
}
