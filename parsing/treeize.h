
extern const std::ustring INDENT_UTEXT;
extern const std::ustring DEDENT_UTEXT;
extern const std::ustring SEPARATE_UTEXT;


enum Precedence {
    OPENING,
    SEPARATING, // comma must end controls: :if x then: y else: z, ...
    LABELING,   // must be lower than DECLARING: each: i?
    ASSIGNING,  // must be lower than DECLARING: x? Int = 8
    DECLARING,
    LOGICAL_LOW,    // must be lower than COMPARING
    LOGICAL_MED,
    LOGICAL_HIGH,
    COMPARING,
    ADDITIVE,
    MULTIPLICATIVE,
    EXPONENTIAL,
    UNARY,
    REGULAR,
    CLOSING
};


struct Node {
    enum NodeType {
        NONE, OPEN, CLOSE,
        UNSIGNED_NUMBER, STRING,
        INITIALIZER, MATCHER,
        IDENTIFIER, LABEL, CONTROL, EVAL, DECLARATION,
        SEPARATOR
    } type;

    std::string text;
    Precedence back, fore;
    int left;
    int right;
    int up;
    Token token;
    
    Node(NodeType type, const std::string &text, Precedence back, Precedence fore, Token token);
    const char *print_type();
};

std::vector<Node> treeize(std::vector<Token> tokens);
