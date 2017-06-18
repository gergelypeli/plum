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
    try {
        std::string buffer = read_source(argv[1]);
        
        std::vector<Token> tokens = tokenize(buffer);
        //for (auto token : tokens)
        //    std::cerr << "" << token << "\n";
        
        std::vector<Node> nodes = treeize(tokens);

        std::unique_ptr<Expr> expr_root(tupleize(nodes));
        
        print_expr_tree(expr_root.get(), 0, "*");
        
        Scope *root_scope = new Scope();
        
        type_type = new Type("<Type>", 1);
        root_scope->add(type_type);

        function_type = new Type("<Function>", 1);
        root_scope->add(function_type);
        
        void_type = new Type("Void", 0);
        root_scope->add(void_type);
        TS_VOID.push_back(void_type);

        integer_type = new Type("Integer", 0);
        root_scope->add(integer_type);
        
        TypeSpec int_ts;
        int_ts.push_back(integer_type);
        
        std::vector<TypeSpec> arg_tss;
        arg_tss.push_back(int_ts);
        std::vector<std::string> arg_names;
        arg_names.push_back("myarg");
        
        Declaration *integer_add = new Function("plus", int_ts, int_ts, arg_tss, arg_names);
        root_scope->add(integer_add);
        
        Declaration *integer_print = new Function("print", TS_VOID, TS_VOID, arg_tss, arg_names);
        root_scope->add(integer_print);
        
        std::unique_ptr<Value> value_root(typize(expr_root.get(), root_scope));
        
    }
    catch (int &e) {
        std::cerr << e << "\n";
        throw;
    }
    
    return 0;
}
