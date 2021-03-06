#include "../plum.h"


Function::Function(std::string n, PivotRequirement pr, FunctionType ft, std::vector<TypeSpec> ats, std::vector<std::string> ans, std::vector<TypeSpec> rts, TreenumerationType *et, FunctionScope *fs)
    :Identifier(n, pr) {
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

Function *Function::clone_abstract(std::string prefix) {
    // Used in Implementation inside an Abstract only, to get a virtual index
    // while staying abstract
    if (!is_abstract())
        throw INTERNAL_ERROR;
        
    return new Function(prefix + name, pivot_requirement, type, arg_tss, arg_names, res_tss, exception_type, NULL);
}

bool Function::is_abstract() {
    DataScope *ds = ptr_cast<DataScope>(outer_scope);
    
    return ds && ds->is_abstract_scope();
}

void Function::set_outer_scope(Scope *os) {
    // Abuse here, too
    Identifier::set_outer_scope(os);
    
    // Some built-in interface functions may have no fn scope now
    if (fn_scope)
        fn_scope->set_outer_scope(os);
}

Value *Function::matched(Value *cpivot, Scope *scope, TypeMatch &match) {
    return make<FunctionCallValue>(this, cpivot, match);
}

void Function::get_parameters(TypeSpec &pts, TSs &rtss, TSs &atss, Ss &anames, TypeSpec ts, TypeMatch tm) {
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

void Function::allocate() {
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
            associated->override_virtual_entry(virtual_index, mve);
            std::cerr << "Set virtual index " << virtual_index << " for function " << name << ".\n";
        }

        if (associated && (associated->is_requiring() || associated->is_in_requiring()))
            fn_scope->make_associated_offset_storage();
    }
    
    if (fn_scope)
        fn_scope->allocate();
}

Label Function::get_label(Cx *cx) {
    if (is_abstract())
        throw INTERNAL_ERROR;
        
    return label;
}

bool Function::does_implement(TypeMatch tm, Function *iff, TypeMatch iftm) {
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
            (exception_type ? exception_type->name : "-") << " is not " <<
            (iff->exception_type ? iff->exception_type->name : "-") << "!\n";
        return false;
    }
    
    implemented_function = iff;
    return true;
}

void Function::set_associated(Associable *a) {
    associated = a;
}

Label Function::get_method_label(Cx *cx) {
    return get_label(cx);
}

std::string Function::get_method_name() {
    return get_fully_qualified_name();
}

void Function::set_pc_range(int lo, int hi) {
    low_pc = lo;
    high_pc = hi;
}

void Function::debug(TypeMatch tm, Cx *cx) {
    bool virtuality = (virtual_index != 0);

    if (is_abstract()) {
        cx->dwarf->begin_abstract_subprogram_info(get_fully_qualified_name(), virtuality);
        cx->dwarf->end_info();
        return;
    }
    
    if (fn_scope->self_scope->contents.empty())
        throw INTERNAL_ERROR;

    if (low_pc < 0 || high_pc < 0)
        throw INTERNAL_ERROR;

    Label self_label;
    unsigned self_index = self_label.def_index;
    
    cx->dwarf->begin_subprogram_info(get_fully_qualified_name(), low_pc, high_pc, virtuality, self_index);
    
    fn_scope->result_scope->debug(tm, cx);
    
    cx->dwarf->info_def(self_index);
    fn_scope->self_scope->debug(tm, cx);
    
    fn_scope->head_scope->debug(tm, cx);
    
    fn_scope->body_scope->debug(tm, cx);
    
    cx->dwarf->end_info();
}



SysvFunction::SysvFunction(std::string in, std::string n, PivotRequirement pr, FunctionType ft, std::vector<TypeSpec> ats, std::vector<std::string> ans, std::vector<TypeSpec> rts, TreenumerationType *et, FunctionScope *fs)
    :Function(n, pr, ft, ats, ans, rts, et, fs) {
    import_name = in;
    prot = SYSV_FUNCTION;
}

Label SysvFunction::get_label(Cx *cx) {
    return cx->once->compile(this);
}

