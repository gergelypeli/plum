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
std::string local_path, global_path;


std::string read_source(std::string file_name) {
    std::ifstream source(file_name, std::ios::binary);
    
    if (!source.is_open()) {
        std::cerr << "Can't open file " << file_name << "!\n";
        throw INTERNAL_ERROR;
    }
    
    std::string buffer((std::istreambuf_iterator<char>(source)),
                       (std::istreambuf_iterator<char>()));
                       
    buffer.push_back('\0');
    
    return buffer;
}


void import(std::string required_name, Root *root) {
    std::string file_name = required_name;
    
    for (unsigned i = 0; i < file_name.size(); i++)
        if (file_name[i] == '.')
            file_name[i] = '/';
            
    if (file_name == "")
        file_name = local_path;  // application main module
    else if (file_name[0] == '/')
        file_name = local_path + file_name;  // application submodule
    else
        file_name = global_path + "/" + file_name;  // system module
        
    file_name = file_name + ".plum";

    std::string display_name = (required_name.size() ? required_name : "<main>");
    std::cerr << "Importing module " << display_name << " from " << file_name << "\n";
    std::string buffer = read_source(file_name);
    
    std::vector<Token> tokens = tokenize(buffer);
    //for (auto &token : tokens)
    //    std::cerr << "Token: " << token.text << "\n";
    
    std::vector<Node> nodes = treeize(tokens);

    std::unique_ptr<Expr> expr_root(tupleize(nodes));
    //print_expr_tree(expr_root.get(), 0, "*");

    root->typize_module(required_name, expr_root.get());
}


Scope *lookup_module(std::string required_name, Scope *scope) {
    ModuleScope *this_scope = scope->get_module_scope();
    std::string this_name = this_scope->module_name;
    Module *this_module = root->modules_by_name[this_name];
    
    if (!this_module)
        throw INTERNAL_ERROR;
    
    if (required_name[0] == '.')
        required_name = this_module->package_name + required_name;
    
    this_module->required_module_names.insert(required_name);
    
    if (!root->modules_by_name.count(required_name))
        import(required_name, root);

    return root->get_module_scope(required_name);
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

    if (!desuffix(input, ".plum")) {
        std::cerr << "Input file name must have a .plum suffix!\n";
        return 1;
    }

    std::string cwd = get_working_path();

    if (input[0] == '/')
        local_path = input;
    else
        local_path = cwd + "/" + input;
        
    global_path = local_path.substr(0, local_path.rfind('/'));  // TODO

    RootScope *root_scope = init_builtins();
    root = new Root(root_scope);
    
    import("", root);
    root->order_modules("");
    
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
