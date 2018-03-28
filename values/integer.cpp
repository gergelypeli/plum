
const int MAX_SIGNED_DWORD = 2147483647;

class IntegerOperationValue: public GenericOperationValue {
public:
    int os;
    bool is_unsigned;
    
    IntegerOperationValue(OperationType o, Value *pivot, TypeMatch &match)
        :GenericOperationValue(o, op_arg_ts(o, match), op_ret_ts(o, match), pivot) {
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
    
    virtual void exponentiation_by_squaring(X64 *x64) {
        // RDX = RAX ** [RCX]
        
        int xos = os == 0 ? 1 : os;  // No IMULB
        
        Label loop_label;
        Label skip_label;
        
        x64->op(MOVQ % os, RDX, 1);
        
        x64->code_label(loop_label);
        x64->op(TESTQ % os, RCX, 1);
        x64->op(JE, skip_label);
        x64->op(IMUL2Q % xos, RDX, RAX);
        
        x64->code_label(skip_label);
        x64->op(IMUL2Q % xos, RAX, RAX);
        x64->op(SHRQ % os, RCX, 1);
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
        
        switch (ls.where) {
        case CONSTANT: {
            int value = opcode % 3 == NEGQ ? -ls.value : ~ls.value;
            
            if (fits32(value))
                return Storage(CONSTANT, value);
                
            x64->op(MOVQ % os, reg, ls.value);
            x64->op(opcode % os, reg);
            return Storage(REGISTER, reg);
        }
        case REGISTER:
            x64->op(opcode % os, ls.reg);
            return Storage(REGISTER, ls.reg);
        case MEMORY:
            x64->op(MOVQ % os, reg, ls.address);
            x64->op(opcode % os, reg);
            return Storage(REGISTER, reg);
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
            
            x64->op(MOVQ % os, reg, ls.value);
            x64->op(opcode % os, reg, rs.value);
            return Storage(REGISTER, reg);
        }
        case CONSTANT_REGISTER:
            if (commutative) {
                x64->op(opcode % os, rs.reg, ls.value);
                return Storage(REGISTER, rs.reg);
            }
            else {
                x64->op(MOVQ % os, reg, ls.value);
                x64->op(opcode % os, reg, rs.reg);
                return Storage(REGISTER, reg);
            }
        case CONSTANT_MEMORY:
            x64->op(MOVQ % os, reg, ls.value);
            x64->op(opcode % os, reg, rs.address);
            return Storage(REGISTER, reg);
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
            x64->op(MOVQ % os, reg, ls.address);
            x64->op(opcode % os, reg, rs.value);
            return Storage(REGISTER, reg);
        case MEMORY_REGISTER:
            if (commutative) {
                x64->op(opcode % os, rs.reg, ls.address);
                return Storage(REGISTER, rs.reg);
            }
            else {
                x64->op(MOVQ % os, reg, ls.address);
                x64->op(opcode % os, reg, rs.reg);
                return Storage(REGISTER, reg);
            }
        case MEMORY_MEMORY:
            x64->op(MOVQ % os, reg, ls.address);
            x64->op(opcode % os, reg, rs.address);
            return Storage(REGISTER, reg);
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
                
            x64->op(MOVQ % os, reg, ls.value);
            x64->op(IMUL3Q % ios, reg, reg, rs.value);

            return Storage(REGISTER, reg);
        }
        case CONSTANT_REGISTER:
            x64->op(IMUL3Q % ios, rs.reg, rs.reg, ls.value);
            return Storage(REGISTER, rs.reg);
        case CONSTANT_MEMORY:
            x64->op(IMUL3Q % ios, reg, rs.address, ls.value);
            return Storage(REGISTER, reg);
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
            x64->op(IMUL3Q % ios, reg, ls.address, rs.value);
            return Storage(REGISTER, reg);
        case MEMORY_REGISTER:
            x64->op(IMUL2Q % ios, rs.reg, ls.address);
            return Storage(REGISTER, rs.reg);
        case MEMORY_MEMORY:
            x64->op(MOVQ % os, reg, ls.address);
            x64->op(IMUL2Q % ios, reg, rs.address);
            return Storage(REGISTER, reg);
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

    virtual Storage binary_divmod(X64 *x64, bool mod, bool save_lsaddr) {
        subcompile(x64);

        if (ls.where == CONSTANT && rs.where == CONSTANT)
            return Storage(CONSTANT, mod ? ls.value % rs.value : ls.value / rs.value);

        if (save_lsaddr && ls.where == MEMORY && ls.address.base != RBP)
            x64->op(PUSHQ, ls.address.base);
        
        arrange_in_rax_rcx(x64);
        
        SimpleOp prep = (os == 0 ? CBW : os == 1 ? CWD : os == 2 ? CDQ : CQO);
        UnaryOp opcode = (is_unsigned ? DIVQ : IDIVQ);

        x64->op(prep);
        x64->op(opcode % os, RCX);
            
        if (save_lsaddr && ls.where == MEMORY && ls.address.base != RBP) {
            x64->op(POPQ, RCX);
            ls.address.base = RCX;
        }
            
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
        if (reg == RCX)
            reg = RAX;

        switch (ls.where * rs.where) {
        case CONSTANT_CONSTANT:
            return Storage(CONSTANT, opcode % 3 == SHLQ ? ls.value << rs.value : ls.value >> rs.value);
        case CONSTANT_REGISTER:
            x64->op(MOVQ % os, RCX, rs.reg);
            x64->op(MOVQ % os, reg, ls.value);
            x64->op(opcode % os, reg, CL);
            return Storage(REGISTER, reg);
        case CONSTANT_MEMORY:
            x64->op(MOVQ % os, RCX, rs.address);
            x64->op(MOVQ % os, reg, ls.value);
            x64->op(opcode % os, reg, CL);
            return Storage(REGISTER, reg);
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
                x64->op(MOVQ % os, reg, ls.reg);
                x64->op(MOVQ % os, RCX, rs.address);
                x64->op(opcode % os, reg, CL);
                return Storage(REGISTER, reg);
            }
            else {
                x64->op(MOVQ % os, RCX, rs.address);
                x64->op(opcode % os, ls.reg, CL);
                return Storage(REGISTER, ls.reg);
            }
        case MEMORY_CONSTANT:
            x64->op(MOVQ % os, reg, ls.address);
            x64->op(opcode % os, reg, rs.value);
            return Storage(REGISTER, reg);
        case MEMORY_REGISTER:
            if (rs.reg != RCX)
                x64->op(MOVQ % os, RCX, rs.reg);
                
            x64->op(MOVQ % os, reg, ls.address);
            x64->op(opcode % os, reg, CL);
            return Storage(REGISTER, reg);
        case MEMORY_MEMORY:
            x64->op(MOVQ % os, reg, ls.address);
            x64->op(MOVQ % os, RCX, rs.address);
            x64->op(opcode % os, reg, CL);
            return Storage(REGISTER, reg);
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Storage binary_exponent(X64 *x64, bool save_lsaddr) {
        subcompile(x64);

        if (save_lsaddr && ls.where == MEMORY && ls.address.base != RBP)
            x64->op(PUSHQ, ls.address.base);

        arrange_in_rax_rcx(x64);
        
        // base in RAX, exponent in RCX, result in RDX
        exponentiation_by_squaring(x64);

        if (save_lsaddr && ls.where == MEMORY && ls.address.base != RBP) {
            x64->op(POPQ, RCX);
            ls.address.base = RCX;
        }
        
        return Storage(REGISTER, RDX);
    }

    virtual Storage binary_compare(X64 *x64, BitSetOp opcode) {
        subcompile(x64);

        switch (ls.where * rs.where) {
        case CONSTANT_CONSTANT: {
            bool holds = (
                opcode == SETE ? ls.value == rs.value :
                opcode == SETNE ? ls.value != rs.value :
                opcode == SETL || opcode == SETB ? ls.value < rs.value :
                opcode == SETLE || opcode == SETBE ? ls.value <= rs.value :
                opcode == SETG || opcode == SETA ? ls.value > rs.value :
                opcode == SETGE || opcode == SETAE ? ls.value >= rs.value :
                throw INTERNAL_ERROR
            );

            return Storage(CONSTANT, holds ? 1 : 0);
            }
        case CONSTANT_REGISTER:
            x64->op(CMPQ % os, rs.reg, ls.value);
            return Storage(FLAGS, negate_ordering(opcode));
        case CONSTANT_MEMORY:
            x64->op(CMPQ % os, rs.address, ls.value);
            return Storage(FLAGS, negate_ordering(opcode));
        case REGISTER_CONSTANT:
            x64->op(CMPQ % os, ls.reg, rs.value);
            return Storage(FLAGS, opcode);
        case REGISTER_REGISTER:
            x64->op(CMPQ % os, ls.reg, rs.reg);
            return Storage(FLAGS, opcode);
        case REGISTER_MEMORY:
            x64->op(CMPQ % os, ls.reg, rs.address);
            return Storage(FLAGS, opcode);
        case MEMORY_CONSTANT:
            x64->op(CMPQ % os, ls.address, rs.value);
            return Storage(FLAGS, opcode);
        case MEMORY_REGISTER:
            x64->op(CMPQ % os, ls.address, rs.reg);
            return Storage(FLAGS, opcode);
        case MEMORY_MEMORY:
            x64->op(MOVQ % os, reg, ls.address);
            x64->op(CMPQ % os, reg, rs.address);
            return Storage(FLAGS, opcode);
        default:
            throw INTERNAL_ERROR;
        }
    }

    // NOTE: for lvalue operations, reg is allocated so that it can be used
    // for the left operand. Since it is MEMORY, it may be the base register of its
    // address, and it may have been chosen to restore a spilled dynamic address.
    // As such, unlike with rvalue operations, reg is not guaranteed to be
    // distinct from ls.address.base. Since these operations return the left address,
    // for a working register for values RBX must be used.
    
    virtual Storage assign_binary(X64 *x64, BinaryOp opcode) {
        reg = RBX;
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
            x64->op(MOVQ % os, reg, rs.address);
            x64->op(opcode % os, ls.address, reg);
            return ls;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Storage assign_multiply(X64 *x64) {
        reg = RBX;
        Storage s = binary_multiply(x64);

        if (ls.where != MEMORY)
            throw INTERNAL_ERROR;

        if (s.where == REGISTER)
            x64->op(MOVQ % os, ls.address, s.reg);
        else
            throw INTERNAL_ERROR;
            
        return ls;
    }

    virtual Storage assign_divmod(X64 *x64, bool mod) {
        Storage s = binary_divmod(x64, mod, true);

        if (ls.where != MEMORY)
            throw INTERNAL_ERROR;
        
        if (s.where == REGISTER)
            x64->op(MOVQ % os, ls.address, s.reg);  // ls.address.base is either RBP or RCX
        else
            throw INTERNAL_ERROR;

        return ls;
    }

    virtual Storage assign_exponent(X64 *x64) {
        Storage s = binary_exponent(x64, true);

        if (ls.where != MEMORY)
            throw INTERNAL_ERROR;
        
        if (s.where == REGISTER)
            x64->op(MOVQ % os, ls.address, s.reg);  // ls.address.base is either RBP or RCX
        else
            throw INTERNAL_ERROR;

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
        Regs clob = GenericOperationValue::precompile(preferred);
    
        switch (operation) {
        case DIVIDE:
        case ASSIGN_DIVIDE:
        case MODULO:
        case ASSIGN_MODULO:
        case EXPONENT:
        case ASSIGN_EXPONENT:
            clob.add(RAX);
            clob.add(RDX);
            clob.add(RCX);
            break;
        case SHIFT_LEFT:
        case SHIFT_RIGHT:
            clob.add(RAX);
            clob.add(RCX);
            break;
        case ASSIGN_SHIFT_LEFT:
        case ASSIGN_SHIFT_RIGHT:
            clob.add(RCX);
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
            return binary_divmod(x64, false, false);
        case MODULO:
            return binary_divmod(x64, true, false);
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
            return binary_exponent(x64, false);
        case EQUAL:
            return binary_compare(x64, SETE);
        case NOT_EQUAL:
            return binary_compare(x64, SETNE);
        case LESS:
            return binary_compare(x64, is_unsigned ? SETB : SETL);
        case GREATER:
            return binary_compare(x64, is_unsigned ? SETA : SETG);
        case LESS_EQUAL:
            return binary_compare(x64, is_unsigned ? SETBE : SETLE);
        case GREATER_EQUAL:
            return binary_compare(x64, is_unsigned ? SETAE : SETGE);
        //case INCOMPARABLE:
        //    return Storage(CONSTANT, 0);
        //case ASSIGN:
        //    return assign_binary(x64, MOVQ);
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
            return GenericOperationValue::compile(x64);
        }
    }
};
