
// Values

class Value {
public:
    TypeSpec ts;
    Token token;
    
    Value(TypeSpec t)
        :ts(t) {
    }

    //virtual void set_ts(TypeSpec ts) {
    //    this->ts = ts;
    //}
    
    virtual Value *set_token(Token t) {
        token = t;
        return this;
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *) {
        return (args.size() == 0 && kwargs.size() == 0);
    }
    
    virtual StorageWhere complexity() {
        std::cerr << "This Value shouldn't have been compiled!\n";
        throw INTERNAL_ERROR;
    }
    
    virtual Storage compile(X64 *, Regs) {
        std::cerr << "This Value shouldn't have been compiled!\n";
        throw INTERNAL_ERROR;
    }
    
    virtual void compile_to_void(X64 *x64, Regs regs) {
        Storage s = compile(x64, regs);
        store(ts, s, Storage(), x64);
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

    virtual StorageWhere complexity() {
        return NOWHERE;
    }
    
    virtual Storage compile(X64 *x64, Regs regs) {
        // FIXME: this works for a bunch of declarations, but not in general

        if (items.size() == 1 && kwitems.size() == 0)
            return items[0]->compile(x64, regs);
            
        for (auto &item : items)
            item->compile_to_void(x64, regs);
            
        for (auto &kv : kwitems)
            kv.second->compile_to_void(x64, regs);
            
        return Storage();
    }
};


// Represents the argument list of a function
class FunctionHeadValue: public Value {
public:
    FunctionHeadScope *head_scope;
    
    FunctionHeadValue(FunctionHeadScope *s)
        :Value(BOGUS_TS) {
        head_scope = s;
    }
    
    virtual StorageWhere complexity() {
        return MEMORY;
    }
    
    virtual Storage compile(X64 *, Regs) {
        return Storage(MEMORY, Address(RBP, head_scope->offset));
    }
};


// Represents the local variable list of a function
class FunctionBodyValue: public Value {
public:
    FunctionBodyScope *body_scope;
    
    FunctionBodyValue(FunctionBodyScope *s)
        :Value(BOGUS_TS) {
        body_scope = s;
    }

    virtual StorageWhere complexity() {
        return MEMORY;
    }

    virtual Storage compile(X64 *, Regs) {
        //std::cerr << "XXX Body offset is " << body_scope->offset << "\n";
        return Storage(MEMORY, Address(RBP, body_scope->offset));
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
    
    virtual StorageWhere complexity() {
        return MEMORY;
    }

    virtual Storage compile(X64 *x64, Regs regs) {
        Storage s = pivot->compile(x64, regs);
        
        switch (s.where) {
        case MEMORY: return Storage(MEMORY, s.address + variable->offset);
        default: throw INTERNAL_ERROR;
        }
    }
};


// The value of a :function statement
class FunctionDefinitionValue: public Value {
public:
    std::unique_ptr<Value> ret;
    std::unique_ptr<Value> head;
    std::unique_ptr<Value> body;
    FunctionScope *fn_scope;
    
    Function *function;  // If declared with a name, which is always, for now
        
    FunctionDefinitionValue(TypeSpec fn_ts, Value *r, Value *h, Value *b, FunctionScope *f)
        :Value(fn_ts) {
        ret.reset(r);
        head.reset(h);
        body.reset(b);
        fn_scope = f;
    }
    
    void set_function(Function *f) {
        function = f;
    }
    
    virtual StorageWhere complexity() {
        return NOWHERE;
    }

    virtual Storage compile(X64 *x64, Regs regs) {
        //fn_scope->allocate();  // Hm, do we call all allocate-s in one step?
        
        unsigned frame_size = fn_scope->body_scope->size;

        if (function)
            x64->code_label_export(function->x64_label, function->name, 0, true);
        else {
            std::cerr << "Nameless function!\n";
            throw TYPE_ERROR;
        }
        
        x64->op(PUSHQ, RBP);
        x64->op(MOVQ, RBP, RSP);
        x64->op(SUBQ, RSP, frame_size);
        
        body->compile_to_void(x64, regs);
        
        // TODO: destructors
        x64->code_label(fn_scope->body_scope->get_rollback_label());
        x64->op(ADDQ, RSP, frame_size);
        x64->op(POPQ, RBP);
        x64->op(RET);
        
        return Storage();
    }
};


// The value of calling a function
class FunctionValue: public Value {
public:
    Function *function;
    std::unique_ptr<Value> pivot;
    std::vector<std::unique_ptr<Value>> items;  // FIXME
    
    FunctionValue(Function *f, Value *p)
        :Value(f->get_return_typespec()) {
        function = f;
        pivot.reset(p);
        
        for (unsigned i = 0; i < function->get_argument_count(); i++)
            items.push_back(NULL);
    }

