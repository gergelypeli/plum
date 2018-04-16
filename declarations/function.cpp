
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
    int virtual_index;
    FunctionType type;
    FunctionProt prot;
    
    Role *containing_role;
    Function *implemented_function;

    Label label;
    
    Function(std::string n, TypeSpec pts, FunctionType ft, std::vector<TypeSpec> ats, std::vector<std::string> ans, std::vector<TypeSpec> rts, TreenumerationType *et)
        :Identifier(n, pts) {
        type = ft;
        arg_tss = ats;
        arg_names = ans;
        res_tss = rts;
        exception_type = et;
        virtual_index = -1;

        prot = NATIVE_FUNCTION;
        containing_role = NULL;
        implemented_function = NULL;
    }

    virtual Value *matched(Value *cpivot, TypeMatch &match) {
        // TODO: do this properly!
        
        if (type == INTERFACE_FUNCTION) {
            std::cerr << "Oops, interface function " << name << " was called instead of an implementation!\n";
            throw INTERNAL_ERROR;
        }
        
        return make_function_call_value(this, cpivot, match);
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
            if (!containing_role) {
                std::vector<VirtualEntry *> vt;
                vt.push_back(this);
                virtual_index = ds->virtual_reserve(vt);
                std::cerr << "Reserved new virtual index " << virtual_index << " for function " << name << ".\n";
            }
            else {
                // Copying it is necessary, as overriding functions can only get it from each other
                virtual_index = implemented_function->virtual_index;
                ds->set_virtual_entry(virtual_index, this);
                std::cerr << "Set virtual index " << virtual_index << " for function " << name << ".\n";
            }
        }
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
    
    virtual bool does_implement(TypeMatch tm, Function *iff, TypeMatch iftm) {
        if (name != iff->name)
            return false;
    
        if (get_argument_tss(tm) != iff->get_argument_tss(iftm)) {
            std::cerr << "Mismatching " << name << " implementation argument types: " <<
                get_argument_tss(tm) << " should be " << iff->get_argument_tss(iftm) << "!\n";
            return false;
        }
        
        if (get_argument_names() != iff->get_argument_names()) {
            std::cerr << "Mismatching implementation argument names!\n";
            return false;
        }

        if (get_result_tss(tm) != iff->get_result_tss(iftm)) {
            std::cerr << "Mismatching implementation result types!\n";
            return false;
        }
        
        // TODO: this should be referred somehow even if anonymous!
        if (exception_type != iff->exception_type) {
            std::cerr << "Mismatching exception types, " <<
                print_exception_type(exception_type) << " is not " <<
                print_exception_type(iff->exception_type) << "!\n";
            return false;
        }
        
        implemented_function = iff;
        return true;
    }

    virtual bool set_role_scope(RoleScope *rs) {
        containing_role = rs->get_role();
        
        Declaration *original_declaration = rs->get_original_declaration(name);
        Function *original_function = ptr_cast<Function>(original_declaration);
        
        if (!original_function) {
            std::cerr << "No original function " << name << "!\n";
            return false;
        }
        
        TypeMatch role_tm;  // assume parameterless outermost class, derive role parameters
        containing_role->compute_match(role_tm);

        return does_implement(TypeMatch(), original_function, role_tm);
    }
};


class SysvFunction: public Function {
public:
    std::string import_name;
    
    SysvFunction(std::string in, std::string n, TypeSpec pts, FunctionType ft, std::vector<TypeSpec> ats, std::vector<std::string> ans, std::vector<TypeSpec> rts, TreenumerationType *et)
        :Function(n, pts, ft, ats, ans, rts, et) {
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
    
    virtual Value *matched(Value *cpivot, TypeMatch &match) {
        return make_float_function_value(this, cpivot, match);
    }
};
