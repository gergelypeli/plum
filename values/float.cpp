#include "../plum.h"


FloatOperationValue::FloatOperationValue(OperationType o, Value *p, TypeMatch &match)
    :OptimizedOperationValue(o, op_arg_ts(o, match), op_ret_ts(o, match), p,
    is_assignment(o) ? PTR_SUBSET : FPR_SUBSET, FPR_SUBSET
    ) {
}

Storage FloatOperationValue::negate(X64 *x64) {
    ls = left->compile(x64);

    switch (ls.where) {
    case FPREGISTER:
        x64->op(PXOR, ls.fpr, Address(x64->runtime->float_minus_zero_label, 0));
        return ls;
    case MEMORY:
        x64->op(MOVSD, auxls.fpr, ls.address);
        x64->op(PXOR, auxls.fpr, Address(x64->runtime->float_minus_zero_label, 0));
        return auxls;
    default:
        throw INTERNAL_ERROR;
    }
}

Storage FloatOperationValue::binary(X64 *x64, FprFprmemOp opcode) {
    bool commutative = (opcode == ADDSD || opcode == MULSD);

    subcompile(x64);

    switch (ls.where * rs.where) {
    case FPREGISTER_FPREGISTER:
        x64->op(opcode, ls.fpr, rs.fpr);
        return ls;
    case FPREGISTER_MEMORY:
        x64->op(opcode, ls.fpr, rs.address);
        return ls;
    case MEMORY_FPREGISTER:
        if (commutative) {
            x64->op(opcode, rs.fpr, ls.address);
            return rs;
        }
        else {
            x64->op(MOVSD, auxls.fpr, ls.address);
            x64->op(opcode, auxls.fpr, rs.fpr);
            return auxls;
        }
    case MEMORY_MEMORY:
        x64->op(MOVSD, auxls.fpr, ls.address);
        x64->op(opcode, auxls.fpr, rs.address);
        return auxls;
    default:
        throw INTERNAL_ERROR;
    }
}

Storage FloatOperationValue::compare(X64 *x64, ConditionCode cc) {
    subcompile(x64);

    // NOT_EQUAL is not an ordering status, but the negation of one
    ConditionCode test_cc = (cc == CC_NOT_EQUAL ? CC_EQUAL : cc);
    ConditionCode res_cc = (cc == CC_NOT_EQUAL ? CC_NOT_EQUAL : CC_EQUAL);

    switch (ls.where * rs.where) {
    case FPREGISTER_FPREGISTER:
        x64->floatcmp(test_cc, ls.fpr, rs.fpr);
        return Storage(FLAGS, res_cc);
    case FPREGISTER_MEMORY:
        x64->op(MOVSD, FPR15, rs.address);
        x64->floatcmp(test_cc, ls.fpr, FPR15);
        return Storage(FLAGS, res_cc);
    case MEMORY_FPREGISTER:
        x64->op(MOVSD, FPR15, ls.address);
        x64->floatcmp(test_cc, FPR15, rs.fpr);
        return Storage(FLAGS, res_cc);
    case MEMORY_MEMORY:
        x64->op(MOVSD, FPR14, ls.address);
        x64->op(MOVSD, FPR15, rs.address);
        x64->floatcmp(test_cc, FPR14, FPR15);
        return Storage(FLAGS, res_cc);
    default:
        throw INTERNAL_ERROR;
    }
}

Storage FloatOperationValue::assign_binary(X64 *x64, FprFprmemOp opcode) {
    subcompile(x64);

    switch (ls.where * rs.where) {
    case MEMORY_FPREGISTER:
        x64->op(MOVSD, FPR15, ls.address);
        x64->op(opcode, FPR15, rs.fpr);
        x64->op(MOVSD, ls.address, FPR15);
        return ls;
    case MEMORY_MEMORY:
        x64->op(MOVSD, FPR15, ls.address);
        x64->op(opcode, FPR15, rs.address);
        x64->op(MOVSD, ls.address, FPR15);
        return ls;
    default:
        throw INTERNAL_ERROR;
    }
}

Storage FloatOperationValue::compile(X64 *x64) {
    switch (operation) {
    case NEGATE:
        return negate(x64);
    case ADD:
        return binary(x64, ADDSD);
    case SUBTRACT:
        return binary(x64, SUBSD);
    case MULTIPLY:
        return binary(x64, MULSD);
    case DIVIDE:
        return binary(x64, DIVSD);
    case EQUAL:
        return compare(x64, CC_EQUAL);
    case NOT_EQUAL:
        return compare(x64, CC_NOT_EQUAL);
    case LESS:
        return compare(x64, CC_BELOW);
    case GREATER:
        return compare(x64, CC_ABOVE);
    case LESS_EQUAL:
        return compare(x64, CC_BELOW_EQUAL);
    case GREATER_EQUAL:
        return compare(x64, CC_ABOVE_EQUAL);
    case ASSIGN_ADD:
        return assign_binary(x64, ADDSD);
    case ASSIGN_SUBTRACT:
        return assign_binary(x64, SUBSD);
    case ASSIGN_MULTIPLY:
        return assign_binary(x64, MULSD);
    case ASSIGN_DIVIDE:
        return assign_binary(x64, DIVSD);
    default:
        return OptimizedOperationValue::compile(x64);
    }
}




FloatFunctionValue::FloatFunctionValue(ImportedFloatFunction *f, Value *l, TypeMatch &match)
    :GenericValue(f->arg_ts, f->res_ts, l) {
    function = f;
}

Regs FloatFunctionValue::precompile(Regs preferred) {
    if (right)
        rclob = right->precompile_tail();

    left->precompile(~rclob);
        
    return Regs::all();
}

Storage FloatFunctionValue::compile(X64 *x64) {
    auto arg_fprs = x64->abi_arg_fprs();
    auto res_fprs = x64->abi_res_fprs();
    Storage ls = left->compile(x64);
    
    if (ls.regs() & rclob) {
        ls = left->ts.store(ls, Storage(STACK), x64);
    }
    
    if (right)
        right->compile_and_store(x64, Storage(FPREGISTER, arg_fprs[1]));
    
    switch (ls.where) {
    case FPREGISTER:
        x64->op(MOVSD, arg_fprs[0], ls.fpr);
        break;
    case STACK:
        x64->op(MOVSD, arg_fprs[0], Address(RSP, 0));
        x64->op(ADDQ, RSP, FLOAT_SIZE);
        break;
    case MEMORY:
        x64->op(MOVSD, arg_fprs[0], ls.address);
        break;
    default:
        throw INTERNAL_ERROR;
    }
    
    x64->runtime->call_sysv_got(function->get_label(x64));
    
    return Storage(FPREGISTER, res_fprs[0]);
}



FloatIsnanValue::FloatIsnanValue(Value *p, TypeMatch &tm)
    :GenericValue(NO_TS, BOOLEAN_TS, p) {
}

bool FloatIsnanValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    return GenericValue::check(args, kwargs, scope);
}

Regs FloatIsnanValue::precompile(Regs preferred) {
    return left->precompile_tail();
}

Storage FloatIsnanValue::compile(X64 *x64) {
    ls = left->compile(x64);

    switch (ls.where) {
    case FPREGISTER:
        x64->floatcmp(CC_EQUAL, ls.fpr, ls.fpr);
        return Storage(FLAGS, CC_NOT_EQUAL);
    case MEMORY:
        x64->op(MOVSD, FPR15, ls.address);
        x64->floatcmp(CC_EQUAL, FPR15, FPR15);
        return Storage(FLAGS, CC_NOT_EQUAL);
    default:
        throw INTERNAL_ERROR;
    }
}