    virtual bool check_arg(unsigned i, Value *v) {
        if (i >= items.size()) {
            std::cerr << "Too many arguments!\n";
            return false;
        }
    
        if (items[i]) {
            std::cerr << "Argument " << i << " already supplied!\n";
            return false;
        }
            
        TypeSpec var_ts = function->get_argument_typespec(i);
        
        if (!(v->ts >> var_ts)) {
            std::cerr << "Argument type mismatch, " << v->ts << " is not a " << var_ts << "!\n";
            return false;
        }
        
        items[i] = std::unique_ptr<Value>(v);
        return true;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        for (unsigned i = 0; i < args.size(); i++) {
            Expr *e = args[i].get();
            
            if (!check_arg(i, typize(e, scope)))
                return false;
        }
                
        for (auto &kv : kwargs) {
            unsigned i = function->get_argument_index(kv.first);
            if (i == (unsigned)-1) {
                std::cerr << "No argument named " << kv.first << "!\n";
                return false;
            }
            
            Expr *e = kv.second.get();
            
            if (!check_arg(i, typize(e, scope)))
                return false;
        }

        for (auto &item : items) {
            if (!item) {
                std::cerr << "Not all arguments supplied!\n";
                return false;
            }
        }
        
        return true;
    }
    
    virtual void sysv_prologue(X64 *x64, unsigned passed_size) {
        switch (passed_size) {
        case 0:
            break;
        case 8:
            x64->op(MOVQ, RDI, Address(RSP, 0));
            break;
        case 16:
            x64->op(MOVQ, RDI, Address(RSP, 8));
            x64->op(MOVQ, RSI, Address(RSP, 0));
            break;
        case 24:
            x64->op(MOVQ, RDI, Address(RSP, 16));
            x64->op(MOVQ, RSI, Address(RSP, 8));
            x64->op(MOVQ, RDX, Address(RSP, 0));
            break;
        case 32:
            x64->op(MOVQ, RDI, Address(RSP, 24));
            x64->op(MOVQ, RSI, Address(RSP, 16));
            x64->op(MOVQ, RDX, Address(RSP, 8));
            x64->op(MOVQ, RCX, Address(RSP, 0));
            break;
        default:
            std::cerr << "Oops, too many arguments to a SysV function!\n";
            throw INTERNAL_ERROR;
        }
    }
    
    virtual void sysv_epilogue(X64 *x64, unsigned passed_size) {
        x64->op(MOVQ, Address(ESP, passed_size), RAX);
    }
    
    virtual StorageWhere complexity() {
        return STACK;
    }

    virtual Storage compile(X64 *x64, Regs regs) {
        std::cerr << "Compiling call of " << function->name << "...\n";
        TypeSpec ret_ts = function->get_return_typespec();
        unsigned ret_size = round_up(measure(ret_ts));
        
        if (ret_size)
            x64->op(SUBQ, RSP, ret_size);
        
        unsigned passed_size = 0;
        
        if (pivot) {
            Storage s = pivot->compile(x64, regs);
            store(pivot->ts, s, Storage(STACK), x64);
            passed_size += round_up(measure(pivot->ts));
        }
        
        for (auto &item : items) {
            Storage s = item->compile(x64, regs);
            store(item->ts, s, Storage(STACK), x64);
            passed_size += round_up(measure(item->ts));
        }

        if (function->is_sysv && passed_size > 0)
            sysv_prologue(x64, passed_size);
        
        x64->op(CALL, function->x64_label);
        
        if (function->is_sysv && ret_size > 0)
            sysv_epilogue(x64, passed_size);
        
        for (int i = items.size() - 1; i >= 0; i--)
            store(items[i]->ts, Storage(STACK), Storage(), x64);
            
        if (pivot)
            store(pivot->ts, Storage(STACK), Storage(), x64);
            
        std::cerr << "Compiled call of " << function->name << ".\n";
        return ret_size ? Storage(STACK) : Storage();
    }
};


class TypeValue: public Value {
public:
    TypeValue(TypeSpec ts)
        :Value(ts) {
    }
    
    virtual Storage compile(X64 *, Regs) {
        return Storage();
    }
};


class FunctionReturnValue: public Value {
public:
    Scope *scope;
    Declaration *rollback_declaration;
    FunctionReturnScope *return_scope;
    std::unique_ptr<Value> value;
    
    FunctionReturnValue(Scope *s, Value *v)
        :Value(VOID_TS) {
        scope = s;
        value.reset(v);
        
        // This must be saved now, becaus it changes during the typization!
        rollback_declaration = scope->get_rollback_declaration();
        
        FunctionScope *fn_scope = NULL;
        
        // TODO: should be solved by some form of HardScope base class instead of looping
        for (Scope *s = scope; s; s = s->outer) {
            fn_scope = dynamic_cast<FunctionScope *>(s);
            if (fn_scope)
                break;
        }
                
        if (!fn_scope) {
            std::cerr << "Return statement outside of a function!\n";
            throw TYPE_ERROR;
        }
    
        return_scope = fn_scope->return_scope;
    }

    virtual StorageWhere complexity() {
        return NOWHERE;
    }
    
