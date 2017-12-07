
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
        ArgInfos x;
        
        if (arg_ts != VOID_TS)
            x.push_back({ "arg", &arg_ts, scope, &right });
            
        return check_arguments(args, kwargs, x);
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
        is_left_lvalue = is_assignment(operation);
        reg = NOREG;
    }

    GenericOperationValue(TypeSpec at, TypeSpec rt, Value *l)
        :GenericValue(at, rt, l) {
        operation = TWEAK;
        is_left_lvalue = is_assignment(operation);
        reg = NOREG;
    }
    
    static TypeSpec op_arg_ts(OperationType o, TypeMatch &match) {
        return is_unary(o) ? VOID_TS : match[0].rvalue();
    }

    static TypeSpec op_ret_ts(OperationType o, TypeMatch &match) {
        return o == COMPARE ? INTEGER_TS : is_comparison(o) ? BOOLEAN_TS : match[0];
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
            if (left->ts.where(false) == REGISTER) {
                left->ts.store(ls, Storage(REGISTER, reg), x64);
                ls = Storage(REGISTER, reg);
            }
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

    virtual Storage compare(X64 *x64) {
        subcompile(x64);

        left->ts.compare(ls, rs, x64, reg);
        
        return Storage(REGISTER, reg);
    }

    virtual Storage compile(X64 *x64) {
        switch (operation) {
        case ASSIGN:
            return assign(x64);
        case COMPARE:
            return compare(x64);
        default:
            throw INTERNAL_ERROR;
        }
    }
    
    virtual Scope *unwind(X64 *x64) {
        left->ts.store(ls, Storage(), x64);
        return NULL;
    }
};


class UnwrapValue: public Value {
public:
    std::unique_ptr<Value> pivot;

    UnwrapValue(Value *p, TypeSpec internal_ts)
        :Value(internal_ts) {
        pivot.reset(p);
    }

    virtual Regs precompile(Regs preferred) {
        return pivot->precompile(preferred);
    }

    virtual Storage compile(X64 *x64) {
        return pivot->compile(x64);
    }
};


class WrapperValue: public GenericValue {
public:
    TypeSpec internal_arg_ts;
    std::string arg_name;

    WrapperValue(TypeSpec ats, TypeSpec rts, std::string an, Value *pivot)
        :GenericValue(ats, rts, pivot) {
        arg_name = an;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!GenericValue::check(args, kwargs, scope))
            return false;
            
        if (right) {
            GenericValue *generic_left = dynamic_cast<GenericValue *>(left.get());
            if (!generic_left)
                throw INTERNAL_ERROR;
            
            Value *k = right.release();
        
            if (arg_name.size())
                k = k->ts.lookup_inner(arg_name, k);
                
            generic_left->right.reset(k);
        }
        
        return true;
    }
    
    virtual Regs precompile(Regs preferred) {
        return left->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        Storage s = left->compile(x64);
        
        if (s.where == REGISTER && ts == STRING_TS) {
            x64->op(PUSHQ, s.reg);
            s = Storage(STACK);
        }
        
        return s;
    }
};


bool check_argument(unsigned i, Expr *e, const std::vector<ArgInfo> &arg_infos) {
    if (i >= arg_infos.size()) {
        std::cerr << "Too many arguments!\n";
        return false;
    }

    if (*arg_infos[i].target) {
        std::cerr << "Argument " << i << " already supplied!\n";
        return false;
    }

    CodeScope *code_scope = NULL;
    
    if ((*arg_infos[i].context)[0] == code_type) {
        code_scope = new CodeScope;
        arg_infos[i].scope->add(code_scope);
    }

    Value *v = typize(e, code_scope ? code_scope : arg_infos[i].scope, arg_infos[i].context);
    TypeMatch match;
    
    if (!typematch(*arg_infos[i].context, v, match, code_scope)) {
        std::cerr << "Argument type mismatch, " << get_typespec(v) << " is not a " << *arg_infos[i].context << "!\n";
        return false;
    }

    if (code_scope && !code_scope->is_taken)
        throw INTERNAL_ERROR;

    arg_infos[i].target->reset(v);
    return true;
}


bool check_arguments(Args &args, Kwargs &kwargs, const ArgInfos &arg_infos) {
    for (unsigned i = 0; i < args.size(); i++) {
        Expr *e = args[i].get();
        
        if (!check_argument(i, e, arg_infos))
            return false;
    }
            
    for (auto &kv : kwargs) {
        unsigned i = (unsigned)-1;
        
        for (unsigned j = 0; j < arg_infos.size(); j++) {
            if (arg_infos[j].name == kv.first) {
                i = j;
                break;
            }
        }
            
        if (i == (unsigned)-1) {
            std::cerr << "No argument named " << kv.first << "!\n";
            return false;
        }
        
        Expr *e = kv.second.get();
        
        if (!check_argument(i, e, arg_infos))
            return false;
    }
    
    return true;
}
