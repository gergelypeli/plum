
class IntegerOperationValue: public Value {
public:
    NumericOperation operation;
    TypeSpec arg_ts;
    std::unique_ptr<Value> left, right;
    
    IntegerOperationValue(NumericOperation o, TypeSpec t, Value *pivot)
        :Value(is_comparison(o) ? BOOLEAN_TS : t) {
        operation = o;
        arg_ts = rvalue(t);
        left.reset(pivot);
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
        
            if (!(r->ts >> arg_ts)) {
                std::cerr << "Incompatible right argument to integer binary operation!\n";
                std::cerr << "Type " << r->ts << " is not " << arg_ts << "!\n";
                return false;
            }
        
            right.reset(r);
            return true;
        }
    }

    virtual void exponentiation_by_squaring(X64 *x64) {
        // RAX = RDX ** [RSP]
        Label loop_label;
        Label skip_label;
        loop_label.allocate();
        skip_label.allocate();
        
        x64->op(MOVQ, RAX, 1);
        
        x64->code_label(loop_label);
        x64->op(TESTQ, Address(RSP, 0), 1);
        x64->op(JE, skip_label);
        x64->op(IMUL2Q, RAX, RDX);
        
        x64->code_label(skip_label);
        x64->op(IMUL2Q, RDX, RDX);
        x64->op(SHRQ, Address(RSP, 0), 1);
        x64->op(JNE, loop_label);
    }

    virtual Storage unary(X64 *x64, Regs regs, UnaryOp opcode) {
        Storage ls = left->compile(x64, regs);
        Register reg = ls.where != REGISTER ? regs.get_any() : NOREG;
        
        switch (ls.where) {
        case CONSTANT: {
            int value = (opcode | 3) == NEGQ ? -ls.value : ~ls.value;
            return Storage(CONSTANT, value);
        }
        case REGISTER:
            x64->op(opcode, ls.reg);
            return Storage(REGISTER, ls.reg);
        case STACK:
            x64->op(POPQ, reg);
            x64->op(opcode, reg);
            return Storage(REGISTER, reg);
        case MEMORY:
            x64->op(MOVQ, reg, ls.address);
            x64->op(opcode, reg);
            return Storage(REGISTER, reg);
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Storage binary_simple(X64 *x64, Regs regs, BinaryOp opcode, bool *swap = NULL) {
        // We swap the operands in some cases, which is not healthy for subtractions
        // and comparisons.

        Storage ls = left->compile(x64, regs);
        
        if (ls.where == REGISTER)
            regs.remove(ls.reg);
            
        Storage rs = right->compile(x64, regs);

        // We always have an available register if neither side took it
        Register reg = (ls.where != REGISTER && rs.where != REGISTER) ? regs.get_any() : NOREG;
        
        switch (ls.where * rs.where) {
        case CONSTANT_CONSTANT: {
            int value = (
                (opcode | 3) == ADDQ ? ls.value + rs.value :
                (opcode | 3) == SUBQ ? ls.value - rs.value :
                (opcode | 3) == ANDQ ? ls.value & rs.value :
                (opcode | 3) == ORQ  ? ls.value | rs.value :
                (opcode | 3) == XORQ ? ls.value ^ rs.value :
                (opcode | 3) == CMPQ ? ls.value - rs.value :  // kinda special
                throw X64_ERROR
            );
            return Storage(CONSTANT, value);
        }
        case CONSTANT_REGISTER:
            x64->op(opcode, rs.reg, ls.value);
            if (swap) *swap = true;
            return Storage(REGISTER, rs.reg);
        case CONSTANT_STACK:
            x64->op(POPQ, reg);
            x64->op(opcode, reg, ls.value);
            if (swap) *swap = true;
            return Storage(REGISTER, reg);
        case CONSTANT_MEMORY:
            x64->op(MOVQ, reg, ls.value);
            x64->op(opcode, reg, rs.address);  // may be shorter for CMP
            return Storage(REGISTER, reg);
        case REGISTER_CONSTANT:
            x64->op(opcode, ls.reg, rs.value);
            return Storage(REGISTER, ls.reg);
        case REGISTER_REGISTER:
            x64->op(opcode, ls.reg, rs.reg);
            return Storage(REGISTER, ls.reg);
        case REGISTER_STACK:
            x64->op(opcode, ls.reg, Address(RSP, 0));
            x64->op(ADDQ, RSP, 8);
            return Storage(REGISTER, ls.reg);
        case REGISTER_MEMORY:
            x64->op(opcode, ls.reg, rs.address);
            return Storage(REGISTER, ls.reg);
        case STACK_CONSTANT:
            x64->op(POPQ, reg);
            x64->op(opcode, reg, rs.value);
            return Storage(REGISTER, reg);
        case STACK_REGISTER:
            x64->op(opcode, rs.reg, Address(RSP, 0));
            x64->op(ADDQ, RSP, 8);
            if (swap) *swap = true;
            return Storage(REGISTER, rs.reg);
        case STACK_STACK:
            x64->op(MOVQ, reg, Address(RSP, 8));
            x64->op(opcode, reg, Address(RSP, 0));
            x64->op(ADDQ, RSP, 16);
            return Storage(REGISTER, reg);
        case STACK_MEMORY:
            x64->op(POPQ, reg);
            x64->op(opcode, reg, rs.address);
            return Storage(REGISTER, reg);
        case MEMORY_CONSTANT:
            x64->op(MOVQ, reg, ls.address);
            x64->op(opcode, reg, rs.value);  // may be shorter for CMP
            return Storage(REGISTER, reg);
        case MEMORY_REGISTER:
            x64->op(opcode, rs.reg, ls.address);
            if (swap) *swap = true;
            return Storage(REGISTER, rs.reg);
        case MEMORY_STACK:
            x64->op(POPQ, reg);
            x64->op(opcode, reg, ls.address);
            if (swap) *swap = true;
            return Storage(REGISTER, reg);
        case MEMORY_MEMORY:
            x64->op(MOVQ, reg, ls.address);
            x64->op(opcode, reg, rs.address);
            return Storage(REGISTER, reg);
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Storage binary_subtract(X64 *x64, Regs regs) {
        bool swap = false;
        Storage s = binary_simple(x64, regs, SUBQ, &swap);

        if (s.where == REGISTER && swap)
            x64->op(NEGQ, s.reg);
            
        return s;
    }

    virtual Storage binary_multiply(X64 *x64, Regs regs, Address *lsaddr = NULL) {
        Storage ls = left->compile(x64, regs);
        
        if (ls.where == REGISTER)
            regs.remove(ls.reg);
        
        if (lsaddr)
            *lsaddr = ls.address;
            
        Storage rs = right->compile(x64, regs);

        // We always have an available register if neither side took it
        Register reg = (ls.where != REGISTER && rs.where != REGISTER) ? regs.get_any() : NOREG;

        switch (ls.where * rs.where) {
        case CONSTANT_CONSTANT:
            return Storage(CONSTANT, ls.value * rs.value);
        case CONSTANT_REGISTER:
            x64->op(IMUL3Q, rs.reg, rs.reg, ls.value);
            return Storage(REGISTER, rs.reg);
        case CONSTANT_STACK:
            x64->op(POPQ, reg);
            x64->op(IMUL3Q, reg, reg, ls.value);
            return Storage(REGISTER, reg);
        case CONSTANT_MEMORY:
            x64->op(IMUL3Q, reg, rs.address, ls.value);
            return Storage(REGISTER, reg);
        case REGISTER_CONSTANT:
            x64->op(IMUL3Q, ls.reg, ls.reg, rs.value);
            return Storage(REGISTER, ls.reg);
        case REGISTER_REGISTER:
            x64->op(IMUL2Q, ls.reg, rs.reg);
            return Storage(REGISTER, ls.reg);
        case REGISTER_STACK:
            x64->op(IMUL2Q, ls.reg, Address(RSP, 0));
            x64->op(ADDQ, RSP, 8);
            return Storage(REGISTER, ls.reg);
        case REGISTER_MEMORY:
            x64->op(IMUL2Q, ls.reg, rs.address);
            return Storage(REGISTER, ls.reg);
        case STACK_CONSTANT:
            x64->op(POPQ, reg);
            x64->op(IMUL3Q, reg, reg, rs.value);
            return Storage(REGISTER, reg);
        case STACK_REGISTER:
            x64->op(IMUL2Q, rs.reg, Address(RSP, 0));
            x64->op(ADDQ, RSP, 8);
            return Storage(REGISTER, rs.reg);
        case STACK_STACK:
            x64->op(POPQ, reg);
            x64->op(IMUL2Q, reg, Address(RSP, 0));
            x64->op(ADDQ, RSP, 8);
            return Storage(REGISTER, reg);
        case STACK_MEMORY:
            x64->op(POPQ, reg);
            x64->op(IMUL2Q, reg, rs.address);
            return Storage(REGISTER, reg);
        case MEMORY_CONSTANT:
            x64->op(IMUL3Q, reg, ls.address, rs.value);
            return Storage(REGISTER, reg);
        case MEMORY_REGISTER:
            x64->op(IMUL2Q, rs.reg, ls.address);
            return Storage(REGISTER, rs.reg);
        case MEMORY_STACK:
            x64->op(POPQ, reg);
            x64->op(IMUL2Q, reg, ls.address);
            return Storage(REGISTER, reg);
        case MEMORY_MEMORY:
            x64->op(MOVQ, reg, ls.address);
            x64->op(IMUL2Q, reg, rs.address);
            return Storage(REGISTER, reg);
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Storage binary_divmod(X64 *x64, Regs regs, bool mod, Address *lsaddr = NULL) {
        if (!regs.has(RAX) || !regs.has(RDX))
            throw INTERNAL_ERROR;
            
        Storage ls = left->compile(x64, regs);
        
        switch (ls.where) {
            case CONSTANT:
            //    x64->op(MOVQ, RAX, ls.value);  See below
                break;
            case REGISTER:
                x64->op(MOVQ, RAX, ls.reg);
                break;
            case STACK:
                x64->op(POPQ, RAX);
                break;
            case MEMORY:
                x64->op(MOVQ, RAX, ls.address);
                if (lsaddr)
                    *lsaddr = ls.address;
                break;
            default:
                throw INTERNAL_ERROR;
        }
        
        regs.remove(RAX);
            
        Storage rs = right->compile(x64, regs);

        if (ls.where == CONSTANT) {
            if (rs.where == CONSTANT)
                return Storage(CONSTANT, mod ? ls.value % rs.value : ls.value / rs.value);
            else
                x64->op(MOVQ, RAX, ls.value);
        }

        switch (rs.where) {
        case CONSTANT:
            x64->op(PUSHQ, rs.value);
            x64->op(CQO);
            x64->op(IDIVQ, Address(RSP, 0));
            x64->op(ADDQ, RSP, 8);
            return Storage(REGISTER, mod ? RDX : RAX);
        case REGISTER:
            if (rs.reg == RDX) {
                x64->op(PUSHQ, RDX);
                x64->op(CQO);
                x64->op(IDIVQ, Address(RSP, 0));
                x64->op(ADDQ, RSP, 8);
                return Storage(REGISTER, mod ? RDX : RAX);
            }
            else {
                x64->op(CQO);
                x64->op(IDIVQ, rs.reg);
                return Storage(REGISTER, mod ? RDX : RAX);
            }
        case STACK:
            x64->op(CQO);
            x64->op(IDIVQ, Address(RSP, 0));
            x64->op(ADDQ, RSP, 8);
            return Storage(REGISTER, mod ? RDX : RAX);
        case MEMORY:
            x64->op(CQO);
            x64->op(IDIVQ, rs.address);
            return Storage(REGISTER, mod ? RDX : RAX);
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Storage binary_shift(X64 *x64, Regs regs, ShiftOp opcode) {
        if (!regs.has(RCX))
            throw INTERNAL_ERROR;
            
        regs.remove(RCX);
            
        Storage ls = left->compile(x64, regs);
        
        regs.add(RCX);
        
        if (ls.where == REGISTER)
            regs.remove(ls.reg);
            
        Storage rs = right->compile(x64, regs);

        // We always have a non-RCX register if the left side didn't take it
        // This may be equal to the right side's choice, but that's OK.
        regs.remove(RCX);
        Register reg = (ls.where != REGISTER) ? regs.get_any() : NOREG;
    
        switch (ls.where * rs.where) {
        case CONSTANT_CONSTANT:
            return Storage(CONSTANT, opcode == SHLQ ? ls.value << rs.value : ls.value >> rs.value);
        case CONSTANT_REGISTER:
            x64->op(MOVQ, RCX, rs.reg);
            x64->op(MOVQ, reg, ls.value);
            x64->op(opcode, reg, CL);
            return Storage(REGISTER, reg);
        case CONSTANT_STACK:
            x64->op(MOVQ, reg, ls.value);
            x64->op(POPQ, RCX);
            x64->op(opcode, reg, CL);
            return Storage(REGISTER, reg);
        case CONSTANT_MEMORY:
            x64->op(MOVQ, reg, ls.value);
            x64->op(MOVQ, RCX, rs.address);
            x64->op(opcode, reg, CL);
            return Storage(REGISTER, reg);
        case REGISTER_CONSTANT:
            x64->op(opcode, ls.reg, rs.value);
            return Storage(REGISTER, ls.reg);
        case REGISTER_REGISTER:
            x64->op(MOVQ, RCX, rs.reg);
            x64->op(opcode, ls.reg, CL);
            return Storage(REGISTER, ls.reg);
        case REGISTER_STACK:
            x64->op(POPQ, RCX);
            x64->op(opcode, ls.reg, CL);
            return Storage(REGISTER, ls.reg);
        case REGISTER_MEMORY:
            x64->op(MOVQ, RCX, rs.address);
            x64->op(opcode, ls.reg, CL);
            return Storage(REGISTER, ls.reg);
        case STACK_CONSTANT:
            x64->op(POPQ, reg);
            x64->op(opcode, reg, rs.value);
            return Storage(REGISTER, reg);
        case STACK_REGISTER:
            x64->op(MOVQ, RCX, rs.reg);
            x64->op(POPQ, reg);
            x64->op(opcode, reg, CL);
            return Storage(REGISTER, reg);
        case STACK_STACK:
            x64->op(POPQ, RCX);
            x64->op(POPQ, reg);
            x64->op(opcode, reg, CL);
            return Storage(REGISTER, reg);
        case STACK_MEMORY:
            x64->op(POPQ, reg);
            x64->op(MOVQ, RCX, rs.address);
            x64->op(opcode, reg, CL);
            return Storage(REGISTER, reg);
        case MEMORY_CONSTANT:
            x64->op(MOVQ, reg, ls.address);
            x64->op(opcode, reg, rs.value);
            return Storage(REGISTER, reg);
        case MEMORY_REGISTER:
            x64->op(MOVQ, RCX, rs.reg);
            x64->op(MOVQ, reg, ls.address);
            x64->op(opcode, reg, CL);
            return Storage(REGISTER, reg);
        case MEMORY_STACK:
            x64->op(POPQ, RCX);
            x64->op(MOVQ, reg, ls.address);
            x64->op(opcode, reg, CL);
            return Storage(REGISTER, reg);
        case MEMORY_MEMORY:
            x64->op(MOVQ, reg, ls.address);
            x64->op(MOVQ, RCX, rs.address);
            x64->op(opcode, reg, CL);
            return Storage(REGISTER, reg);
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Storage binary_exponent(X64 *x64, Regs regs, Address *lsaddr = NULL) {
        if (!regs.has(RAX) || !regs.has(RDX))
            throw INTERNAL_ERROR;
            
        Storage ls = left->compile(x64, regs);
        
        switch (ls.where) {
            case CONSTANT:
            //    x64->op(MOVQ, RAX, ls.value);  See below
                break;
            case REGISTER:
                x64->op(MOVQ, RDX, ls.reg);
                break;
            case STACK:
                x64->op(POPQ, RDX);
                break;
            case MEMORY:
                x64->op(MOVQ, RDX, ls.address);
                if (lsaddr)
                    *lsaddr = ls.address;
                break;
            default:
                throw INTERNAL_ERROR;
        }
        
        regs.remove(RDX);
            
        Storage rs = right->compile(x64, regs);

        if (ls.where == CONSTANT) {
            if (rs.where == CONSTANT)
                return Storage(CONSTANT, 0);  // FIXME: seriously
            else
                x64->op(MOVQ, RDX, ls.value);
        }

        switch (rs.where) {
        case CONSTANT:
            x64->op(PUSHQ, rs.value);
            break;
        case REGISTER:
            x64->op(PUSHQ, rs.reg);
            break;
        case STACK:
            break;
        case MEMORY:
            x64->op(PUSHQ, rs.address);
            break;
        default:
            throw INTERNAL_ERROR;
        }

        // base in RDX, exponent on stack
        exponentiation_by_squaring(x64);
        x64->op(ADDQ, RSP, 8);
        
        return Storage(REGISTER, RAX);
    }

    virtual Storage binary_compare(X64 *x64, Regs regs, BitSetOp opcode) {
        bool swap = false;
        Storage s = binary_simple(x64, regs, CMPQ, &swap);
        
        if (s.where == CONSTANT) {
            bool holds = (
                (s.value < 0 && (opcode == SETNE || opcode == SETL || opcode == SETLE)) ||
                (s.value > 0 && (opcode == SETNE || opcode == SETG || opcode == SETGE)) ||
                (s.value == 0 && (opcode == SETE || opcode == SETLE || opcode == SETGE))
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

    virtual Storage assign_binary(X64 *x64, Regs regs, BinaryOp opcode) {
        Storage ls = left->compile(x64, regs);
        
        if (ls.where != MEMORY)
            throw INTERNAL_ERROR;
            
        Storage rs = right->compile(x64, regs);
        
        // We always have an available register if neither side took it
        Register reg = (ls.where != REGISTER && rs.where != REGISTER) ? regs.get_any() : NOREG;

        switch (rs.where) {
        case CONSTANT:
            x64->op(opcode, ls.address, rs.value);
            return ls;
        case REGISTER:
            x64->op(opcode, ls.address, rs.reg);
            return ls;
        case STACK:
            if ((opcode | 3) == MOVQ)
                x64->op(POPQ, ls.address);
            else {
                x64->op(POPQ, reg);
                x64->op(opcode, ls.address, reg);
            }
            return ls;
        case MEMORY:
            x64->op(MOVQ, reg, rs.address);
            x64->op(opcode, ls.address, reg);
            return ls;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Storage assign_multiply(X64 *x64, Regs regs) {
        Address lsaddr;
        Storage s = binary_multiply(x64, regs, &lsaddr);
        x64->op(MOVQ, lsaddr, s.reg);
        return Storage(MEMORY, lsaddr);
    }

    virtual Storage assign_divmod(X64 *x64, Regs regs, bool mod) {
        Address lsaddr;
        Storage s = binary_divmod(x64, regs, mod, &lsaddr);
        x64->op(MOVQ, lsaddr, s.reg);
        return Storage(MEMORY, lsaddr);
    }

    virtual Storage assign_exponent(X64 *x64, Regs regs) {
        Address lsaddr;
        Storage s = binary_exponent(x64, regs, &lsaddr);
        x64->op(MOVQ, lsaddr, s.reg);
        return Storage(MEMORY, lsaddr);
    }

    virtual Storage assign_shift(X64 *x64, Regs regs, ShiftOp opcode) {
        Storage ls = left->compile(x64, regs);
        
        if (ls.where != MEMORY)
            throw INTERNAL_ERROR;
            
        Storage rs = right->compile(x64, regs);
        
        // We always have RCX

        switch (rs.where) {
        case CONSTANT:
            x64->op(opcode, ls.address, rs.value);
            return ls;
        case REGISTER:
            x64->op(MOVQ, RCX, rs.reg);
            x64->op(opcode, ls.address, CL);
            return ls;
        case STACK:
            x64->op(POPQ, RCX);
            x64->op(opcode, ls.address, CL);
            return ls;
        case MEMORY:
            x64->op(MOVQ, RCX, rs.address);
            x64->op(opcode, ls.address, CL);
            return ls;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual StorageWhere complexity() {
        if (is_unary(operation))
            return left->complexity() == CONSTANT ? CONSTANT : REGISTER;
        else
            return left->complexity() == CONSTANT && right->complexity() == CONSTANT ? CONSTANT : REGISTER;
    }

    enum Spill {
        SPILL_ANY,
        SPILL_RAX_AND_RDX,
        SPILL_RCX_AND_ANY,
        SPILL_RCX
    };

    enum Spilled {
        SPILLED_NONE,
    
        SPILLED_RAX,
        SPILLED_RCX,
        SPILLED_RDX,
        
        SPILLED_RAX_AND_RDX,
        SPILLED_RAX_AND_RCX
    };

    virtual Spilled spill(X64 *x64, Regs &regs, Spill to_spill) {
        // NOTE: must take Regs by reference because we modify it
        Spilled s = (
            to_spill == SPILL_RAX_AND_RDX ?
                (regs.has(RAX) ?
                    (regs.has(RDX) ? SPILLED_NONE : SPILLED_RDX) :
                    (regs.has(RDX) ? SPILLED_RAX : SPILLED_RAX_AND_RDX)
                ) :
            to_spill == SPILL_RCX_AND_ANY ?
                (regs.has(RCX) ?
                    (regs.has_other(RCX) ? SPILLED_NONE : SPILLED_RAX) :
                    (regs.has_other(RCX) ? SPILLED_RCX : SPILLED_RAX_AND_RCX)
                ) :
            to_spill == SPILL_RCX ?
                (regs.has(RCX) ? SPILLED_NONE : SPILLED_RCX) :
            to_spill == SPILL_ANY ?
                (regs.has_any() ? SPILLED_NONE : SPILLED_RAX) :
            throw INTERNAL_ERROR
        );
        
        if (s == SPILLED_RAX || s == SPILLED_RAX_AND_RDX || s == SPILLED_RAX_AND_RCX)
            x64->op(PUSHQ, RAX);
            
        if (s == SPILLED_RCX || s == SPILLED_RAX_AND_RCX)
            x64->op(PUSHQ, RCX);
            
        if (s == SPILLED_RDX || s == SPILLED_RAX_AND_RDX)
            x64->op(PUSHQ, RDX);
            
        return s;
    }

    virtual Storage fill(X64 *x64, Storage s, Spilled spilled) {
        if (s.where == STACK) {
            std::cerr << "This was a bad idea!";
            throw INTERNAL_ERROR;
        }
        
        switch (spilled) {
        case SPILLED_NONE:
            return s;
        case SPILLED_RAX:
            if (s.where == REGISTER && s.reg == RAX) {
                x64->op(XCHGQ, RAX, Address(RSP, 0));
                return Storage(STACK);
            }
            else {
                x64->op(POPQ, RAX);
                return s;
            }
        case SPILLED_RCX:
            if (s.where == REGISTER && s.reg == RCX) {
                x64->op(XCHGQ, RCX, Address(RSP, 0));
                return Storage(STACK);
            }
            else {
                x64->op(POPQ, RCX);
                return s;
            }
        case SPILLED_RDX:
            if (s.where == REGISTER && s.reg == RDX) {
                x64->op(XCHGQ, RDX, Address(RSP, 0));
                return Storage(STACK);
            }
            else {
                x64->op(POPQ, RDX);
                return s;
            }
        case SPILLED_RAX_AND_RCX:
            if (s.where == REGISTER && s.reg == RAX) {
                x64->op(POPQ, RCX);
                x64->op(XCHGQ, RAX, Address(RSP, 0));
                return Storage(STACK);
            }
            else if (s.where == REGISTER && s.reg == RCX) {
                x64->op(XCHGQ, RAX, RCX);
                x64->op(POPQ, RCX);
                x64->op(XCHGQ, RAX, Address(RSP, 0));
                return Storage(STACK);
            }
            else {
                x64->op(POPQ, RCX);
                x64->op(POPQ, RAX);
                return s;
            }
        case SPILLED_RAX_AND_RDX:
            if (s.where == REGISTER && s.reg == RAX) {
                x64->op(POPQ, RDX);
                x64->op(XCHGQ, RAX, Address(RSP, 0));
                return Storage(STACK);
            }
            else if (s.where == REGISTER && s.reg == RDX) {
                x64->op(XCHGQ, RAX, RDX);
                x64->op(POPQ, RDX);
                x64->op(XCHGQ, RAX, Address(RSP, 0));
                return Storage(STACK);
            }
            else {
                x64->op(POPQ, RDX);
                x64->op(POPQ, RAX);
                return s;
            }
        default:
            throw INTERNAL_ERROR;
        }
    }
    
    virtual Storage compile(X64 *x64, Regs regs) {
        Spill to_spill = (
            operation == DIVIDE || operation == ASSIGN_DIVIDE ||
            operation == MODULO || operation == ASSIGN_MODULO ||
            operation == EXPONENT || operation == ASSIGN_EXPONENT ? SPILL_RAX_AND_RDX :
            operation == SHIFT_LEFT || operation == SHIFT_RIGHT ? SPILL_RCX_AND_ANY :
            operation == ASSIGN_SHIFT_LEFT || operation == ASSIGN_SHIFT_RIGHT ? SPILL_RCX :
            SPILL_ANY
        );
        
        Spilled spilled = spill(x64, regs, to_spill);
        
        Storage s;
        
        switch (operation) {
        case COMPLEMENT:
            s = unary(x64, regs, NOTQ); break;
        case NEGATE:
            s = unary(x64, regs, NEGQ); break;
        case ADD:
            s = binary_simple(x64, regs, ADDQ); break;
        case SUBTRACT:
            s = binary_subtract(x64, regs); break;
        case MULTIPLY:
            s = binary_multiply(x64, regs); break;
        case DIVIDE:
            s = binary_divmod(x64, regs, false); break;
        case MODULO:
            s = binary_divmod(x64, regs, true); break;
        case OR:
            s = binary_simple(x64, regs, ORQ); break;
        case XOR:
            s = binary_simple(x64, regs, XORQ); break;
        case AND:
            s = binary_simple(x64, regs, ANDQ); break;
        case SHIFT_LEFT:
            s = binary_shift(x64, regs, SHLQ); break;
        case SHIFT_RIGHT:
            s = binary_shift(x64, regs, SHRQ); break;
        case EXPONENT:
            s = binary_exponent(x64, regs); break;
        case EQUAL:
            s = binary_compare(x64, regs, SETE); break;
        case NOT_EQUAL:
            s = binary_compare(x64, regs, SETNE); break;
        case LESS:
            s = binary_compare(x64, regs, SETL); break;
        case GREATER:
            s = binary_compare(x64, regs, SETG); break;
        case LESS_EQUAL:
            s = binary_compare(x64, regs, SETLE); break;
        case GREATER_EQUAL:
            s = binary_compare(x64, regs, SETGE); break;
        case INCOMPARABLE:
            s = Storage(CONSTANT, 0); break;
        case ASSIGN:
            s = assign_binary(x64, regs, MOVQ); break;
        case ASSIGN_ADD:
            s = assign_binary(x64, regs, ADDQ); break;
        case ASSIGN_SUBTRACT:
            s = assign_binary(x64, regs, SUBQ); break;
        case ASSIGN_MULTIPLY:
            s = assign_multiply(x64, regs); break;
        case ASSIGN_DIVIDE:
            s = assign_divmod(x64, regs, false); break;
        case ASSIGN_MODULO:
            s = assign_divmod(x64, regs, true); break;
        case ASSIGN_EXPONENT:
            s = assign_exponent(x64, regs); break;
        case ASSIGN_OR:
            s = assign_binary(x64, regs, ORQ); break;
        case ASSIGN_XOR:
            s = assign_binary(x64, regs, XORQ); break;
        case ASSIGN_AND:
            s = assign_binary(x64, regs, ANDQ); break;
        case ASSIGN_SHIFT_LEFT:
            s = assign_shift(x64, regs, SHLQ); break;
        case ASSIGN_SHIFT_RIGHT:
            s = assign_shift(x64, regs, SHRQ); break;
        default:
            std::cerr << "Unknown integer operator!\n";
            throw INTERNAL_ERROR;
        }
        
        return fill(x64, s, spilled);
    }
};

