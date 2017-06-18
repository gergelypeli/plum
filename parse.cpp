#include <string>
#include <vector>
#include <map>
#include <memory>
#include <iostream>
#include <sstream>
#include <fstream>

#include <stdarg.h>

#include "util.cpp"
#include "stage_1_tokenize.cpp"
#include "stage_2_treeize.cpp"
#include "stage_3_tupleize.cpp"
#include "stage_4_typize.cpp"


std::string read_source(const char *filename) {
    std::ifstream source(filename, std::ios::binary);
    
    if (!source.is_open())
        return "";
    
    std::string buffer((std::istreambuf_iterator<char>(source)),
                       (std::istreambuf_iterator<char>()));
                       
    buffer.push_back('\0');
    
    return buffer;
}


int main(int argc, char **argv) {
    std::string buffer = read_source(argv[1]);
    
    std::vector<Token> tokens = tokenize(buffer);
    
    std::vector<Node> nodes = treeize(tokens);

    std::unique_ptr<Expr> expr_root(tupleize(nodes));
    print_expr_tree(expr_root.get(), 0, "*");

    Scope *root_scope = init_types();
    std::unique_ptr<Value> value_root(typize(expr_root.get(), root_scope));
    
    return 0;
}
