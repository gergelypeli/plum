
enum FunctionType {
    GENERIC_FUNCTION, INTERFACE_FUNCTION, INITIALIZER_FUNCTION, FINALIZER_FUNCTION
};


enum FunctionProt {
    NATIVE_FUNCTION, SYSV_FUNCTION
};

    
class Function: public Identifier, public VirtualEntry {
public:
    std::vector<TypeSpec> arg_tss;
    std::vector<std::string> arg_names;
    std::vector<TypeSpec> res_tss;
    TreenumerationType *exception_type;
    FunctionScope *fn_scope;
    
    int virtual_index;
    FunctionType type;
    FunctionProt prot;
    
    Role *associated_role;
    Function *implemented_function;

    Label label;
    
    Function(std::string n, TypeSpec pts, FunctionType ft, std::vector<TypeSpec> ats, std::vector<std::string> ans, std::vector<TypeSpec> rts, TreenumerationType *et, FunctionScope *fs)
        :Identifier(n, pts) {
        type = ft;
        arg_tss = ats;
        arg_names = ans;
        res_tss = rts;
        exception_type = et;
        fn_scope = fs;
        
        virtual_index = -1;  // for class methods only
        prot = NATIVE_FUNCTION;
        associated_role = NULL;  // for overriding class methods only
        implemented_function = NULL;
    }

    virtual void set_outer_scope(Scope *os) {
        // Abuse here, too
        Identifier::set_outer_scope(os);
        
        // Some built-in interface functions may have no fn scope now
        if (fn_scope)
            fn_scope->set_outer_scope(os);
    }

    virtual Value *matched(Value *cpivot, Scope *scope, TypeMatch &match) {
        // TODO: do this properly!
        
        if (type == INTERFACE_FUNCTION) {
            std::cerr << "Oops, interface function " << name << " was called instead of an implementation!\n";
            throw INTERNAL_ERROR;
        }
        
        return make<FunctionCallValue>(this, cpivot, match);
    }

    virtual std::vector<TypeSpec> get_result_tss(TypeMatch &match) {
        std::vector<TypeSpec> tss;
        for (auto &ts : res_tss)
            tss.push_back(typesubst(ts, match));
        return tss;
    }
    
    virtual TypeSpec get_pivot_typespec(TypeMatch &match) {
        return typesubst(pivot_ts, match);
    }
    
    virtual std::vector<TypeSpec> get_argument_tss(TypeMatch &match) {
        std::vector<TypeSpec> tss;
        for (auto &ts : arg_tss)
            tss.push_back(typesubst(ts, match));
        return tss;
    }
    
    virtual std::vector<std::string> get_argument_names() {
        return arg_names;
    }

    virtual void allocate() {
        DataScope *ds = ptr_cast<DataScope>(outer_scope);
        
        if (ds && ds->is_virtual_scope() && type == GENERIC_FUNCTION) {
            if (!associated_role) {
                std::vector<VirtualEntry *> vt;
                vt.push_back(this);
                virtual_index = ds->virtual_reserve(vt);
                std::cerr << "Reserved new virtual index " << virtual_index << " for function " << name << ".\n";
            }
            else {
                // Copying it is necessary, as overriding functions can only get it from each other
                virtual_index = implemented_function->virtual_index;
                int virtual_offset = role_get_virtual_offset(associated_role);
                ds->set_virtual_entry(virtual_offset + virtual_index, this);
                std::cerr << "Set virtual index " << virtual_offset << "+" << virtual_index << " for function " << name << ".\n";
            }
        }
        
        if (fn_scope)
            fn_scope->allocate();
    }
    
    virtual Label get_virtual_entry_label(TypeMatch tm, X64 *x64) {
        // We're not yet ready to compile templated functions
        if (tm[1] != NO_TS)
            throw INTERNAL_ERROR;
            
        //std::cerr << "Function entry " << name << ".\n";
        return get_label(x64);
    }
    
    virtual Label get_label(X64 *x64) {
        return label;
    }
    
    virtual bool does_implement(std::string prefix, TypeMatch tm, Function *iff, TypeMatch iftm) {
        // The pivot type is not checked, since it is likely to be different
        
        if (name != prefix + iff->name)
            return false;
    
        // The interface arguments must be converted to the implementation arguments
        TSs imp_arg_tss = get_argument_tss(tm);
        TSs int_arg_tss = iff->get_argument_tss(iftm);
        Ss imp_arg_names = get_argument_names();
        Ss int_arg_names = iff->get_argument_names();
        
        if (imp_arg_tss.size() != int_arg_tss.size()) {
            std::cerr << "Mismatching implementation argument length!\n";
            return false;
        }
        
        for (unsigned i = 0; i < imp_arg_tss.size(); i++) {
            if (imp_arg_names[i] != int_arg_names[i]) {
                std::cerr << "Mismatching implementation argument names!\n";
                return false;
            }
            
            if (!converts(int_arg_tss[i], imp_arg_tss[i])) {
                std::cerr << "Mismatching implementation argument types!\n";
                return false;
            }
        }

        // The implementation result must be converted to the interface results
        TSs imp_res_tss = get_result_tss(tm);
        TSs int_res_tss = iff->get_result_tss(iftm);
        
        if (imp_res_tss.size() != int_res_tss.size()) {
            std::cerr << "Mismatching implementation result length!\n";
            return false;
        }
        
        for (unsigned i = 0; i < imp_res_tss.size(); i++) {
            if (!converts(imp_res_tss[i], int_res_tss[i])) {
                std::cerr << "Mismatching implementation result types!\n";
                return false;
            }
        }

        // TODO: this should be referred somehow even if anonymous!
        if (exception_type != iff->exception_type) {
            std::cerr << "Mismatching implementation exception types, " <<
                print_exception_type(exception_type) << " is not " <<
                print_exception_type(iff->exception_type) << "!\n";
            return false;
        }
        
        implemented_function = iff;
        return true;
    }

    virtual void set_associated_role(Role *ar) {
        associated_role = ar;
    }
    
    virtual void set_associated_lself(Lself *al) {
        Scope *ss = fn_scope->self_scope;
        if (ss->contents.size() != 1)
            throw INTERNAL_ERROR;
            
        Variable *self_var = ptr_cast<Variable>(ss->contents[0].get());
        if (!self_var)
            throw INTERNAL_ERROR;
            
        pivot_ts = pivot_ts.lvalue();
        self_var->alloc_ts = self_var->alloc_ts.lvalue();
    }
};


class SysvFunction: public Function {
public:
    std::string import_name;
    
    SysvFunction(std::string in, std::string n, TypeSpec pts, FunctionType ft, std::vector<TypeSpec> ats, std::vector<std::string> ans, std::vector<TypeSpec> rts, TreenumerationType *et, FunctionScope *fs)
        :Function(n, pts, ft, ats, ans, rts, et, fs) {
        import_name = in;
        prot = SYSV_FUNCTION;
    }

    virtual Label get_label(X64 *x64) {
        return x64->once->import_got(import_name);
    }
};


class ImportedFloatFunction: public Identifier {
public:
    std::string import_name;
    TypeSpec arg_ts;
    TypeSpec res_ts;
    
    ImportedFloatFunction(std::string in, std::string n, TypeSpec pts, TypeSpec ats, TypeSpec rts)
        :Identifier(n, pts) {
        import_name = in;
        arg_ts = ats;
        res_ts = rts;
    }
    
    virtual Value *matched(Value *cpivot, Scope *scope, TypeMatch &match) {
        return make<FloatFunctionValue>(this, cpivot, match);
    }
};
