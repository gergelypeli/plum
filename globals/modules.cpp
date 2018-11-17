
class Module {
public:
    std::string module_name;
    std::string package_name;
    ModuleScope *module_scope;
    DataBlockValue *value_root;
    std::set<std::string> required_module_names;
    std::vector<SingletonType *> singleton_types;
    
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
        
        // Must complete the type first
        for (auto &c : module_scope->contents) {
            SingletonType *s = ptr_cast<SingletonType>(c.get());
            
            if (s)
                singleton_types.push_back(s);
        }
        
        std::cerr << "Module " << module_name << " has " << singleton_types.size() << " singletons.\n";

        ok = ok && value_root->define_data() && value_root->define_code();
    
        module_scope->leave();

        return ok;
    }

    virtual void collect_initializer_labels(std::vector<Label> &labels, X64 *x64) {
        for (SingletonType *s : singleton_types)
            labels.push_back(s->compile_initializer(x64));
    }

    virtual void collect_finalizer_labels(std::vector<Label> &labels, X64 *x64) {
        for (SingletonType *s : singleton_types)
            labels.push_back(s->compile_finalizer(x64));
    }
};


class Root {
public:
    RootScope *root_scope;
    std::map<std::string, Module *> modules_by_name;
    std::vector<Module *> modules_in_order;

    Root(RootScope *rs) {
        root_scope = rs;
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
            std::cerr << "Error compiling module " << module_name << "!\n";
            throw TYPE_ERROR;
        }
        
        return m;
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
    
        std::vector<Label> initializer_labels, finalizer_labels;

        for (Module *m : modules_in_order) {
            m->value_root->precompile(Regs::all());
            m->value_root->compile(x64);

            m->collect_initializer_labels(initializer_labels, x64);
            m->collect_finalizer_labels(finalizer_labels, x64);
        }

        Label init_count, init_ptrs, fin_count, fin_ptrs;
        x64->data_label_global(init_count, "initializer_count");
        x64->data_qword(initializer_labels.size());
    
        x64->data_label_global(init_ptrs, "initializer_pointers");
        for (Label l : initializer_labels)
            x64->data_reference(l);

        x64->data_label_global(fin_count, "finalizer_count");
        x64->data_qword(finalizer_labels.size());
    
        x64->data_label_global(fin_ptrs, "finalizer_pointers");
        for (Label l : finalizer_labels)
            x64->data_reference(l);
    }
};

