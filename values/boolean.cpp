
class BooleanOperationValue: public Value {
public:
    NumericOperation operation;
    std::unique_ptr<Value> left, right;
    Register reg;
    
    BooleanOperationValue(NumericOperation o, Value *pivot)
        :Value(BOOLEAN_TS) {
        operation = o;
        left.reset(pivot);
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (is_unary(operation)) {
            if (args.size() != 0 || kwargs.size() != 0) {
                std::cerr << "Whacky boolean unary operation!\n";
                return false;
            }
            
            return true;
        }
        else {
            if (args.size() != 1 || kwargs.size() != 0) {
                std::cerr << "Whacky boolean binary operation!\n";
                return false;
            }

            Value *r = typize(args[0].get(), scope);
            Value *cr = convertible(BOOLEAN_TS, r);
        
            if (!cr) {
                std::cerr << "Incompatible right argument to boolean binary operation!\n";
                std::cerr << "Type " << r->ts << " is not " << BOOLEAN_TS << "!\n";
                return false;
            }
        
            right.reset(cr);
            return true;
        }
    }

    virtual Regs precompile(Regs regs) {
        Regs ret = regs;
        
        if (left)
            ret.intersect(left->precompile(regs));
        
        if (right)
            ret.intersect(right->precompile(regs));
        
        reg = ret.remove_any();
        return ret;
    }

    virtual Storage compile(X64 *x64) {
        Storage ls = left->compile(x64);
            
        if (operation == COMPLEMENT) {
            switch (ls.where) {
            case CONSTANT:
                return Storage(CONSTANT, !ls.value);
            case FLAGS:
                return Storage(FLAGS, negate(ls.bitset));
            case REGISTER:
                x64->op(NOTB, ls.reg);
                return Storage(REGISTER, ls.reg);
            case STACK:
                x64->op(NOTB, Address(RSP, 0));
                return Storage(STACK);
            case MEMORY:
                x64->op(TESTB, ls.address, 1);
                return Storage(FLAGS, SETNE);
            default:
                throw INTERNAL_ERROR;
            }
        }
        else if (operation == AND) {
            Label then_end;
            Label else_end;
            then_end.allocate();
            else_end.allocate();
            
            switch (ls.where) {
            case CONSTANT:
                if (ls.value)
                    return right->compile(x64);
                else
                    return Storage(CONSTANT, 0);
            case FLAGS:
                x64->op(branchize(negate(ls.bitset)), then_end);
                break;
            case REGISTER:
                x64->op(TESTB, ls.reg, 1);
                x64->op(JE, then_end);
                break;
            case STACK:
                x64->op(POPQ, reg);
                x64->op(TESTB, reg, 1);
                x64->op(JE, then_end);
                break;
            case MEMORY:
                x64->op(TESTB, ls.address, 1);
                x64->op(JE, then_end);
                break;
            default:
                throw INTERNAL_ERROR;
            }

            // Then branch
            Storage rs = right->compile(x64);
            Storage s;
            
            if (rs.where == CONSTANT || rs.where == FLAGS) {
                s = Storage(REGISTER, reg);
                right->ts.store(rs, s, x64);
            }
            else
                s = rs;

            x64->op(JMP, else_end);
            x64->code_label(then_end);
            
            // Else branch (use same storage as the then branch)
            if (s.where == REGISTER)
                x64->op(MOVB, s.reg, 0);
            else if (s.where == STACK)
                x64->op(PUSHQ, 0);
            else
                throw INTERNAL_ERROR;
                
            x64->code_label(else_end);
            
            return s;
        }
        else if (operation == OR) {
            Label then_end;
            Label else_end;
            then_end.allocate();
            else_end.allocate();
            
            switch (ls.where) {
            case CONSTANT:
                if (ls.value)
                    return Storage(CONSTANT, 1);
                else
                    return right->compile(x64);
            case FLAGS:
                x64->op(branchize(negate(ls.bitset)), then_end);
                break;
            case REGISTER:
                x64->op(TESTB, ls.reg, 1);
                x64->op(JE, then_end);
                break;
            case STACK:
                x64->op(POPQ, reg);
                x64->op(TESTQ, RAX, 1);
                x64->op(JE, then_end);
                break;
            case MEMORY:
                x64->op(TESTB, ls.address, 1);
                x64->op(JE, then_end);
                break;
            default:
                throw INTERNAL_ERROR;
            }

            Storage s;

            // Then branch
            if (ls.where == FLAGS) {
                s = Storage(REGISTER, reg);
                left->ts.store(ls, s, x64);
            }
            else
                s = ls;
                
            if (s.where == REGISTER)
                x64->op(MOVB, s.reg, 1);
            else if (s.where == STACK)
                x64->op(PUSHQ, 1);
            else
                throw INTERNAL_ERROR;
                
            x64->op(JMP, else_end);
            x64->code_label(then_end);

            // Else branch (use same storage as the condition)
            right->compile_and_store(x64, s);
            x64->code_label(else_end);
            
            return s;
        }
        else if (operation == ASSIGN) {
            if (ls.where != MEMORY)
                throw INTERNAL_ERROR;
                
            Storage rs = right->compile(x64);
            
            switch (rs.where) {
            case CONSTANT:
                x64->op(MOVB, ls.address, rs.value);
                break;
            case FLAGS:
                x64->op(rs.bitset, ls.address);
                break;
            case REGISTER:
                x64->op(MOVB, ls.address, rs.reg);
                break;
            case STACK:
                x64->op(POPQ, reg);
                x64->op(MOVB, ls.address, reg);
                break;
            case MEMORY:
                x64->op(MOVB, reg, rs.address);
                x64->op(MOVB, ls.address, reg);
                break;
            default:
                throw INTERNAL_ERROR;
            }
            
            return ls;
        }
        else {
            std::cerr << "Unknown boolean operator!\n";
            throw INTERNAL_ERROR;
        }
    }
};


