
// Values

class Value {
public:
    TypeSpec ts;
    Token token;
    Marker marker;
    
    Value(TypeSpec t)
        :ts(t) {
    }

    virtual Value *set_token(Token t) {
        token = t;
        return this;
    }

    virtual Value *set_marker(Marker m) {
        marker = m;
        return this;
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        return (args.size() == 0 && kwargs.size() == 0);
    }
    
    virtual Regs precompile(Regs) {
        std::cerr << "This Value shouldn't have been precompiled!\n";
        throw INTERNAL_ERROR;
    }

    virtual Regs precompile() {
        return precompile(Regs::all());  // no particular preference
    }

    virtual Storage compile(X64 *) {
        std::cerr << "This Value shouldn't have been compiled!\n";
        throw INTERNAL_ERROR;
    }
    
    virtual void compile_and_store(X64 *x64, Storage t) {
        Storage s = compile(x64);
        //std::cerr << "Compiled and storing a " << ts << " from " << s << " to " << t << ".\n";
        ts.store(s, t, x64);
    }
    
    // DeclarationValue invokes these methods if this expression appears in a declaration.
    // In clode blocks first declare_impure is called, which may or may not return a Variable.
    // If not, then declare_pure is called, which must return any Declaration.
    // Data blocks invoke only declare_pure, and if that returns NULL, that's a semantic error.
    
    virtual Variable *declare_impure(std::string name, Scope *scope) {
        if (ts == VOID_TS)
            return NULL;
            
        return new Variable(name, VOID_TS, scope->variable_type_hint(ts.rvalue()));
    }
    
    virtual Declaration *declare_pure(std::string name, Scope *scope) {
        return NULL;
    }
};


class VariableValue: public Value {
public:
    Variable *variable;
    std::unique_ptr<Value> pivot;
    Register reg;
    
    VariableValue(Variable *v, Value *p)
        :Value(v->var_ts) {
        variable = v;
        pivot.reset(p);
        reg = NOREG;
    }
    
    virtual Regs precompile(Regs preferred) {
        Regs clob = pivot ? pivot->precompile(preferred) : Regs();
            
        if (!variable->xxx_is_allocated)
            throw INTERNAL_ERROR;
            
        if (variable->where == ALIAS) {
            // Just a sanity check, aliases are function arguments, and must have no pivot
            if (pivot)
                throw INTERNAL_ERROR;
                
            reg = preferred.get_any();
            //std::cerr << "Alias variable " << variable->name << " loaded to " << reg << "\n";
            return clob.add(reg);
        }
        else
            return clob;
    }
    
    virtual Storage compile(X64 *x64) {
        Storage s;
        
        if (pivot)
            s = pivot->compile(x64);
        else
            s = Storage(MEMORY, Address(RBP, 0));
        
        Storage t = variable->get_storage(s);
        
        if (t.where == ALIAS) {
            x64->op(MOVQ, reg, t.address);
            t = Storage(MEMORY, Address(reg, 0));
        }
        
        return t;    
    }
};


class GenericValue: public Value {
public:
    TypeSpec arg_ts;
    std::unique_ptr<Value> left, right;
    
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
};


class GenericOperationValue: public GenericValue {
public:
    OperationType operation;
    bool is_left_lvalue;
    Regs clob, rclob;
    Register reg;
    Storage ls, rs;
    
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
        
        rs = right ? right->compile(x64) : Storage();
        
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
            if (!rclob.has_any())
                throw INTERNAL_ERROR;
        
            Storage s(REGISTER, rclob.get_any());
            right->ts.store(rs, s, x64);
            rs = s;
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
};


#include "literal.cpp"
#include "type.cpp"
#include "block.cpp"
#include "function.cpp"
#include "integer.cpp"
#include "boolean.cpp"
#include "array.cpp"
#include "reference.cpp"
#include "record.cpp"


TypeSpec get_typespec(Value *value) {
    return value ? value->ts : VOID_TS;
}


Value *make_variable_value(Variable *decl, Value *pivot) {
    return new VariableValue(decl, pivot);
}


Value *make_function_call_value(Function *decl, Value *pivot) {
    return new FunctionCallValue(decl, pivot);
}


Value *make_type_value(TypeSpec ts) {
    return new TypeValue(ts);
}


Value *make_block_value(TypeSpec *context) {
    return new BlockValue(context);
}


Value *make_declaration_value(std::string name, TypeSpec *context) {
    return new DeclarationValue(name, context);
}


Value *make_basic_value(TypeSpec ts, int number) {
    return new BasicValue(ts, number);
}


Value *make_string_literal_value(std::string text) {
    return new StringLiteralValue(text);
}


Value *make_code_value(Value *value, bool escape_last) {
    return new CodeValue(value, escape_last);
}


Value *make_void_conversion_value(Value *p) {
    return new VoidConversionValue(p);
}


Value *make_boolean_conversion_value(Value *p) {
    return new BooleanConversionValue(p);
}


Value *make_boolean_not_value(Value *p) {
    TypeMatch match;
    
    if (!typematch(BOOLEAN_TS, p, match))
        throw INTERNAL_ERROR;
        
    return new BooleanOperationValue(COMPLEMENT, p, match);
}


Value *make_null_reference_value(TypeSpec ts) {
    return new NullReferenceValue(ts);
}


Value *make_unicode_character_value() {
    return new UnicodeCharacterValue();
}


Value *make_integer_definition_value() {
    return new IntegerDefinitionValue();
}


Value *make_enumeration_definition_value() {
    return new EnumerationDefinitionValue();
}


Value *make_record_definition_value() {
    return new RecordDefinitionValue();
}
