
enum FunctionType {
    GENERIC_FUNCTION, ABSTRACT_FUNCTION, INITIALIZER_FUNCTION, FINALIZER_FUNCTION
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
    
    Associable *associated;
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
        
        virtual_index = 0;  // for class methods only
        prot = NATIVE_FUNCTION;
        associated = NULL;  // for overriding methods only
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
        
        //if (type == ABSTRACT_FUNCTION) {
        //    std::cerr << "Oops, interface function " << name << " was called instead of an implementation!\n";
        //    throw INTERNAL_ERROR;
        //}
        
        return make<FunctionCallValue>(this, cpivot, match);
    }

    virtual void get_parameters(TypeSpec &pts, TSs &rtss, TSs &atss, Ss &anames, TypeSpec ts, TypeMatch tm) {
        if (type == ABSTRACT_FUNCTION) {
            if (pivot_ts == ANY_TS)
                pts = ts.rvalue();
            else if (pivot_ts == ANY_LVALUE_TS)
                pts = ts;
            else
                pts = typesubst(pivot_ts, tm);
        }
        else
            pts = typesubst(pivot_ts, tm);
        
        for (auto &rts : res_tss)
            rtss.push_back(typesubst(rts, tm));
        
        for (auto &ats : arg_tss)
            atss.push_back(typesubst(ats, tm));
        
        anames = arg_names;
    }

    virtual void allocate() {
        DataScope *ds = ptr_cast<DataScope>(outer_scope);
        bool needs_virtual_index = (ds && ds->is_virtual_scope() && (type == GENERIC_FUNCTION || type == ABSTRACT_FUNCTION));
        
        if (needs_virtual_index) {
            if (!associated) {
                //devector<VirtualEntry *> vt;
                //vt.append(this);
                virtual_index = ds->virtual_reserve(this);
                std::cerr << "Reserved new virtual index " << virtual_index << " for function " << name << ".\n";
            }
            else {
                // Copying it is necessary, as overriding functions can only get it from each other
                virtual_index = implemented_function->virtual_index;
                associable_override_virtual_entry(associated, virtual_index, this);
                std::cerr << "Set virtual index " << virtual_index << " for function " << name << ".\n";
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

    virtual std::ostream &out_virtual_entry(std::ostream &os, TypeMatch tm) {
        return os << "FUNC " << name;
    }
    
    virtual Label get_label(X64 *x64) {
        if (type == ABSTRACT_FUNCTION)
            throw INTERNAL_ERROR;
            
        return label;
    }
    
    virtual bool does_implement(TypeMatch tm, Function *iff, TypeMatch iftm) {
        // The pivot type is not checked, since it is likely to be different

        TypeSpec int_pivot_ts, imp_pivot_ts;
        TSs int_res_tss, imp_res_tss;
        TSs int_arg_tss, imp_arg_tss;
        Ss int_arg_names, imp_arg_names;
        
        iff->get_parameters(int_pivot_ts, int_res_tss, int_arg_tss, int_arg_names, WHATEVER_TS, iftm);
        get_parameters(imp_pivot_ts, imp_res_tss, imp_arg_tss, imp_arg_names, WHATEVER_TS, tm);
        
        // The interface arguments must be converted to the implementation arguments
        
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

    virtual void set_associated(Associable *a) {
        associated = a;
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
