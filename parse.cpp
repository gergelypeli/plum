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
#include "stage_2_opize.cpp"
#include "stage_3_typize.cpp"


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
        std::vector<std::string> tokens = tokenize(buffer);
        std::vector<Op> ops = operate(tokens);

        Scope *root_scope = new Scope();
        
        generic_builtin = new Declaration();
        tuple_builtin = new Declaration();
        
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
        Declaration *integer_add = new Function("plus", int_ts, int_ts, int_ts);
        root_scope->add(integer_add);
        
        Declaration *integer_print = new Function("print", TS_VOID, TS_VOID, int_ts);  // FIXME
        root_scope->add(integer_print);
        
        std::unique_ptr<Expr> root(resolve(ops, 0, root_scope));
        
        print_expr_tree(root.get(), 0, "* ");
        
        //for (auto token : tokens)
        //    std::cout << "" << token << "\n";
    }
    catch (Error &e) {
        std::cerr << e << "\n";
        throw;
    }
    
    return 0;
}
