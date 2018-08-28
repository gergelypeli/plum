
class Module {
public:
    std::string module_name;
    std::string package_name;
    ModuleScope *module_scope;
    DataBlockValue *value_root;
    std::set<std::string> required_module_names;
    std::vector<SingletonType *> singleton_types;
    
    Module(std::string mn) {
        module_name = mn;
        package_name = mn;  // TODO: allow explicit submodule declarations
        
        module_scope = new ModuleScope(module_name);
        value_root = new DataBlockValue(module_scope);
    }
    
    bool typize(Expr *expr_root) {
        bool ok = true;

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

        ok = ok && value_root->complete_definition();
    
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

    void typize_module(std::string module_name, Expr *expr_root) {
        // Must install Module entry before typization to collect imported modules
        Module *m = new Module(module_name);
        modules_by_name[module_name] = m;
        m->module_scope->outer_scope = root_scope;  // temporary hack

        bool ok = m->typize(expr_root);
    
        if (!ok) {
            std::cerr << "Error compiling module " << module_name << "!\n";
            throw TYPE_ERROR;
        }
    }

    Scope *get_module_scope(std::string mn) {
        Scope *s = modules_by_name[mn]->module_scope;
        
        if (!s->is_left) {
            std::cerr << "Circular module dependencies in " << mn << "!\n";
            throw TYPE_ERROR;
        }
        
        return s;
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

