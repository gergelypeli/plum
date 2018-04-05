
class FloatOperationValue: public OptimizedOperationValue {
public:
    //Register reg;
    //SseRegister sse;
    //Regs rclob;
    
    FloatOperationValue(OperationType o, Value *p, TypeMatch &match)
        :OptimizedOperationValue(o, op_arg_ts(o, match), op_ret_ts(o, match), p) {
        //is_left_lvalue = false;
    }
    /*
    virtual Regs preallocate_gpr(Regs preferred, Regs lclob, Regs rclob) {
        // We want to be sure we'll have a clobbered register that is
        // unused by the right result (max 2 registers). It should be a preferred one,
        // but we're willing to use a nonpreferred one if we have to.
        
        const int RIGHT_MAX = 2;
        Regs clob = lclob | rclob;
        Regs prefclob = preferred & clob;

        if ((prefclob & ~rclob).has_any()) {
            // Case 1a: there will be a preferred clobbered unused by the right result.
            return clob;
        }
        
        if (prefclob.count() > RIGHT_MAX) {
            // Case 1b: there will be a preferred clobbered unused by the right result.
            return clob;
        }
        
        // Prefclob is in rclob, and has at most two registers, which may be occupied.
        // Must allocate a new register. Try a preferred one.
        Regs okay = preferred & ~clob;
        
        if (okay.has_any()) {
            // Case 1c: now there will be a preferred clobbered unused by the right result.
            return clob | okay.get_any();
        }
        
        // We have at most two preferred registers, both right clobbered. Must add a
        // nonpreferred register. If there's a third clobbered one, that's fine,
        // otherwise mu must add an arbitrary nonclobbered one.
        
        if (clob.count() > RIGHT_MAX) {
            // Case 2a: there will be a nonpreferred clobbered unused by the right result.
            return clob;
        }
        
        // Case 2b: now there will be a nonpreferred clobbered unused by the right result.
        return clob | (~clob).get_any();
    }
    
    virtual Register postallocate_gpr(Regs preferred, Regs clob, Regs rregs) {
        // Time to pick the working register, 

        Regs prefclob = preferred & clob & ~rregs;

        if (prefclob.has_any()) {
            return prefclob.get_any();
        else
            (clob & ~rregs).get_any();
    }
    
    virtual void spill_lvalue_left(X64 *x64) {
        ls = left->compile(x64);
        
        switch (ls.where) {
        case MEMORY:
            if (ls.regs() & rclob) {
                // Must spill the address
                x64->op(LEA, RBX, ls.address);
                x64->op(PUSHQ, RBX);
                ls = Storage(ALISTACK);
            }
            break;
        case ALISTACK:
            break;
        case ALIAS:
            break;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual void spill_gpr_left(X64 *x64) {
        ls = left->compile(x64);
        
        switch (ls.where) {
        case CONSTANT:
            break;
        case REGISTER:
            if (Regs(ls.reg) & rclob) {
                x64->op(PUSHQ, ls.reg);
                ls = Storage(STACK);
            }
            break;
        case STACK:
            break;
        case MEMORY:
            
        }
    }
    
    virtual void destack_gpr_right(X64 *x64) {
        rs = right->compile(x64);
        
        // Select lreg first, because it has a preferred set
        Register lreg = postallocate_gpr(preferred, clob, rs.regs());  // may not be needed
        
        // The select rreg to anything that does not interfere with the left registers
        Register rreg = (~(ls.regs() | Regs(lreg))).get_any();
        
        switch (rs.where) {
        case CONSTANT:
            break;
        case REGISTER:
            break;
        case STACK:
            x64->op(POPQ, rreg);
            rs = Storage(REGISTER, rreg);
            break;
        case MEMORY:
            break;
        case ALISTACK:
            x64->op(POPQ, rreg);
            rs = Storage(MEMORY, Address(rreg, 0));
            break;
        case ALIAS:
            x64->op(MOVQ, rreg, rs.address);
            rs = Storage(MEMORY, Address(rreg, 0));
            break;
        default:
            throw INTERNAL_ERROR;
        }
        
        return lreg;
    }
    
    virtual void fill_lvalue_left(X64 *x64) {
        switch (ls.where) {
        case MEMORY:
            break;
        case ALISTACK:
            x64->op(POPQ, lreg);
            ls = Storage(MEMORY, Address(lreg, 0));
            break;
        case ALIAS:
            x64->op(MOVQ, lreg, ls.address);
            ls = Storage(MEMORY, Address(lreg, 0));
            break;
        default:
            throw INTERNAL_ERROR;
        }
    }
    
    
    virtual Regs precompile(Regs preferred) {
        // For XMM registers, the register sets are fairly simple.
        // preferred is the contiguous tail that is not clobbered by our right sibling.
        // rclob is the contiguous head that is clobbered by our right child.
        // preferred is always nonempty, if our right sibling clobbers everything, then
        // it's just the full set, as our result will be spilled anyway.
        
        Regs rpref = Regs::all();
        rclob = (right ? right->precompile(rpref) : Regs());
        
        Regs lpref = preferred & ~rclob;
        
        if (is_left_lvalue) {
            if (!lpref.has_any())
                lpref = Regs::all();
        }
        else {
            if (!lpref.has_any_sse())
                lpref = Regs::all();
        }
        
        Regs lclob = left->precompile(lpref);
        Regs clob = lclob | rclob;

        // Reserve a GPR for addresses
        if (is_left_lvalue) {
            reg = (lclob.has_any() ? lclob.get_any() : lpref.get_any());
            clob = clob | reg;
        }
            
        // And always reserve an SSE
        sse = (lclob.has_any_sse() ? lclob.get_any_sse() : lpref.get_any_sse());
        clob = clob | sse;

        return clob;
    }
    
    virtual void subcompile(X64 *x64) {
        ls = left->compile(x64);
        
        if (is_left_lvalue) {
            if (ls.regs() & rclob) {
                if (ls.where != MEMORY)
                    throw INTERNAL_ERROR;
                    
                // Must spill the address
                x64->op(LEA, RBX, ls.address);
                x64->op(PUSHQ, RBX);
                ls = Storage(ALISTACK);
            }
        }
        else {
            
        }
        
        if (ls.regs() & rclob) {
            // Must spill
            x64->op(SUBQ, RSP, 8);
            x64->op(MOVSD, Address(RSP, 0), ls.sse);
            ls = Storage(STACK);
        }
        
        rs = (right ? right->compile(x64) : Storage());
        
        
    }
    
    virtual Storage negate(X64 *x64) {
        ls = left->compile(x64);
        
        switch (ls.where) {
        case CONSTANT:
            x64->op(MOVSD, XMM0, Label::thaw(ls.value));
            x64->op(PXOR, XMM0, x64->runtime->float_minus_zero_label);
            return Storage(REGISTER, XMM0);
        case REGISTER:
            x64->op(PXOR, XMM0, x64->runtime->float_minus_zero_label);
            return Storage(REGISTER, XMM0);
        case MEMORY:
            x64->op(MOVSD, XMM0, ls.address);
            x64->op(PXOR, XMM0, x64->runtime->float_minus_zero_label);
            return Storage(REGISTER, XMM0);
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Storage binary(X64 *x64, SseSsememOp op) {
        ls = left->compile(x64);
        
        switch (ls.where) {
        case CONSTANT:
            x64->op(MOVSD, XMM0, Label::thaw(ls.value));
            x64->op(op, XMM0, Label::thaw*rs.value));
            return Storage(REGISTER, XMM0);
        case REGISTER:
            x64->op(op, XMM0, x64->runtime->float_minus_zero_label);
            return Storage(REGISTER, XMM0);
        case MEMORY:
            x64->op(MOVSD, XMM0, ls.address);
            x64->op(op, XMM0, x64->runtime->float_minus_zero_label);
            return Storage(REGISTER, XMM0);
        default:
            throw INTERNAL_ERROR;
        }
    }
    
    virtual Storage compile(X64 *x64) {
        switch (operation) {
        case NEGATE:
            return negate(x64);
        case ADD:
            return binary_simple(x64, ADDQ);
        case SUBTRACT:
            return binary_simple(x64, SUBQ);
        case MULTIPLY:
            return binary_multiply(x64);
        case DIVIDE:
            return binary_divmod(x64, false, false);
        case MODULO:
            return binary_divmod(x64, true, false);
        case EXPONENT:
            return binary_exponent(x64, false);
        case EQUAL:
            return binary_compare(x64, CC_EQUAL);
        case NOT_EQUAL:
            return binary_compare(x64, CC_NOT_EQUAL);
        case LESS:
            return binary_compare(x64, is_unsigned ? CC_BELOW : CC_LESS);
        case GREATER:
            return binary_compare(x64, is_unsigned ? CC_ABOVE : CC_GREATER);
        case LESS_EQUAL:
            return binary_compare(x64, is_unsigned ? CC_BELOW_EQUAL : CC_LESS_EQUAL);
        case GREATER_EQUAL:
            return binary_compare(x64, is_unsigned ? CC_ABOVE_EQUAL : CC_GREATER_EQUAL);
        case ASSIGN_ADD:
            return assign_binary(x64, ADDQ);
        case ASSIGN_SUBTRACT:
            return assign_binary(x64, SUBQ);
        case ASSIGN_MULTIPLY:
            return assign_multiply(x64);
        case ASSIGN_DIVIDE:
            return assign_divmod(x64, false);
        case ASSIGN_MODULO:
            return assign_divmod(x64, true);
        case ASSIGN_EXPONENT:
            return assign_exponent(x64);
        default:
            return OptimizedOperationValue::compile(x64);
        }
    }
    */
};
