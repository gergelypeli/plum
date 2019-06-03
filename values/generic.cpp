#include "../plum.h"


GenericValue::GenericValue(TypeSpec at, TypeSpec rt, Value *l)
    :Value(rt) {
    arg_ts = at;
    left.reset(l);

    if (arg_ts == VOID_TS) {
        std::cerr << "Void used as generic argument type, probably should be NO_TS!\n";
        throw INTERNAL_ERROR;
    }
}

bool GenericValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    //std::cerr << "Generic check.\n";
    ArgInfos x;
    
    if (arg_ts != NO_TS) {
        //std::cerr << "Generic argument " << arg_ts << ".\n";
        x.push_back({ "arg", &arg_ts, scope, &right });
    }
        
    return check_arguments(args, kwargs, x);
}

void GenericValue::compile_and_store_both(X64 *x64, Storage l, Storage r) {
    left->compile_and_store(x64, l);
    ls = l;
    
    x64->unwind->push(this);
    
    right->compile_and_store(x64, r);
    rs = r;
    
    x64->unwind->pop(this);
}

CodeScope *GenericValue::unwind(X64 *x64) {
    left->ts.store(ls, Storage(), x64);
    return NULL;
}


// Unoptimized version, but works with STACK valued types

GenericOperationValue::GenericOperationValue(OperationType o, TypeSpec at, TypeSpec rt, Value *l)
    :GenericValue(at, rt, l) {
    operation = o;
    is_left_lvalue = is_assignment(operation);
}

TypeSpec GenericOperationValue::op_arg_ts(OperationType o, TypeMatch &match) {
    return is_unary(o) ? NO_TS : match[0].rvalue();
}

TypeSpec GenericOperationValue::op_ret_ts(OperationType o, TypeMatch &match) {
    return o == COMPARE ? INTEGER_TS : is_comparison(o) ? BOOLEAN_TS : match[0];
}

void GenericOperationValue::need_rvalue() {
    GenericLvalue::need_rvalue();
    
    // Since even if we take an lvalue pivot, we surely modify it, so that won't be
    // an rvalue, even if our parent doesn't want to modify us.
}

Regs GenericOperationValue::precompile(Regs preferred) {
    rclob = right ? right->precompile_tail() : Regs();
    
    Regs lclob = left->precompile(preferred & ~rclob);
    clob = lclob | rclob;
    
    if (operation == EQUAL || operation == NOT_EQUAL)
        clob = clob | EQUAL_CLOB;
    else if (operation == COMPARE)
        clob = clob | COMPARE_CLOB;
    
    clob.reserve_gpr(3);

    // NOTE: lvalue returning operations return the pivot operand, which is an lvalue itself.
    // Since it will be modified, the pivot value will set the necessary clobbering flags.
    // Initialization is considered a modification for this reason only.
    
    return clob;
}

Storage GenericOperationValue::lmemory(X64 *x64) {
    // Load the left side lvalue argument to a temporary MEMORY storage
    
    if (ls.where == ALISTACK) {
        // Surely a dynamic address
        Register r = (clob & ~rs.regs()).get_gpr();
        int offset = (rs.where == STACK ? right->ts.measure_stack() : 0);
        
        x64->op(MOVQ, r, Address(RSP, offset));
        return Storage(MEMORY, Address(r, 0));
    }
    else if (ls.where == ALIAS) {
        Register r = (clob & ~rs.regs()).get_gpr();
        x64->op(MOVQ, r, ls.address);
        return Storage(MEMORY, Address(r, ls.value));
    }
    else if (ls.where == MEMORY)
        return ls;
    else
        throw INTERNAL_ERROR;
}

Storage GenericOperationValue::assign_create(X64 *x64) {
    ls = left->compile(x64);
    
    if (ls.where != MEMORY && ls.where != ALIAS)
        throw INTERNAL_ERROR;

    if (ls.regs() & rclob) {
        // a clobberable ls must be a MEMORY with a dynamic address
        x64->op(LEA, R10, ls.address);
        x64->op(PUSHQ, 0);
        x64->op(PUSHQ, R10);
        ls = Storage(ALISTACK);
    }

    x64->unwind->push(this);
    rs = right->compile(x64);
    x64->unwind->pop(this);

    Storage als = lmemory(x64);

    if (operation == ASSIGN)
        ts.store(rs, als, x64);
    else if (operation == CREATE)
        ts.create(rs, als, x64);
    else
        throw INTERNAL_ERROR;
    
    return ls;
}

