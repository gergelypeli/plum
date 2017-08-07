
class BooleanOperationValue: public GenericOperationValue {
public:
    BooleanOperationValue(OperationType o, Value *p, TypeMatch &match)
        :GenericOperationValue(o, match[0].rvalue(), match[0], p) {
    }
    
    virtual Storage complement(X64 *x64) {
        subcompile(x64);
        
        switch (ls.where) {
        case CONSTANT:
            return Storage(CONSTANT, !ls.value);
        case FLAGS:
            return Storage(FLAGS, negate(ls.bitset));
        case REGISTER:
            x64->op(CMPB, ls.reg, 0);
            return Storage(FLAGS, SETE);
        case MEMORY:
            x64->op(CMPB, ls.address, 0);
            return Storage(FLAGS, SETE);
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Storage compare(X64 *x64) {
        subcompile(x64);

        switch (ls.where * rs.where) {
        case CONSTANT_CONSTANT:
            return Storage(CONSTANT, ls.value == rs.value);
        case CONSTANT_FLAGS:
            return Storage(FLAGS, ls.value ? rs.bitset : negate(rs.bitset));
        case CONSTANT_REGISTER:
            x64->op(CMPB, rs.reg, ls.value);
            return Storage(FLAGS, SETE);
        case CONSTANT_MEMORY:
            x64->op(CMPB, rs.address, ls.value);
            return Storage(FLAGS, SETE);
        case FLAGS_CONSTANT:
            return Storage(FLAGS, rs.value ? ls.bitset : negate(ls.bitset));
        case REGISTER_CONSTANT:
            x64->op(CMPB, ls.reg, rs.value);
            return Storage(FLAGS, SETE);
        case REGISTER_FLAGS:
            x64->op(rs.bitset, BL);
            x64->op(CMPB, ls.reg, BL);
            return Storage(FLAGS, SETE);
        case REGISTER_REGISTER:
            x64->op(CMPB, ls.reg, rs.reg);
            return Storage(FLAGS, SETE);
        case REGISTER_MEMORY:
            x64->op(CMPB, ls.reg, rs.address);
            return Storage(FLAGS, SETE);
        case MEMORY_CONSTANT:
            x64->op(CMPB, ls.address, rs.value);
            return Storage(FLAGS, SETE);
        case MEMORY_FLAGS:
            x64->op(rs.bitset, BL);
            x64->op(CMPB, BL, ls.address);
            return Storage(FLAGS, SETE);
        case MEMORY_REGISTER:
            x64->op(CMPB, ls.address, rs.reg);
            return Storage(FLAGS, SETE);
        case MEMORY_MEMORY:
            x64->op(MOVB, reg, ls.address);
            x64->op(CMPB, reg, rs.address);
            return Storage(FLAGS, SETE);
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Storage assign(X64 *x64) {
        subcompile(x64);

        if (ls.where != MEMORY)
            throw INTERNAL_ERROR;

        switch (rs.where) {
        case CONSTANT:
            x64->op(MOVB, ls.address, rs.value);
            return ls;
        case FLAGS:
            x64->op(rs.bitset, ls.address);
            return ls;
        case REGISTER:
            x64->op(MOVB, ls.address, rs.reg);
            return ls;
        case MEMORY:
            x64->op(MOVB, reg, rs.address);
            x64->op(MOVB, ls.address, reg);
            return ls;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Storage compile(X64 *x64) {
        switch (operation) {
        case COMPLEMENT:
            return complement(x64);
        case EQUAL:
            return compare(x64);
        case NOT_EQUAL: {
            Storage s = compare(x64);
            if (s.where == CONSTANT)
                return Storage(CONSTANT, !s.value);
            else if (s.where == FLAGS)
                return Storage(FLAGS, negate(s.bitset));
            else
                throw INTERNAL_ERROR;
        }
        case ASSIGN:
            return assign(x64);
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class BooleanOrValue: public Value {
public:
    std::unique_ptr<Value> left, right;
    Register reg;
    
    BooleanOrValue(OperationType o, Value *p, TypeMatch &match)
        :Value(match[0]) {
        left.reset(p);
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() != 1 || kwargs.size() != 0) {
            std::cerr << "Whacky boolean or operation!\n";
            return false;
        }

        Value *r = typize(args[0].get(), scope, &ts);
        TypeMatch match;
    
        if (!typematch(ts, r, match)) {
            ts = BOOLEAN_TS;
            
            Value *l = left.release();
            
            if (!typematch(ts, l, match)) {
                std::cerr << "Logical or left is not boolean: " << get_typespec(l) << "\n";
                throw INTERNAL_ERROR;
            }
                
            left.reset(l);
            
            if (!typematch(ts, r, match))
                throw INTERNAL_ERROR;
        }
    
        right.reset(r);
        return true;
    }

    virtual Regs precompile(Regs preferred) {
        Regs clobbered = left->precompile(preferred) | right->precompile(preferred);
        
        // This won't be bothered by either branches
        reg = preferred.get_any();
        clobbered.add(reg);
        
        return clobbered;
    }

    virtual Storage compile(X64 *x64) {
        // Evaluate pivot (left), and check its boolean value without converting.
        // If true, return it. If false, evaluate arg (right), and return that.

        StorageWhere where = ts.where(false);
        Storage s = (
            where == REGISTER ? Storage(REGISTER, reg) :
            where == STACK ? Storage(STACK) :
            throw INTERNAL_ERROR
        );
        
        left->compile_and_store(x64, s);
        
        Storage bs = ts.boolval(s, x64, true);
        Label else_end;
        
        switch (bs.where) {
        case CONSTANT:
            if (bs.value)
                return s;
            else {
                ts.store(s, Storage(), x64);
                right->compile_and_store(x64, s);
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
        ts.store(s, Storage(), x64);
        right->compile_and_store(x64, s);
        x64->code_label(else_end);
        
        return s;
    }
};


class BooleanAndValue: public Value {
public:
    std::unique_ptr<Value> left, right;
    Register reg;
    
    BooleanAndValue(OperationType o, Value *p, TypeMatch &match)
        :Value(VOID_TS) {  // Will be overridden
        left.reset(p);
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() != 1 || kwargs.size() != 0) {
            std::cerr << "Whacky boolean and operation!\n";
            return false;
        }

        Value *r = typize(args[0].get(), scope);
        TypeMatch match;
        
        if (!typematch(ANY_TS, r, match))
            throw INTERNAL_ERROR;
            
        ts = match[0];
        right.reset(r);
        
        return true;
    }

    virtual Regs precompile(Regs preferred) {
        Regs clobbered = left->precompile(preferred) | right->precompile(preferred);
        
        // This won't be bothered by either branches
        reg = preferred.get_any();
        clobbered.add(reg);
        
        return clobbered;
    }

    virtual Storage compile(X64 *x64) {
        // Evaluate arg (left), which is boolean. It true, evaluate pivot (right),
        // and return it. If false, return the clear value of the pivot type.
        
        StorageWhere where = ts.where(false);
        Storage s = (
            where == REGISTER ? Storage(REGISTER, reg) :
            where == STACK ? Storage(STACK) :
            throw INTERNAL_ERROR
        );
        
        Storage ls = left->compile(x64);
        Label then_end;
        
        switch (ls.where) {
        case CONSTANT:
            if (ls.value)
                return right->compile(x64);
            else {
                // By the way, this is a compile time decision, so it
                // doesn't need to be sync with any "other" branch.
                ts.store(Storage(), s, x64);
                return s;
            }
        case FLAGS:
            x64->op(branchize(negate(ls.bitset)), then_end);
            break;
        case REGISTER:
            x64->op(CMPB, ls.reg, 0);
            x64->op(JE, then_end);
            break;
        case MEMORY:
            x64->op(CMPB, ls.address, 0);
            x64->op(JE, then_end);
            break;
        default:
            throw INTERNAL_ERROR;
        }

        // Then branch, evaluate the right expression
        Storage rs = right->compile(x64);
        
        if (ls.where == FLAGS && rs.where == FLAGS && ls.bitset == rs.bitset) {
            // Optimized special case it both subexpressions returned the same flags.
            // This happens when the same comparison is used on both sides.
            x64->code_label(then_end);
            return ls;
        }
        else {
            Label else_end;
            
            ts.store(rs, s, x64);
            x64->op(JMP, else_end);
            x64->code_label(then_end);
        
            // Else branch, create a clear value of the right type (use the same storage)
            ts.create(Storage(), s, x64);
            x64->code_label(else_end);
        
            return s;
        }
    }
};


class BooleanIfValue: public Value {
public:
    std::unique_ptr<Value> condition;
    std::unique_ptr<Value> then_branch;
    std::unique_ptr<Value> else_branch;
    Register reg;
    
    BooleanIfValue(OperationType o, Value *pivot, TypeMatch &match)
        :Value(VOID_TS) {  // Will be overridden later
        //condition.reset(pivot);
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() != 1) {
            std::cerr << "Not one positional argument to :if!\n";
            return false;
        }
        
        Value *c = typize(args[0].get(), scope, &BOOLEAN_TS);
        TypeMatch match;
        
        if (!typematch(BOOLEAN_TS, c, match)) {
            std::cerr << "Not a Boolean condition to :if!\n";
            return false;
        }
        
        condition.reset(c);
        
        for (auto &kv : kwargs) {
            if (kv.first == "then")
                then_branch.reset(make_code_value(typize(kv.second.get(), scope, &VOID_CODE_TS)));
            else if (kv.first == "else")
                else_branch.reset(make_code_value(typize(kv.second.get(), scope, &VOID_CODE_TS)));
            else {
                std::cerr << "Invalid argument to Boolean if!\n";
                return false;
            }
        }

        if (then_branch && else_branch) {
            // Can't return an lvalue, because one Storage can only represent
            // a compile time fixed variable location.
            TypeSpec tts = then_branch->ts;
            TypeSpec ets = else_branch->ts;
            
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
        reg = preferred.get_any();
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

