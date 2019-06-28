#include "../plum.h"


BooleanOperationValue::BooleanOperationValue(OperationType o, Value *p, TypeMatch &match)
    :OptimizedOperationValue(o, op_arg_ts(o, match), op_ret_ts(o, match), p,
    is_assignment(o) ? PTR_SUBSET : GPR_SUBSET, GPR_SUBSET
    ) {
}




BooleanNotValue::BooleanNotValue(Value *p, TypeMatch &match)
    :Value(BOOLEAN_TS) {
    value.reset(p);
}

bool BooleanNotValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    if (args.size() != 0 || kwargs.size() != 0) {
        std::cerr << "Whacky boolean not operation!\n";
        return false;
    }

    return true;
}

Regs BooleanNotValue::precompile(Regs preferred) {
    return value->precompile_tail();
}

Storage BooleanNotValue::compile(Cx *cx) {
    Storage s = value->compile(cx);
    
    switch (s.where) {
    case CONSTANT:
        return Storage(CONSTANT, !s.value);
    case FLAGS:
        return Storage(FLAGS, negated(s.cc));
    case REGISTER:
        cx->op(CMPB, s.reg, 0);
        return Storage(FLAGS, CC_EQUAL);
    case MEMORY:
        cx->op(CMPB, s.address, 0);
        return Storage(FLAGS, CC_EQUAL);
    default:
        throw INTERNAL_ERROR;
    }
}




BooleanBinaryValue::BooleanBinaryValue(Value *p, TypeMatch &match, bool nt)
    :Value(BOOLEAN_TS) {
    left.reset(p);

    // Logical or => need true
    // Logical and => don't need
    need_true = nt;
}

bool BooleanBinaryValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    return check_arguments(args, kwargs, { { "arg", &BOOLEAN_TS, scope, &right } });
}

Regs BooleanBinaryValue::precompile(Regs preferred) {
    Regs clobbered = right->precompile(preferred);
    clobbered = clobbered | left->precompile(preferred & ~clobbered);
    
    // This won't be clobbered
    reg = preferred.has_gpr() ? preferred.get_gpr() : RAX;
    
    return clobbered | reg;
}

Storage BooleanBinaryValue::compile(Cx *cx) {
    Storage ls = left->compile(cx);
    Storage rs;
    Label right_end, end;
    
    switch (ls.where) {
    case CONSTANT:
        if (need_true ? ls.value : !ls.value)
            return ls;
        else
            return right->compile(cx);
    case FLAGS:
        cx->op(branch(need_true ? ls.cc : negated(ls.cc)), right_end);
        break;
    case REGISTER:
        cx->op(CMPB, ls.reg, need_true ? 1 : 0);
        cx->op(JE, right_end);
        break;
    case MEMORY:
        cx->op(CMPB, ls.address, need_true ? 1 : 0);
        cx->op(JE, right_end);
        break;
    case STACK:
        cx->op(POPQ, reg);
        cx->op(CMPB, reg, need_true ? 1 : 0);
        cx->op(JE, right_end);
        ls = Storage(REGISTER, reg);
        break;
    default:
        throw INTERNAL_ERROR;
    }

    // Need to evaluate the right hand side
    rs = right->compile(cx);
    
    if (
        (ls.where == FLAGS && rs.where == FLAGS && ls.cc == rs.cc) ||
        (ls.where == REGISTER && rs.where == REGISTER && ls.reg == rs.reg) ||
        (ls.where == STACK && rs.where == STACK)
    ) {
        // Uses the same storage, no need to move data
        cx->code_label(right_end);
        return ls;
    }
    else if (ls.where == REGISTER) {
        // Use the left storage, move only the right side result
        switch (rs.where) {
        case FLAGS:
            cx->op(bitset(rs.cc), ls.reg);
            break;
        case REGISTER:
            cx->op(MOVB, ls.reg, rs.reg);
            break;
        case MEMORY:
            cx->op(MOVB, ls.reg, rs.address);
            break;
        case STACK:
            cx->op(POPQ, ls.reg);
            break;
        default:
            throw INTERNAL_ERROR;
        }

        cx->code_label(right_end);
        return ls;
    }
    else {
        // Find a register, and use it to store the result from both sides
        switch (rs.where) {
        case FLAGS:
            cx->op(bitset(rs.cc), reg);
            break;
        case REGISTER:
            reg = rs.reg;
            break;
        case MEMORY:
            cx->op(MOVB, reg, rs.address);
            break;
        case STACK:
            cx->op(POPQ, reg);
            break;
        default:
            throw INTERNAL_ERROR;
        }

        cx->op(JMP, end);
    
        cx->code_label(right_end);
        cx->op(MOVB, reg, need_true ? 1 : 0);
    
        cx->code_label(end);
        return Storage(REGISTER, reg);
    }
}



BooleanOrValue::BooleanOrValue(Value *p, TypeMatch &match)
    :BooleanBinaryValue(p, match, true) {
}



BooleanAndValue::BooleanAndValue(Value *p, TypeMatch &match)
    :BooleanBinaryValue(p, match, false) {
}
