#include <string>
#include <vector>
#include <map>
#include <stack>
#include <memory>
#include <iostream>
#include <sstream>
#include <fstream>

#include <stdarg.h>

#include "util.cpp"
#include "utf8.c"
#include "arch/ork.cpp"
#include "arch/x64.cpp"
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
    
    std::vector<Node> nodes = treeize(tokens);

    std::unique_ptr<Expr> expr_root(tupleize(nodes));
    print_expr_tree(expr_root.get(), 0, "*");

    Scope *root_scope = init_types();
    Scope *module_scope = new Scope();
    root_scope->add(module_scope);
    std::unique_ptr<Value> value_root(typize(expr_root.get(), module_scope));
    
    X64 *x64 = new X64();
    x64->init("mymodule");

    root_scope->allocate();

    // Must mark imported functions first as sysv
    for (auto &decl : root_scope->contents) {
        Function *f = dynamic_cast<Function *>(decl.get());
        if (f)
            f->import(x64);
    }
    
    // This one is not part of the user scope
    std::vector<TypeSpec> no_types;
    std::vector<std::string> no_names;
    Function *alloc_function = new Function("memalloc", VOID_TS, no_types, no_names, VOID_TS);
    Function *free_function = new Function("memfree", VOID_TS, no_types, no_names, VOID_TS);
    alloc_function->allocate();
    alloc_function->import(x64);
    free_function->allocate();
    free_function->import(x64);
    
    x64->init_memory_management(alloc_function->x64_label, free_function->x64_label);
    
    value_root->precompile();
    value_root->compile(x64);
    
    x64->done(argv[2]);
    
    return 0;
}
