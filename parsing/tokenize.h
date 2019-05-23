
struct Token {
    std::ustring utext;
    int file_index;
    int row;  // one-based
    int column;  // zero-based

    Token();
    Token(const std::ustring &ut, int f, int r, int c);
};


std::string get_source_file_display_name(int index);

std::ostream &operator<<(std::ostream &os, const Token &token) {
    os << get_source_file_display_name(token.file_index) << ":" << token.row << ":" << token.column << ":\"" << token.utext << "\"";
    return os;
}


std::vector<Token> tokenize(std::ustring buffer, int file_index);
