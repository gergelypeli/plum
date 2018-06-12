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
#include <math.h>

#include "utf8.c"
#include "util.cpp"

#include "arch/ork.cpp"
#include "arch/heap.h"
#include "arch/basics.cpp"
#include "arch/x64.cpp"
#include "arch/runtime.cpp"
#include "arch/storage.cpp"

#include "tokenize.cpp"
#include "treeize.cpp"
#include "tupleize.cpp"

#include "global_types.h"
#include "global_functions.h"
#include "builtins.h"
#include "builtins_errno.h"

#include "declarations/declaration.cpp"
#include "values/value.cpp"

#include "builtins.cpp"
#include "global_functions.cpp"
#include "global_types.cpp"

#include "typize.cpp"

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


struct Module {
    std::string file_name;
    std::unique_ptr<Value> value;
    ModuleScope *module_scope;
    std::set<std::string> required_module_names;
};


std::map<std::string, Module> modules_by_name;
std::vector<Module> modules_in_order;


void import(std::string module_name, std::string file_name, Scope *root_scope) {
    std::cerr << "Importing module " << module_name << " from " << file_name << "\n";
    std::string buffer = read_source(file_name);
    
    std::vector<Token> tokens = tokenize(buffer);
    //for (auto &token : tokens)
    //    std::cerr << "Token: " << token.text << "\n";
    
    std::vector<Node> nodes = treeize(tokens);

    std::unique_ptr<Expr> expr_root(tupleize(nodes));
    //print_expr_tree(expr_root.get(), 0, "*");

    ModuleScope *module_scope = new ModuleScope(module_name);
    module_scope->outer_scope = root_scope;
    //root_scope->add(module_scope);
    DataBlockValue *value_root = new DataBlockValue(module_scope);

    // Must install Module entry before typization
    modules_by_name[module_name] = Module {
        file_name,
        std::unique_ptr<Value>(value_root),
        module_scope,
        {}
    };

    for (auto &a : expr_root->args)
        value_root->check_statement(a.get());
        
    if (!value_root->complete_definition()) {
        std::cerr << "Error compiling module " << module_name << "!\n";
        throw INTERNAL_ERROR;  // FIXME
    }
}


ModuleScope *lookup_module(std::string module_name, ModuleScope *module_scope) {
    if (modules_by_name.count(module_scope->module_name) != 1)
        throw INTERNAL_ERROR;
        
    Module &this_module = modules_by_name[module_scope->module_name];
    this_module.required_module_names.insert(module_name);
    
    if (!modules_by_name.count(module_name)) {
        std::string prefix;
        std::string::size_type i = this_module.file_name.rfind("/");
        
        if (i != std::string::npos)
            prefix = this_module.file_name.substr(0, i + 1);
        
        std::string file_name = prefix + module_name + ".plum";
        Scope *root_scope = module_scope->outer_scope;
        
        import(module_name, file_name, root_scope);
    }
    
    return modules_by_name[module_name].module_scope;
}


void order_modules(std::string name) {
    if (!modules_by_name.count(name))
        return;  // already collected
        
    for (auto &n : modules_by_name[name].required_module_names)
        order_modules(n);
        
    modules_in_order.push_back(std::move(modules_by_name[name]));
    modules_by_name.erase(name);
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

    Scope *root_scope = init_builtins();
    
    import("main", input, root_scope);
    
    order_modules("main");
    
    // Does not allocate the module contents
    root_scope->allocate();

    for (Module &m : modules_in_order)
        m.module_scope->allocate();
    
    X64 *x64 = new X64();
    x64->init("mymodule");

    x64->unwind = new Unwind();
    x64->once = new Once();
    x64->runtime = new Runtime(x64);

    for (Module &m : modules_in_order) {
        m.value->precompile(Regs::all());
        m.value->compile(x64);
    }
    
    x64->once->for_all(x64);
    x64->done(output);
    
    std::cerr << "Done.\n";
    return 0;
}
