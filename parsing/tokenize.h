
struct Token {
    std::ustring utext;
    int file_index;
    int row;  // one-based
    int column;  // zero-based

    Token();
    Token(const std::ustring &ut, int f, int r, int c);
};

std::ostream &operator<<(std::ostream &os, const Token &token);

std::vector<Token> tokenize(std::ustring buffer, int file_index);
