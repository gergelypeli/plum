
class GenericValue: public Value {
public:
    TypeSpec arg_ts;
    std::unique_ptr<Value> left, right;
    Storage ls, rs;
    
    GenericValue(TypeSpec at, TypeSpec rt, Value *l)
        :Value(rt) {
        arg_ts = at;
        left.reset(l);
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        std::cerr << "Generic check.\n";
        ArgInfos x;
        
        if (arg_ts == VOID_TS) {
            std::cerr << "Void used as generic argument type, probably should be NO_TS!\n";
            throw INTERNAL_ERROR;
        }
        
        if (arg_ts != NO_TS) {
            std::cerr << "Generic argument " << arg_ts << ".\n";
            x.push_back({ "arg", &arg_ts, scope, &right });
        }
            
        return check_arguments(args, kwargs, x);
    }
    
    virtual void compile_and_store_both(X64 *x64, Storage l, Storage r) {
        left->compile_and_store(x64, l);
        ls = l;
        
        x64->unwind->push(this);
        
        right->compile_and_store(x64, r);
        rs = r;
        
        x64->unwind->pop(this);
    }
    
    virtual Scope *unwind(X64 *x64) {
        left->ts.store(ls, Storage(), x64);
        return NULL;
    }
};


// Unoptimized version, but works with STACK valued types
class GenericOperationValue: public GenericValue {
public:
    OperationType operation;
    bool is_left_lvalue;
    Regs clob, rclob;
    
    GenericOperationValue(OperationType o, TypeSpec at, TypeSpec rt, Value *l)
        :GenericValue(at, rt, l) {
        operation = o;
        is_left_lvalue = is_assignment(operation);
    }
    
    static TypeSpec op_arg_ts(OperationType o, TypeMatch &match) {
        return is_unary(o) ? NO_TS : match[0].rvalue();
    }

    static TypeSpec op_ret_ts(OperationType o, TypeMatch &match) {
        return o == COMPARE ? INTEGER_TS : is_comparison(o) ? BOOLEAN_TS : match[0];
    }
    
    virtual Regs precompile(Regs preferred) {
        rclob = right ? right->precompile() : Regs();
        
        Regs lclob = left->precompile(Regs::all());
        clob = lclob | rclob;
        
        if (operation == EQUAL || operation == NOT_EQUAL)
            clob = clob | EQUAL_CLOB;
        else if (operation == COMPARE)
            clob = clob | COMPARE_CLOB;
        
        if (clob.count() <= 2)
            clob = clob | (~clob).get_any();  // have at least one lame working register
        
        return clob;
    }

    virtual Storage assign(X64 *x64) {
        ls = left->compile(x64);

        if (ls.where != MEMORY)
            throw INTERNAL_ERROR;

        if (ls.regs() & rclob) {
            Storage s = Storage(ALISTACK);
            left->ts.store(ls, s, x64);
            ls = s;
        }
        
        x64->unwind->push(this);
        rs = right->compile(x64);
        x64->unwind->pop(this);
        
        bool need_alipop = false;
        
        if (ls.where == ALISTACK) {
            Register r = (clob & ~rs.regs()).get_any();
            
            if (rs.where == STACK) {
                x64->op(MOVQ, r, Address(RSP, right->ts.measure_stack()));
                need_alipop = true;
            }
            else {
                x64->op(POPQ, r);
            }
            
            ls = Storage(MEMORY, Address(r, 0));
        }
        else if (ls.where == ALIAS) {
            Register r = (clob & ~rs.regs()).get_any();
            x64->op(MOVQ, r, ls.address);
            ls = Storage(MEMORY, Address(r, 0));
        }

        ts.store(rs, ls, x64);
        
        if (need_alipop)
            x64->op(ADDQ, RSP, ADDRESS_SIZE);
        
        return ls;
    }

