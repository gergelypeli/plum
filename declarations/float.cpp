#include "../plum.h"


FloatType::FloatType(std::string n, MetaType *mt)
    :Type(n, {}, mt ? mt : value_metatype) {
}

Allocation FloatType::measure(TypeMatch tm) {
    return Allocation(FLOAT_SIZE);
}

void FloatType::store(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    switch (s.where * t.where) {
    case NOWHERE_FPREGISTER:
        cx->op(MOVF, t.fpr, Address(cx->runtime->float_zero_label, 0));
        break;
    case NOWHERE_STACK:
        cx->op(PUSHQ, 0);  // using that 0.0 is represented as all zeroes
        break;

    case FPREGISTER_NOWHERE:
        break;
    case FPREGISTER_FPREGISTER:
        if (s.fpr != t.fpr)
            cx->op(MOVF, t.fpr, s.fpr);
        break;
    case FPREGISTER_STACK:
        cx->op(SUBQ, RSP, FLOAT_SIZE);
        cx->op(MOVF, Address(RSP, 0), s.fpr);
        break;
    case FPREGISTER_MEMORY:
        cx->op(MOVF, t.address, s.fpr);
        break;
        
    case STACK_NOWHERE:
        cx->op(ADDQ, RSP, FLOAT_SIZE);
        break;
    case STACK_FPREGISTER:
        cx->op(MOVF, s.fpr, Address(RSP, 0));
        cx->op(ADDQ, RSP, FLOAT_SIZE);
        break;
        
    case STACK_STACK:
        break;
        
    case MEMORY_NOWHERE:
        break;
    case MEMORY_FPREGISTER:
        cx->op(MOVF, t.fpr, s.address);
        break;
    case MEMORY_STACK:
        cx->op(PUSHQ, s.address);
        break;
    case MEMORY_MEMORY:
        cx->op(MOVQ, R10, s.address);
        cx->op(MOVQ, t.address, R10);
        break;
    default:
        Type::store(tm, s, t, cx);
    }
}

void FloatType::create(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    switch (s.where * t.where) {
    case NOWHERE_MEMORY:
        cx->op(MOVQ, t.address, 0);  // using that 0.0 is represented as all zeroes
        break;
    case FPREGISTER_MEMORY:
        cx->op(MOVF, t.address, s.fpr);
        break;
    case STACK_MEMORY:
        cx->op(POPQ, t.address);
        break;
    case MEMORY_MEMORY:
        cx->op(MOVQ, R10, s.address);
        cx->op(MOVQ, t.address, R10);
        break;
    default:
        throw INTERNAL_ERROR;
    }
}

void FloatType::destroy(TypeMatch tm, Storage s, Cx *cx) {
    if (s.where == MEMORY)
        ;
    else
        throw INTERNAL_ERROR;
}

void FloatType::equal(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    // No need to take care of STACK here, GenericOperationValue takes care of it.
    // Values are equal iff ZF && !PF. Thanks, NaN!
    
    switch (s.where * t.where) {
    case FPREGISTER_FPREGISTER:
        cx->floatcmp(CC_EQUAL, s.fpr, t.fpr);
        return;
    case FPREGISTER_MEMORY:
        cx->op(MOVF, FPR15, t.address);
        cx->floatcmp(CC_EQUAL, s.fpr, FPR15);
        return;
    case MEMORY_FPREGISTER:
        cx->op(MOVF, FPR15, s.address);
        cx->floatcmp(CC_EQUAL, FPR15, t.fpr);
        return;
    case MEMORY_MEMORY:
        cx->op(MOVF, FPR14, s.address);
        cx->op(MOVF, FPR15, t.address);
        cx->floatcmp(CC_EQUAL, FPR14, FPR15);
        return;
    default:
        throw INTERNAL_ERROR;
    }
}

void FloatType::compare(TypeMatch tm, Storage s, Storage t, Cx *cx) {
    // We need to do something with NaN-s, so do what Java does, and treat them
    // as greater than everything, including positive infinity. Chuck Norris likes this.
    Label finite, end;
    
    switch (s.where * t.where) {
    case FPREGISTER_FPREGISTER:
        cx->floatorder(s.fpr, t.fpr);
        break;
    case FPREGISTER_MEMORY:
        cx->op(MOVF, FPR15, t.address);
        cx->floatorder(s.fpr, FPR15);
        break;
    case MEMORY_FPREGISTER:
        cx->op(MOVF, FPR15, s.address);
        cx->floatorder(FPR15, t.fpr);
        break;
    case MEMORY_MEMORY:
        cx->op(MOVF, FPR14, s.address);
        cx->op(MOVF, FPR15, t.address);
        cx->floatorder(FPR14, FPR15);
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

void FloatType::streamify(TypeMatch tm, Cx *cx) {
    auto arg_regs = cx->abi_arg_regs();
    auto arg_fprs = cx->abi_arg_fprs();
    Address value_addr(RSP, ALIAS_SIZE);
    Address alias_addr(RSP, 0);

    // SysV
    cx->op(MOVF, arg_fprs[0], value_addr);
    cx->op(MOVQ, arg_regs[0], alias_addr);
    
    cx->runtime->call_sysv(cx->runtime->sysv_streamify_float_label);
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

void FloatType::type_info(TypeMatch tm, Cx *cx) {
    cx->dwarf->base_type_info(name, FLOAT_SIZE, DW_ATE_float);
}
