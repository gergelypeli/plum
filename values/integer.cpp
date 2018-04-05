
const int MAX_SIGNED_DWORD = 2147483647;

class IntegerOperationValue: public OptimizedOperationValue {
public:
    int os;
    bool is_unsigned;
    
    IntegerOperationValue(OperationType o, Value *pivot, TypeMatch &match)
        :OptimizedOperationValue(o, op_arg_ts(o, match), op_ret_ts(o, match), pivot) {
        int size = match[0].measure_raw();
        os = (
            size == 1 ? 0 :
            size == 2 ? 1 :
            size == 4 ? 2 :
            size == 8 ? 3 :
            throw INTERNAL_ERROR
        );

        if (operation != ASSIGN && operation != EQUAL && operation != NOT_EQUAL)
            ptr_cast<BasicType>(match[0].rvalue()[0])->get_unsigned();
    }
    
    virtual void exponentiation_by_squaring(X64 *x64, Register BASE, Register EXP, Register RES) {
        // RES = BASE ** EXP
        
        int xos = os == 0 ? 1 : os;  // No IMULB
        
        Label loop_label;
        Label skip_label;
        
        x64->op(MOVQ % os, RES, 1);
        
        x64->code_label(loop_label);
        x64->op(TESTQ % os, EXP, 1);
        x64->op(JE, skip_label);
        x64->op(IMUL2Q % xos, RES, BASE);
        
        x64->code_label(skip_label);
        x64->op(IMUL2Q % xos, BASE, BASE);
        x64->op(SHRQ % os, EXP, 1);
        x64->op(JNE, loop_label);
    }

    virtual bool fits8(int value) {
        return value >= -128 && value <= 127;
    }

    virtual bool fits32(int value) {
        // We don't want to rely on the C++ truncation mechanism, so
        // as soon as the values don't fit in our types, stop keeping
        // them constants. Also, x64 immediate values can only be at
        // most signed dwords, so anything not fitting in that must
        // be dropped, too.
        
        return (
            is_unsigned ? (
                os == 0 ? value >= 0 && value <= 255 :
                os == 1 ? value >= 0 && value <= 65535 :
                value >= 0 && value <= MAX_SIGNED_DWORD
            ) : (
                os == 0 ? value >= -128 && value <= 127 :
                os == 1 ? value >= -32768 && value <= 32767 :
                value >= -1 - MAX_SIGNED_DWORD && value <= MAX_SIGNED_DWORD
            )
        );
    }

