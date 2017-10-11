
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
        if (arg_ts == VOID_TS) {
            if (args.size() != 0 || kwargs.size() != 0) {
                std::cerr << "Operation needs no arguments!\n";
                return false;
            }
            
            return true;
        }
        else {
            if (args.size() != 1 || kwargs.size() != 0) {
                std::cerr << "Operation needs one positional argument!\n";
                return false;
            }
        
            Value *r = typize(args[0].get(), scope, &arg_ts);
            TypeMatch match;
        
            if (!typematch(arg_ts, r, match)) {
                std::cerr << "Argument is " << r->ts << ", not " << arg_ts << "!\n";
                return false;
            }
        
            right.reset(r);
            return true;
        }
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


class GenericOperationValue: public GenericValue {
public:
    OperationType operation;
    bool is_left_lvalue;
    Regs clob, rclob;
    Register reg;
    
    GenericOperationValue(OperationType o, TypeSpec at, TypeSpec rt, Value *l)
        :GenericValue(at, rt, l) {
        operation = o;
        is_left_lvalue = is_assignment(o);
        reg = NOREG;
    }
    
    virtual Register pick_early_register(Regs preferred) {
        if ((clob & ~rclob).has_any()) {
            // We have registers clobbered by the left side only, use one
            return (clob & ~rclob).get_any();
        }
        else if ((preferred & ~rclob).has_any()) {
            // We have preferred registers not clobbered by the right side, allocate one
            return (preferred & ~rclob).get_any();
        }
        else if (rclob.count() >= 2) {
            // The right side clobbers many (all?) registers, so pick one for the left later
            return NOREG;
        }
        else {
            // Just allocate a register that is not clobbered by the right side
            return (~rclob).get_any();
        }
    }

    virtual Register pick_late_register() {
        // The right side clobbered many registers, pick one that is not used by its value
        return (clob & ~rs.regs()).get_any();
    }

    virtual Regs precompile(Regs preferred) {
        rclob = right ? right->precompile() : Regs();
        Regs lpref = (preferred & ~rclob).has_any() ? preferred & ~rclob : preferred;  // must be nonempty
        Regs lclob = left->precompile(lpref);
        clob = lclob | rclob;
        
        // We may need a register to perform the operation, and also return the result with.
        // If the left value is spilled, we also reload it to this one. For a lvo this
        // register may contain the address of the returned lvalue.
        reg = pick_early_register(preferred);
        if (reg != NOREG)
            clob.add(reg);
        
        return clob;
    }

