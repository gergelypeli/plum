
// Values

class Value {
public:
    TypeSpec ts;
    Token token;
    
    Value(TypeSpec t)
        :ts(t) {
    }

    virtual Value *set_token(Token t) {
        token = t;
        return this;
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *) {
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


class ConvertedValue: public Value {
public:
    std::unique_ptr<Value> orig;
    
    ConvertedValue(TypeSpec t, Value *o)
        :Value(t) {
        orig.reset(o);
    }
    
    virtual Regs precompile(Regs) {
        return Regs();  // TODO: allow the conversion to allocate registers?
    }
    
    virtual Storage compile(X64 *x64) {
        Storage s = orig->compile(x64);
        Storage t = orig->ts.convert(ts, s, x64);
        return t;
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
    NumericOperation operation;
    TypeSpec arg_ts;
    std::unique_ptr<Value> left, right;
    Regs clob, rclob;
    Register reg;
    Storage ls, rs;
    
    GenericOperationValue(NumericOperation o, TypeSpec at, TypeSpec rt, Value *l)
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
            Value *cr = convertible(arg_ts, r);
        
            if (!cr) {
                std::cerr << "Operation with incompatible argument!\n";
                return false;
            }
        
            right.reset(cr);
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


class ArrayOperationValue: public GenericOperationValue {
public:
    ArrayOperationValue(NumericOperation o, TypeSpec t, Value *l)
        :GenericOperationValue(o, t.rvalue(), is_comparison(o) ? BOOLEAN_TS : t, l) {
    }

    virtual Storage equal(X64 *x64, BitSetOp op) {
        subcompile(x64);
        
        switch (ls.where * rs.where) {
        case REGISTER_REGISTER:
            x64->decref(ls.reg);
            x64->decref(rs.reg);
            x64->op(CMPQ, ls.reg, rs.reg);
            return Storage(FLAGS, op);
        case REGISTER_MEMORY:
            x64->decref(ls.reg);
            x64->op(CMPQ, ls.reg, rs.address);
            return Storage(FLAGS, op);
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Storage assign(X64 *x64) {
        subcompile(x64);

        switch (ls.where * rs.where) {
        case MEMORY_REGISTER:
            x64->incref(rs.reg);
            x64->op(XCHGQ, rs.reg, ls.address);
            x64->decref(rs.reg);
            return ls;
        case MEMORY_MEMORY:
            x64->op(MOVQ, reg, rs.address);
            x64->incref(reg);
            x64->op(XCHGQ, reg, ls.address);
            x64->decref(reg);
            return ls;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Storage compile(X64 *x64) {
        switch (operation) {
        case EQUAL:
            return equal(x64, SETE);
        case NOT_EQUAL:
            return equal(x64, SETNE);
        case ASSIGN:
            return assign(x64);
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class ArrayItemValue: public GenericOperationValue {
public:
    Register mreg;

    ArrayItemValue(TypeSpec t, Value *a)  // FIXME: ADD?
        :GenericOperationValue(ADD, INTEGER_TS, t.rvalue().unprefix(reference_type).unprefix(array_type).lvalue(), a) {
    }

    virtual Register pick_early_register(Regs preferred) {
        // And we need to allocate a special address-only register for the real return value
        return preferred.has_ptr() ? preferred.get_ptr() : Regs::all_ptrs().get_ptr();
    }

    virtual Storage compile(X64 *x64) {
        int size = item_size(ts.measure());
        int offset = 8;
        
        subcompile(x64);
    
        // TODO: probably this is the point where we need to borrow a reference to the
        // array, and unborrow it sometimes later during the stack unwinding.
    
        // NOTE: the reg we selected is a PTR register, and that means that either
        // MEMORY side may be using it. So before we overwrite it, we must make sure
        // we already dereferenced it.
    
        switch (ls.where * rs.where) {
        case REGISTER_CONSTANT:
            x64->op(LEA, reg, Address(ls.reg, rs.value * size + offset));
            return Storage(MEMORY, Address(reg, 0));
        case REGISTER_REGISTER:
            x64->op(IMUL3Q, reg, rs.reg, size);
            x64->op(ADDQ, reg, ls.reg);
            return Storage(MEMORY, Address(reg, offset));
        case REGISTER_MEMORY:
            x64->op(IMUL3Q, reg, rs.address, size);  // reg may be the base of rs.address
            x64->op(ADDQ, reg, ls.reg);
            return Storage(MEMORY, Address(reg, offset));
        case MEMORY_CONSTANT:
            x64->op(MOVQ, reg, ls.address);  // reg may be the base of ls.address
            return Storage(MEMORY, Address(reg, rs.value * size + offset));
        case MEMORY_REGISTER:
            if (reg != ls.address.base) {
                x64->op(IMUL3Q, reg, rs.reg, size);  // reg is not the base of ls.address
                x64->op(ADDQ, reg, ls.address);
            }
            else {
                x64->op(IMUL3Q, rs.reg, rs.reg, size);
                x64->op(MOVQ, reg, ls.address);  // reg is the base of ls.address
                x64->op(ADDQ, reg, rs.reg);
            }
            return Storage(MEMORY, Address(reg, offset));
        case MEMORY_MEMORY:
            if (reg != ls.address.base) {
                x64->op(IMUL3Q, reg, rs.address, size);  // reg may be the base of rs.address
                x64->op(ADDQ, reg, ls.address);
            }
            else {
                x64->op(MOVQ, reg, ls.address);  // reg is the base of ls.address
                x64->op(IMUL3Q, RBX, rs.address, size);
                x64->op(ADDQ, reg, RBX);
            }
            return Storage(MEMORY, Address(reg, offset));
        default:
            throw INTERNAL_ERROR;
        }
    }    
};


class ArrayConcatenationValue: public GenericOperationValue {
public:
    ArrayConcatenationValue(TypeSpec t, Value *l)
        :GenericOperationValue(ADD /*FIXME*/, t, t, l) {
    }

    virtual Regs precompile(Regs preferred) {
        GenericOperationValue::precompile(preferred);
        return Regs().add(RAX).add(RBX).add(RCX).add(RSI).add(RDI);
    }

    virtual Storage compile(X64 *x64) {
        // TODO: this only works for arrays of basic types now, that can be just copied
        // TODO: don't inline this
        int size = item_size(ts.unprefix(reference_type).unprefix(array_type).measure());
        
        subcompile(x64);
        
        left->ts.store(ls, Storage(STACK), x64);
        right->ts.store(rs, Storage(STACK), x64);
        
        x64->op(MOVQ, RAX, Address(RSP, 8));
        x64->op(MOVQ, RBX, Address(RAX, 0));
        
        x64->op(MOVQ, RAX, Address(RSP, 0));
        x64->op(ADDQ, RBX, Address(RAX, 0));  // total size
        
        x64->op(IMUL3Q, RAX, RBX, size);
        x64->op(ADDQ, RAX, 8);
        
        x64->alloc();  // Allocate this many bytes with a refcount of 1, return in RAX
        
        x64->op(MOVQ, Address(RAX, 0), RBX);
        x64->op(LEA, RDI, Address(RAX, 8));
        
        x64->op(MOVQ, RSI, Address(RSP, 8));
        x64->op(IMUL3Q, RCX, Address(RSI, 0), size);
        x64->op(ADDQ, RSI, 8);
        x64->op(REPMOVSB);

        x64->op(MOVQ, RSI, Address(RSP, 0));
        x64->op(IMUL3Q, RCX, Address(RSI, 0), size);
        x64->op(ADDQ, RSI, 8);
        x64->op(REPMOVSB);
        
        right->ts.store(Storage(STACK), Storage(), x64);
        left->ts.store(Storage(STACK), Storage(), x64);
        
        return Storage(REGISTER, RAX);
    }
};


class BlockValue: public Value {
public:
    // FIXME: must keep kwarg order!
    std::vector<std::unique_ptr<Value>> items;
    std::map<std::string, std::unique_ptr<Value>> kwitems;

    BlockValue()
        :Value(VOID_TS) {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        for (auto &arg : args)
            items.push_back(std::unique_ptr<Value>(typize(arg.get(), scope)));
            
        for (auto &kv : kwargs)
            kwitems.insert(decltype(kwitems)::value_type(kv.first, typize(kv.second.get(), scope)));
            
        if (items.size() == 1 && kwitems.size() == 0)
            ts = items[0]->ts;  // maybe we should get this in the constructor instead?

        std::cerr << "BlockValue " << token << " ts: " << ts << "\n";
        return true;
    }

    virtual Regs precompile(Regs preferred) {
        if (items.size() == 1 && kwitems.size() == 0)
            return items[0]->precompile(preferred);
            
        for (auto &item : items)
            item->precompile();
            
        for (auto &kv : kwitems)
            kv.second->precompile();
            
        return Regs::all();
    }

    virtual Storage compile(X64 *x64) {
        // FIXME: this works for a bunch of declarations, but not in general

        if (items.size() == 1 && kwitems.size() == 0)
            return items[0]->compile(x64);
            
        for (auto &item : items) {
            item->compile_and_store(x64, Storage());
            x64->op(NOP);  // For readability
        }
            
        for (auto &kv : kwitems) {
            kv.second->compile_and_store(x64, Storage());
            x64->op(NOP);  // For readability
        }
            
        return Storage();
    }
};


#include "function.cpp"
#include "literal.cpp"
#include "integer.cpp"
#include "boolean.cpp"


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
            
            Variable *variable = new Variable(name, VOID_TS, var_ts);
            decl = variable;
        }
        else if (value->ts[0] != void_type) {
            TypeSpec var_ts = value->ts.lvalue();
            
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
        }
        else
            value->compile_and_store(x64, Storage());
        
        // TODO: eventually this must manage the rollback labels, too.
        return Storage();
    }
};


Value *convertible(TypeSpec to, Value *value) {
    return value ? value->ts.convertible(to, value) : NULL;
}


TypeSpec get_typespec(Value *value) {
    return value ? value->ts : VOID_TS;
}

/*
Value *make_function_head_value(FunctionHeadScope *s) {
    return new FunctionHeadValue(s);
}


Value *make_function_body_value(FunctionBodyScope *s) {
    return new FunctionBodyValue(s);
}
*/

Value *make_function_return_value(Variable *result_var, Declaration *marker, Value *value) {
    return new FunctionReturnValue(result_var, marker, value);
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


Value *make_function_definition_value(TypeSpec fn_ts, Value *ret, Value *head, Value *body, FunctionScope *fn_scope) {
    return new FunctionDefinitionValue(fn_ts, ret, head, body, fn_scope);
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


Value *make_integer_operation_value(NumericOperation o, TypeSpec t, Value *pivot) {
    return new IntegerOperationValue(o, t, pivot);
}


Value *make_boolean_operation_value(NumericOperation o, Value *pivot) {
    std::cerr << "YYY: " << (int)o << " " << pivot->ts << "\n";
    return new BooleanOperationValue(o, pivot);
}


Value *make_boolean_if_value(Value *pivot) {
    return new BooleanIfValue(pivot);
}


Value *make_converted_value(TypeSpec ts, Value *orig) {
    return new ConvertedValue(ts, orig);
}


Value *make_array_item_value(TypeSpec t, Value *array) {
    return new ArrayItemValue(t, array);
}


Value *make_array_concatenation_value(TypeSpec t, Value *array) {
    return new ArrayConcatenationValue(t, array);
}
