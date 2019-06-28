#include "../plum.h"


FloatOperationValue::FloatOperationValue(OperationType o, Value *p, TypeMatch &match)
    :OptimizedOperationValue(o, op_arg_ts(o, match), op_ret_ts(o, match), p,
    is_assignment(o) ? PTR_SUBSET : FPR_SUBSET, FPR_SUBSET
    ) {
}

Storage FloatOperationValue::negate(Cx *cx) {
    ls = left->compile(cx);

    switch (ls.where) {
    case FPREGISTER:
        cx->op(XORF, ls.fpr, Address(cx->runtime->float_minus_zero_label, 0));
        return ls;
    case MEMORY:
        cx->op(MOVF, auxls.fpr, ls.address);
        cx->op(XORF, auxls.fpr, Address(cx->runtime->float_minus_zero_label, 0));
        return auxls;
    default:
        throw INTERNAL_ERROR;
    }
}

Storage FloatOperationValue::binary(Cx *cx, FprFprmemOp opcode) {
    bool commutative = (opcode == ADDF || opcode == MULF);

    subcompile(cx);

    switch (ls.where * rs.where) {
    case FPREGISTER_FPREGISTER:
        cx->op(opcode, ls.fpr, rs.fpr);
        return ls;
    case FPREGISTER_MEMORY:
        cx->op(opcode, ls.fpr, rs.address);
        return ls;
    case MEMORY_FPREGISTER:
        if (commutative) {
            cx->op(opcode, rs.fpr, ls.address);
            return rs;
        }
        else {
            cx->op(MOVF, auxls.fpr, ls.address);
            cx->op(opcode, auxls.fpr, rs.fpr);
            return auxls;
        }
    case MEMORY_MEMORY:
        cx->op(MOVF, auxls.fpr, ls.address);
        cx->op(opcode, auxls.fpr, rs.address);
        return auxls;
    default:
        throw INTERNAL_ERROR;
    }
}

Storage FloatOperationValue::compare(Cx *cx, ConditionCode cc) {
    subcompile(cx);

    // NOT_EQUAL is not an ordering status, but the negation of one
    ConditionCode test_cc = (cc == CC_NOT_EQUAL ? CC_EQUAL : cc);
    ConditionCode res_cc = (cc == CC_NOT_EQUAL ? CC_NOT_EQUAL : CC_EQUAL);

    switch (ls.where * rs.where) {
    case FPREGISTER_FPREGISTER:
        cx->floatcmp(test_cc, ls.fpr, rs.fpr);
        return Storage(FLAGS, res_cc);
    case FPREGISTER_MEMORY:
        cx->op(MOVF, FPR15, rs.address);
        cx->floatcmp(test_cc, ls.fpr, FPR15);
        return Storage(FLAGS, res_cc);
    case MEMORY_FPREGISTER:
        cx->op(MOVF, FPR15, ls.address);
        cx->floatcmp(test_cc, FPR15, rs.fpr);
        return Storage(FLAGS, res_cc);
    case MEMORY_MEMORY:
        cx->op(MOVF, FPR14, ls.address);
        cx->op(MOVF, FPR15, rs.address);
        cx->floatcmp(test_cc, FPR14, FPR15);
        return Storage(FLAGS, res_cc);
    default:
        throw INTERNAL_ERROR;
    }
}

Storage FloatOperationValue::assign_binary(Cx *cx, FprFprmemOp opcode) {
    subcompile(cx);

    switch (ls.where * rs.where) {
    case MEMORY_FPREGISTER:
        cx->op(MOVF, FPR15, ls.address);
        cx->op(opcode, FPR15, rs.fpr);
        cx->op(MOVF, ls.address, FPR15);
        return ls;
    case MEMORY_MEMORY:
        cx->op(MOVF, FPR15, ls.address);
        cx->op(opcode, FPR15, rs.address);
        cx->op(MOVF, ls.address, FPR15);
        return ls;
    default:
        throw INTERNAL_ERROR;
    }
}

Storage FloatOperationValue::compile(Cx *cx) {
    switch (operation) {
    case NEGATE:
        return negate(cx);
    case ADD:
        return binary(cx, ADDF);
    case SUBTRACT:
        return binary(cx, SUBF);
    case MULTIPLY:
        return binary(cx, MULF);
    case DIVIDE:
        return binary(cx, DIVF);
    case EQUAL:
        return compare(cx, CC_EQUAL);
    case NOT_EQUAL:
        return compare(cx, CC_NOT_EQUAL);
    case LESS:
        return compare(cx, CC_BELOW);
    case GREATER:
        return compare(cx, CC_ABOVE);
    case LESS_EQUAL:
        return compare(cx, CC_BELOW_EQUAL);
    case GREATER_EQUAL:
        return compare(cx, CC_ABOVE_EQUAL);
    case ASSIGN_ADD:
        return assign_binary(cx, ADDF);
    case ASSIGN_SUBTRACT:
        return assign_binary(cx, SUBF);
    case ASSIGN_MULTIPLY:
        return assign_binary(cx, MULF);
    case ASSIGN_DIVIDE:
        return assign_binary(cx, DIVF);
    default:
        return OptimizedOperationValue::compile(cx);
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

Storage FloatFunctionValue::compile(Cx *cx) {
    auto arg_fprs = cx->abi_arg_fprs();
    auto res_fprs = cx->abi_res_fprs();
    Storage ls = left->compile(cx);
    
    if (ls.regs() & rclob) {
        ls = left->ts.store(ls, Storage(STACK), cx);
    }
    
    if (right)
        right->compile_and_store(cx, Storage(FPREGISTER, arg_fprs[1]));
    
    switch (ls.where) {
    case FPREGISTER:
        cx->op(MOVF, arg_fprs[0], ls.fpr);
        break;
    case STACK:
        cx->op(MOVF, arg_fprs[0], Address(RSP, 0));
        cx->op(ADDQ, RSP, FLOAT_SIZE);
        break;
    case MEMORY:
        cx->op(MOVF, arg_fprs[0], ls.address);
        break;
    default:
        throw INTERNAL_ERROR;
    }
    
    cx->runtime->call_sysv_got(function->get_label(cx));
    
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

Storage FloatIsnanValue::compile(Cx *cx) {
    ls = left->compile(cx);

    switch (ls.where) {
    case FPREGISTER:
        cx->floatcmp(CC_EQUAL, ls.fpr, ls.fpr);
        return Storage(FLAGS, CC_NOT_EQUAL);
    case MEMORY:
        cx->op(MOVF, FPR15, ls.address);
        cx->floatcmp(CC_EQUAL, FPR15, FPR15);
        return Storage(FLAGS, CC_NOT_EQUAL);
    default:
        throw INTERNAL_ERROR;
    }
}
