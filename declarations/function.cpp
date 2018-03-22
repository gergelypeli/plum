
enum FunctionType {
    GENERIC_FUNCTION, INTERFACE_FUNCTION, INITIALIZER_FUNCTION, FINALIZER_FUNCTION
};

    
class Function: public Identifier {
public:
    std::vector<TypeSpec> arg_tss;
    std::vector<std::string> arg_names;
    std::vector<TypeSpec> res_tss;
    TreenumerationType *exception_type;
    int virtual_index;
    //Allocation self_adjustment;
    //Function *implemented_function;
    Function *override_function;
    std::vector<Role *> override_roles;
    FunctionType type;

    Label x64_label;
    bool is_sysv;
    
    Function(std::string n, TypeSpec pts, FunctionType ft, std::vector<TypeSpec> ats, std::vector<std::string> ans, std::vector<TypeSpec> rts, TreenumerationType *et)
        :Identifier(n, pts) {
        type = ft;
        arg_tss = ats;
        arg_names = ans;
        res_tss = rts;
        exception_type = et;
        virtual_index = -1;
        //implemented_function = NULL;
        override_function = NULL;
        
        is_sysv = false;
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

    /*
    virtual int set_self_adjustment(Allocation alloc) {
        self_adjustment = alloc;
        virtual_index = implemented_function->virtual_index;
        return virtual_index;
    }
    */
    
    virtual void allocate() {
        DataScope *ds = ptr_cast<DataScope>(outer_scope);
        
        if (ds && ds->is_virtual_scope() && type == GENERIC_FUNCTION) {
            if (!override_function) {
                std::vector<Function *> vt;
                vt.push_back(this);
                virtual_index = ds->virtual_reserve(vt);
            }
            else {
                int vti = override_function->virtual_index;
                for (Role *role : override_roles)
                    vti += role->virtual_offset;
                ds->set_virtual_entry(vti, this);
            }
        }
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
        
        //implemented_function = iff;
        return true;
    }
    
    virtual bool set_override(Function *f, std::vector<Role *> roles) {
        override_function = f;
        override_roles = roles;
        
        TypeMatch empty_match;
        return does_implement(empty_match, f, empty_match);
    }
    
    virtual Allocation get_override_offset() {
        // FIXME: this should be a general Allocation, but we need to store the TypeMatch for
        // each role, too, to make that work!
        int offset = 0;
        
        for (Role *role : override_roles) {
            offset += role->offset.concretize();
        }
        
        return Allocation(offset);
    }
};


class ImportedFunction: public Function {
public:
    static std::vector<ImportedFunction *> to_be_imported;
    
    static void import_all(X64 *x64) {
        for (auto i : to_be_imported)
            i->import(x64);
    }
    
    std::string import_name;
    
    ImportedFunction(std::string in, std::string n, TypeSpec pts, FunctionType ft, std::vector<TypeSpec> ats, std::vector<std::string> ans, std::vector<TypeSpec> rts, TreenumerationType *et)
        :Function(n, pts, ft, ats, ans, rts, et) {
        import_name = in;
        to_be_imported.push_back(this);
    }

    virtual void import(X64 *x64) {
        is_sysv = true;
        x64->code_label_import(x64_label, import_name);
    }
};

std::vector<ImportedFunction *> ImportedFunction::to_be_imported;

