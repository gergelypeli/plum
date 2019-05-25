
// Structs
struct ArgInfo;
typedef std::vector<ArgInfo> ArgInfos;
struct ExprInfo;
typedef std::vector<ExprInfo> ExprInfos;
struct PartialInfo;
struct TreenumInput;
//struct Allocation;
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

std::ostream &operator<<(std::ostream &os, const std::ustring &x);


// https://stackoverflow.com/questions/3610933/iterating-c-vector-from-the-end-to-the-begin
template <class T>
class backward {
    T& _obj;
public:
    backward(T &obj) : _obj(obj) {}
    auto begin() {return _obj.rbegin();}
    auto end() {return _obj.rend();}
};



struct Allocation {
    int bytes;
    int count1;
    int count2;
    int count3;
    
    Allocation(int b = 0, int c1 = 0, int c2 = 0, int c3 = 0);
    int concretize();
    Allocation stack_size();
};

Allocation operator+(Allocation a, Allocation b);
Allocation operator*(Allocation a, int c);
std::ostream &operator<<(std::ostream &os, const Allocation &a);


enum OperationType {
    TWEAK,
    COMPLEMENT, NEGATE, GENERIC_UNARY,
    ADD, SUBTRACT, MULTIPLY, DIVIDE, MODULO, EXPONENT,
    OR, XOR, AND, SHIFT_LEFT, SHIFT_RIGHT, 
    EQUAL, NOT_EQUAL, LESS, GREATER, LESS_EQUAL, GREATER_EQUAL, COMPARE,
    ASSIGN, ASSIGN_ADD, ASSIGN_SUBTRACT, ASSIGN_MULTIPLY, ASSIGN_DIVIDE, ASSIGN_MODULO, ASSIGN_EXPONENT,
    ASSIGN_OR, ASSIGN_XOR, ASSIGN_AND, ASSIGN_SHIFT_LEFT, ASSIGN_SHIFT_RIGHT,
    CREATE, LTWEAK
};

bool is_unary(OperationType o);
bool is_comparison(OperationType o);
bool is_assignment(OperationType o);


enum InheritAs {
    AS_ROLE, AS_BASE, AS_MAIN, AS_REQUIRE
};


bool desuffix(std::string &value, std::string const &ending);
bool deprefix(std::string &value, std::string const &beginning);
bool has_prefix(std::string &value, std::string const &beginning);
std::string unqualify(std::string name);
std::string get_working_path();
std::ostream &operator<<(std::ostream &os, const std::ustring &x);
std::vector<std::ustring> brace_split(std::ustring s);

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


struct PartialInfo {
    std::set<std::string> uninitialized_member_names;
    std::set<std::string> initialized_member_names;
    
    PartialInfo();
    
    void set_member_names(std::vector<std::string> mn);
    void be_initialized(std::string name);
    bool is_initialized(std::string name);
    bool is_uninitialized(std::string name);
    bool is_complete();
    void be_complete();
    bool is_dirty();
};


int character_code(std::string name);
std::string character_name(int code);
int uni_code(std::string name);

std::ustring decode_utf8(std::string text);
std::string encode_utf8(std::ustring text);
std::string encode_ascii(std::ustring text);
std::vector<std::ustring> interpolate_characters(std::vector<std::ustring> in);


struct TreenumInput {
    const char *kw;
    unsigned p;
};


unsigned stack_size(unsigned size);
unsigned elem_size(unsigned size);
