
const int MAX_SIGNED_DWORD = 2147483647;

class IntegerOperationValue: public Value {
public:
    NumericOperation operation;
    TypeSpec arg_ts;
    std::unique_ptr<Value> left, right;
    int os;
    bool is_unsigned;
    Register reg;
    Regs rclob;
    
    IntegerOperationValue(NumericOperation o, TypeSpec t, Value *pivot)
        :Value(is_comparison(o) ? BOOLEAN_TS : t) {
        operation = o;
        arg_ts = t.rvalue();
        left.reset(pivot);
        
        int size = t.measure();
        os = (
            size == 1 ? 0 :
            size == 2 ? 1 :
            size == 4 ? 2 :
            size == 8 ? 3 :
            throw INTERNAL_ERROR
        );
        
        Type *type = arg_ts[0];
        is_unsigned = (
            type == unsigned_integer_type ||
            type == unsigned_integer32_type ||
            type == unsigned_integer16_type ||
            type == unsigned_integer8_type
        );
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (is_unary(operation)) {
            if (args.size() != 0 || kwargs.size() != 0) {
                std::cerr << "Whacky integer unary operation!\n";
                return false;
            }
            
            return true;
        }
        else {
            if (args.size() != 1 || kwargs.size() != 0) {
                std::cerr << "Whacky integer binary operation!\n";
                return false;
            }

            Value *r = typize(args[0].get(), scope);
            Value *cr = convertible(arg_ts, r);
        
            if (!cr) {
                std::cerr << "Incompatible right argument to integer binary operation!\n";
                std::cerr << "Type " << r->ts << " is not " << arg_ts << "!\n";
                return false;
            }
        
            right.reset(cr);
            return true;
        }
    }

    virtual void exponentiation_by_squaring(X64 *x64) {
        // RAX = RDX ** [RBX]
        
        int xos = os == 0 ? 1 : os;  // No IMULB
        
        Label loop_label;
        Label skip_label;
        
        x64->op(MOVQ % os, RAX, 1);
        
        x64->code_label(loop_label);
        x64->op(TESTQ % os, RBX, 1);
        x64->op(JE, skip_label);
        x64->op(IMUL2Q % xos, RAX, RDX);
        
        x64->code_label(skip_label);
        x64->op(IMUL2Q % xos, RDX, RDX);
        x64->op(SHRQ % os, RBX, 1);
        x64->op(JNE, loop_label);
    }

    virtual bool fits8(int value) {
        return value >= -128 && value <= 127;
    }

