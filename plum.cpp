#include "precompiled.h"

#include "environment/typedefs.h"
#include "environment/text.h"
#include "environment/heap.h"

#include "arch/ork.cpp"
#include "arch/basics.cpp"
#include "arch/asm64.cpp"
#include "arch/storage.cpp"

#include "util.h"

#include "declarations/all.h"
#include "values/all.h"
#include "globals/all.h"
#include "parsing/all.h"

// Top
ModuleScope *lookup_module(std::string required_name, Scope *scope);
ModuleScope *import_module(std::string required_name, Scope *scope);


#include "util.cpp"

#include "parsing/tokenize.cpp"
#include "parsing/treeize.cpp"
#include "parsing/tupleize.cpp"

#include "declarations/all.cpp"
#include "values/all.cpp"
#include "globals/all.cpp"

#include "parsing/typize.cpp"


Root *root;
bool matchlog;
std::string local_path, global_path, project_path;
std::vector<std::string> source_file_names;


std::string get_source_file_name(int index) {
    if (index == -1)
        return "???";
        
    std::string file_name = source_file_names[index];
    
    if (deprefix(file_name, project_path))
        return file_name.substr(1);  // remove leading slash as well
    else
        return file_name;
}


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


Module *import(std::string module_name) {
    std::string file_name = module_name;
    
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
    
    int file_index = source_file_names.size();
    source_file_names.push_back(file_name);

    std::string display_name = (module_name.size() ? module_name : "<main>");
    std::cerr << "Importing module " << display_name << " from " << file_name << "\n";
    std::string buffer = read_source(file_name);
    
    std::ustring text = decode_utf8(buffer);
    
    std::vector<Token> tokens = tokenize(text, file_index);
    //for (auto &token : tokens)
    //    std::cerr << "Token: " << token.text << "\n";
    
    std::vector<Node> nodes = treeize(tokens);

    std::unique_ptr<Expr> expr_root(tupleize(nodes));
    //print_expr_tree(expr_root.get(), 0, "*");

    return root->typize_module(module_name, expr_root.get());
}


ModuleScope *import_module(std::string required_name, Scope *scope) {
    std::string module_name = root->resolve_module(required_name, scope);
    Module *m = root->get_module(module_name);
    
    if (!m)
        m = import(module_name);
        
    return m->module_scope;
}


ModuleScope *lookup_module(std::string required_name, Scope *scope) {
    std::string module_name = root->resolve_module(required_name, scope);
    Module *m = root->get_module(module_name);
    
    if (!m) {
        std::cerr << "Using unimported module " << required_name << "!\n";
        throw TYPE_ERROR;
    }

    return m->module_scope;
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

    // 'to/app.plum'
    if (!desuffix(input, ".plum")) {
        std::cerr << "Input file name must have a .plum suffix!\n";
        return 1;
    }

    std::string cwd = get_working_path();  // '/path'

    if (input[0] == '/')
        local_path = input;
    else
        local_path = cwd + "/" + input;  // '/path/to/app'
        
    // Used to shorten name of file names for printing
    project_path = local_path.substr(0, local_path.rfind('/'));  // '/path/to'
    
    global_path = project_path;  // TODO: path to the global modules

    RootScope *root_scope = init_builtins();
    root = new Root(root_scope);
    
    import("");
    root->order_modules("");
    
    // Allocate builtins and modules
    unsigned application_size = root->allocate_modules();
    
    X64 *x64 = new X64(application_size);
    x64->init("mymodule");

    root->compile_modules(x64);
    
    x64->finish(output, source_file_names);
    
    std::cerr << "Done.\n";
    return 0;
}