Storage GenericOperationValue::compare(X64 *x64) {
    ls = left->compile(x64);
    
    if (ls.regs() & rclob) {
        ls = left->ts.store(ls, Storage(STACK), x64);
    }

    x64->unwind->push(this);
    rs = right->compile(x64);
    x64->unwind->pop(this);

    int stack_size = left->ts.measure_stack();
    Storage s, t;

    bool lx = (ls.where == STACK);
    bool rx = (rs.where == STACK);
    
    if (lx && rx) {
        s = Storage(MEMORY, Address(RSP, stack_size));
        t = Storage(MEMORY, Address(RSP, 0));
    }
    else if (lx) {
        s = Storage(MEMORY, Address(RSP, 0));
        t = rs;
    }
    else if (rx) {
        s = ls;
        t = Storage(MEMORY, Address(RSP, 0));
    }
    else {
        s = ls;
        t = rs;
    }
    
    left->ts.compare(s, t, x64);

    Register r = clob.get_gpr();
    x64->op(MOVSXBQ, r, R10B);  // sign extend byte to qword

    right->ts.store(rs, Storage(), x64);
    left->ts.store(ls, Storage(), x64);

    return Storage(REGISTER, r);
}

Storage GenericOperationValue::equal(X64 *x64, bool negate) {
    ls = left->compile(x64);
    
    if (ls.regs() & rclob) {
        ls = left->ts.store(ls, Storage(STACK), x64);
    }

    x64->unwind->push(this);
    rs = right->compile(x64);
    x64->unwind->pop(this);

    int stack_size = left->ts.measure_stack();
    Storage s, t;
    
    bool lx = (ls.where == STACK);
    bool rx = (rs.where == STACK);
    
    if (lx && rx) {
        s = Storage(MEMORY, Address(RSP, stack_size));
        t = Storage(MEMORY, Address(RSP, 0));
    }
    else if (lx) {
        s = Storage(MEMORY, Address(RSP, 0));
        t = rs;
    }
    else if (rx) {
        s = ls;
        t = Storage(MEMORY, Address(RSP, 0));
    }
    else {
        s = ls;
        t = rs;
    }
    
    left->ts.equal(s, t, x64);

    Register r = clob.get_gpr();
    x64->op(negate ? SETNE : SETE, r);

    right->ts.store(rs, Storage(), x64);
    left->ts.store(ls, Storage(), x64);
    
    return Storage(REGISTER, r);
}

Storage GenericOperationValue::compile(X64 *x64) {
    switch (operation) {
    case ASSIGN:
        return assign_create(x64);
    case CREATE:
        return assign_create(x64);
    case COMPARE:
        return compare(x64);
    case EQUAL:
        return equal(x64, false);
    case NOT_EQUAL:
        return equal(x64, true);
    default:
        throw INTERNAL_ERROR;
    }
}

CodeScope *GenericOperationValue::unwind(X64 *x64) {
    left->ts.store(ls, Storage(), x64);
    return NULL;
}




OptimizedOperationValue::OptimizedOperationValue(OperationType o, TypeSpec at, TypeSpec rt, Value *l, RegSubset lss, RegSubset rss)
    :GenericOperationValue(o, at, rt, l) {

    StorageWhere lw = left->ts.where(AS_VALUE);
    StorageWhere rw = (at != NO_TS ? at.where(AS_VALUE) : NOWHERE);
    
    if ((lw != REGISTER && lw != SSEREGISTER) || (rw != REGISTER && rw != SSEREGISTER && rw != NOWHERE))
        throw INTERNAL_ERROR;
        
    lsubset = lss;
    rsubset = rss;
}

Storage OptimizedOperationValue::pick_early_auxls(Regs preferred) {
    if (lsubset == GPR_SUBSET || lsubset == PTR_SUBSET) {
        Register r = NOREG;
    
        if ((preferred & clob & ~rclob).has_gpr()) {
            // We have preferred registers clobbered by the left side only, use one
            r = (preferred & clob & ~rclob).get_gpr();
        }
        else if ((clob & ~rclob).has_gpr()) {
            // We have registers clobbered by the left side only, use one
            r = (clob & ~rclob).get_gpr();
        }
        else if ((preferred & ~rclob).has_gpr()) {
            // We have preferred registers not clobbered by the right side, allocate one
            r = (preferred & ~rclob).get_gpr();
        }
        else if (rclob.count_gpr() <= 2) {
            // Just allocate a register that is not clobbered by the right side
            r = (~rclob).get_gpr();
        }
        else {
            // The right side clobbers many registers, so pick one for the left later
            return Storage();
        }
    
        if (lsubset == PTR_SUBSET)
            return Storage(MEMORY, Address(r, 0));
        else
            return Storage(REGISTER, r);
    }
    else if (lsubset == SSE_SUBSET) {
        SseRegister r = NOSSE;
    
        if ((preferred & clob & ~rclob).has_sse()) {
            // We have preferred registers clobbered by the left side only, use one
            r = (preferred & clob & ~rclob).get_sse();
        }
        else if ((clob & ~rclob).has_sse()) {
            // We have registers clobbered by the left side only, use one
            r = (clob & ~rclob).get_sse();
        }
        else if ((preferred & ~rclob).has_sse()) {
            // We have preferred registers not clobbered by the right side, allocate one
            r = (preferred & ~rclob).get_sse();
        }
        else if (rclob.count_sse() <= 2) {
            // Just allocate a register that is not clobbered by the right side
            r = (~rclob).get_sse();
        }
        else {
            // The right side clobbers many registers, so pick one for the left later
            return Storage();
        }
    
        return Storage(SSEREGISTER, r);
    }
    else
        throw INTERNAL_ERROR;
}

