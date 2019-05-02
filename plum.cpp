#include <string>
#include <cstring>
#include <vector>
#include <map>
#include <set>
#include <stack>
#include <memory>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <iomanip>

#include <stdarg.h>
#include <math.h>
#include <unistd.h>

#include <elf.h>
#include <libdwarf/dwarf.h>

// Shared files between the compiler and the runtime, in C
#include "environment/typedefs.h"
#include "environment/text.h"
#include "environment/heap.h"

// Code generation
#include "arch/elf.cpp"
#include "arch/dwarf.cpp"
#include "arch/basics.cpp"
#include "arch/asm64.cpp"
#include "arch/storage.cpp"

// Forward declarations
#include "util.h"
#include "declarations/all.h"
#include "values/all.h"
#include "globals/all.h"
#include "parsing/all.h"
ModuleScope *import_module(std::string required_name, Scope *scope);

// Stuff gets done here
#include "util.cpp"
#include "declarations/all.cpp"
#include "values/all.cpp"
#include "globals/all.cpp"
#include "parsing/all.cpp"

bool matchlog;
Root *root;


ModuleScope *import_module(std::string required_name, Scope *scope) {
    return root->import_relative(required_name, scope);
}

std::string get_source_file_display_name(int index) {
    return root->get_source_file_name(index);
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
    std::string local_path = (input[0] == '/' ? input : cwd + "/" + input);  // '/path/to/app'
    std::string global_path = local_path.substr(0, local_path.rfind('/'));  // '/path/to'

    RootScope *root_scope = init_builtins();
    root = new Root(root_scope, local_path, global_path);
    
    root->import_absolute("");
    root->order_modules("");
    
    // Allocate builtins and modules
    unsigned application_size = root->allocate_modules();
    
    X64 *x64 = new X64("mymodule", application_size, root->source_file_names);

    root->compile_modules(x64);
    
    x64->dwarf->begin_compile_unit_info(root->source_file_names[1], "plum", x64->code.size());
    x64->dwarf->end_info();

    x64->finish(output);
    
    std::cerr << "Done.\n";
    return 0;
}
