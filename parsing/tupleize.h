
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
    
    Expr(ExprType t, Token k);
    Expr(ExprType t, Token k, std::string tx);
    
    Expr *set_pivot(Expr *p);
    Expr *add_arg(Expr *a);
    Expr *add_kwarg(std::string k, Expr *v);
    const char *print_type();
};


Expr *tupleize(std::vector<Node> &nodes, int i);
Expr *tupleize(std::vector<Node> nodes);