Storage OptimizedOperationValue::pick_late_auxls() {
    // The right side clobbered many registers, pick one that is not used by its value
    
    if (lsubset == GPR_SUBSET || lsubset == PTR_SUBSET) {
        Register r = (clob & ~rs.regs()).get_gpr();

        if (lsubset == PTR_SUBSET)
            return Storage(MEMORY, Address(r, 0));
        else
            return Storage(REGISTER, r);
    }
    else if (lsubset == SSE_SUBSET) {
        SseRegister r = (clob & ~rs.regs()).get_sse();

        return Storage(REGISTER, r);
    }
    else
        throw INTERNAL_ERROR;
}

Storage OptimizedOperationValue::pick_auxrs(RegSubset rss) {
    // Since the code can be more optimal if we don't load the right argument
    // into registers before actually executing the operation, we may leave it
    // in memory in many cases, only have to load stuff from ALIAS.

    if (rss == GPR_SUBSET || rss == PTR_SUBSET) {
        Register r = (~(ls.regs() | auxls.regs())).get_gpr();
    
        if (rss == PTR_SUBSET)
            return Storage(MEMORY, Address(r, 0));
        else
            return Storage(REGISTER, r);
    }
    else if (rss == SSE_SUBSET) {
        SseRegister r = (~(ls.regs() | auxls.regs())).get_sse();
    
        return Storage(SSEREGISTER, r);
    }
    else
        throw INTERNAL_ERROR;
}

Regs OptimizedOperationValue::precompile(Regs preferred) {
    rclob = right ? right->precompile_tail() : Regs();
    
    // lpref must be nonempty
    Regs lpref = (preferred & ~rclob).has_gpr() ? preferred & ~rclob : (~rclob).has_gpr() ? ~rclob : Regs::allregs();
    Regs lclob = left->precompile(lpref);
    clob = lclob | rclob;
    
    // We may need a register to perform the operation, and also return the result with.
    // If the left value is spilled, we also reload it to this one. For a lvo this
    // register may contain the address of the returned lvalue.
    auxls = pick_early_auxls(preferred);
    clob = clob | auxls.regs();
    
    if (operation == EQUAL || operation == NOT_EQUAL)
        clob = clob | EQUAL_CLOB;
    else if (operation == COMPARE)
        clob = clob | COMPARE_CLOB;
    
    return clob;
}