    virtual Storage compare(X64 *x64) {
        ls = left->compile(x64);
        
        if (ls.regs() & rclob) {
            Storage s = Storage(STACK);
            left->ts.store(ls, s, x64);
            ls = s;
        }

        x64->unwind->push(this);
        rs = right->compile(x64);
        x64->unwind->pop(this);

        int pop = 0;
        int stack_size = left->ts.measure_stack();
        Storage s, t;
        
        // NOTE: we assume destroy leaves all data as it is
        
        if (ls.where == STACK && rs.where == STACK) {
            s = Storage(MEMORY, Address(RSP, stack_size));
            t = Storage(MEMORY, Address(RSP, 0));
            right->ts.destroy(t, x64);
            left->ts.destroy(s, x64);
            pop = 2 * stack_size;
        }
        else if (ls.where == STACK) {
            s = Storage(MEMORY, Address(RSP, 0));
            t = rs;
            left->ts.destroy(s, x64);
            pop = stack_size;
        }
        else if (rs.where == STACK) {
            s = ls;
            t = Storage(MEMORY, Address(RSP, 0));
            right->ts.destroy(t, x64);
            pop = stack_size;
        }
        else {
            s = ls;
            t = rs;
        }

        left->ts.compare(s, t, x64);

        Register r = clob.get_any();
        
        x64->op(MOVSXBQ, r, BL);  // sign extend byte to qword

        if (pop)
            x64->op(LEA, RSP, Address(RSP, pop));  // leave flags
        
        return Storage(REGISTER, r);
    }

    virtual Storage equal(X64 *x64, bool negate) {
        ls = left->compile(x64);
        
        if (ls.regs() & rclob) {
            Storage s = Storage(STACK);
            left->ts.store(ls, s, x64);
            ls = s;
        }

        x64->unwind->push(this);
        rs = right->compile(x64);
        x64->unwind->pop(this);

        int pop = 0;
        int stack_size = left->ts.measure_stack();
        Storage s, t;
        
        // NOTE: we assume destroy leaves all data as it is
        
        if (ls.where == STACK && rs.where == STACK) {
            s = Storage(MEMORY, Address(RSP, stack_size));
            t = Storage(MEMORY, Address(RSP, 0));
            right->ts.destroy(t, x64);
            left->ts.destroy(s, x64);
            pop = 2 * stack_size;
        }
        else if (ls.where == STACK) {
            s = Storage(MEMORY, Address(RSP, 0));
            t = rs;
            left->ts.destroy(s, x64);
            pop = stack_size;
        }
        else if (rs.where == STACK) {
            s = ls;
            t = Storage(MEMORY, Address(RSP, 0));
            right->ts.destroy(t, x64);
            pop = stack_size;
        }
        else {
            s = ls;
            t = rs;
        }

        left->ts.equal(s, t, x64);

        if (pop)
            x64->op(LEA, RSP, Address(RSP, pop));  // leave flags
        
        return Storage(FLAGS, negate ? CC_NOT_EQUAL : CC_EQUAL);
    }

    virtual Storage compile(X64 *x64) {
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
    
    virtual Scope *unwind(X64 *x64) {
        left->ts.store(ls, Storage(), x64);
        return NULL;
    }
};


class OptimizedOperationValue: public GenericOperationValue {
public:
    Storage auxls;
    RegSubset lsubset, rsubset;
    
    OptimizedOperationValue(OperationType o, TypeSpec at, TypeSpec rt, Value *l, RegSubset lss, RegSubset rss)
        :GenericOperationValue(o, at, rt, l) {

        if (left->ts.where(AS_VALUE) != REGISTER)
            throw INTERNAL_ERROR;
            
        if (at != NO_TS && at.where(AS_VALUE) != REGISTER)
            throw INTERNAL_ERROR;
            
        lsubset = lss;
        rsubset = rss;
    }
    
