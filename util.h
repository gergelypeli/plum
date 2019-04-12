
// Structs
struct ArgInfo;
typedef std::vector<ArgInfo> ArgInfos;
struct ExprInfo;
typedef std::vector<ExprInfo> ExprInfos;
struct PartialInfo;
struct TreenumInput;
struct Allocation;
struct Token;
class Expr;
typedef std::vector<std::unique_ptr<Expr>> Args;
typedef std::map<std::string, std::unique_ptr<Expr>> Kwargs;


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
    
    T get(int i) const {
        return deque.at(i + center);
    }

    void set(int i, const T &x) {
        deque.at(i + center) = x;
    }
};


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


