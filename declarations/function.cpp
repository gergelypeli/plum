
enum FunctionType {
    GENERIC_FUNCTION, LVALUE_FUNCTION, INITIALIZER_FUNCTION, FINALIZER_FUNCTION
};


enum FunctionProt {
    NATIVE_FUNCTION, SYSV_FUNCTION
};


//VirtualEntry *make_method_virtual_entry(Function *f);

    
class Function: public Identifier, public Methodlike {
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
    int low_pc;
    int high_pc;
    
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
        
        low_pc = -1;
        high_pc = -1;
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
        
        pts = typesubst(pivot_ts, tm);
        
        if (res_tss.size() == 1 && res_tss[0] == SAMETUPLE_TS) {
            // This is a hack to allow the full return tuple be the tuple type parameter
            tm[1].unpack_tuple(rtss);
        }
        else {
            for (auto &rts : res_tss)
                rtss.push_back(typesubst(rts, tm));
        }
        
        for (auto &ats : arg_tss)
            atss.push_back(typesubst(ats, tm));
        
        anames = arg_names;
    }

    virtual void allocate() {
        DataScope *ds = ptr_cast<DataScope>(outer_scope);
        bool needs_virtual_index = (ds && ds->is_virtual_scope() && type == GENERIC_FUNCTION);
        
        if (needs_virtual_index) {
            VirtualEntry *mve = new MethodVirtualEntry(this);
            
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
    
    virtual Label get_method_label(X64 *x64) {
        return get_label(x64);
    }
    
    virtual std::string get_method_name() {
        return get_fully_qualified_name();
    }
    
    virtual void set_pc_range(int lo, int hi) {
        low_pc = lo;
        high_pc = hi;
    }
    
    virtual void debug(TypeMatch tm, X64 *x64) {
        bool virtuality = (virtual_index != 0);

        if (is_abstract()) {
            x64->dwarf->begin_abstract_subprogram_info(get_fully_qualified_name(), virtuality);
            x64->dwarf->end_info();
            return;
        }
        
        if (fn_scope->self_scope->contents.empty())
            throw INTERNAL_ERROR;

        if (low_pc < 0 || high_pc < 0)
            throw INTERNAL_ERROR;

        Label self_label;
        unsigned self_index = self_label.def_index;

        x64->dwarf->begin_subprogram_info(get_fully_qualified_name(), low_pc, high_pc, virtuality, self_index);
        
        fn_scope->result_scope->debug(tm, x64);
        
        x64->dwarf->info_def(self_index);
        fn_scope->self_scope->debug(tm, x64);
        
        fn_scope->head_scope->debug(tm, x64);
        
        fn_scope->body_scope->debug(tm, x64);
        
        x64->dwarf->end_info();
    }
};


class SysvFunction: public Function, public Deferrable {
public:
    std::string import_name;
    
    SysvFunction(std::string in, std::string n, FunctionType ft, std::vector<TypeSpec> ats, std::vector<std::string> ans, std::vector<TypeSpec> rts, TreenumerationType *et = NULL, FunctionScope *fs = NULL)
        :Function(n, ft, ats, ans, rts, et, fs) {
        import_name = in;
        prot = SYSV_FUNCTION;
    }

    virtual Label get_label(X64 *x64) {
        return x64->once->compile(this);
    }

    virtual void deferred_compile(Label label, X64 *x64) {
        std::vector<TypeSpec> pushed_tss;
        std::vector<unsigned> pushed_sizes;

        TypeSpec pivot_ts = get_pivot_ts();
        
        if (pivot_ts != NO_TS && pivot_ts != VOID_TS) {  // FIXME
            pushed_tss.push_back(pivot_ts);
            pushed_sizes.push_back(pivot_ts.measure_where(stacked(pivot_ts.where(AS_ARGUMENT))));
        }
        
        for (auto &ats : arg_tss) {
            pushed_tss.push_back(ats);
            pushed_sizes.push_back(ats.measure_where(stacked(ats.where(AS_ARGUMENT))));
        }
        
        unsigned passed_size = 0;
        for (unsigned &s : pushed_sizes)
            passed_size += s;

        if (arg_tss.size() > 5) {
            std::cerr << "Oops, too many arguments to a SysV function!\n";
            throw INTERNAL_ERROR;
        }
        
        if (res_tss.size() > 1) {
            std::cerr << "Oops, too many results from a SysV function!\n";
            throw INTERNAL_ERROR;
        }

        if (res_tss.size() == 1) {
            StorageWhere simple_where = res_tss[0].where(AS_VALUE);
            
            if (simple_where != REGISTER && simple_where != SSEREGISTER) {
                std::cerr << "Oops, not a simple result from a SysV function!\n";
                throw INTERNAL_ERROR;
            }
        }

        x64->code_label_local(label, get_fully_qualified_name() + "__sysv_wrapper");

        // Create a proper stack frame for debugging
        x64->op(PUSHQ, RBP);
        x64->op(MOVQ, RBP, RSP);

        Register regs[] = { RDI, RSI, RDX, RCX, R8, R9 };
        SseRegister sses[] = { XMM0, XMM1, XMM2, XMM3, XMM4, XMM5 };
        unsigned reg_index = 0;
        unsigned sse_index = 0;
        
        // Account for the return address and the saved RBP
        unsigned stack_offset = passed_size + 2 * ADDRESS_SIZE;

        for (unsigned i = 0; i < pushed_tss.size(); i++) {
            // Must move raw values so it doesn't count as a copy
            stack_offset -= pushed_sizes[i];
            
            StorageWhere pushed_where = pushed_tss[i].where(AS_VALUE);
            
            if (pushed_where == NOWHERE)
                ;  // happens for singleton pivots
            else if (pushed_where == SSEREGISTER)
                x64->op(MOVSD, sses[sse_index++], Address(RBP, stack_offset));
            else if (pushed_sizes[i] == ADDRESS_SIZE)
                x64->op(MOVQ, regs[reg_index++], Address(RBP, stack_offset));
            else
                x64->op(LEA, regs[reg_index++], Address(RBP, stack_offset));
        }
        
        Label got_label = x64->once->import_got(import_name);
        x64->runtime->call_sysv_got(got_label);

        //x64->runtime->dump("Returned from SysV.");

        // We return simple values in RAX and XMM0 like SysV.
        // But exceptions are always in RAX, so it may need a fix.
        StorageWhere simple_where = (res_tss.size() ? res_tss[0].where(AS_VALUE) : NOWHERE);

        switch (simple_where) {
        case NOWHERE:
            if (exception_type) {
                x64->op(MOVQ, RDX, RAX);
            }
            break;
        case REGISTER:
            if (exception_type) {
                x64->op(XCHGQ, RDX, RAX);
            }
            break;
        case SSEREGISTER:
            if (exception_type) {
                x64->op(MOVQ, RDX, RAX);
            }
            break;
        default:
            throw INTERNAL_ERROR;
        }

        // Raised exception in RDX.
        if (exception_type)
            x64->op(CMPQ, RDX, NO_EXCEPTION);

        x64->op(POPQ, RBP);
        x64->op(RET);
    }

    virtual void debug(TypeMatch tm, X64 *x64) {
        // Empty
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
    
    virtual Label get_label(X64 *x64) {
        return x64->once->import_got(import_name);
    }
};

