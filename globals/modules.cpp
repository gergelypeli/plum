
class Module {
public:
    std::string module_name;
    std::string package_name;
    ModuleScope *module_scope;
    DataBlockValue *value_root;
    std::set<std::string> required_module_names;
    //std::vector<GlobalVariable *> member_globals;
    
    Module(std::string mn, RootScope *rs) {
        module_name = mn;
        package_name = mn;  // TODO: allow explicit submodule declarations
        
        module_scope = new ModuleScope(module_name, rs);
        value_root = new DataBlockValue(module_scope);
    }
    
    bool typize(Expr *expr_root) {
        bool ok = true;
        module_scope->enter();

        if (expr_root->type == Expr::TUPLE) {
            for (auto &a : expr_root->args)
                ok = ok && value_root->check_statement(a.get());
        }
        else {
            ok = value_root->check_statement(expr_root);
        }
        
        ok = ok && value_root->define_data() && value_root->define_code();
    
        module_scope->leave();

        return ok;
    }
};


class Root {
public:
    std::string local_path, global_path, project_path;
    std::vector<std::string> source_file_names;

    RootScope *root_scope;
    std::map<std::string, Module *> modules_by_name;
    std::vector<Module *> modules_in_order;

    Root(RootScope *rs, std::string lp, std::string gp) {
        root_scope = rs;
        
        local_path = lp;
        global_path = gp;
        
        // Used to shorten name of file names for printing
        project_path = local_path.substr(0, local_path.rfind('/'));  // '/path/to'

        // Such things always come handy
        source_file_names.push_back("<unknown>");
    }

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

    std::string resolve_module(std::string required_name, Scope *scope) {
        ModuleScope *this_scope = scope->get_module_scope();
        std::string this_name = this_scope->name;
        Module *this_module = modules_by_name[this_name];
    
        if (!this_module)
            throw INTERNAL_ERROR;
    
        if (required_name[0] == '.')
            required_name = this_module->package_name + required_name;
    
        this_module->required_module_names.insert(required_name);
        
        return required_name;
    }
    
    Module *get_module(std::string module_name) {
        Module *m = modules_by_name[module_name];
        
        if (m && !m->module_scope->is_left) {
            std::cerr << "Circular module dependencies in " << module_name << "!\n";
            throw TYPE_ERROR;
        }
        
        return m;
    }

    Module *typize_module(std::string module_name, Expr *expr_root) {
        // Must install Module entry before typization to collect imported modules
        Module *m = new Module(module_name, root_scope);
        modules_by_name[module_name] = m;

        bool ok = m->typize(expr_root);
    
        if (!ok) {
            std::string display_name = (module_name.size() ? module_name : "<main>");
            std::cerr << "Error compiling module " << display_name << "!\n";
            throw TYPE_ERROR;
        }
        
        return m;
    }

    Module *import_absolute(std::string module_name) {
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

        return typize_module(module_name, expr_root.get());
    }

    ModuleScope *import_relative(std::string required_name, Scope *scope) {
        std::string module_name = resolve_module(required_name, scope);
        Module *m = get_module(module_name);
    
        if (!m)
            m = import_absolute(module_name);
        
        return m->module_scope;
    }
    
    void order_modules(std::string name) {
        if (!modules_by_name.count(name))
            return;  // already collected
        
        for (auto &n : modules_by_name[name]->required_module_names)
            order_modules(n);
        
        // Now add to the root scope in order (for allocate)
        modules_by_name[name]->module_scope->outer_scope = NULL;
        root_scope->add(modules_by_name[name]->module_scope);  // proper addition
    
        modules_in_order.push_back(modules_by_name[name]);
        modules_by_name.erase(name);
    }

    unsigned allocate_modules() {
        root_scope->leave();
        root_scope->allocate();
        
        return root_scope->size.concretize();
    }

    void compile_modules(X64 *x64) {
        root_scope->set_application_label(x64->runtime->application_label);
    
        for (Module *m : modules_in_order) {
            m->value_root->precompile(Regs::all());
            m->value_root->compile(x64);

            //m->collect_initializer_labels(initializer_labels, x64);
            //m->collect_finalizer_labels(finalizer_labels, x64);
        }

        Module *main_module = modules_in_order.back();
        std::vector<GlobalVariable *> global_variables = root_scope->list_global_variables();
        GlobalVariable *main_global = NULL;
        std::vector<Label> initializer_labels, finalizer_labels;
        
        for (auto g : global_variables) {
            if (g->is_called("Main") && g->outer_scope == main_module->module_scope)
                main_global = g;
                
            initializer_labels.push_back(g->compile_initializer(x64));
            finalizer_labels.push_back(g->compile_finalizer(x64));
        }
    
        if (!main_global) {
            std::cerr << "Missing .Main global variable!\n";
            throw TYPE_ERROR;
        }

        // Generate entry point: invoke the .Main.@.start virtual method.
        // That's the only method of the Main global variable in the main module,
        // which implements the Application abstract.
        Storage main_storage = main_global->get_local_storage();
        
        x64->runtime->compile_start(main_storage, initializer_labels, finalizer_labels);
    }
};
