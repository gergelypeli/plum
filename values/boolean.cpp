
class BooleanOperationValue: public Value {
public:
    NumericOperation operation;
    std::unique_ptr<Value> pivot, arg;
    Register reg;
    
    BooleanOperationValue(NumericOperation o, Value *p)
        :Value(o == COMPLEMENT ? BOOLEAN_TS : p->ts.rvalue()) {
        // !T     => B
        // T || U => T
        // T && U => U (the second argument is our pivot, special handling!)
        operation = o;
        pivot.reset(p);
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (is_unary(operation)) {
            if (args.size() != 0 || kwargs.size() != 0) {
                std::cerr << "Whacky boolean not operation!\n";
                return false;
            }
            
            return true;
        }
        else {
            if (args.size() != 1 || kwargs.size() != 0) {
                std::cerr << "Whacky boolean binary operation!\n";
                return false;
            }

            TypeSpec arg_ts = (operation == OR ? ts : BOOLEAN_TS);
            Value *a = typize(args[0].get(), scope);
            Value *ca;
            
            if (operation == OR) {
                ca = convertible(ts, a);
        
                if (!ca) {
                    std::cerr << "Incompatible right argument to logical or operation!\n";
                    return false;
                }
            }
            else if (operation == AND) {
                ca = convertible(BOOLEAN_TS, a);
        
                if (!ca) {
                    // Yes, this is the left argument according to the user
                    std::cerr << "Non-valued left argument to logical and operation!\n";
                    return false;
                }
            }
            else
                throw INTERNAL_ERROR;
        
            arg.reset(ca);
            return true;
        }
    }

    virtual Regs precompile(Regs preferred) {
        Regs clobbered = pivot->precompile(preferred);
        
        if (arg)
            clobbered = clobbered | arg->precompile(preferred);
        
        // This won't be bothered by either branches
        reg = preferred.get_gpr();
        clobbered.add(reg);
        
        return clobbered;
    }

