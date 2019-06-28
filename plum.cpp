#include "plum.h"


#if defined __x86_64__
typedef Cx_X64 Cx_HOST;
#elif defined __aarch64__
typedef Cx_A64 Cx_HOST;
#else
#error "Must be compiled on x86-64 or aarch64 hosts!"
#endif


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
    
    Cx *cx = new Cx_HOST("mymodule");
    cx->init(application_size, root->source_file_names);

    int low_pc = cx->get_pc();
    root->compile_modules(cx);
    cx->compile_rest();
    int high_pc = cx->get_pc();
    
    cx->dwarf->begin_compile_unit_info(root->source_file_names[1], "plum", low_pc, high_pc);
    root_scope->debug(TypeMatch(), cx);
    cx->debug_rest();
    cx->dwarf->end_info();

    cx->dwarf->add_frame_description_entry(low_pc, high_pc);

    cx->dwarf->finish();
    
    cx->done(output);
    
    std::cerr << "Done.\n";
    return 0;
}