    virtual bool fits(int value) {
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

    virtual Storage left_compile(X64 *x64) {
        Storage ls = left->compile(x64);
        
        if (ls.is_clobbered(rclob)) {
            Storage s(STACK);
            left->ts.store(ls, s, x64);
            return s;
        }
        else if (ls.is_dangling(rclob)) {
            Storage s(REGISTER, ls.address.base);
            left->ts.store(ls, s, x64);
            return s;
        }
        
        return ls;
    }

    virtual Storage right_compile(X64 *x64) {
        return right->compile(x64);
    }

    virtual Storage unary(X64 *x64, UnaryOp opcode) {
        Storage ls = left_compile(x64);
        
        switch (ls.where) {
        case CONSTANT: {
            int value = opcode % 3 == NEGQ ? -ls.value : ~ls.value;
            
            if (fits(value))
                return Storage(CONSTANT, value);
                
            x64->op(MOVQ % os, reg, ls.value);
            x64->op(opcode % os, reg);
            return Storage(REGISTER, reg);
        }
        case REGISTER:
            x64->op(opcode % os, ls.reg);
            return Storage(REGISTER, ls.reg);
        case STACK:
            x64->op(POPQ, reg);
            x64->op(opcode % os, reg);
            return Storage(REGISTER, reg);
        case MEMORY:
            x64->op(MOVQ % os, reg, ls.address);
            x64->op(opcode % os, reg);
            return Storage(REGISTER, reg);
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Storage binary_simple(X64 *x64, BinaryOp opcode, bool *swap = NULL) {
        // We swap the operands in some cases, which is not healthy for subtractions
        // and comparisons.

        Storage ls = left_compile(x64);
        Storage rs = right_compile(x64);

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
            
            if (fits(value) || opcode % 3 == CMPQ)
                return Storage(CONSTANT, value);
            
            x64->op(MOVQ % os, reg, ls.value);
            x64->op(opcode % os, reg, rs.value);
            return Storage(REGISTER, reg);
        }
        case CONSTANT_REGISTER:
            x64->op(opcode % os, rs.reg, ls.value);
            if (swap) *swap = true;
            return Storage(REGISTER, rs.reg);
        case CONSTANT_STACK:
            x64->op(POPQ, reg);
            x64->op(opcode % os, reg, ls.value);
            if (swap) *swap = true;
            return Storage(REGISTER, reg);
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
        case REGISTER_STACK:
            x64->op(opcode % os, ls.reg, Address(RSP, 0));
            x64->op(ADDQ, RSP, 8);
            return Storage(REGISTER, ls.reg);
        case REGISTER_MEMORY:
            x64->op(opcode % os, ls.reg, rs.address);
            return Storage(REGISTER, ls.reg);
        case STACK_CONSTANT:
            x64->op(POPQ, reg);
            x64->op(opcode % os, reg, rs.value);
            return Storage(REGISTER, reg);
        case STACK_REGISTER:
            x64->op(opcode % os, rs.reg, Address(RSP, 0));
            x64->op(ADDQ, RSP, 8);
            if (swap) *swap = true;
            return Storage(REGISTER, rs.reg);
        case STACK_STACK:
            x64->op(MOVQ % os, reg, Address(RSP, 8));
            x64->op(opcode % os, reg, Address(RSP, 0));
            x64->op(ADDQ, RSP, 16);
            return Storage(REGISTER, reg);
        case STACK_MEMORY:
            x64->op(POPQ, reg);
            x64->op(opcode % os, reg, rs.address);
            return Storage(REGISTER, reg);
        case MEMORY_CONSTANT:
            x64->op(MOVQ % os, reg, ls.address);
            x64->op(opcode % os, reg, rs.value);  // may be shorter for CMP
            return Storage(REGISTER, reg);
        case MEMORY_REGISTER:
            x64->op(opcode % os, rs.reg, ls.address);
            if (swap) *swap = true;
            return Storage(REGISTER, rs.reg);
        case MEMORY_STACK:
            x64->op(POPQ, reg);
            x64->op(opcode % os, reg, ls.address);
            if (swap) *swap = true;
            return Storage(REGISTER, reg);
        case MEMORY_MEMORY:
            x64->op(MOVQ % os, reg, ls.address);
            x64->op(opcode % os, reg, rs.address);
            return Storage(REGISTER, reg);
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Storage binary_subtract(X64 *x64) {
        bool swap = false;
        Storage s = binary_simple(x64, SUBQ % os, &swap);

        if (swap) {
            if (s.where == REGISTER)
                x64->op(NEGQ % os, s.reg);
            else
                throw INTERNAL_ERROR;
        }
            
        return s;
    }

    virtual Storage binary_multiply(X64 *x64, Address *lsaddr = NULL) {
        Storage ls = left_compile(x64);
        
        if (lsaddr)
            *lsaddr = ls.address;
            
        Storage rs = right_compile(x64);

        // Yay, IMUL2 and IMUL3 truncates the result, so it can be used for
        // both signed and unsigned multiplication.
        // Use IMULW for bytes, and ignore the upper byte.
        int xos = os == 0 ? 1 : os;

        switch (ls.where * rs.where) {
        case CONSTANT_CONSTANT: {
            int value = ls.value * rs.value;
            
            if (fits(value))
                return Storage(CONSTANT, value);
                
            x64->op(MOVQ % os, reg, ls.value);

            if (fits8(rs.value))
                x64->op(IMUL3Q % xos, reg, reg, rs.value);
            else {
                x64->op(PUSHQ, rs.value);
                x64->op(IMUL2Q % xos, reg, Address(RSP, 0));
                x64->op(ADDQ, RSP, 8);
            }

            return Storage(REGISTER, reg);
        }
        case CONSTANT_REGISTER:
            if (fits8(ls.value))
                x64->op(IMUL3Q % xos, rs.reg, rs.reg, ls.value);
            else {
                x64->op(PUSHQ, ls.value);
                x64->op(IMUL2Q % xos, rs.reg, Address(RSP, 0));
                x64->op(ADDQ, RSP, 8);
            }
            return Storage(REGISTER, rs.reg);
        case CONSTANT_STACK:
            if (fits8(ls.value)) {
                x64->op(POPQ, reg);
                x64->op(IMUL3Q % xos, reg, reg, ls.value);
            }
            else {
                x64->op(MOVQ % os, reg, ls.value);
                x64->op(IMUL2Q % xos, reg, Address(RSP, 0));
                x64->op(ADDQ, RSP, 8);
            }
            return Storage(REGISTER, reg);
        case CONSTANT_MEMORY:
            if (fits8(ls.value))
                x64->op(IMUL3Q % xos, reg, rs.address, ls.value);
            else {
                x64->op(MOVQ % os, reg, ls.value);
                x64->op(IMUL2Q % xos, reg, rs.address);
            }
            return Storage(REGISTER, reg);
        case REGISTER_CONSTANT:
            if (fits8(rs.value))
                x64->op(IMUL3Q % xos, ls.reg, ls.reg, rs.value);
            else {
                x64->op(PUSHQ, rs.value);
                x64->op(IMUL2Q % xos, ls.reg, Address(RSP, 0));
                x64->op(ADDQ, RSP, 8);
            }
            return Storage(REGISTER, ls.reg);
        case REGISTER_REGISTER:
            x64->op(IMUL2Q % xos, ls.reg, rs.reg);
            return Storage(REGISTER, ls.reg);
        case REGISTER_STACK:
            x64->op(IMUL2Q % xos, ls.reg, Address(RSP, 0));
            x64->op(ADDQ, RSP, 8);
            return Storage(REGISTER, ls.reg);
        case REGISTER_MEMORY:
            x64->op(IMUL2Q % xos, ls.reg, rs.address);
            return Storage(REGISTER, ls.reg);
        case STACK_CONSTANT:
            if (fits8(rs.value)) {
                x64->op(POPQ, reg);
                x64->op(IMUL3Q % xos, reg, reg, rs.value);
            }
            else {
                x64->op(MOVQ % os, reg, rs.value);
                x64->op(IMUL2Q % xos, reg, Address(RSP, 0));
                x64->op(ADDQ, RSP, 8);
            }
            return Storage(REGISTER, reg);
        case STACK_REGISTER:
            x64->op(IMUL2Q % xos, rs.reg, Address(RSP, 0));
            x64->op(ADDQ, RSP, 8);
            return Storage(REGISTER, rs.reg);
        case STACK_STACK:
            x64->op(POPQ, reg);
            x64->op(IMUL2Q % xos, reg, Address(RSP, 0));
            x64->op(ADDQ, RSP, 8);
            return Storage(REGISTER, reg);
        case STACK_MEMORY:
            x64->op(POPQ, reg);
            x64->op(IMUL2Q % xos, reg, rs.address);
            return Storage(REGISTER, reg);
        case MEMORY_CONSTANT:
            if (fits8(rs.value))
                x64->op(IMUL3Q % xos, reg, ls.address, rs.value);
            else {
                x64->op(MOVQ % os, reg, rs.value);
                x64->op(IMUL2Q % xos, reg, ls.address);
            }
            return Storage(REGISTER, reg);
        case MEMORY_REGISTER:
            x64->op(IMUL2Q % xos, rs.reg, ls.address);
            return Storage(REGISTER, rs.reg);
        case MEMORY_STACK:
            x64->op(POPQ, reg);
            x64->op(IMUL2Q % xos, reg, ls.address);
            return Storage(REGISTER, reg);
        case MEMORY_MEMORY:
            x64->op(MOVQ % os, reg, ls.address);
            x64->op(IMUL2Q % xos, reg, rs.address);
            return Storage(REGISTER, reg);
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Storage binary_divmod(X64 *x64, bool mod, Address *lsaddr = NULL) {
        Storage ls = left_compile(x64);

        if (ls.where == MEMORY && lsaddr)
            *lsaddr = ls.address;

        Storage rs = right_compile(x64);
        
        // Put right into RBX or MEMORY (left cannot be in RBX)
        switch (rs.where) {
        case CONSTANT:
            x64->op(MOVQ % os, RBX, rs.value);
            break;
        case REGISTER:
            x64->op(MOVQ % os, RBX, rs.reg);
            break;
        case STACK:
            x64->op(POPQ, RBX);
            break;
        case MEMORY:
            break;
        default:
            throw INTERNAL_ERROR;
        }
        
        // Put left into RAX
        switch (ls.where) {
        case CONSTANT:
            x64->op(MOVQ % os, RAX, ls.value);
            break;
        case REGISTER:
            x64->op(MOVQ % os, RAX, ls.reg);
            break;
        case STACK:
            x64->op(POPQ, RAX);
            break;
        case MEMORY:
            x64->op(MOVQ % os, RAX, ls.address);
            break;
        default:
            throw INTERNAL_ERROR;
        }
        
        SimpleOp prep = (os == 0 ? CBW : os == 1 ? CWD : os == 2 ? CDQ : CQO);
        UnaryOp opcode = (is_unsigned ? DIVQ : IDIVQ);

        x64->op(prep);
        
        if (rs.where != MEMORY)
            x64->op(opcode % os, RBX);
        else
            x64->op(opcode % os, rs.address);
            
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
        Storage ls = left_compile(x64);
        
        Storage rs = right_compile(x64);

        // Move right to RCX or CONSTANT (left cannot be in RCX)
        switch (rs.where) {
        case CONSTANT:
            break;
        case REGISTER:
            x64->op(MOVQ % os, RCX, rs.reg);
            break;
        case STACK:
            x64->op(POPQ, RCX);
            break;
        case MEMORY:
            x64->op(MOVQ % os, RCX, rs.address);
            break;
        default:
            throw INTERNAL_ERROR;
        }
        
        // Move left to RBX
        switch (ls.where) {
        case CONSTANT:
            x64->op(MOVQ % os, RBX, ls.value);
            break;
        case REGISTER:
            x64->op(MOVQ % os, RBX, ls.reg);
            break;
        case STACK:
            x64->op(POPQ, RBX);
            break;
        case MEMORY:
            x64->op(MOVQ % os, RBX, ls.address);
            break;
        default:
            throw INTERNAL_ERROR;
        }

        if (rs.where == CONSTANT)
            x64->op(opcode % os, RBX, rs.value);
        else
            x64->op(opcode % os, RBX, CL);

        return Storage(REGISTER, RBX);
    }

    virtual Storage binary_exponent(X64 *x64, Address *lsaddr = NULL) {
        Storage ls = left_compile(x64);
        
        if (ls.where == MEMORY && lsaddr)
            *lsaddr = ls.address;
            
        Storage rs = right_compile(x64);

        // Put right into RBX (left cannot be in RBX)
        switch (rs.where) {
        case CONSTANT:
            x64->op(MOVQ % os, RBX, rs.value);
            break;
        case REGISTER:
            x64->op(MOVQ % os, RBX, rs.reg);
            break;
        case STACK:
            x64->op(POPQ, RBX);
            break;
        case MEMORY:
            x64->op(MOVQ % os, RBX, rs.address);
            break;
        default:
            throw INTERNAL_ERROR;
        }

        // Move left into RDX
        switch (ls.where) {
        case CONSTANT:
            x64->op(MOVQ % os, RDX, ls.value);
            break;
        case REGISTER:
            x64->op(MOVQ % os, RDX, ls.reg);
            break;
        case STACK:
            x64->op(POPQ, RDX);
            break;
        case MEMORY:
            x64->op(MOVQ % os, RDX, ls.address);
            break;
        default:
            throw INTERNAL_ERROR;
        }
        
        // base in RDX, exponent in RBX, result in RAX
        exponentiation_by_squaring(x64);
        
        return Storage(REGISTER, RAX);
    }

    virtual Storage binary_compare(X64 *x64, BitSetOp opcode) {
        bool swap = false;
        Storage s = binary_simple(x64, CMPQ, &swap);
        
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
            // But must negate the condition if the arguments were swapped
            if (swap)
                opcode = negate(opcode);
                
            //x64->op(opcode, s.reg);
            return Storage(FLAGS, opcode);
        }
        else
            throw INTERNAL_ERROR;
    }

    virtual Storage assign_binary(X64 *x64, BinaryOp opcode) {
        Storage ls = left_compile(x64);
        
        if (ls.where != MEMORY)
            throw INTERNAL_ERROR;
            
        Storage rs = right_compile(x64);
        
        switch (rs.where) {
        case CONSTANT:
            x64->op(opcode % os, ls.address, rs.value);
            return ls;
        case REGISTER:
            x64->op(opcode % os, ls.address, rs.reg);
            return ls;
        case STACK:
            if (opcode == MOVQ)
                x64->op(POPQ, ls.address);
            else {
                x64->op(POPQ, reg);
                x64->op(opcode % os, ls.address, reg);
            }
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
        Address lsaddr;
        Storage s = binary_multiply(x64, &lsaddr);

        if (s.where == REGISTER)
            x64->op(MOVQ % os, lsaddr, s.reg);
        else
            throw INTERNAL_ERROR;
            
        return Storage(MEMORY, lsaddr);
    }

    virtual Storage assign_divmod(X64 *x64, bool mod) {
        Address lsaddr;
        Storage s = binary_divmod(x64, mod, &lsaddr);
        
        if (s.where == REGISTER)
            x64->op(MOVQ % os, lsaddr, s.reg);
        else
            throw INTERNAL_ERROR;

        return Storage(MEMORY, lsaddr);
    }

    virtual Storage assign_exponent(X64 *x64) {
        Address lsaddr;
        Storage s = binary_exponent(x64, &lsaddr);
        
        if (s.where == REGISTER)
            x64->op(MOVQ % os, lsaddr, s.reg);
        else
            throw INTERNAL_ERROR;

        return Storage(MEMORY, lsaddr);
    }

    virtual Storage assign_shift(X64 *x64, ShiftOp opcode) {
        Storage ls = left_compile(x64);
        
        if (ls.where != MEMORY)
            throw INTERNAL_ERROR;
            
        Storage rs = right_compile(x64);
        
        switch (rs.where) {
        case CONSTANT:
            x64->op(opcode % os, ls.address, rs.value);
            return ls;
        case REGISTER:
            x64->op(MOVQ % os, RCX, rs.reg);
            x64->op(opcode % os, ls.address, CL);
            return ls;
        case STACK:
            x64->op(POPQ, RCX);
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
        rclob = right ? right->precompile() : Regs();
        Regs lclob = left->precompile(preferred.clobbered(rclob));
        Regs xclob = Regs();
    
        switch (operation) {
        case DIVIDE:
        case ASSIGN_DIVIDE:
        case MODULO:
        case ASSIGN_MODULO:
        case EXPONENT:
        case ASSIGN_EXPONENT:
            xclob.add(RAX);
            xclob.add(RDX);
            xclob.add(RBX);   // will use RBX, too
            rclob.add(RBX);  // left value should be pushed out of RBX
            break;
        case SHIFT_LEFT:
        case SHIFT_RIGHT:
            xclob.add(RCX);
            xclob.add(RBX);   // will use RBX, too
            rclob.add(RCX);  // left value should be pushed out of RCX
            break;
        case ASSIGN_SHIFT_LEFT:
        case ASSIGN_SHIFT_RIGHT:
            xclob.add(RCX);
            break;
        default:
            // If the left value didn't need a register, we get one for ourselves
            if (!lclob) {
                reg = preferred.get_any();
                xclob.add(reg);
            }
        }

        return rclob | lclob | xclob;
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
            return binary_subtract(x64);
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

