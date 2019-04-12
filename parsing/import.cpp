
Root *root;
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