    virtual Storage compile(X64 *x64, Regs regs) {
        // TODO: destructors
        Storage s = value->compile(x64, regs);
        
        Declaration *decl = return_scope->contents[0].get();
        Variable *anon = dynamic_cast<Variable *>(decl);
        int ret_offset = return_scope->offset + anon->offset;
        Storage ret_storage(MEMORY, Address(RBP, ret_offset));
        store(value->ts, s, ret_storage, x64);
        
        x64->op(JMP, rollback_declaration->get_rollback_label());
        return Storage();
    }
};


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
            std::cerr << "Declaration needs a type " << args[0]->token << "!\n";
            return false;
        }
        else if (value->ts[0] == type_type) {
            TypeSpec var_ts;
            
            if (!scope->is_readonly())
                var_ts.push_back(lvalue_type);
            
            for (unsigned i = 1; i < value->ts.size(); i++)
                var_ts.push_back(value->ts[i]);
        
            Variable *variable = new Variable(name, VOID_TS, var_ts);
            decl = variable;
        }
        else if (value->ts[0] == function_type) {
            FunctionDefinitionValue *fdv = dynamic_cast<FunctionDefinitionValue *>(value.get());
            TypeSpec ret_ts;
            std::vector<TypeSpec> arg_tss;
            std::vector<std::string> arg_names;

            for (unsigned i = 1; i < fdv->ts.size(); i++)
                ret_ts.push_back(fdv->ts[i]);

            std::cerr << "It's a function with return type " << ret_ts << ".\n";

            Scope *head_scope = fdv->fn_scope->head_scope;
            unsigned n = head_scope->get_length();

            for (unsigned i = 0; i < n; i++) {
                Declaration *xd = head_scope->get_declaration(i);
                Variable *vd = dynamic_cast<Variable *>(xd);
                arg_tss.push_back(vd->var_ts);
                arg_names.push_back(vd->name);
            }

            Function *function = new Function(name, VOID_TS, arg_tss, arg_names, ret_ts);
            fdv->set_function(function);
            decl = function;
        }
        else {
            std::cerr << "Now what is this?\n";
            return false;
        }
            
        scope->add(decl);
        return true;
    }
    
    virtual StorageWhere complexity() {
        return NOWHERE;
    }

    virtual Storage compile(X64 *x64, Regs regs) {
        value->compile_to_void(x64, regs);  // no declaration by real values yet
        
        // TODO: eventually this must manage the rollback labels, too.
        return Storage();
    }
};


// This class must only return 32-bit constants as CONSTANT storage class,
// because that is what we can treat as immediate value in instructions.
// If a greater value is specified, it must be loaded into a register, and
// returned as REGISTER.

class NumberValue: public Value {
public:
    int number;
    
    NumberValue(std::string t)
        :Value(INTEGER_TS) {
        number = std::stoi(t);
    }

    virtual StorageWhere complexity() {
        return CONSTANT;
    }
    
    virtual Storage compile(X64 *, Regs) {
        return Storage(CONSTANT, number);
    }
};


#include "typize_values_integer.cpp"


class BooleanIfValue: public Value {
public:
    std::unique_ptr<Value> condition;
    std::unique_ptr<Value> then_branch;
    std::unique_ptr<Value> else_branch;
    
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

        std::cerr << "XXX boolean if " << !!then_branch << " " << !!else_branch << "\n";
        
        return true;
    }
    
    virtual Storage compile(X64 *x64, Regs regs) {
        Label then_end;
        Label else_end;
        then_end.allocate();
        else_end.allocate();
        
        Storage cs = condition->compile(x64, regs);
        
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
            
        case STACK:
            x64->op(XCHGQ, RAX, Address(RSP, 0));
            x64->op(TESTQ, RAX, 1);
            x64->op(POPQ, RAX);
            
            if (then_branch)
                x64->op(JNE, then_end);
            else if (else_branch)
                x64->op(JE, else_end);
                
            break;
            
        case MEMORY:
            x64->op(TESTB, cs.address, 1);

            if (then_branch)
                x64->op(JNE, then_end);
            else if (else_branch)
                x64->op(JE, else_end);
            
            break;
            
        default:
            throw INTERNAL_ERROR;
        }

        if (then_branch) {
            then_branch->compile_to_void(x64, regs);
            
            if (else_branch)
                x64->op(JMP, else_end);

            x64->code_label(then_end);
        }
        
        if (else_branch) {
            else_branch->compile_to_void(x64, regs);
            x64->code_label(else_end);
        }
    
        return Storage();
    }
};

TypeSpec get_typespec(Value *v) {
    return v ? v->ts : VOID_TS;
}


Value *make_function_head_value(FunctionHeadScope *s) {
    return new FunctionHeadValue(s);
}


Value *make_function_body_value(FunctionBodyScope *s) {
    return new FunctionBodyValue(s);
}


Value *make_function_return_value(Scope *s, Value *v) {
    return new FunctionReturnValue(s, v);
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

Value *make_integer_arithmetic_value(ArithmeticOperation o, TypeSpec t, Value *pivot) {
    return new IntegerArithmeticValue(o, t, pivot);
}

Value *make_boolean_if_value(Value *pivot) {
    return new BooleanIfValue(pivot);
}
