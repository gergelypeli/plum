#include <string>
#include <vector>
#include <map>
#include <stack>
#include <memory>
#include <iostream>
#include <sstream>
#include <fstream>

#include <stdarg.h>

#include "utf8.c"
#include "util.cpp"
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
    //for (auto &token : tokens)
    //    std::cerr << "Token: " << token.text << "\n";
    
    std::vector<Node> nodes = treeize(tokens);

    std::unique_ptr<Expr> expr_root(tupleize(nodes));
    //print_expr_tree(expr_root.get(), 0, "*");

    Scope *root_scope = init_builtins();
    DataScope *module_scope = new DataScope;
    module_scope->set_pivot_type_hint(VOID_TS);  // FIXME: something else!
    root_scope->add(module_scope);
    std::unique_ptr<Value> value_root;
    
    //try {
        Value *v = typize(expr_root.get(), module_scope);
        value_root.reset(v);
    //} catch (Error) {
    //    std::cerr << "Typize error, terminating!\n";
    //    //return 1;
    //    throw;
    //}

    root_scope->allocate();
    
    X64 *x64 = new X64();
    x64->init("mymodule");

    UnwindStack unwind_stack;
    x64->unwind = &unwind_stack;

    // Must mark imported functions first as sysv
    for (auto &decl : root_scope->contents) {
        ImportedFunction *f = dynamic_cast<ImportedFunction *>(decl.get());
        if (f)
            f->import(x64);
    }
    
    value_root->precompile();
    value_root->compile(x64);
    
    x64->done(argv[2]);
    
    return 0;
}