    virtual void subcompile(X64 *x64) {
        ls = left->compile(x64);

        // Put the left value in a safe place
        if (is_left_lvalue) {
            switch (ls.where) {
            case MEMORY:
                if (ls.is_clobbered(rclob)) {
                    // We got a dynamic address clobbered by the right side, spill to stack
                    left->ts.store(ls, Storage(ALISTACK), x64);
                    ls = Storage(ALISTACK);
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
            switch (ls.where) {
            case CONSTANT:
                break;
            case FLAGS:
                break;
            case REGISTER:
                if (ls.is_clobbered(rclob)) {
                    left->ts.store(ls, Storage(STACK), x64);
                    ls = Storage(STACK);
                }
                break;
            case STACK:
                break;
            case MEMORY:
                // We must also be careful that the right side may change any variable!
                // And reg is a register that we allocate for values, so make sure
                // a dynamic address is not using that!
                
                if (!rclob.has_any() && reg != ls.address.base) {
                    // Okay, the right side has no side effects, and we don't want to
                    // destroy the address either, so keep the MEMORY storage.
                }
                else if (reg != NOREG) {
                    // We already know a register that won't be clobbered, save value there
                    // This may actually reuse the same register, but that's OK
                    left->ts.store(ls, Storage(REGISTER, reg), x64);
                    ls = Storage(REGISTER, reg);
                }
                else {
                    // Nothing is sure, push the value onto the stack
                    left->ts.store(ls, Storage(STACK), x64);
                    ls = Storage(STACK);
                }
                break;
            case ALISTACK:
                // The address itself may be safe, but the value may be not.
                // Store is only defined from ALISTACK to MEMORY, so do this manually.
                // And we can't leave any address in RBX, that's for scratch only.

                x64->op(POPQ, RBX);
                ls = Storage(MEMORY, Address(RBX, 0));
                
                if (reg != NOREG) {
                    // We already know a register that won't be clobbered, save value there
                    left->ts.store(ls, Storage(REGISTER, reg), x64);
                    ls = Storage(REGISTER, reg);
                }
                else {
                    // Nothing is sure, push the value onto the stack
                    left->ts.store(ls, Storage(STACK), x64);
                    ls = Storage(STACK);
                }
                break;
            case ALIAS:
                // The address itself may be safe, but the value may be not.
                // Store is only defined from ALIAS to MEMORY, so do this manually.
                // And we can't leave any address in RBX, that's for scratch only.

                x64->op(MOVQ, RBX, ls.address);
                ls = Storage(MEMORY, Address(RBX, 0));
                
                if (reg != NOREG) {
                    // We already know a register that won't be clobbered, save value there
                    left->ts.store(ls, Storage(REGISTER, reg), x64);
                    ls = Storage(REGISTER, reg);
                }
                else {
                    // Nothing is sure, push the value onto the stack
                    left->ts.store(ls, Storage(STACK), x64);
                    ls = Storage(STACK);
                }
                break;
            default:
                throw INTERNAL_ERROR;
            }
        }
        
        x64->unwind->push(this);
        
        rs = right ? right->compile(x64) : Storage();
        
        x64->unwind->pop(this);
        
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
            if (right->ts.where(false) == REGISTER) {
                if (!rclob.has_any())
                    throw INTERNAL_ERROR;
                    
                Storage s(REGISTER, rclob.get_any());
                right->ts.store(rs, s, x64);
                rs = s;
            }
            }
            break;
        case MEMORY:
            break;
        case ALISTACK: {
            if (!rclob.has_any())
                throw INTERNAL_ERROR;

            Storage s(MEMORY, Address(rclob.get_any(), 0));
            right->ts.store(rs, s, x64);
            rs = s;
            }
            break;
        case ALIAS: {
            if (!rclob.has_any())
                throw INTERNAL_ERROR;
                
            Storage s(MEMORY, Address(rclob.get_any(), 0));
            right->ts.store(rs, s, x64);
            rs = s;
            }
            break;
        default:
            throw INTERNAL_ERROR;
        }
        
        if (reg == NOREG) {
            //std::cerr << "clob=" << clob.available << " rs regs=" << rs.regs().available << "\n";
            reg = pick_late_register();
        }
        
        // Restore the spilled left side
        switch (ls.where) {
        case CONSTANT:
            break;
        case FLAGS:
            break;
        case REGISTER:
            break;
        case STACK:
            left->ts.store(ls, Storage(REGISTER, reg), x64);
            ls = Storage(REGISTER, reg);
            break;
        case MEMORY:
            break;
        case ALISTACK:
            left->ts.store(ls, Storage(MEMORY, Address(reg, 0)), x64);
            ls = Storage(MEMORY, Address(reg, 0));
            break;
        case ALIAS:
            left->ts.store(ls, Storage(MEMORY, Address(reg, 0)), x64);
            ls = Storage(MEMORY, Address(reg, 0));
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

    virtual Storage compile(X64 *x64) {
        switch (operation) {
        case ASSIGN:
            return assign(x64);
        default:
            throw INTERNAL_ERROR;
        }
    }
    
    virtual Scope *unwind(X64 *x64) {
        left->ts.store(ls, Storage(), x64);
        return NULL;
    }
};


bool check_argument(unsigned i, Expr *e, Scope *scope, const std::vector<TypeSpec> &arg_tss, std::vector<std::unique_ptr<Value>> &values) {
    if (i >= values.size()) {
        std::cerr << "Too many arguments!\n";
        return false;
    }

    if (values[i]) {
        std::cerr << "Argument " << i << " already supplied!\n";
        return false;
    }
        
    TypeSpec arg_ts = arg_tss[i];
    Value *v = typize(e, scope, &arg_ts);
    TypeMatch match;
    
    if (!typematch(arg_ts, v, match)) {
        std::cerr << "Argument type mismatch, " << get_typespec(v) << " is not a " << arg_ts << "!\n";
        return false;
    }
    
    values[i] = std::unique_ptr<Value>(v);
    return true;
}


bool check_arguments(
    Args &args, Kwargs &kwargs, Scope *scope,
    const std::vector<TypeSpec> &arg_tss,
    const std::vector<std::string> &arg_names,
    std::vector<std::unique_ptr<Value>> &values
) {
    for (unsigned i = 0; i< arg_tss.size(); i++)
        values.push_back(NULL);

    for (unsigned i = 0; i < args.size(); i++) {
        Expr *e = args[i].get();
        
        if (!check_argument(i, e, scope, arg_tss, values))
            return false;
    }
            
    for (auto &kv : kwargs) {
        unsigned i = (unsigned)-1;
        
        for (unsigned j = 0; j < arg_names.size(); j++)
            if (arg_names[j] == kv.first) {
                i = j;
                break;
            }
            
        if (i == (unsigned)-1) {
            std::cerr << "No argument named " << kv.first << "!\n";
            return false;
        }
        
        Expr *e = kv.second.get();
        
        if (!check_argument(i, e, scope, arg_tss, values))
            return false;
    }
    
    return true;
}