    virtual Storage compile(X64 *x64) {
        if (operation == COMPLEMENT) {
            // Just complement a boolean value
            Storage ps = pivot->compile(x64);
            
            switch (ps.where) {
            case CONSTANT:
                return Storage(CONSTANT, !ps.value);
            case FLAGS:
                return Storage(FLAGS, negate(ps.bitset));
            case REGISTER:
                x64->op(CMPB, ps.reg, 0);
                return Storage(FLAGS, SETNE);
            case MEMORY:
                x64->op(CMPB, ps.address, 0);
                return Storage(FLAGS, SETNE);
            default:
                throw INTERNAL_ERROR;
            }
        }
        else if (operation == AND) {
            // Evaluate arg (left), which is boolean. It true, evaluate pivot (right),
            // and return it. If false, return the clear value of the pivot type.
            
            StorageWhere where = pivot->ts.where();
            Storage s = (
                where == REGISTER ? Storage(REGISTER, reg) :
                where == STACK ? Storage(STACK) :
                throw INTERNAL_ERROR
            );
            
            Storage as = arg->compile(x64);
            Label then_end;
            Label else_end;
            
            switch (as.where) {
            case CONSTANT:
                if (as.value)
                    return pivot->compile(x64);
                else {
                    // By the way, this is a compile time decision, so it
                    // doesn't need to be sync with any "other" branch.
                    pivot->ts.create(s, x64);
                    return s;
                }
            case FLAGS:
                x64->op(branchize(negate(as.bitset)), then_end);
                break;
            case REGISTER:
                x64->op(CMPB, as.reg, 0);
                x64->op(JE, then_end);
                break;
            case MEMORY:
                x64->op(CMPB, as.address, 0);
                x64->op(JE, then_end);
                break;
            default:
                throw INTERNAL_ERROR;
            }

            // Then branch, evaluate the main expression
            pivot->compile_and_store(x64, s);
            x64->op(JMP, else_end);
            x64->code_label(then_end);
            
            // Else branch, create a clear value of the main type (use the same storage)
            pivot->ts.create(s, x64);
            x64->code_label(else_end);
            
            return s;
        }
        else if (operation == OR) {
            // Evaluate pivot (left), and check its boolean value without converting.
            // If true, return it. If false, evaluate arg (right), and return that.

            StorageWhere where = pivot->ts.where();
            Storage s = (
                where == REGISTER ? Storage(REGISTER, reg) :
                where == STACK ? Storage(STACK) :
                throw INTERNAL_ERROR
            );
            
            pivot->compile_and_store(x64, s);
            
            Storage bs = pivot->ts.boolval(s, x64);
            Label else_end;
            
            switch (bs.where) {
            case CONSTANT:
                if (bs.value)
                    return s;
                else {
                    pivot->ts.store(s, Storage(), x64);
                    arg->compile_and_store(x64, s);
                    return s;
                }
            case FLAGS:
                x64->op(branchize(bs.bitset), else_end);
                break;
            case REGISTER:
                x64->op(CMPB, bs.reg, 0);
                x64->op(JNE, else_end);
                break;
            default:
                throw INTERNAL_ERROR;
            }

            // These boolean storage types can be just thrown away

            // Our then branch is empty, we just return the previously computed value

            // Else branch, evaluate the arg, and return that (use the same storage)
            pivot->ts.store(s, Storage(), x64);
            arg->compile_and_store(x64, s);
            x64->code_label(else_end);
            
            return s;
        }
        else if (operation == ASSIGN) {
            Storage ps = pivot->compile(x64);

            if (ps.where != MEMORY)
                throw INTERNAL_ERROR;
                
            Storage as = arg->compile(x64);
            
            switch (as.where) {
            case CONSTANT:
                x64->op(MOVB, ps.address, as.value);
                break;
            case FLAGS:
                x64->op(as.bitset, ps.address);
                break;
            case REGISTER:
                x64->op(MOVB, ps.address, as.reg);
                break;
            case MEMORY:
                x64->op(MOVB, reg, as.address);
                x64->op(MOVB, ps.address, reg);
                break;
            default:
                throw INTERNAL_ERROR;
            }
            
            return ps;
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
        :Value(VOID_TS) {  // Will be overridden later
        condition.reset(pivot);
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() > 0) {
            std::cerr << "Positional arguments to Boolean if!\n";
            return false;
        }
        
        for (auto &kv : kwargs) {
            if (kv.first == "then")
                then_branch.reset(make_code_value(typize(kv.second.get(), scope)));
            else if (kv.first == "else")
                else_branch.reset(make_code_value(typize(kv.second.get(), scope)));
            else {
                std::cerr << "Invalid argument to Boolean if!\n";
                return false;
            }
        }

        if (then_branch && else_branch) {
            // Can't return an lvalue, because one Storage can only represent
            // a compile time fixed variable location.
            TypeSpec tts = then_branch->ts.unprefix(code_type);
            TypeSpec ets = else_branch->ts.unprefix(code_type);
            
            if (tts != VOID_TS && tts == ets) {
                ts = tts;
                std::cerr << "Boolean if at " << token << " is " << ts << ".\n";
            }
        }
        
        return true;
    }
    
    virtual Regs precompile(Regs preferred) {
        Regs clobbered = Regs();
        
        clobbered = clobbered | condition->precompile();
        
        if (then_branch)
            clobbered = clobbered | then_branch->precompile(preferred);
                       
        if (else_branch)
            clobbered = clobbered | else_branch->precompile(preferred);
        
        // This won't be bothered by either branches
        reg = preferred.get_gpr();
        clobbered.add(reg);
        
        return clobbered;
    }
    
    virtual Storage compile(X64 *x64) {
        Label then_end;
        Label else_end;
        
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
            x64->op(CMPB, cs.reg, 0);
            
            if (then_branch)
                x64->op(JE, then_end);
            else if (else_branch)
                x64->op(JNE, else_end);
                
            break;
        case STACK:
            x64->op(POPQ, reg);
            x64->op(CMPB, RAX, 0);
            
            if (then_branch)
                x64->op(JE, then_end);
            else if (else_branch)
                x64->op(JNE, else_end);
                
            break;
        case MEMORY:
            x64->op(CMPB, cs.address, 0);

            if (then_branch)
                x64->op(JE, then_end);
            else if (else_branch)
                x64->op(JNE, else_end);
            
            break;
        default:
            throw INTERNAL_ERROR;
        }

        // TODO: we need a function to get the recommended storage for this type!
        Storage s = ts != VOID_TS ? Storage(REGISTER, reg) : Storage();

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

