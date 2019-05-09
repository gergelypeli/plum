
struct Token {
    std::ustring utext;
    int file_index;
    int row;  // one-based
    int column;  // zero-based

    Token() {
        //text = "";
        file_index = -1;
        row = -1;
        column = -1;
    }
    
    Token(const std::ustring &ut, int f, int r, int c) {
        utext = ut;
        file_index = f;
        row = r;
        column = c;
    }
};


std::string get_source_file_display_name(int index);

std::ostream &operator<<(std::ostream &os, const Token &token) {
    os << get_source_file_display_name(token.file_index) << ":" << token.row << ":" << token.column << ":\"" << token.utext << "\"";
    return os;
}


class Expr {
public:
    enum ExprType {
        TUPLE,
        UNSIGNED_NUMBER, NEGATIVE_NUMBER, STRING,
        INITIALIZER, MATCHER, IDENTIFIER,
        CONTROL, EVAL, DECLARATION
    } type;
    Token token;
    std::string text;
    
    std::unique_ptr<Expr> pivot;
    Args args;
    Kwargs kwargs;
    
    Expr(ExprType t, Token k) {
        type = t;
        token = k;
    }

    Expr(ExprType t, Token k, std::string tx) {
        type = t;
        token = k;
        text = tx;
    }
    
    Expr *set_pivot(Expr *p) {
        pivot.reset(p);
        return this;
    }
    
    Expr *add_arg(Expr *a) {
        if (kwargs.size()) {
            std::cerr << "Positional params after keyword at " << a->token << "!\n";
            throw TUPLE_ERROR;
        }
            
        args.push_back(std::unique_ptr<Expr>(a));
        return this;
    }
    
    Expr *add_kwarg(std::string k, Expr *v) {
        if (kwargs[k]) {
            std::cerr << "Duplicate keyword argument " << k << " at " << token << "!\n";
            throw TUPLE_ERROR;
        }
            
        kwargs[k] = std::unique_ptr<Expr>(v);
        return this;
    }

    const char *print_type() {
        return (
            type == TUPLE ? "TUPLE" :
            type == UNSIGNED_NUMBER ? "UNSIGNED_NUMBER" :
            type == NEGATIVE_NUMBER ? "NEGATIVE_NUMBER" :
            type == STRING ? "STRING" :
            type == INITIALIZER ? "INITIALIZER" :
            type == MATCHER ? "MATCHER" :
            type == IDENTIFIER ? "IDENTIFIER" :
            type == CONTROL ? "CONTROL" :
            type == EVAL ? "EVAL" :
            type == DECLARATION ? "DECLARATION" :
            throw TUPLE_ERROR
        );
    }
};


std::vector<Token> tokenize(std::ustring buffer, int file_index);

struct Node;
std::vector<Node> treeize(std::vector<Token> tokens);

Expr *tupleize(std::vector<Node> nodes);

// Typize
bool is_typedefinition(Expr *expr);
Value *typize(Expr *expr, Scope *scope, TypeSpec *context = NULL);
Value *lookup_identifier(std::string name, Value *pivot, Scope *scope, Token token, Args &args, Kwargs &kwargs, TypeSpec *context = NULL);
Value *lookup_fake(std::string name, Value *pivot, Scope *scope, Token token, TypeSpec *context, Variable *arg_var = NULL);
Value *lookup_switch_variable(Scope *scope, Token token);