    virtual Storage pick_early_auxls(Regs preferred) {
        if (lsubset == GPR_SUBSET || lsubset == PTR_SUBSET) {
            Register r = NOREG;
        
            if ((preferred & clob & ~rclob).has_any()) {
                // We have preferred registers clobbered by the left side only, use one
                r = (preferred & clob & ~rclob).get_any();
            }
            else if ((clob & ~rclob).has_any()) {
                // We have registers clobbered by the left side only, use one
                r = (clob & ~rclob).get_any();
            }
            else if ((preferred & ~rclob).has_any()) {
                // We have preferred registers not clobbered by the right side, allocate one
                r = (preferred & ~rclob).get_any();
            }
            else if (rclob.count() <= 2) {
                // Just allocate a register that is not clobbered by the right side
                r = (~rclob).get_any();
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
        
            return Storage(REGISTER, r);
        }
        else
            throw INTERNAL_ERROR;
    }

    virtual Storage pick_late_auxls() {
        // The right side clobbered many registers, pick one that is not used by its value
        
        if (lsubset == GPR_SUBSET || lsubset == PTR_SUBSET) {
            Register r = (clob & ~rs.regs()).get_any();

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

    virtual Storage pick_auxrs(RegSubset rss) {
        // Since the code can be more optimal if we don't load the right argument
        // into registers before actually executing the operation, we may leave it
        // in memory in many cases, only have to load stuff from ALISTACK and ALIAS.

        if (rss == GPR_SUBSET || rss == PTR_SUBSET) {
            Register r = (~(ls.regs() | auxls.regs())).get_any();
        
            if (rss == PTR_SUBSET)
                return Storage(MEMORY, Address(r, 0));
            else
                return Storage(REGISTER, r);
        }
        else if (rss == SSE_SUBSET) {
            SseRegister r = (~(ls.regs() | auxls.regs())).get_sse();
        
            return Storage(REGISTER, r);
        }
        else
            throw INTERNAL_ERROR;
    }

    virtual Regs precompile(Regs preferred) {
        rclob = right ? right->precompile() : Regs();
        
        // lpref must be nonempty
        Regs lpref = (preferred & ~rclob).has_any() ? preferred & ~rclob : (~rclob).has_any() ? ~rclob : Regs::all();
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

    virtual void subcompile(X64 *x64) {
        ls = left->compile(x64);

        // Put the left value in a safe place
        if (is_left_lvalue) {
            // Address handling
            
            switch (ls.where) {
            case MEMORY:
                if (ls.regs() & rclob) {
                    // We got a dynamic address clobbered by the right side
                    
                    if (auxls.where == MEMORY) {
                        // Save to another register  FIXME: really need a mstore method!
                        // Then we could just initialize auxls to ALISTACK,
                        // and spill without thinking.
                        x64->op(LEA, auxls.address.base, ls.address);
                        ls = auxls;
                    }
                    else {
                        // Spill address to stack
                        left->ts.store(ls, Storage(ALISTACK), x64);
                        ls = Storage(ALISTACK);
                    }
                }
                break;
            case ALISTACK:
                // Already on stack, fine
                break;
            case ALIAS:
                // Aliases are at static addresses, can't be clobbered.
                // And they never change, so we don't have to load them just to be sure.
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
                    left->ts.store(ls, auxls, x64);
                    ls = auxls;
                }
                else {
                    left->ts.store(ls, Storage(STACK), x64);
                    ls = Storage(STACK);
                }
                break;
            case REGISTER:
                if (ls.regs() & rclob) {
                    if (auxls.where == REGISTER) {
                        left->ts.store(ls, auxls, x64);
                        ls = auxls;
                    }
                    else {
                        left->ts.store(ls, Storage(STACK), x64);
                        ls = Storage(STACK);
                    }
                }
                break;
            case STACK:
                break;
            case MEMORY:
                // We must also be careful that the right side may change any variable!
                // And reg is a register that we allocate for values, so make sure
                // a dynamic address is not using that!
                
                if (!rclob.has_any()) {
                    // TODO: we need to have a better way to detect side effects!
                    // Okay, the right side has no side effects, and we don't want to
                    // destroy the address either, so keep the MEMORY storage.
                }
                else if (auxls.where == REGISTER) {
                    // We already know a register that won't be clobbered, save value there
                    // This may actually reuse the same register, but that's OK
                    left->ts.store(ls, auxls, x64);
                    ls = auxls;
                }
                else {
                    // Nothing is sure, push the value onto the stack
                    left->ts.store(ls, Storage(STACK), x64);
                    ls = Storage(STACK);
                }
                break;
            case ALISTACK: {
                // The address itself may be safe, but the value may be not.
                // Store is only defined from ALISTACK to MEMORY, so do this manually.

                Register tmpr = clob.get_any();
                x64->op(POPQ, tmpr);
                ls = Storage(MEMORY, Address(tmpr, 0));
                
                if (auxls.where == REGISTER) {
                    // We already know a register that won't be clobbered, save value there
                    left->ts.store(ls, auxls, x64);
                    ls = auxls;
                }
                else {
                    // Nothing is sure, push the value onto the stack
                    left->ts.store(ls, Storage(STACK), x64);
                    ls = Storage(STACK);
                }
                }
                break;
            case ALIAS: {
                // The address itself may be safe, but the value may be not.
                // Store is only defined from ALIAS to MEMORY, so do this manually.

                Register tmpr = clob.get_any();
                x64->op(MOVQ, tmpr, ls.address);
                ls = Storage(MEMORY, Address(tmpr, 0));
                
                if (auxls.where == REGISTER) {
                    // We already know a register that won't be clobbered, save value there
                    left->ts.store(ls, auxls, x64);
                    ls = auxls;
                }
                else {
                    // Nothing is sure, push the value onto the stack
                    left->ts.store(ls, Storage(STACK), x64);
                    ls = Storage(STACK);
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
        case STACK: {
            Storage auxrs = pick_auxrs(rsubset);
            right->ts.store(rs, auxrs, x64);
            rs = auxrs;
            }
            break;
        case MEMORY:
            break;
        case ALISTACK: {
            Storage auxrs = pick_auxrs(PTR_SUBSET);
            right->ts.store(rs, auxrs, x64);
            rs = auxrs;
            }
            break;
        case ALIAS: {
            Storage auxrs = pick_auxrs(PTR_SUBSET);
            right->ts.store(rs, auxrs, x64);
            rs = auxrs;
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
        case STACK:
            if (auxls.where == REGISTER) {
                left->ts.store(ls, auxls, x64);
                ls = auxls;
            }
            else
                throw INTERNAL_ERROR;
            break;
        case MEMORY:
            break;
        case ALISTACK:
            if (auxls.where == MEMORY) {
                left->ts.store(ls, auxls, x64);
                ls = auxls;
            }
            else
                throw INTERNAL_ERROR;
            break;
        case ALIAS:
            if (auxls.where == MEMORY) {
                left->ts.store(ls, auxls, x64);
                ls = auxls;
            }
            else
                throw INTERNAL_ERROR;
            break;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Storage assign(X64 *x64) {
        subcompile(x64);

        if (ls.where != MEMORY)
            throw INTERNAL_ERROR;

        ts.store(rs, ls, x64);
        
        return ls;
    }

    virtual Storage compare(X64 *x64) {
        subcompile(x64);

        left->ts.compare(ls, rs, x64);
        
        if (auxls.where != REGISTER)
            throw INTERNAL_ERROR;
        
        x64->op(MOVSXBQ, auxls.reg, BL);  // sign extend byte to qword
        
        return auxls;
    }

    virtual Storage equal(X64 *x64, bool negate) {
        subcompile(x64);

        left->ts.equal(ls, rs, x64);

        return Storage(FLAGS, negate ? CC_NOT_EQUAL : CC_EQUAL);
    }

    virtual Storage compile(X64 *x64) {
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
    
    virtual Scope *unwind(X64 *x64) {
        left->ts.store(ls, Storage(), x64);
        return NULL;
    }
};


