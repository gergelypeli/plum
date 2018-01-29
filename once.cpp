
class Once {
public:
    typedef void (*FunctionCompiler)(Label, X64 *);
    typedef void (*TypedFunctionCompiler)(Label, TypeSpec, X64 *);
    typedef std::pair<TypedFunctionCompiler, TypeSpec> FunctionCompilerTuple;
    
    std::map<FunctionCompiler, Label> function_compiler_labels;
    std::map<FunctionCompilerTuple, Label> typed_function_compiler_labels;
    
    std::set<FunctionCompiler> function_compiler_todo;
    std::set<FunctionCompilerTuple> typed_function_compiler_todo;
    
    Label compile(FunctionCompiler fc) {
        int before = function_compiler_labels.size();
        Label label = function_compiler_labels[fc];
        int after = function_compiler_labels.size();
        
        if (after != before) {
            function_compiler_todo.insert(fc);
            //std::cerr << "Will compile once " << (void *)fc << " as " << label.def_index << ".\n";
        }
        
        return label;
    }

    Label compile(TypedFunctionCompiler tfc, TypeSpec ts) {
        int before = typed_function_compiler_labels.size();
        FunctionCompilerTuple t = make_pair(tfc, ts);
        Label label = typed_function_compiler_labels[t];
        int after = typed_function_compiler_labels.size();
        
        if (after != before) {
            typed_function_compiler_todo.insert(t);
            //std::cerr << "Will compile once " << (void *)tfc << " " << ts << " as " << label.def_index << ".\n";
        }
        
        return label;
    }

    void for_all(X64 *x64) {
        // NOTE: once functions may ask to once compile other functions.
        
        while (typed_function_compiler_todo.size() || function_compiler_todo.size()) {
            while (typed_function_compiler_todo.size()) {
                FunctionCompilerTuple t = *typed_function_compiler_todo.begin();
                typed_function_compiler_todo.erase(typed_function_compiler_todo.begin());
            
                TypedFunctionCompiler tfc = t.first;
                TypeSpec ts = t.second;
                Label label = typed_function_compiler_labels[t];
        
                //std::cerr << "Now compiling " << (void *)tfc << " " << ts << " as " << label.def_index << ".\n";
                tfc(label, ts, x64);
            }

            while (function_compiler_todo.size()) {
                FunctionCompiler fc = *function_compiler_todo.begin();
                function_compiler_todo.erase(function_compiler_todo.begin());
            
                Label label = function_compiler_labels[fc];

                //std::cerr << "Now compiling " << (void *)fc << " as " << label.def_index << ".\n";
                fc(label, x64);
            }
        }
    }
};