    virtual Storage unary(X64 *x64, UnaryOp opcode) {
        subcompile(x64);

        if (auxls.where != REGISTER)
            throw INTERNAL_ERROR;
        
        switch (ls.where) {
        case CONSTANT: {
            int value = opcode % 3 == NEGQ ? -ls.value : ~ls.value;
            
            if (fits32(value))
                return Storage(CONSTANT, value);
                
            x64->op(MOVQ % os, auxls.reg, ls.value);
            x64->op(opcode % os, auxls.reg);
            return auxls;
        }
        case REGISTER:
            x64->op(opcode % os, ls.reg);
            return Storage(REGISTER, ls.reg);
        case MEMORY:
            x64->op(MOVQ % os, auxls.reg, ls.address);
            x64->op(opcode % os, auxls.reg);
            return auxls;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Storage binary_simple(X64 *x64, BinaryOp opcode) {
        bool commutative = (opcode % 3) != SUBQ;

        subcompile(x64);

        switch (ls.where * rs.where) {
        case CONSTANT_CONSTANT: {
            int value = (
                (opcode % 3) == ADDQ ? ls.value + rs.value :
                (opcode % 3) == SUBQ ? ls.value - rs.value :
                (opcode % 3) == ANDQ ? ls.value & rs.value :
                (opcode % 3) == ORQ  ? ls.value | rs.value :
                (opcode % 3) == XORQ ? ls.value ^ rs.value :
                throw INTERNAL_ERROR
            );
            
            if (fits32(value))
                return Storage(CONSTANT, value);
            
            x64->op(MOVQ % os, auxls.reg, ls.value);
            x64->op(opcode % os, auxls.reg, rs.value);
            return auxls;
        }
        case CONSTANT_REGISTER:
            if (commutative) {
                x64->op(opcode % os, rs.reg, ls.value);
                return Storage(REGISTER, rs.reg);
            }
            else {
                x64->op(MOVQ % os, auxls.reg, ls.value);
                x64->op(opcode % os, auxls.reg, rs.reg);
                return auxls;
            }
        case CONSTANT_MEMORY:
            x64->op(MOVQ % os, auxls.reg, ls.value);
            x64->op(opcode % os, auxls.reg, rs.address);
            return auxls;
        case REGISTER_CONSTANT:
            x64->op(opcode % os, ls.reg, rs.value);
            return Storage(REGISTER, ls.reg);
        case REGISTER_REGISTER:
            x64->op(opcode % os, ls.reg, rs.reg);
            return Storage(REGISTER, ls.reg);
        case REGISTER_MEMORY:
            x64->op(opcode % os, ls.reg, rs.address);
            return Storage(REGISTER, ls.reg);
        case MEMORY_CONSTANT:
            x64->op(MOVQ % os, auxls.reg, ls.address);
            x64->op(opcode % os, auxls.reg, rs.value);
            return auxls;
        case MEMORY_REGISTER:
            if (commutative) {
                x64->op(opcode % os, rs.reg, ls.address);
                return Storage(REGISTER, rs.reg);
            }
            else {
                x64->op(MOVQ % os, auxls.reg, ls.address);
                x64->op(opcode % os, auxls.reg, rs.reg);
                return auxls;
            }
        case MEMORY_MEMORY:
            x64->op(MOVQ % os, auxls.reg, ls.address);
            x64->op(opcode % os, auxls.reg, rs.address);
            return auxls;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Storage binary_multiply(X64 *x64) {
        subcompile(x64);

        // Yay, IMUL2 and IMUL3 truncates the result, so it can be used for
        // both signed and unsigned multiplication.
        // Use IMULW for bytes, and ignore the upper byte.
        int ios = os == 0 ? 1 : os;

        switch (ls.where * rs.where) {
        case CONSTANT_CONSTANT: {
            int value = ls.value * rs.value;
            
            if (fits32(value))
                return Storage(CONSTANT, value);
                
            x64->op(MOVQ % os, auxls.reg, ls.value);
            x64->op(IMUL3Q % ios, auxls.reg, auxls.reg, rs.value);

            return auxls;
        }
        case CONSTANT_REGISTER:
            x64->op(IMUL3Q % ios, rs.reg, rs.reg, ls.value);
            return Storage(REGISTER, rs.reg);
        case CONSTANT_MEMORY:
            x64->op(IMUL3Q % ios, auxls.reg, rs.address, ls.value);
            return auxls;
        case REGISTER_CONSTANT:
            x64->op(IMUL3Q % ios, ls.reg, ls.reg, rs.value);
            return Storage(REGISTER, ls.reg);
        case REGISTER_REGISTER:
            x64->op(IMUL2Q % ios, ls.reg, rs.reg);
            return Storage(REGISTER, ls.reg);
        case REGISTER_MEMORY:
            x64->op(IMUL2Q % ios, ls.reg, rs.address);
            return Storage(REGISTER, ls.reg);
        case MEMORY_CONSTANT:
            x64->op(IMUL3Q % ios, auxls.reg, ls.address, rs.value);
            return auxls;
        case MEMORY_REGISTER:
            x64->op(IMUL2Q % ios, rs.reg, ls.address);
            return Storage(REGISTER, rs.reg);
        case MEMORY_MEMORY:
            x64->op(MOVQ % os, auxls.reg, ls.address);
            x64->op(IMUL2Q % ios, auxls.reg, rs.address);
            return auxls;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual void arrange_in_rax_rcx(X64 *x64) {
        // Careful, not to destroy the base register of the other address before dereferencing!

        switch (ls.where * rs.where) {
        case CONSTANT_CONSTANT:
            x64->op(MOVQ % os, RAX, ls.value);
            x64->op(MOVQ % os, RCX, rs.value);
            break;
        case CONSTANT_REGISTER:
            x64->op(MOVQ % os, RCX, rs.reg);  // Order!
            x64->op(MOVQ % os, RAX, ls.value);
            break;
        case CONSTANT_MEMORY:
            x64->op(MOVQ % os, RCX, rs.address);  // Order!
            x64->op(MOVQ % os, RAX, ls.value);
            break;
        case REGISTER_CONSTANT:
            x64->op(MOVQ % os, RAX, ls.reg);  // Order!
            x64->op(MOVQ % os, RCX, rs.value);
            break;
        case REGISTER_REGISTER:
            x64->op(MOVQ % os, RBX, rs.reg);
            x64->op(MOVQ % os, RAX, ls.reg);
            x64->op(MOVQ % os, RCX, RBX);
            break;
        case REGISTER_MEMORY:
            x64->op(MOVQ % os, RBX, rs.address);
            x64->op(MOVQ % os, RAX, ls.reg);
            x64->op(MOVQ % os, RCX, RBX);
            break;
        case MEMORY_CONSTANT:
            x64->op(MOVQ % os, RAX, ls.address);  // Order!
            x64->op(MOVQ % os, RCX, rs.value);
            break;
        case MEMORY_REGISTER:
            x64->op(MOVQ % os, RBX, rs.reg);
            x64->op(MOVQ % os, RAX, ls.address);
            x64->op(MOVQ % os, RCX, RBX);
            break;
        case MEMORY_MEMORY:
            x64->op(MOVQ % os, RBX, rs.address);
            x64->op(MOVQ % os, RAX, ls.address);
            x64->op(MOVQ % os, RCX, RBX);
            break;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Storage binary_divmod(X64 *x64, bool mod) {
        subcompile(x64);

        if (ls.where == CONSTANT && rs.where == CONSTANT)
            return Storage(CONSTANT, mod ? ls.value % rs.value : ls.value / rs.value);

        arrange_in_rax_rcx(x64);
        
        SimpleOp prep = (os == 0 ? CBW : os == 1 ? CWD : os == 2 ? CDQ : CQO);
        UnaryOp opcode = (is_unsigned ? DIVQ : IDIVQ);

        x64->op(prep);
        x64->op(opcode % os, RCX);
            
        if (!mod)
            return Storage(REGISTER, RAX);
        else {
            if (os == 0) {
                // Result in AH, get it into a sane register
                x64->op(SHRW, RAX, 8);
                return Storage(REGISTER, RAX);
            }
            else
                return Storage(REGISTER, RDX);
        }
    }

    virtual Storage binary_shift(X64 *x64, ShiftOp opcode) {
        subcompile(x64);

        // The register allocation gave us a register that looked promising as
        // a return value. But that we cannot exclude that it returned RCX,
        // which is inappropriate in this case, as it will be used for the
        // right hand side. So this is why we force allocated RAX, too.
        if (auxls.reg == RCX)
            auxls.reg = RAX;

        switch (ls.where * rs.where) {
        case CONSTANT_CONSTANT:
            return Storage(CONSTANT, opcode % 3 == SHLQ ? ls.value << rs.value : ls.value >> rs.value);
        case CONSTANT_REGISTER:
            x64->op(MOVQ % os, RCX, rs.reg);
            x64->op(MOVQ % os, auxls.reg, ls.value);
            x64->op(opcode % os, auxls.reg, CL);
            return auxls;
        case CONSTANT_MEMORY:
            x64->op(MOVQ % os, RCX, rs.address);
            x64->op(MOVQ % os, auxls.reg, ls.value);
            x64->op(opcode % os, auxls.reg, CL);
            return auxls;
        case REGISTER_CONSTANT:
            x64->op(opcode % os, ls.reg, rs.value);
            return Storage(REGISTER, ls.reg);
        case REGISTER_REGISTER:
            if (ls.reg == RCX) {
                x64->op(XCHGQ % os, ls.reg, rs.reg);
                x64->op(opcode % os, rs.reg, CL);
                return Storage(REGISTER, rs.reg);
            }
            else if (rs.reg != RCX) {
                x64->op(MOVQ % os, RCX, rs.reg);
                x64->op(opcode % os, ls.reg, CL);
                return Storage(REGISTER, ls.reg);
            }
            else {
                x64->op(opcode % os, ls.reg, CL);
                return Storage(REGISTER, ls.reg);
            }
        case REGISTER_MEMORY:
            if (ls.reg == RCX) {
                x64->op(MOVQ % os, auxls.reg, ls.reg);  // not RCX
                x64->op(MOVQ % os, RCX, rs.address);
                x64->op(opcode % os, auxls.reg, CL);
                return auxls;
            }
            else {
                x64->op(MOVQ % os, RCX, rs.address);
                x64->op(opcode % os, ls.reg, CL);
                return Storage(REGISTER, ls.reg);
            }
        case MEMORY_CONSTANT:
            x64->op(MOVQ % os, auxls.reg, ls.address);
            x64->op(opcode % os, auxls.reg, rs.value);
            return auxls;
        case MEMORY_REGISTER:
            if (rs.reg != RCX)
                x64->op(MOVQ % os, RCX, rs.reg);
                
            x64->op(MOVQ % os, auxls.reg, ls.address);  // not RCX
            x64->op(opcode % os, auxls.reg, CL);
            return auxls;
        case MEMORY_MEMORY:
            x64->op(MOVQ % os, auxls.reg, ls.address);  // not RCX
            x64->op(MOVQ % os, RCX, rs.address);
            x64->op(opcode % os, auxls.reg, CL);
            return auxls;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Storage binary_exponent(X64 *x64) {
        subcompile(x64);

        arrange_in_rax_rcx(x64);
        
        // base in RAX, exponent in RCX, result in RDX
        exponentiation_by_squaring(x64, RAX, RCX, RDX);

        return Storage(REGISTER, RDX);
    }

    virtual Storage binary_compare(X64 *x64, ConditionCode cc) {
        subcompile(x64);

        switch (ls.where * rs.where) {
        case CONSTANT_CONSTANT: {
            bool holds = (
                cc == CC_EQUAL ? ls.value == rs.value :
                cc == CC_NOT_EQUAL ? ls.value != rs.value :
                cc == CC_LESS || cc == CC_BELOW ? ls.value < rs.value :
                cc == CC_LESS_EQUAL || cc == CC_BELOW_EQUAL ? ls.value <= rs.value :
                cc == CC_GREATER || cc == CC_ABOVE ? ls.value > rs.value :
                cc == CC_GREATER_EQUAL || cc == CC_ABOVE_EQUAL ? ls.value >= rs.value :
                throw INTERNAL_ERROR
            );

            return Storage(CONSTANT, holds ? 1 : 0);
            }
        case CONSTANT_REGISTER:
            x64->op(CMPQ % os, rs.reg, ls.value);
            return Storage(FLAGS, swapped(cc));
        case CONSTANT_MEMORY:
            x64->op(CMPQ % os, rs.address, ls.value);
            return Storage(FLAGS, swapped(cc));
        case REGISTER_CONSTANT:
            x64->op(CMPQ % os, ls.reg, rs.value);
            return Storage(FLAGS, cc);
        case REGISTER_REGISTER:
            x64->op(CMPQ % os, ls.reg, rs.reg);
            return Storage(FLAGS, cc);
        case REGISTER_MEMORY:
            x64->op(CMPQ % os, ls.reg, rs.address);
            return Storage(FLAGS, cc);
        case MEMORY_CONSTANT:
            x64->op(CMPQ % os, ls.address, rs.value);
            return Storage(FLAGS, cc);
        case MEMORY_REGISTER:
            x64->op(CMPQ % os, ls.address, rs.reg);
            return Storage(FLAGS, cc);
        case MEMORY_MEMORY:
            x64->op(MOVQ % os, auxls.reg, ls.address);
            x64->op(CMPQ % os, auxls.reg, rs.address);
            return Storage(FLAGS, cc);
        default:
            throw INTERNAL_ERROR;
        }
    }

    // TODO: Hm... this seems a bit questionable...
    // NOTE: for lvalue operations, reg is allocated so that it can be used
    // for the left operand. Since it is MEMORY, it may be the base register of its
    // address, and it may have been chosen to restore a spilled dynamic address.
    // As such, unlike with rvalue operations, reg is not guaranteed to be
    // distinct from ls.address.base. Since these operations return the left address,
    // for a working register for values RBX must be used.
    
    virtual Storage assign_binary(X64 *x64, BinaryOp opcode) {
        subcompile(x64);
        
        if (ls.where != MEMORY)
            throw INTERNAL_ERROR;
            
        switch (rs.where) {
        case CONSTANT:
            x64->op(opcode % os, ls.address, rs.value);
            return ls;
        case REGISTER:
            x64->op(opcode % os, ls.address, rs.reg);
            return ls;
        case MEMORY:
            x64->op(MOVQ % os, RBX, rs.address);
            x64->op(opcode % os, ls.address, RBX);
            return ls;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Storage assign_multiply(X64 *x64) {
        subcompile(x64);

        if (ls.where != MEMORY)
            throw INTERNAL_ERROR;

        // Use IMULW for bytes, and ignore the upper byte.
        int ios = os == 0 ? 1 : os;

        switch (rs.where) {
        case CONSTANT:
            x64->op(IMUL3Q % ios, RBX, ls.address, rs.value);
            x64->op(MOVQ % os, ls.address, RBX);
            return ls;
        case REGISTER:
            x64->op(IMUL2Q % ios, rs.reg, ls.address);
            x64->op(MOVQ % os, ls.address, rs.reg);
            return ls;
        case MEMORY:
            x64->op(MOVQ % os, RBX, ls.address);
            x64->op(IMUL2Q % ios, RBX, rs.address);
            x64->op(MOVQ % os, ls.address, RBX);
            return ls;
        default:
            throw INTERNAL_ERROR;
        }

        return ls;
    }

    virtual void big_prearrange(X64 *x64) {
        // Get rs into a register, but out of RAX/RCX/RDX
        switch (rs.where) {
        case CONSTANT:
            x64->op(MOVQ % os, RBX, rs.value);
            rs = Storage(REGISTER, RBX);
            break;
        case REGISTER:
            if (rs.reg == RAX || rs.reg == RCX || rs.reg == RDX) {
                x64->op(MOVQ % os, RBX, rs.reg);
                rs = Storage(REGISTER, RBX);
            }
            break;
        case MEMORY:
            x64->op(MOVQ % os, RBX, rs.reg);
            rs = Storage(REGISTER, RBX);
            break;
        default:
            throw INTERNAL_ERROR;
        }
        
        // Get ls out of RAX/RDX
        if (ls.is_clobbered(Regs(RAX, RDX))) {
            x64->op(LEA, RCX, ls.address);
            ls = Storage(MEMORY, Address(RCX, 0));
        }
    }
    
    virtual Storage assign_divmod(X64 *x64, bool mod) {
        subcompile(x64);

        if (ls.where != MEMORY)
            throw INTERNAL_ERROR;
        
        big_prearrange(x64);

        x64->op(MOVQ % os, RAX, ls.address);

        x64->op(os == 0 ? CBW : os == 1 ? CWD : os == 2 ? CDQ : CQO);
        x64->op((is_unsigned ? DIVQ : IDIVQ) % os, rs.reg);

        if (mod && os == 0)
            x64->op(MOVB, DL, AH);  // Result in AH, get it into a sane register

        x64->op(MOVQ % os, ls.address, mod ? RDX : RAX);
        return ls;
    }

    virtual Storage assign_exponent(X64 *x64) {
        subcompile(x64);

        if (ls.where != MEMORY)
            throw INTERNAL_ERROR;
        
        big_prearrange(x64);

        x64->op(MOVQ % os, RAX, ls.address);

        exponentiation_by_squaring(x64, RAX, rs.reg, RDX);
        
        x64->op(MOVQ % os, ls.address, RDX);
        return ls;
    }

    virtual Storage assign_shift(X64 *x64, ShiftOp opcode) {
        subcompile(x64);
        
        if (ls.where != MEMORY)
            throw INTERNAL_ERROR;
            
        switch (rs.where) {
        case CONSTANT:
            x64->op(opcode % os, ls.address, rs.value);
            return ls;
        case REGISTER:
            if (ls.address.base != RCX) {
                x64->op(MOVB, CL, rs.reg);
                x64->op(opcode % os, ls.address, CL);
                return ls;
            }
            else {
                x64->op(XCHGQ, ls.address.base, rs.reg);
                x64->op(MOVB, CL, ls.address.base);
                ls.address.base = rs.reg;
                x64->op(opcode % os, ls.address, CL);
                return ls;
            }
        case MEMORY:
            if (ls.address.base != RCX) {
                x64->op(MOVB, CL, rs.address);
                x64->op(opcode % os, ls.address, CL);
                return ls;
            }
            else {
                x64->op(MOVQ, RBX, RCX);
                ls.address.base = RBX;
                x64->op(MOVB, CL, rs.address);
                x64->op(opcode % os, ls.address, CL);
                x64->op(MOVQ, RCX, RBX);
                ls.address.base = RCX;
                return ls;
            }
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = OptimizedOperationValue::precompile(preferred);
    
        switch (operation) {
        case DIVIDE:
        case ASSIGN_DIVIDE:
        case MODULO:
        case ASSIGN_MODULO:
        case EXPONENT:
        case ASSIGN_EXPONENT:
            clob = clob | RAX | RDX | RCX;
            break;
        case SHIFT_LEFT:
        case SHIFT_RIGHT:
            clob = clob | RAX | RCX;
            break;
        case ASSIGN_SHIFT_LEFT:
        case ASSIGN_SHIFT_RIGHT:
            clob = clob | RCX;
            break;
        default:
            break;
        }
        
        return clob;
    }
    
    virtual Storage compile(X64 *x64) {
        switch (operation) {
        case COMPLEMENT:
            return unary(x64, NOTQ);
        case NEGATE:
            return unary(x64, NEGQ);
        case ADD:
            return binary_simple(x64, ADDQ);
        case SUBTRACT:
            return binary_simple(x64, SUBQ);
        case MULTIPLY:
            return binary_multiply(x64);
        case DIVIDE:
            return binary_divmod(x64, false);
        case MODULO:
            return binary_divmod(x64, true);
        case OR:
            return binary_simple(x64, ORQ);
        case XOR:
            return binary_simple(x64, XORQ);
        case AND:
            return binary_simple(x64, ANDQ);
        case SHIFT_LEFT:
            return binary_shift(x64, SHLQ);
        case SHIFT_RIGHT:
            return binary_shift(x64, SHRQ);
        case EXPONENT:
            return binary_exponent(x64);
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
        case ASSIGN_OR:
            return assign_binary(x64, ORQ);
        case ASSIGN_XOR:
            return assign_binary(x64, XORQ);
        case ASSIGN_AND:
            return assign_binary(x64, ANDQ);
        case ASSIGN_SHIFT_LEFT:
            return assign_shift(x64, SHLQ);
        case ASSIGN_SHIFT_RIGHT:
            return assign_shift(x64, SHRQ);
        default:
            return OptimizedOperationValue::compile(x64);
        }
    }
};
