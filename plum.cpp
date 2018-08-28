#include "precompiled.h"

#include "environment/typedefs.h"
#include "environment/utf8.h"
#include "environment/heap.h"

#include "arch/ork.cpp"
#include "arch/basics.cpp"
#include "arch/asm64.cpp"
#include "arch/storage.cpp"

#include "util.cpp"

#include "tokenize.cpp"
#include "treeize.cpp"
#include "tupleize.cpp"

#include "all.h"
#include "structs.cpp"

#include "declarations/all.cpp"
#include "values/all.cpp"
#include "globals/all.cpp"

#include "typize.cpp"


Root *root;
bool matchlog;


std::string read_source(std::string filename) {
    std::ifstream source(filename, std::ios::binary);
    
    if (!source.is_open())
        return "";
    
    std::string buffer((std::istreambuf_iterator<char>(source)),
                       (std::istreambuf_iterator<char>()));
                       
    buffer.push_back('\0');
    
    return buffer;
}


void import(std::string module_name, std::string file_name, Root *root) {
    std::cerr << "Importing module " << module_name << " from " << file_name << "\n";
    std::string buffer = read_source(file_name);
    
    std::vector<Token> tokens = tokenize(buffer);
    //for (auto &token : tokens)
    //    std::cerr << "Token: " << token.text << "\n";
    
    std::vector<Node> nodes = treeize(tokens);

    std::unique_ptr<Expr> expr_root(tupleize(nodes));
    //print_expr_tree(expr_root.get(), 0, "*");

    root->compile_module(module_name, file_name, expr_root.get());
}


Scope *lookup_module(std::string module_name, ModuleScope *module_scope) {
    Module *this_module = root->modules_by_name[module_scope->module_name];
    
    if (!this_module)
        throw INTERNAL_ERROR;
    
    this_module->required_module_names.insert(module_name);
    
    if (!root->modules_by_name.count(module_name)) {
        std::string prefix;
        std::string::size_type i = this_module->file_name.rfind("/");
        
        if (i != std::string::npos)
            prefix = this_module->file_name.substr(0, i + 1);
        
        std::string file_name = prefix + module_name + ".plum";
        
        import(module_name, file_name, root);
    }
    
    return root->modules_by_name[module_name]->module_scope;
}


int main(int argc, char **argv) {
    matchlog = false;
    std::string input, output;
    
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (argv[i][1] == 'm')
                matchlog = true;
            else {
                std::cerr << "Invalid option: " << argv[i] << "\n";
                return 1;
            }
        }
        else {
            if (input.size() == 0)
                input = argv[i];
            else if (output.size() == 0)
                output = argv[i];
            else {
                std::cerr << "Excess argument: " << argv[i] << "\n";
                return 1;
            }
        }
    }
    
    if (output.size() == 0) {
        std::cerr << "Not enough arguments!\n";
        return 1;
    }

    RootScope *root_scope = init_builtins();
    root = new Root(root_scope);
    
    import("main", input, root);
    root->order_modules("main");
    
    // Allocate builtins and modules
    unsigned application_size = root->allocate_modules();
    
    X64 *x64 = new X64();
    x64->init("mymodule");

    x64->unwind = new Unwind();
    x64->once = new Once();
    x64->runtime = new Runtime(x64, application_size);

    root->compile_modules(x64);
    
    x64->once->for_all(x64);
    x64->done(output);
    
    std::cerr << "Done.\n";
    return 0;
}