void SysvFunction::deferred_compile(Label label, Cx *cx) {
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
        
        if (simple_where != REGISTER && simple_where != FPREGISTER) {
            std::cerr << "Oops, not a simple result from a SysV function!\n";
            throw INTERNAL_ERROR;
        }
    }

    cx->code_label_local(label, get_fully_qualified_name() + "__sysv_wrapper");

    // Create a proper stack frame for debugging
    cx->prologue();

    std::array<Register, 4> arg_regs = cx->abi_arg_regs();
    std::array<FpRegister, 4> arg_fprs = cx->abi_arg_fprs();
    unsigned reg_index = 0;
    unsigned fpr_index = 0;
    
    // Account for the return address and the saved RBP
    unsigned stack_offset = passed_size + RIP_SIZE + ADDRESS_SIZE;

    for (unsigned i = 0; i < pushed_tss.size(); i++) {
        // Must move raw values so it doesn't count as a copy
        stack_offset -= pushed_sizes[i];
        
        StorageWhere optimal_where = pushed_tss[i].where(AS_VALUE);
        StorageWhere pushed_where = stacked(pushed_tss[i].where(AS_ARGUMENT));
        
        if (optimal_where == NOWHERE)
            ;  // happens for singleton pivots
        else if (optimal_where == FPREGISTER)
            cx->op(MOVF, arg_fprs[fpr_index++], Address(RBP, stack_offset));
        else if (pushed_where == ALISTACK)
            cx->op(MOVQ, arg_regs[reg_index++], Address(RBP, stack_offset));
        else if (pushed_sizes[i] == ADDRESS_SIZE)
            cx->op(MOVQ, arg_regs[reg_index++], Address(RBP, stack_offset));
        else
            cx->op(LEA, arg_regs[reg_index++], Address(RBP, stack_offset));
    }
    
    Label got_label = cx->once->import_got(import_name);
    cx->runtime->call_sysv_got(got_label);

    //cx->runtime->dump("Returned from SysV.");

    // We return simple values in RAX and XMM0 like SysV.
    // But simulated exceptions are always received in res_reg[0], and must be
    // put in RDX, so it may need a fix.
    StorageWhere simple_where = (res_tss.size() ? res_tss[0].where(AS_VALUE) : NOWHERE);
    std::array<Register, 2> res_regs = cx->abi_res_regs();
    std::array<FpRegister, 2> res_fprs = cx->abi_res_fprs();

    switch (simple_where) {
    case NOWHERE:
        if (exception_type) {
            cx->op(MOVQ, RDX, res_regs[0]);
        }
        break;
    case REGISTER:
        if (exception_type) {
            cx->op(MOVQ, R10, res_regs[0]);  // exc
            cx->op(MOVQ, R11, res_regs[1]);  // res
            cx->op(MOVQ, RDX, R10);
            cx->op(MOVQ, RAX, R11);
        }
        else {
            cx->op(MOVQ, RAX, res_regs[0]);
        }
        break;
    case FPREGISTER:
        if (exception_type) {
            cx->op(MOVQ, RDX, res_regs[0]);
            cx->op(MOVF, FPR0, res_fprs[0]);
        }
        else {
            cx->op(MOVF, FPR0, res_fprs[0]);
        }
        break;
    default:
        throw INTERNAL_ERROR;
    }

    // Raised exception in RDX.
    if (exception_type)
        cx->op(CMPQ, RDX, NO_EXCEPTION);

    cx->epilogue();
}

void SysvFunction::debug(TypeMatch tm, Cx *cx) {
    // Empty
}



ImportedFloatFunction::ImportedFloatFunction(std::string in, std::string n, PivotRequirement pr, TypeSpec ats, TypeSpec rts)
    :Identifier(n, pr) {
    import_name = in;
    arg_ts = ats;
    res_ts = rts;
}

Value *ImportedFloatFunction::matched(Value *cpivot, Scope *scope, TypeMatch &match) {
    return make<FloatFunctionValue>(this, cpivot, match);
}

Label ImportedFloatFunction::get_label(Cx *cx) {
    return cx->once->import_got(import_name);
}

