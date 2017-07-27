
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
};


class TypeValue: public Value {
public:
    TypeValue(TypeSpec ts)
        :Value(ts) {
    }
    
    virtual Regs precompile(Regs) {
        return Regs();
    }
    
    virtual Storage compile(X64 *) {
        return Storage();
    }
};


class VoidConversionValue: public Value {
public:
    std::unique_ptr<Value> orig;
    
    VoidConversionValue(Value *o)
        :Value(VOID_TS) {
        orig.reset(o);
    }
    
    virtual Regs precompile(Regs preferred) {
        return orig->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        Storage s = orig->compile(x64);
        Storage t = Storage();
        orig->ts.store(s, t, x64);
        return t;
    }
};


class BooleanConversionValue: public Value {
public:
    std::unique_ptr<Value> orig;
    
    BooleanConversionValue(Value *o)
        :Value(BOOLEAN_TS) {
        orig.reset(o);
    }
    
    virtual Regs precompile(Regs preferred) {
        return orig->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        Storage s = orig->compile(x64);
        return orig->ts.boolval(s, x64, false);
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
        
            Value *r = typize(args[0].get(), scope);
            //Value *cr = convertible(arg_ts, r);
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


class BlockValue: public Value {
public:
    std::vector<std::unique_ptr<Value>> statements;

    BlockValue()
        :Value(VOID_TS) {  // Will be overridden
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() < 2) {
            std::cerr << "Weird, I thought tuples contain at least two expressions!\n";
            throw INTERNAL_ERROR;
        }

        if (kwargs.size() > 0) {
            std::cerr << "Labeled statements make no sense!\n";
            throw TYPE_ERROR;
        }
        
        for (auto &arg : args) {
            Value *value = typize(arg.get(), scope);
            
            if (!declaration_value_cast(value))
                value = make_code_value(value);
                
            statements.push_back(std::unique_ptr<Value>(value));
        }
            
        ts = statements.back()->ts;  // TODO: rip code_type
            
        return true;
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob;
        
        for (unsigned i = 0; i < statements.size() - 1; i++)
            clob = clob | statements[i]->precompile();

        clob = clob | statements.back()->precompile(preferred);
            
        return clob;
    }

    virtual Storage compile(X64 *x64) {
        for (unsigned i = 0; i < statements.size() - 1; i++) {
            statements[i]->compile_and_store(x64, Storage());
            x64->op(NOP);  // For readability
        }
        
        return statements.back()->compile(x64);
    }
};


class CodeValue: public Value {
public:
    std::unique_ptr<Value> value;
    CodeScope *code_scope;

    CodeValue(Value *v)
        :Value(v->ts.rvalue().prefix(code_type)) {
        value.reset(v);
        code_scope = new CodeScope();
        value->marker.scope->intrude(value->marker.last, code_scope);
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        throw INTERNAL_ERROR;
    }

    virtual Regs precompile(Regs preferred) {
        return value->precompile(preferred);
    }

    virtual Storage compile(X64 *x64) {
        Storage s = value->compile(x64);
        code_scope->finalize_scope(Storage(MEMORY, Address(RBP, 0)), x64);
        return s;
    }
};


#include "function.cpp"
#include "literal.cpp"
#include "integer.cpp"
#include "boolean.cpp"
#include "array.cpp"
#include "reference.cpp"


class DeclarationValue: public Value {
public:
    std::string name;
    Declaration *decl;
    std::unique_ptr<Value> value;
    
    DeclarationValue(std::string n)
        :Value(VOID_TS) {
        name = n;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() != 1 || kwargs.size() != 0) {
            std::cerr << "Whacky declaration!\n";
            return false;
        }
        
        value.reset(typize(args[0].get(), scope));
        
        if (value->ts.size() == 0) {
            std::cerr << "Declaration needs something " << args[0]->token << "!\n";
            return false;
        }
        else if (value->ts[0] == function_type) {
            FunctionDefinitionValue *fdv = dynamic_cast<FunctionDefinitionValue *>(value.get());
            std::vector<TypeSpec> arg_tss;
            std::vector<std::string> arg_names;
            TypeSpec result_ts;
            
            fdv->get_interesting_stuff(arg_tss, arg_names, result_ts);
            std::cerr << "It's a function with result type " << result_ts << ".\n";

            Function *function = new Function(name, VOID_TS, arg_tss, arg_names, result_ts);
            fdv->set_function(function);
            decl = function;
        }
        else if (value->ts[0] == type_type) {
            TypeSpec var_ts = value->ts.unprefix(type_type);

            if (dynamic_cast<HeapType *>(var_ts[0]))
                var_ts = var_ts.prefix(reference_type);
            
            var_ts = var_ts.lvalue();
            ts = var_ts;
            
            Variable *variable = new Variable(name, VOID_TS, var_ts);
            decl = variable;
        }
        else if (value->ts[0] != void_type) {
            TypeSpec var_ts = value->ts.lvalue();
            ts = var_ts;
            
            Variable *variable = new Variable(name, VOID_TS, var_ts);
            decl = variable;
        }
        else {
            std::cerr << "Now what is this?\n";
            return false;
        }
            
        scope->add(decl);
        return true;
    }
    
    virtual Regs precompile(Regs preferred) {
        return value->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        Variable *v = dynamic_cast<Variable *>(decl);
        
        if (v) {
            // TODO: for references, we now need to first zero out the variable, then
            // the store will do an assignment. This could be simpler.
            Storage fn_storage(MEMORY, Address(RBP, 0));  // this must be a local variable
            Storage t = v->get_storage(fn_storage);
            v->var_ts.create(t, x64);

            Storage s = value->compile(x64);
            
            if (s.where != NOWHERE)
                v->var_ts.store(s, t, x64);
                
            return v->get_storage(Storage(MEMORY, Address(RBP, 0)));
        }
        else {
            value->compile_and_store(x64, Storage());
            return Storage();
        }
    }
};


DeclarationValue *declaration_value_cast(Value *value) {
    return dynamic_cast<DeclarationValue *>(value);
}


TypeSpec get_typespec(Value *value) {
    return value ? value->ts : VOID_TS;
}


Value *make_variable_value(Variable *decl, Value *pivot) {
    return new VariableValue(decl, pivot);
}


Value *make_function_value(Function *decl, Value *pivot) {
    return new FunctionValue(decl, pivot);
}


Value *make_type_value(TypeSpec ts) {
    return new TypeValue(ts);
}


Value *make_block_value() {
    return new BlockValue();
}


Value *make_declaration_value(std::string name) {
    return new DeclarationValue(name);
}


Value *make_number_value(std::string text) {
    return new NumberValue(text);
}


Value *make_string_value(std::string text) {
    return new StringValue(text);
}


Value *make_code_value(Value *orig) {
    return new CodeValue(orig);
}


Value *make_void_conversion_value(Value *p) {
    return new VoidConversionValue(p);
}


Value *make_boolean_conversion_value(Value *p) {
    return new BooleanConversionValue(p);
}