void OptimizedOperationValue::subcompile(X64 *x64) {
    ls = left->compile(x64);

    // Put the left value in a safe place
    if (is_left_lvalue) {
        // Address handling
        
        switch (ls.where) {
        case MEMORY:
            if (ls.regs() & rclob) {
                // We got a dynamic address clobbered by the right side
                
                if (auxls.where == MEMORY) {
                    // It's possible to store the address into an unclobbered register.
                    // There's no MEMORY_MEMORY store that just converts addresses,
                    // so do a LEA here directly.
                    x64->op(LEA, auxls.address.base, ls.address);
                    ls = auxls;
                }
                else {
                    // Spill dynamic address to stack
                    x64->op(LEA, R10, ls.address);
                    x64->op(PUSHQ, 0);
                    x64->op(PUSHQ, R10);
                    ls = Storage(ALISTACK);
                }
            }
            break;
        case ALIAS:
            // Aliases are at static addresses, can't be clobbered.
            // And they never change, so we don't have to load them just to be sure.
            break;
        case ALISTACK:
            // We can handle it later
            break;
        default:
            throw INTERNAL_ERROR;
        }
    }
    else {
        // Value handling
        
        switch (ls.where) {
        case CONSTANT:
            break;
        case FLAGS:
            if (auxls.where == REGISTER) {
                ls = left->ts.store(ls, auxls, x64);
            }
            else {
                ls = left->ts.store(ls, Storage(STACK), x64);
            }
            break;
        case REGISTER:
        case SSEREGISTER:
            if (ls.regs() & rclob) {
                if (auxls.where == REGISTER || auxls.where == SSEREGISTER) {
                    ls = left->ts.store(ls, auxls, x64);
                }
                else {
                    ls = left->ts.store(ls, Storage(STACK), x64);
                }
            }
            break;
        case STACK:
            break;
        case MEMORY:
            // We must also be careful that the right side may change any variable!
            // And reg is a register that we allocate for values, so make sure
            // a dynamic address is not using that!
            
            if (!rclob.has_gpr()) {
                // TODO: we need to have a better way to detect side effects!
                // Okay, the right side has no side effects, and we don't want to
                // destroy the address either, so keep the MEMORY storage.
            }
            else if (auxls.where == REGISTER || auxls.where == SSEREGISTER) {
                // We already know a register that won't be clobbered, save value there
                // This may actually reuse the same register, but that's OK
                ls = left->ts.store(ls, auxls, x64);
            }
            else {
                // Nothing is sure, push the value onto the stack
                ls = left->ts.store(ls, Storage(STACK), x64);
            }
            break;
        case ALIAS: {
            // The address itself may be safe, but the value may be not.
            // Store is only defined from ALIAS to MEMORY, so do this manually.

            Register tmpr = clob.get_gpr();
            x64->op(MOVQ, tmpr, ls.address);
            ls = Storage(MEMORY, Address(tmpr, ls.value));
            
            if (auxls.where == REGISTER || auxls.where == SSEREGISTER) {
                // We already know a register that won't be clobbered, save value there
                ls = left->ts.store(ls, auxls, x64);
            }
            else {
                // Nothing is sure, push the value onto the stack
                ls = left->ts.store(ls, Storage(STACK), x64);
            }
            }
            break;
        default:
            throw INTERNAL_ERROR;
        }
    }

    if (right) {
        x64->unwind->push(this);
        rs = right->compile(x64);
        x64->unwind->pop(this);
    }
    
    // auxls has prference, should be picked first
    if (auxls.where == NOWHERE)
        auxls = pick_late_auxls();

    // Load right values from too indirect places, and from the stack for fill
    switch (rs.where) {
    case NOWHERE:
        break;
    case CONSTANT:
        break;
    case FLAGS:
        break;
    case REGISTER:
        break;
    case SSEREGISTER:
        break;
    case STACK:
        rs = right->ts.store(rs, pick_auxrs(rsubset), x64);
        break;
    case MEMORY:
        break;
    case ALIAS: {
        Storage auxrs = pick_auxrs(PTR_SUBSET);  // MEMORY with 0 offset
        x64->op(MOVQ, auxrs.address.base, rs.address);
        rs = auxrs + rs.value;
    }
        break;
    default:
        throw INTERNAL_ERROR;
    }
    
    // Restore the spilled left side
    switch (ls.where) {
    case CONSTANT:
        break;
    case FLAGS:
        throw INTERNAL_ERROR;
    case REGISTER:
        break;
    case SSEREGISTER:
        break;
    case STACK:
        if (auxls.where == REGISTER || auxls.where == SSEREGISTER) {
            ls = left->ts.store(ls, auxls, x64);
        }
        else
            throw INTERNAL_ERROR;
        break;
    case MEMORY:
        break;
    case ALIAS:
        // Must use lmemory for direct access
        break;
    case ALISTACK:
        // Must use lmemory for direct access
        break;
    default:
        throw INTERNAL_ERROR;
    }
}

Storage OptimizedOperationValue::assign(X64 *x64) {
    subcompile(x64);

    Storage als = lmemory(x64);

    ts.store(rs, als, x64);
    
    return ls;
}

Storage OptimizedOperationValue::compare(X64 *x64) {
    subcompile(x64);

    left->ts.compare(ls, rs, x64);
    
    if (auxls.where != REGISTER) {
        // This happens when the comparison used SSE registers
        x64->op(MOVSXBQ, R10, R10B);  // sign extend byte to qword
        x64->op(PUSHQ, R10);
        return Storage(STACK);
    }
    else {
        x64->op(MOVSXBQ, auxls.reg, R10B);  // sign extend byte to qword
        return auxls;
    }
}

Storage OptimizedOperationValue::equal(X64 *x64, bool negate) {
    subcompile(x64);

    left->ts.equal(ls, rs, x64);

    return Storage(FLAGS, negate ? CC_NOT_EQUAL : CC_EQUAL);
}

Storage OptimizedOperationValue::compile(X64 *x64) {
    switch (operation) {
    case ASSIGN:
        return assign(x64);
    case COMPARE:
        return compare(x64);
    case EQUAL:
        return equal(x64, false);
    case NOT_EQUAL:
        return equal(x64, true);
    default:
        throw INTERNAL_ERROR;
    }
}

CodeScope *OptimizedOperationValue::unwind(X64 *x64) {
    left->ts.store(ls, Storage(), x64);
    return NULL;
}
