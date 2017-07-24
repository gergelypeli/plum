
const int MAX_SIGNED_DWORD = 2147483647;

class IntegerOperationValue: public GenericOperationValue {
public:
    int os;
    bool is_unsigned;
    
    IntegerOperationValue(NumericOperation o, TypeSpec t, Value *pivot)
        :GenericOperationValue(o, t.rvalue(), is_comparison(o) ? BOOLEAN_TS : t, pivot) {
        int size = t.measure();
        os = (
            size == 1 ? 0 :
            size == 2 ? 1 :
            size == 4 ? 2 :
            size == 8 ? 3 :
            throw INTERNAL_ERROR
        );

        if (operation != ASSIGN && operation != EQUAL && operation != NOT_EQUAL)
            is_unsigned = arg_ts.is_unsigned();
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
        bool commutative = (opcode % 3) != SUBQ && (opcode % 3) != CMPQ;

        subcompile(x64);

        switch (ls.where * rs.where) {
        case CONSTANT_CONSTANT: {
            int value = (
                (opcode % 3) == ADDQ ? ls.value + rs.value :
                (opcode % 3) == SUBQ ? ls.value - rs.value :
                (opcode % 3) == ANDQ ? ls.value & rs.value :
                (opcode % 3) == ORQ  ? ls.value | rs.value :
                (opcode % 3) == XORQ ? ls.value ^ rs.value :
                (opcode % 3) == CMPQ ? ls.value - rs.value :  // kinda special
                throw X64_ERROR
            );
            
            if (fits32(value) || opcode % 3 == CMPQ)
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
            //if (swap) *swap = true;
            //return Storage(REGISTER, rs.reg);
        case CONSTANT_MEMORY:
            x64->op(MOVQ % os, reg, ls.value);
            x64->op(opcode % os, reg, rs.address);  // may be shorter for CMP
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
            x64->op(opcode % os, reg, rs.value);  // may be shorter for CMP
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
        int xos = os == 0 ? 1 : os;

        switch (ls.where * rs.where) {
        case CONSTANT_CONSTANT: {
            int value = ls.value * rs.value;
            
            if (fits32(value))
                return Storage(CONSTANT, value);
                
            x64->op(MOVQ % os, reg, ls.value);
            x64->op(IMUL3Q % xos, reg, reg, rs.value);

            return Storage(REGISTER, reg);
        }
        case CONSTANT_REGISTER:
            x64->op(IMUL3Q % xos, rs.reg, rs.reg, ls.value);
            return Storage(REGISTER, rs.reg);
        case CONSTANT_MEMORY:
            x64->op(IMUL3Q % xos, reg, rs.address, ls.value);
            return Storage(REGISTER, reg);
        case REGISTER_CONSTANT:
            x64->op(IMUL3Q % xos, ls.reg, ls.reg, rs.value);
            return Storage(REGISTER, ls.reg);
        case REGISTER_REGISTER:
            x64->op(IMUL2Q % xos, ls.reg, rs.reg);
            return Storage(REGISTER, ls.reg);
        case REGISTER_MEMORY:
            x64->op(IMUL2Q % xos, ls.reg, rs.address);
            return Storage(REGISTER, ls.reg);
        case MEMORY_CONSTANT:
            x64->op(IMUL3Q % xos, reg, ls.address, rs.value);
            return Storage(REGISTER, reg);
        case MEMORY_REGISTER:
            x64->op(IMUL2Q % xos, rs.reg, ls.address);
            return Storage(REGISTER, rs.reg);
        case MEMORY_MEMORY:
            x64->op(MOVQ % os, reg, ls.address);
            x64->op(IMUL2Q % xos, reg, rs.address);
            return Storage(REGISTER, reg);
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual void arrange_in_rax_rcx(X64 *x64) {
        switch (ls.where * rs.where) {
        case CONSTANT_CONSTANT:
            x64->op(MOVQ % os, RAX, ls.value);
            x64->op(MOVQ % os, RCX, rs.value);
            break;
        case CONSTANT_REGISTER:
            x64->op(MOVQ % os, RCX, rs.reg);
            x64->op(MOVQ % os, RAX, ls.value);
            break;
        case CONSTANT_MEMORY:
            x64->op(MOVQ % os, RAX, ls.value);
            x64->op(MOVQ % os, RCX, rs.address);
            break;
        case REGISTER_CONSTANT:
            x64->op(MOVQ % os, RAX, ls.reg);
            x64->op(MOVQ % os, RCX, rs.value);
            break;
        case REGISTER_REGISTER:
            if (ls.reg == RAX) {
                if (rs.reg != RCX)
                    x64->op(MOVQ % os, RCX, rs.reg);
            }
            else if (rs.reg == RCX) {
                x64->op(MOVQ % os, RAX, ls.reg);
            }
            else if (ls.reg == RCX) {
                if (rs.reg == RAX)
                    x64->op(XCHGQ % os, RAX, RCX);
                else {
                    x64->op(MOVQ % os, RAX, ls.reg);
                    x64->op(MOVQ % os, RCX, rs.reg);
                }
            }
            else {
                x64->op(MOVQ % os, RCX, rs.reg);
                x64->op(MOVQ % os, RAX, ls.reg);
            }
            break;
        case REGISTER_MEMORY:
            x64->op(MOVQ % os, RAX, ls.reg);
            x64->op(MOVQ % os, RCX, rs.address);
            break;
        case MEMORY_CONSTANT:
            x64->op(MOVQ % os, RAX, ls.address);
            x64->op(MOVQ % os, RCX, rs.value);
            break;
        case MEMORY_REGISTER:
            x64->op(MOVQ % os, RCX, rs.reg);
            x64->op(MOVQ % os, RAX, ls.address);
            break;
        case MEMORY_MEMORY:
            x64->op(MOVQ % os, RAX, ls.address);
            x64->op(MOVQ % os, RCX, rs.address);
            break;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Storage binary_divmod(X64 *x64, bool mod, Address *lsaddr = NULL) {
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

    virtual Storage binary_exponent(X64 *x64, Address *lsaddr = NULL) {
        subcompile(x64);
        
        arrange_in_rax_rcx(x64);
        
        // base in RAX, exponent in RCX, result in RDX
        exponentiation_by_squaring(x64);
        
        return Storage(REGISTER, RDX);
    }

    virtual Storage binary_compare(X64 *x64, BitSetOp opcode) {
        Storage s = binary_simple(x64, CMPQ);
        
        // Our constants are always 32-bit, so the result of the subtraction
        // that CMP did fits in the 64-bit signed int value properly,
        // which we'll check, but not pass on.
        if (s.where == CONSTANT) {
            bool holds = (
                opcode == SETE ? s.value == 0 :
                opcode == SETNE ? s.value != 0 :
                opcode == SETL || opcode == SETB ? s.value < 0 :
                opcode == SETLE || opcode == SETBE ? s.value <= 0 :
                opcode == SETG || opcode == SETA ? s.value > 0 :
                opcode == SETGE || opcode == SETAE ? s.value >= 0 :
                throw INTERNAL_ERROR
            );

            return Storage(CONSTANT, holds ? 1 : 0);
        }
        else if (s.where == REGISTER) {
            // Actually, if the opcode was CMP, then the results are not really in
            // a register, but only in the flags. That's even better for us.
            return Storage(FLAGS, opcode);
        }
        else
            throw INTERNAL_ERROR;
    }

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
            x64->op(MOVQ % os, reg, rs.address);
            x64->op(opcode % os, ls.address, reg);
            return ls;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Storage assign_multiply(X64 *x64) {
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
        Storage s = binary_divmod(x64, mod);

        if (ls.where != MEMORY)
            throw INTERNAL_ERROR;
        
        if (s.where == REGISTER)
            x64->op(MOVQ % os, ls.address, s.reg);
        else
            throw INTERNAL_ERROR;

        return ls;
    }

    virtual Storage assign_exponent(X64 *x64) {
        Storage s = binary_exponent(x64);

        if (ls.where != MEMORY)
            throw INTERNAL_ERROR;
        
        if (s.where == REGISTER)
            x64->op(MOVQ % os, ls.address, s.reg);
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
            x64->op(MOVQ % os, RCX, rs.reg);
            x64->op(opcode % os, ls.address, CL);
            return ls;
        case MEMORY:
            x64->op(MOVQ % os, RCX, rs.address);
            x64->op(opcode % os, ls.address, CL);
            return ls;
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
        case INCOMPARABLE:
            return Storage(CONSTANT, 0);
        case ASSIGN:
            return assign_binary(x64, MOVQ);
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
            std::cerr << "Unknown integer operator!\n";
            throw INTERNAL_ERROR;
        }
    }
};

