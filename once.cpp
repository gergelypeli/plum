
class Once {
public:
    typedef void (*FunctionCompiler)(Label, X64 *);
    typedef void (*TypedFunctionCompiler)(Label, TypeSpec, X64 *);
    
    std::map<FunctionCompiler, Label> function_compiler_labels;
    std::map<std::pair<TypedFunctionCompiler, TypeSpec>, Label> typed_function_compiler_labels;
    
    Label compile(FunctionCompiler fc) {
        Label label = function_compiler_labels[fc];
        //std::cerr << "Will compile once " << (void *)fc << " as " << label.def_index << ".\n";
        return label;
    }

    Label compile(TypedFunctionCompiler tfc, TypeSpec ts) {
        Label label = typed_function_compiler_labels[make_pair(tfc, ts)];
        //std::cerr << "Will compile once " << (void *)tfc << " " << ts << " as " << label.def_index << ".\n";
        return label;
    }

    void for_all(X64 *x64) {
        // NOTE: once functions may ask to once compile other functions.
        // But at least an untyped one can't compile a typed one.
        
        while (typed_function_compiler_labels.size()) {
            auto tmp = typed_function_compiler_labels;
            typed_function_compiler_labels.clear();
            
            for (auto &kv : tmp) {
                auto &key = kv.first;
                TypedFunctionCompiler tfc = key.first;
                TypeSpec ts = key.second;
                Label label = kv.second;
        
                //std::cerr << "Now compiling " << (void *)tfc << " " << ts << " as " << label.def_index << ".\n";
                tfc(label, ts, x64);
            }
        }

        while (function_compiler_labels.size()) {
            auto tmp = function_compiler_labels;
            function_compiler_labels.clear();
            
            for (auto &kv : tmp) {
                FunctionCompiler fc = kv.first;
                Label label = kv.second;

                //std::cerr << "Now compiling " << (void *)fc << " as " << label.def_index << ".\n";
                fc(label, x64);
            }
        }
    }
};

