#include "../plum.h"


FloatOperationValue::FloatOperationValue(OperationType o, Value *p, TypeMatch &match)
    :OptimizedOperationValue(o, op_arg_ts(o, match), op_ret_ts(o, match), p,
    is_assignment(o) ? PTR_SUBSET : SSE_SUBSET, SSE_SUBSET
    ) {
}

Storage FloatOperationValue::negate(X64 *x64) {
    ls = left->compile(x64);

    switch (ls.where) {
    case SSEREGISTER:
        x64->op(PXOR, ls.sse, Address(x64->runtime->float_minus_zero_label, 0));
        return ls;
    case MEMORY:
        x64->op(MOVSD, auxls.sse, ls.address);
        x64->op(PXOR, auxls.sse, Address(x64->runtime->float_minus_zero_label, 0));
        return auxls;
    default:
        throw INTERNAL_ERROR;
    }
}

Storage FloatOperationValue::binary(X64 *x64, SseSsememOp opcode) {
    bool commutative = (opcode == ADDSD || opcode == MULSD);

    subcompile(x64);

    switch (ls.where * rs.where) {
    case SSEREGISTER_SSEREGISTER:
        x64->op(opcode, ls.sse, rs.sse);
        return ls;
    case SSEREGISTER_MEMORY:
        x64->op(opcode, ls.sse, rs.address);
        return ls;
    case MEMORY_SSEREGISTER:
        if (commutative) {
            x64->op(opcode, rs.sse, ls.address);
            return rs;
        }
        else {
            x64->op(MOVSD, auxls.sse, ls.address);
            x64->op(opcode, auxls.sse, rs.sse);
            return auxls;
        }
    case MEMORY_MEMORY:
        x64->op(MOVSD, auxls.sse, ls.address);
        x64->op(opcode, auxls.sse, rs.address);
        return auxls;
    default:
        throw INTERNAL_ERROR;
    }
}

Storage FloatOperationValue::compare(X64 *x64, ConditionCode cc) {
    subcompile(x64);

    // NOTE: COMISD is like an unsigned comparison with a twist
    // unordered => ZF+CF+PF
    // less => CF
    // equal => ZF
    // greater => -
    // Since we need to avoid false positives for unordered results, we must
    // check our conditions together with parity before coming to any conclusion.
    // A NaN makes most comparisons false, except inequality, which becomes true.
    // The parity flag is set on unordered comparison. So normally it is required
    // to be cleared, but for inequality it is sufficient to be set.

    BitSetOp setnp = (cc != CC_NOT_EQUAL ? SETNP : SETP);
    BinaryOp andb = (cc != CC_NOT_EQUAL ? ANDB : ORB);
    
    switch (ls.where * rs.where) {
    case SSEREGISTER_SSEREGISTER:
        x64->op(COMISD, ls.sse, rs.sse);
        x64->op(setnp, R11B);
        x64->op(bitset(cc), R10B);
        x64->op(andb, R10B, R11B);
        return Storage(FLAGS, CC_NOT_EQUAL);
    case SSEREGISTER_MEMORY:
        x64->op(COMISD, ls.sse, rs.address);
        x64->op(setnp, R11B);
        x64->op(bitset(cc), R10B);
        x64->op(andb, R10B, R11B);
        return Storage(FLAGS, CC_NOT_EQUAL);
    case MEMORY_SSEREGISTER:
        x64->op(COMISD, rs.sse, ls.address);  // swapped arguments
        x64->op(setnp, R11B);
        x64->op(bitset(swapped(cc)), R10B);
        x64->op(andb, R10B, R11B);
        return Storage(FLAGS, CC_NOT_EQUAL);
    case MEMORY_MEMORY:
        x64->op(MOVSD, auxls.sse, ls.address);
        x64->op(COMISD, auxls.sse, rs.address);
        x64->op(setnp, R11B);
        x64->op(bitset(cc), R10B);
        x64->op(andb, R10B, R11B);
        return Storage(FLAGS, CC_NOT_EQUAL);
    default:
        throw INTERNAL_ERROR;
    }
}

Storage FloatOperationValue::assign_binary(X64 *x64, SseSsememOp opcode) {
    subcompile(x64);

    switch (ls.where * rs.where) {
    case MEMORY_SSEREGISTER:
        x64->op(MOVSD, XMM15, ls.address);
        x64->op(opcode, XMM15, rs.sse);
        x64->op(MOVSD, ls.address, XMM15);
        return ls;
    case MEMORY_MEMORY:
        x64->op(MOVSD, XMM15, ls.address);
        x64->op(opcode, XMM15, rs.address);
        x64->op(MOVSD, ls.address, XMM15);
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
    auto arg_sses = x64->abi_arg_sses();
    auto res_sses = x64->abi_res_sses();
    Storage ls = left->compile(x64);
    
    if (ls.regs() & rclob) {
        ls = left->ts.store(ls, Storage(STACK), x64);
    }
    
    if (right)
        right->compile_and_store(x64, Storage(SSEREGISTER, arg_sses[1]));
    
    switch (ls.where) {
    case SSEREGISTER:
        x64->op(MOVSD, arg_sses[0], ls.sse);
        break;
    case STACK:
        x64->op(MOVSD, arg_sses[0], Address(RSP, 0));
        x64->op(ADDQ, RSP, FLOAT_SIZE);
        break;
    case MEMORY:
        x64->op(MOVSD, arg_sses[0], ls.address);
        break;
    default:
        throw INTERNAL_ERROR;
    }
    
    x64->runtime->call_sysv_got(function->get_label(x64));
    
    return Storage(SSEREGISTER, res_sses[0]);
}



FloatIsnanValue::FloatIsnanValue(Value *p, TypeMatch &tm)
    :GenericValue(NO_TS, BOOLEAN_TS, p) {
}

bool FloatIsnanValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    return GenericValue::check(args, kwargs, scope);
}

Regs FloatIsnanValue::precompile(Regs preferred) {
    return left->precompile_tail() | Regs(XMM0);
}

Storage FloatIsnanValue::compile(X64 *x64) {
    ls = left->compile(x64);

    switch (ls.where) {
    case SSEREGISTER:
        x64->op(COMISD, ls.sse, ls.sse);
        return Storage(FLAGS, CC_PARITY);
    case MEMORY:
        x64->op(MOVSD, XMM0, ls.address);
        x64->op(COMISD, XMM0, XMM0);
        return Storage(FLAGS, CC_PARITY);
    default:
        throw INTERNAL_ERROR;
    }
}
