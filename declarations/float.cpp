#include "../plum.h"


FloatType::FloatType(std::string n, MetaType *mt)
    :Type(n, {}, mt ? mt : value_metatype) {
}

Allocation FloatType::measure(TypeMatch tm) {
    return Allocation(FLOAT_SIZE);
}

void FloatType::store(TypeMatch tm, Storage s, Storage t, X64 *x64) {
    switch (s.where * t.where) {
    case NOWHERE_FPREGISTER:
        x64->op(MOVF, t.fpr, Address(x64->runtime->float_zero_label, 0));
        break;
    case NOWHERE_STACK:
        x64->op(PUSHQ, 0);  // using that 0.0 is represented as all zeroes
        break;

    case FPREGISTER_NOWHERE:
        break;
    case FPREGISTER_FPREGISTER:
        if (s.fpr != t.fpr)
            x64->op(MOVF, t.fpr, s.fpr);
        break;
    case FPREGISTER_STACK:
        x64->op(SUBQ, RSP, FLOAT_SIZE);
        x64->op(MOVF, Address(RSP, 0), s.fpr);
        break;
    case FPREGISTER_MEMORY:
        x64->op(MOVF, t.address, s.fpr);
        break;
        
    case STACK_NOWHERE:
        x64->op(ADDQ, RSP, FLOAT_SIZE);
        break;
    case STACK_FPREGISTER:
        x64->op(MOVF, s.fpr, Address(RSP, 0));
        x64->op(ADDQ, RSP, FLOAT_SIZE);
        break;
        
    case STACK_STACK:
        break;
        
    case MEMORY_NOWHERE:
        break;
    case MEMORY_FPREGISTER:
        x64->op(MOVF, t.fpr, s.address);
        break;
    case MEMORY_STACK:
        x64->op(PUSHQ, s.address);
        break;
    case MEMORY_MEMORY:
        x64->op(MOVQ, R10, s.address);
        x64->op(MOVQ, t.address, R10);
        break;
    default:
        Type::store(tm, s, t, x64);
    }
}

void FloatType::create(TypeMatch tm, Storage s, Storage t, X64 *x64) {
    switch (s.where * t.where) {
    case NOWHERE_MEMORY:
        x64->op(MOVQ, t.address, 0);  // using that 0.0 is represented as all zeroes
        break;
    case FPREGISTER_MEMORY:
        x64->op(MOVF, t.address, s.fpr);
        break;
    case STACK_MEMORY:
        x64->op(POPQ, t.address);
        break;
    case MEMORY_MEMORY:
        x64->op(MOVQ, R10, s.address);
        x64->op(MOVQ, t.address, R10);
        break;
    default:
        throw INTERNAL_ERROR;
    }
}

void FloatType::destroy(TypeMatch tm, Storage s, X64 *x64) {
    if (s.where == MEMORY)
        ;
    else
        throw INTERNAL_ERROR;
}

void FloatType::equal(TypeMatch tm, Storage s, Storage t, X64 *x64) {
    // No need to take care of STACK here, GenericOperationValue takes care of it.
    // Values are equal iff ZF && !PF. Thanks, NaN!
    
    switch (s.where * t.where) {
    case FPREGISTER_FPREGISTER:
        x64->floatcmp(CC_EQUAL, s.fpr, t.fpr);
        return;
    case FPREGISTER_MEMORY:
        x64->op(MOVF, FPR15, t.address);
        x64->floatcmp(CC_EQUAL, s.fpr, FPR15);
        return;
    case MEMORY_FPREGISTER:
        x64->op(MOVF, FPR15, s.address);
        x64->floatcmp(CC_EQUAL, FPR15, t.fpr);
        return;
    case MEMORY_MEMORY:
        x64->op(MOVF, FPR14, s.address);
        x64->op(MOVF, FPR15, t.address);
        x64->floatcmp(CC_EQUAL, FPR14, FPR15);
        return;
    default:
        throw INTERNAL_ERROR;
    }
}

void FloatType::compare(TypeMatch tm, Storage s, Storage t, X64 *x64) {
    // We need to do something with NaN-s, so do what Java does, and treat them
    // as greater than everything, including positive infinity. Chuck Norris likes this.
    Label finite, end;
    
    switch (s.where * t.where) {
    case FPREGISTER_FPREGISTER:
        x64->floatorder(s.fpr, t.fpr);
        break;
    case FPREGISTER_MEMORY:
        x64->op(MOVF, FPR15, t.address);
        x64->floatorder(s.fpr, FPR15);
        break;
    case MEMORY_FPREGISTER:
        x64->op(MOVF, FPR15, s.address);
        x64->floatorder(FPR15, t.fpr);
        break;
    case MEMORY_MEMORY:
        x64->op(MOVF, FPR14, s.address);
        x64->op(MOVF, FPR15, t.address);
        x64->floatorder(FPR14, FPR15);
        break;
    default:
        throw INTERNAL_ERROR;
    }
}

StorageWhere FloatType::where(TypeMatch tm, AsWhat as_what) {
    return (
        as_what == AS_VALUE ? FPREGISTER :
        as_what == AS_VARIABLE ? MEMORY :
        as_what == AS_ARGUMENT ? MEMORY :
        as_what == AS_LVALUE_ARGUMENT ? ALIAS :
        throw INTERNAL_ERROR
    );
}

Storage FloatType::optimal_value_storage(TypeMatch tm, Regs preferred) {
    if (preferred.has_fpr())
        return Storage(FPREGISTER, preferred.get_fpr());
    else
        return Storage(STACK);
}

void FloatType::streamify(TypeMatch tm, X64 *x64) {
    auto arg_regs = x64->abi_arg_regs();
    auto arg_fprs = x64->abi_arg_fprs();
    Address value_addr(RSP, ALIAS_SIZE);
    Address alias_addr(RSP, 0);

    // SysV
    x64->op(MOVF, arg_fprs[0], value_addr);
    x64->op(MOVQ, arg_regs[0], alias_addr);
    
    x64->runtime->call_sysv(x64->runtime->sysv_streamify_float_label);
}

Value *FloatType::lookup_initializer(TypeMatch tm, std::string name, Scope *scope) {
    if (name == "nan")
        return make<FloatValue>(FLOAT_TS, NAN);
    else if (name == "pinf")
        return make<FloatValue>(FLOAT_TS, INFINITY);
    else if (name == "ninf")
        return make<FloatValue>(FLOAT_TS, -INFINITY);
    else {
        std::cerr << "No Float initializer called " << name << "!\n";
        return NULL;
    }
}

void FloatType::type_info(TypeMatch tm, X64 *x64) {
    x64->dwarf->base_type_info(name, FLOAT_SIZE, DW_ATE_float);
}
