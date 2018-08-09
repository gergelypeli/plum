#include "precompiled.h"

#include "environment/typedefs.h"
#include "environment/utf8.c"
#include "environment/heap.h"

#include "arch/ork.cpp"
#include "arch/basics.cpp"
#include "arch/x64.cpp"
#include "arch/storage.cpp"

#include "util.cpp"

#include "tokenize.cpp"
#include "treeize.cpp"
#include "tupleize.cpp"

#include "global_types.h"
#include "global_types_x64.h"
#include "global_functions.h"
#include "builtins.h"
#include "builtins_errno.h"

#include "declarations/declaration.cpp"
#include "values/value.cpp"

#include "builtins.cpp"
#include "global_functions.cpp"
#include "global_types.cpp"
#include "global_types_x64.cpp"

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
    ModuleType *module_type;
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

    // Don't put in the root scope yet, they will be reordered
    ModuleScope *module_scope = new ModuleScope(module_name);
    module_scope->outer_scope = root_scope;
    DataBlockValue *value_root = new DataBlockValue(module_scope);

    ModuleType *module_type = new ModuleType("<" + module_name + ">", module_scope);
    root_scope->add(module_type);
    module_scope->set_pivot_type_hint(TypeSpec { module_type });

    // Must install Module entry before typization to collect imported modules
    modules_by_name[module_name] = Module {
        file_name,
        std::unique_ptr<Value>(value_root),
        module_scope,
        {},
        module_type
    };
    
    bool ok = true;
    
    if (expr_root->type == Expr::TUPLE) {
        for (auto &a : expr_root->args)
            ok = ok && value_root->check_statement(a.get());
    }
    else {
        ok = value_root->check_statement(expr_root.get());
    }

    // Must complete the type first
    ok = ok && module_type->complete_type();

    ok = ok && value_root->complete_definition();
    
    if (!ok) {
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


void order_modules(std::string name, Scope *root_scope) {
    if (!modules_by_name.count(name))
        return;  // already collected
        
    for (auto &n : modules_by_name[name].required_module_names)
        order_modules(n, root_scope);
        
    // Now add to the root scope in order
    modules_by_name[name].module_scope->outer_scope = NULL;
    root_scope->add(modules_by_name[name].module_scope);
    
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
    
    order_modules("main", root_scope);
    
    // Allocate builtins and modules
    root_scope->allocate();
    
    X64 *x64 = new X64();
    x64->init("mymodule");

    x64->unwind = new Unwind();
    x64->once = new Once();
    x64->runtime = new Runtime(x64, root_scope->size.concretize());

    std::vector<Function *> initializers;

    for (Module &m : modules_in_order) {
        m.value->precompile(Regs::all());
        m.value->compile(x64);
        
        Function *f = m.module_type->get_initializer_function();
        if (f)
            initializers.push_back(f);
    }

    Label init_count, init_ptrs;
    x64->data_label_global(init_count, "initializer_count");
    x64->data_qword(initializers.size());
    
    x64->data_label_global(init_ptrs, "initializer_pointers");
    for (Function *f : initializers)
        x64->data_reference(f->get_label(x64));
    
    x64->once->for_all(x64);
    x64->done(output);
    
    std::cerr << "Done.\n";
    return 0;
}
