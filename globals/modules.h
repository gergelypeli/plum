
class Module {
public:
    std::string module_name;
    std::string package_name;
    ModuleScope *module_scope;
    DataBlockValue *value_root;
    std::set<std::string> required_module_names;
    
    Module(std::string mn, RootScope *rs);

    virtual bool typize(Expr *expr_root);
};


class Root {
public:
    std::string local_path, global_path, project_path;
    std::vector<std::string> source_file_names;

    RootScope *root_scope;
    std::map<std::string, Module *> modules_by_name;
    std::vector<Module *> modules_in_order;

    Root(RootScope *rs, std::string lp, std::string gp);

    virtual std::string get_source_file_name(int index);
    virtual std::string read_source(std::string file_name);
    virtual std::string resolve_module(std::string required_name, Scope *scope);
    virtual Module *get_module(std::string module_name);
    virtual Module *typize_module(std::string module_name, Expr *expr_root);
    virtual Module *import_absolute(std::string module_name);
    virtual ModuleScope *import_relative(std::string required_name, Scope *scope);
    virtual void order_modules(std::string name);
    virtual unsigned allocate_modules();
    virtual void compile_modules(X64 *x64);
};