class BooleanIfValue: public Value {
public:
    std::unique_ptr<Value> condition;
    std::unique_ptr<Value> then_branch;
    std::unique_ptr<Value> else_branch;
    Register reg;
    
    BooleanIfValue(Value *pivot)
        :Value(VOID_TS) {
        condition.reset(pivot);
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() > 0) {
            std::cerr << "Positional arguments to Boolean if!\n";
            return false;
        }
        
        for (auto &kv : kwargs) {
            if (kv.first == "then")
                then_branch.reset(typize(kv.second.get(), scope));
            else if (kv.first == "else")
                else_branch.reset(typize(kv.second.get(), scope));
            else {
                std::cerr << "Invalid argument to Boolean if!\n";
                return false;
            }
        }

        if (then_branch && else_branch) {
            // Can't return an lvalue, because one Storage can only represent
            // a compile time fixed variable location.
            TypeSpec tts = then_branch->ts.rvalue();
            TypeSpec ets = else_branch->ts.rvalue();
            
            if (tts != VOID_TS && tts == ets) {
                ts = tts;
                std::cerr << "Boolean if at " << token << " is " << ts << ".\n";
            }
        }
        
        return true;
    }
    
    virtual Regs precompile(Regs regs) {
        Regs ret = regs;
        
        ret.intersect(condition->precompile(regs));
        
        if (then_branch)
            ret.intersect(then_branch->precompile(regs));
                       
        if (else_branch)
            ret.intersect(else_branch->precompile(regs));
        
        reg = ret.remove_any();
        return ret;
    }
    
    virtual Storage compile(X64 *x64) {
        Label then_end;
        Label else_end;
        then_end.allocate();
        else_end.allocate();
        
        Storage cs = condition->compile(x64);
        
        switch (cs.where) {
        case CONSTANT:
            if (cs.value)
                else_branch.reset(NULL);
            else
                then_branch.reset(NULL);
                
            break;
        case FLAGS:
            if (then_branch) {
                BranchOp opcode = branchize(negate(cs.bitset));
                x64->op(opcode, then_end);
            }
            else if (else_branch) {
                BranchOp opcode = branchize(cs.bitset);
                x64->op(opcode, else_end);
            }
            break;
        case REGISTER:
            x64->op(TESTB, cs.reg, 1);
            
            if (then_branch)
                x64->op(JE, then_end);
            else if (else_branch)
                x64->op(JNE, else_end);
                
            break;
        case STACK:
            x64->op(POPQ, reg);
            x64->op(TESTQ, RAX, 1);
            
            if (then_branch)
                x64->op(JE, then_end);
            else if (else_branch)
                x64->op(JNE, else_end);
                
            break;
        case MEMORY:
            x64->op(TESTB, cs.address, 1);

            if (then_branch)
                x64->op(JE, then_end);
            else if (else_branch)
                x64->op(JNE, else_end);
            
            break;
        default:
            throw INTERNAL_ERROR;
        }

        Storage s = ts != VOID_TS ? Storage(STACK) : Storage();

        if (then_branch) {
            then_branch->compile_and_store(x64, s);
            
            if (else_branch)
                x64->op(JMP, else_end);

            x64->code_label(then_end);
        }
        
        if (else_branch) {
            else_branch->compile_and_store(x64, s);
            x64->code_label(else_end);
        }
    
        return s;
    }
};

