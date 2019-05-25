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

Storage BooleanNotValue::compile(X64 *x64) {
    Storage s = value->compile(x64);
    
    switch (s.where) {
    case CONSTANT:
        return Storage(CONSTANT, !s.value);
    case FLAGS:
        return Storage(FLAGS, negated(s.cc));
    case REGISTER:
        x64->op(CMPB, s.reg, 0);
        return Storage(FLAGS, CC_EQUAL);
    case MEMORY:
        x64->op(CMPB, s.address, 0);
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

Storage BooleanBinaryValue::compile(X64 *x64) {
    Storage ls = left->compile(x64);
    Storage rs;
    Label right_end, end;
    
    switch (ls.where) {
    case CONSTANT:
        if (need_true ? ls.value : !ls.value)
            return ls;
        else
            return right->compile(x64);
    case FLAGS:
        x64->op(branch(need_true ? ls.cc : negated(ls.cc)), right_end);
        break;
    case REGISTER:
        x64->op(CMPB, ls.reg, need_true ? 1 : 0);
        x64->op(JE, right_end);
        break;
    case MEMORY:
        x64->op(CMPB, ls.address, need_true ? 1 : 0);
        x64->op(JE, right_end);
        break;
    case STACK:
        x64->op(POPQ, reg);
        x64->op(CMPB, reg, need_true ? 1 : 0);
        x64->op(JE, right_end);
        ls = Storage(REGISTER, reg);
        break;
    default:
        throw INTERNAL_ERROR;
    }

    // Need to evaluate the right hand side
    rs = right->compile(x64);
    
    if (
        (ls.where == FLAGS && rs.where == FLAGS && ls.cc == rs.cc) ||
        (ls.where == REGISTER && rs.where == REGISTER && ls.reg == rs.reg) ||
        (ls.where == STACK && rs.where == STACK)
    ) {
        // Uses the same storage, no need to move data
        x64->code_label(right_end);
        return ls;
    }
    else if (ls.where == REGISTER) {
        // Use the left storage, move only the right side result
        switch (rs.where) {
        case FLAGS:
            x64->op(bitset(rs.cc), ls.reg);
            break;
        case REGISTER:
            x64->op(MOVB, ls.reg, rs.reg);
            break;
        case MEMORY:
            x64->op(MOVB, ls.reg, rs.address);
            break;
        case STACK:
            x64->op(POPQ, ls.reg);
            break;
        default:
            throw INTERNAL_ERROR;
        }

        x64->code_label(right_end);
        return ls;
    }
    else {
        // Find a register, and use it to store the result from both sides
        switch (rs.where) {
        case FLAGS:
            x64->op(bitset(rs.cc), reg);
            break;
        case REGISTER:
            reg = rs.reg;
            break;
        case MEMORY:
            x64->op(MOVB, reg, rs.address);
            break;
        case STACK:
            x64->op(POPQ, reg);
            break;
        default:
            throw INTERNAL_ERROR;
        }

        x64->op(JMP, end);
    
        x64->code_label(right_end);
        x64->op(MOVB, reg, need_true ? 1 : 0);
    
        x64->code_label(end);
        return Storage(REGISTER, reg);
    }
}



BooleanOrValue::BooleanOrValue(Value *p, TypeMatch &match)
    :BooleanBinaryValue(p, match, true) {
}



BooleanAndValue::BooleanAndValue(Value *p, TypeMatch &match)
    :BooleanBinaryValue(p, match, false) {
}
