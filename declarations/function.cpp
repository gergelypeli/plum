
enum FunctionType {
    GENERIC_FUNCTION, LVALUE_FUNCTION, INITIALIZER_FUNCTION, FINALIZER_FUNCTION
};


enum FunctionProt {
    NATIVE_FUNCTION, SYSV_FUNCTION
};


VirtualEntry *make_method_virtual_entry(Function *f);

    
class Function: public Identifier {
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
    
    Function(std::string n, FunctionType ft, std::vector<TypeSpec> ats, std::vector<std::string> ans, std::vector<TypeSpec> rts, TreenumerationType *et, FunctionScope *fs)
        :Identifier(n) {
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

    Function *clone_abstract(std::string prefix) {
        // Used in Implementation inside an Abstract only, to get a virtual index
        // while staying abstract
        if (!is_abstract())
            throw INTERNAL_ERROR;
            
        return new Function(prefix + name, type, arg_tss, arg_names, res_tss, exception_type, NULL);
    }

    virtual bool is_abstract() {
        DataScope *ds = ptr_cast<DataScope>(outer_scope);
        
        return ds && ds->is_abstract_scope();
    }

    virtual void set_outer_scope(Scope *os) {
        // Abuse here, too
        Identifier::set_outer_scope(os);
        
        // Some built-in interface functions may have no fn scope now
        if (fn_scope)
            fn_scope->set_outer_scope(os);
    }

    virtual Value *matched(Value *cpivot, Scope *scope, TypeMatch &match) {
        return make<FunctionCallValue>(this, cpivot, match);
    }

    virtual void get_parameters(TypeSpec &pts, TSs &rtss, TSs &atss, Ss &anames, TypeSpec ts, TypeMatch tm) {
        TypeSpec pivot_ts = get_pivot_ts();
        
        if (is_abstract()) {
            // FIXME: don't interface methods all have a Ptr pivot now?
            if (pivot_ts == ANY_TS) {
                throw INTERNAL_ERROR;
                pts = ts.rvalue();
            }
            else if (pivot_ts == ANY_LVALUE_TS) {
                throw INTERNAL_ERROR;
                pts = ts;
            }
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
        bool needs_virtual_index = (ds && ds->is_virtual_scope() && type == GENERIC_FUNCTION);
        
        if (needs_virtual_index) {
            VirtualEntry *mve = make_method_virtual_entry(this);
            
            if (!associated) {
                virtual_index = ds->virtual_reserve(mve);
                std::cerr << "Reserved new virtual index " << virtual_index << " for function " << name << ".\n";
            }
            else if (implemented_function->virtual_index == 0) {
                // Implemented an interface function in an abstract/class.
                // This also means that such functions will get the indexes in the
                // implementation order, not in the interface order, but that's fine.
                virtual_index = ds->virtual_reserve(mve);
                std::cerr << "Reserved new abstract virtual index " << virtual_index << " for function " << name << ".\n";
            }
            else {
                // Copying it is necessary, as overriding functions can only get it from each other
                virtual_index = implemented_function->virtual_index;
                associable_override_virtual_entry(associated, virtual_index, mve);
                std::cerr << "Set virtual index " << virtual_index << " for function " << name << ".\n";
            }

            if (associable_is_or_is_in_requiring(associated))
                fn_scope->make_associated_offset_storage();
        }
        
        if (fn_scope)
            fn_scope->allocate();
    }
    
    virtual Label get_label(X64 *x64) {
        if (is_abstract())
            throw INTERNAL_ERROR;
            
        return label;
    }
    
    virtual bool does_implement(TypeMatch tm, Function *iff, TypeMatch iftm) {
        if (iff->type == GENERIC_FUNCTION && type == LVALUE_FUNCTION) {
            std::cerr << "Mismatching implementation procedure for function!\n";
            return false;
        }

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
};


class SysvFunction: public Function {
public:
    std::string import_name;
    
    SysvFunction(std::string in, std::string n, FunctionType ft, std::vector<TypeSpec> ats, std::vector<std::string> ans, std::vector<TypeSpec> rts, TreenumerationType *et = NULL, FunctionScope *fs = NULL)
        :Function(n, ft, ats, ans, rts, et, fs) {
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
    
    ImportedFloatFunction(std::string in, std::string n, TypeSpec ats, TypeSpec rts)
        :Identifier(n) {
        import_name = in;
        arg_ts = ats;
        res_ts = rts;
    }
    
    virtual Value *matched(Value *cpivot, Scope *scope, TypeMatch &match) {
        return make<FloatFunctionValue>(this, cpivot, match);
    }
};


class MethodVirtualEntry: public VirtualEntry {
public:
    Function *function;
    
    MethodVirtualEntry(Function *f) {
        function = f;
    }
    
    virtual Label get_virtual_entry_label(TypeMatch tm, X64 *x64) {
        // We're not yet ready to compile templated functions
        if (tm[1] != NO_TS)
            throw INTERNAL_ERROR;
            
        //std::cerr << "Function entry " << name << ".\n";
        return function->get_label(x64);
    }

    virtual std::ostream &out_virtual_entry(std::ostream &os, TypeMatch tm) {
        return os << "FUNC " << function->get_fully_qualified_name();
    }
};


VirtualEntry *make_method_virtual_entry(Function *f) {
    return new MethodVirtualEntry(f);
}


class PatchMethodVirtualEntry: public VirtualEntry {
public:
    Label trampoline_label;
    Function *function;
    int offset;
    
    PatchMethodVirtualEntry(Function *f, int o) {
        function = f;
        offset = o;
    }
    
    virtual void compile(TypeMatch tm, X64 *x64) {
        x64->code_label(trampoline_label);
        //x64->runtime->log("TRAMPOLINE!");
        x64->op(MOVQ, R11, offset);
        x64->op(JMP, function->get_label(x64));
    }
    
    virtual Label get_virtual_entry_label(TypeMatch tm, X64 *x64) {
        // We're not yet ready to compile templated functions
        if (tm[1] != NO_TS)
            throw INTERNAL_ERROR;
            
        //std::cerr << "Function entry " << name << ".\n";
        return trampoline_label;
    }

    virtual std::ostream &out_virtual_entry(std::ostream &os, TypeMatch tm) {
        return os << "FUNC " << function->get_fully_qualified_name() << " (" << offset << ")";
    }
};

