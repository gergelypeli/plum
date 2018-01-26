
class Once {
public:
    typedef void (*FunctionCompiler)(Label, X64 *);
    typedef void (*TypedFunctionCompiler)(Label, TypeSpec, X64 *);
    
    std::map<FunctionCompiler, Label> function_compiler_labels;
    std::map<std::pair<TypedFunctionCompiler, TypeSpec>, Label> typed_function_compiler_labels;
    
    Label compile(FunctionCompiler fc) {
        return function_compiler_labels[fc];
    }

    Label compile(TypedFunctionCompiler tfc, TypeSpec ts) {
        return typed_function_compiler_labels[make_pair(tfc, ts)];
    }

    void for_all(X64 *x64) {
        for (auto &kv : function_compiler_labels) {
            FunctionCompiler fn = kv.first;
            Label label = kv.second;
        
            fn(label, x64);
        }

        for (auto &kv : typed_function_compiler_labels) {
            auto &key = kv.first;
            TypedFunctionCompiler fn = key.first;
            TypeSpec ts = key.second;
            Label label = kv.second;
        
            fn(label, ts, x64);
        }
    }
};

