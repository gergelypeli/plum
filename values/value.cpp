
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
    
    virtual Variable *declare_impure(std::string name) {
        return new Variable(name, VOID_TS, ts.nonrvalue());
    }
    
    virtual Declaration *declare_pure(std::string name) {
        return NULL;
    }
};


class VariableValue: public Value {
public:
    Variable *variable;
    std::unique_ptr<Value> pivot;
    
    VariableValue(Variable *v, Value *p)
        :Value(v->var_ts) {
        variable = v;
        pivot.reset(p);
    }
    
    virtual Regs precompile(Regs) {
        return Regs();
    }
    
    virtual Storage compile(X64 *x64) {
        Storage s;
        
        if (pivot)
            s = pivot->compile(x64);
        else
            s = Storage(MEMORY, Address(RBP, 0));
        
        return variable->get_storage(s);
    }
};


class GenericOperationValue: public Value {
public:
    OperationType operation;
    TypeSpec arg_ts;
    std::unique_ptr<Value> left, right;
    Regs clob, rclob;
    Register reg;
    Storage ls, rs;
    
    GenericOperationValue(OperationType o, TypeSpec at, TypeSpec rt, Value *l)
        :Value(rt) {
        operation = o;
        arg_ts = at;
        left.reset(l);
        reg = NOREG;
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (is_unary(operation)) {
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

    virtual Register pick_early_register(Regs preferred) {
        if ((clob & ~rclob).has_gpr()) {
            return (clob & ~rclob).get_gpr();  // Great, the left side will have one
        }
        else if ((preferred & ~rclob).has_gpr()) {
            return (preferred & ~rclob).get_gpr();  // Good, we can allocate a preferred one
        }
        else if (rclob.count_gpr() >= 2) {
            return NOREG; // Okay, we'll be able to get one of these non-preferred registers
        }
        else {
            return (~rclob).get_gpr();  // Just allocate one more
        }
    }

    virtual Register pick_late_register() {
        return (clob & ~rs.regs()).get_gpr();
    }

    virtual Regs precompile(Regs preferred) {
        rclob = right ? right->precompile() : Regs();
        Regs pref = (preferred & ~rclob).has_gpr() ? preferred & ~rclob : preferred;  // must be nonempty
        Regs lclob = left->precompile(pref);
        clob = lclob | rclob;
        
        // We'll need a working general register that we can also use to pop
        // a clobbered left value back.
        reg = pick_early_register(preferred);
        if (reg != NOREG)
            clob.add(reg);
        
        return clob;
    }

    virtual void subcompile(X64 *x64) {
        ls = left->compile(x64);
        
        if (is_assignment(operation)) {
            if (ls.where != MEMORY)
                throw INTERNAL_ERROR;
                
            if (ls.is_clobbered(rclob)) {
                // We don't have an official support for stack-pushed addresses,
                // so just hack this in temporarily.
                x64->op(PUSHQ, ls.address.base);
                ls = Storage(CONSTANT, ls.address.offset);
            }
        }
        else {
            if (ls.is_clobbered(rclob)) {
                left->ts.store(ls, Storage(STACK), x64);
                ls = Storage(STACK);
            }
            else if (ls.where == MEMORY) {
                // TODO
                // We can't really gain anything by leaving the left side in memory,
                // but we would risk letting it altered before using it, so grab it now.
                // This is just an overcautious solution now, in the future we may
                // be smarter, and let MEMORY storage for the left side, once we make
                // sure the right side has no side effects.
                
                if (!rclob.has_gpr() && !rclob.has_ptr())
                    ;  // Okay, we can relax in this case
                else if (reg != NOREG) {
                    // We already know a register that won't be clobbered, use that
                    left->ts.store(ls, Storage(REGISTER, reg), x64);
                    ls = Storage(REGISTER, reg);
                }
                else {
                    // Nothing is sure, push it onto the stack
                    left->ts.store(ls, Storage(STACK), x64);
                    ls = Storage(STACK);
                }
            }
        }

        rs = right ? right->compile(x64) : Storage();
        
        if (rs.where == STACK) {
            std::cerr << "The decision was that basic values are not passed as STACK!\n";
            std::cerr << right->token << "\n";
            throw INTERNAL_ERROR;
        }
        
        if (reg == NOREG) {
            //std::cerr << "clob=" << clob.available << " rs regs=" << rs.regs().available << "\n";
            reg = pick_late_register();
        }
            
        if (is_assignment(operation)) {
            if (ls.where == CONSTANT) {
                // We hacked the pushed address, now get it back to a good register
                Register mreg = (~rs.regs()).get_ptr();
                //Register mreg = (rs.where == MEMORY && rs.address.base == RSI ? RDI : RSI);
                x64->op(POPQ, mreg);
                ls = Storage(MEMORY, Address(mreg, ls.value));
            }
        }
        else {
            if (ls.where == STACK) {
                left->ts.store(ls, Storage(REGISTER, reg), x64);
                ls = Storage(REGISTER, reg);
            }
        }
    }
};


#include "function.cpp"
#include "literal.cpp"
#include "integer.cpp"
#include "boolean.cpp"
#include "array.cpp"
#include "reference.cpp"
#include "block.cpp"
#include "type.cpp"


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
