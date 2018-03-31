#include <string>
#include <vector>
#include <map>
#include <set>
#include <stack>
#include <memory>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>

#include <stdarg.h>

#include "utf8.c"
#include "util.cpp"
#include "arch/ork.cpp"
#include "arch/x64.cpp"
#include "arch/runtime.cpp"
#include "arch/storage.cpp"
#include "tokenize.cpp"
#include "treeize.cpp"
#include "tupleize.cpp"
#include "typize.cpp"


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
    if (argc != 3)
        return 1;
        
    std::string buffer = read_source(argv[1]);
    
    std::vector<Token> tokens = tokenize(buffer);
    //for (auto &token : tokens)
    //    std::cerr << "Token: " << token.text << "\n";
    
    std::vector<Node> nodes = treeize(tokens);

    std::unique_ptr<Expr> expr_root(tupleize(nodes));
    //print_expr_tree(expr_root.get(), 0, "*");

    Scope *root_scope = init_builtins();
    DataScope *module_scope = new DataScope;
    //module_scope->set_pivot_type_hint(VOID_TS);  // FIXME: something else!
    root_scope->add(module_scope);
    std::unique_ptr<DataBlockValue> value_root;
    
    value_root.reset(new DataBlockValue(module_scope));
    for (auto &a : expr_root->args)
        value_root->check_statement(a.get());
        
    value_root->complete_definition();
    //Value *v = typize(expr_root.get(), module_scope);

    root_scope->allocate();
    
    X64 *x64 = new X64();
    x64->init("mymodule");

    x64->unwind = new Unwind();
    x64->once = new Once();
    x64->runtime = new Runtime(x64);

    // Must mark imported functions first as sysv
    ImportedFunction::import_all(x64);
    
    value_root->precompile(Regs::all());
    value_root->compile(x64);
    x64->once->for_all(x64);

    x64->done(argv[2]);
    
    return 0;
}
