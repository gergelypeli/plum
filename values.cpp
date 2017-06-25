
// Values

class Value {
public:
    TypeSpec ts;
    Token token;
    
    Value() {
    }

    virtual void set_ts(TypeSpec ts) {
        this->ts = ts;
    }
    
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
    
    virtual Storage compile(X64 *) {
        std::cerr << "This Value shouldn't have been compiled!\n";
        throw INTERNAL_ERROR;
    }
};


class BlockValue: public Value {
public:
    // FIXME: must keep kwarg order!
    std::vector<std::unique_ptr<Value>> items;
    std::map<std::string, std::unique_ptr<Value>> kwitems;

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        for (auto &arg : args)
            items.push_back(std::unique_ptr<Value>(typize(arg.get(), scope)));
            
        for (auto &kv : kwargs)
            kwitems.insert(decltype(kwitems)::value_type(kv.first, typize(kv.second.get(), scope)));
            
        if (items.size() == 1 && kwitems.size() == 0)
            set_ts(items[0]->ts);

        return true;
    }

    virtual StorageWhere complexity() {
        return NOWHERE;
    }
    
    virtual Storage compile(X64 *x64) {
        // FIXME: this works for a bunch of declarations, but not in general

        if (items.size() == 1 && kwitems.size() == 0)
            return items[0]->compile(x64);
            
        for (auto &item : items)
            item->compile(x64);
            
        for (auto &kv : kwitems)
            kv.second->compile(x64);
            
        return Storage();
    }
};


// Represents the argument list of a function
class FunctionHeadValue: public Value {
public:
    FunctionHeadScope *head_scope;
    
    FunctionHeadValue(FunctionHeadScope *s) {
        head_scope = s;
    }
    
    virtual StorageWhere complexity() {
        return MEMORY;
    }
    
    virtual Storage compile(X64 *) {
        return Storage(MEMORY, Address(RBP, head_scope->offset));
    }
};


// Represents the local variable list of a function
class FunctionBodyValue: public Value {
public:
    FunctionBodyScope *body_scope;
    
    FunctionBodyValue(FunctionBodyScope *s) {
        body_scope = s;
    }

    virtual StorageWhere complexity() {
        return MEMORY;
    }

    virtual Storage compile(X64 *) {
        return Storage(MEMORY, Address(RBP, body_scope->offset));
    }
};


class VariableValue: public Value {
public:
    Variable *variable;
    std::unique_ptr<Value> pivot;
    
    VariableValue(Variable *v, Value *p) {
        variable = v;
        pivot.reset(p);
        set_ts(v->var_ts);
    }
    
    virtual StorageWhere complexity() {
        return MEMORY;
    }

    virtual Storage compile(X64 *x64) {
        Storage s = pivot->compile(x64);
        
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
        
    FunctionDefinitionValue(Value *r, Value *h, Value *b, FunctionScope *f) {
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

    virtual Storage compile(X64 *x64) {
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
        
        body->compile(x64);
        
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
    
    FunctionValue(Function *f, Value *p) {
        function = f;
        pivot.reset(p);

        set_ts(function->get_return_typespec());
        
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

    virtual Storage compile(X64 *x64) {
        std::cerr << "Compiling call of " << function->name << "...\n";
        TypeSpec ret_ts = function->get_return_typespec();
        unsigned ret_size = round_up(measure(ret_ts));
        x64->op(SUBQ, RSP, ret_size);
        
        unsigned passed_size = 0;
        
        if (pivot) {
            Storage s = pivot->compile(x64);
            store(pivot->ts, s, Storage(STACK), x64);
            passed_size += round_up(measure(pivot->ts));
        }
        
        for (auto &item : items) {
            Storage s = item->compile(x64);
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
        return Storage(STACK);
    }
};


class TypeValue: public Value {
public:
    TypeValue(TypeSpec ts) {
        set_ts(ts);
    }
};


class FunctionReturnValue: public Value {
public:
    Scope *scope;
    Declaration *rollback_declaration;
    FunctionReturnScope *return_scope;
    std::unique_ptr<Value> value;
    
    FunctionReturnValue(Scope *s, Value *v) {
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
    
    virtual Storage compile(X64 *x64) {
        // TODO: destructors
        Storage s = value->compile(x64);
        
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
    
    DeclarationValue(std::string n) {
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

    virtual Storage compile(X64 *x64) {
        value->compile(x64);
        
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
    
    NumberValue(std::string t) {
        number = std::stoi(t);
        ts.push_back(integer_type);
    }

    virtual StorageWhere complexity() {
        return CONSTANT;
    }
    
    virtual Storage compile(X64 *) {
        return Storage(CONSTANT, number);
    }
};


class IntegerArithmeticValue: public Value {
public:
    ArithmeticOperation operation;
    TypeSpec arg_ts;
    std::unique_ptr<Value> left, right;
    
    IntegerArithmeticValue(ArithmeticOperation o, TypeSpec t, Value *pivot) {
        operation = o;
        arg_ts = t;
        left.reset(pivot);
        
        if (is_comparison(operation))
            ts.push_back(boolean_type);
        else
            ts = arg_ts;
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (is_unary(operation)) {
            if (args.size() != 0 || kwargs.size() != 0) {
                std::cerr << "Whacky integer unary operation!\n";
                return false;
            }
            
            return true;
        }
        else {
            if (args.size() != 1 || kwargs.size() != 0) {
                std::cerr << "Whacky integer binary operation!\n";
                return false;
            }

            Value *r = typize(args[0].get(), scope);
        
            if (!(r->ts >> arg_ts)) {
                std::cerr << "Incompatible right argument to integer binary operation!\n";
                return false;
            }
        
            right.reset(r);
            return true;
        }
    }

    virtual void exponentiation_by_squaring(X64 *x64) {
        // RAX = RBX ** RCX
        Label loop_label;
        Label skip_label;
        loop_label.allocate();
        skip_label.allocate();
        
        x64->op(MOVQ, RAX, 1);
        
        x64->code_label(loop_label);
        x64->op(TESTQ, RCX, 1);
        x64->op(JE, skip_label);
        x64->op(IMUL2Q, RAX, RBX);
        
        x64->code_label(skip_label);
        x64->op(IMUL2Q, RBX, RBX);
        x64->op(SHRQ, RCX, 1);
        x64->op(JNE, loop_label);
    }

    virtual Storage unary(X64 *x64, UnaryOp opcode, Storage ls) {
        switch (ls.where) {
        case CONSTANT: {
            int value = (opcode | 3) == NEGQ ? -ls.value : ~ls.value;
            return Storage(CONSTANT, value);
        }
        case REGISTER:
            x64->op(opcode, RAX);
            return Storage(REGISTER);
        case STACK:
            x64->op(opcode, Address(RSP));
            return Storage(STACK);
        case MEMORY:
            x64->op(MOVQ, RAX, ls.address);
            x64->op(opcode, RAX);
            return Storage(REGISTER);
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Storage binary_simple(X64 *x64, BinaryOp opcode, Storage ls, Storage rs) {
        // We swap the operands in some cases, which is not healthy for subtractions
        // and comparisons, so we return the swappedness as the storage value.
        
        switch (ls.where * rs.where) {
        case CONSTANT_CONSTANT: {
            int value = (
                (opcode | 3) == ADDQ ? ls.value + rs.value :
                (opcode | 3) == SUBQ ? ls.value - rs.value :
                (opcode | 3) == ANDQ ? ls.value & rs.value :
                (opcode | 3) == ORQ  ? ls.value | rs.value :
                (opcode | 3) == XORQ ? ls.value ^ rs.value :
                (opcode | 3) == CMPQ ? ls.value - rs.value :  // kinda special
                throw X64_ERROR
            );
            return Storage(CONSTANT, value);
        }
        case CONSTANT_REGISTER:
            x64->op(opcode, RAX, ls.value);
            return Storage(REGISTER, 1);
        case CONSTANT_STACK:
            x64->op(POPQ, RAX);
            x64->op(opcode, RAX, ls.value);
            return Storage(REGISTER, 1);
        case CONSTANT_MEMORY:
            x64->op(MOVQ, RAX, rs.address);
            x64->op(opcode, RAX, ls.value);
            return Storage(REGISTER, 1);
        case STACK_CONSTANT:
            x64->op(POPQ, RAX);
            x64->op(opcode, RAX, rs.value);
            return Storage(REGISTER);
        case STACK_REGISTER:
            x64->op(POPQ, RBX);
            x64->op(opcode, RAX, RBX);
            return Storage(REGISTER, 1);
        case STACK_STACK:
            x64->op(POPQ, RBX);
            x64->op(POPQ, RAX);
            x64->op(opcode, RAX, RBX);
            return Storage(REGISTER);
        case STACK_MEMORY:
            x64->op(POPQ, RAX);
            x64->op(opcode, RAX, rs.address);
            return Storage(REGISTER);
        case MEMORY_CONSTANT:
            x64->op(MOVQ, RAX, ls.address);
            x64->op(opcode, RAX, rs.value);
            return Storage(REGISTER);
        case MEMORY_REGISTER:
            x64->op(opcode, RAX, ls.address);
            return Storage(REGISTER, 1);
        case MEMORY_STACK:
            x64->op(POPQ, RAX);
            x64->op(opcode, RAX, ls.address);
            return Storage(REGISTER, 1);
        case MEMORY_MEMORY:
            x64->op(MOVQ, RAX, ls.address);
            x64->op(opcode, RAX, rs.address);
            return Storage(REGISTER);
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Storage binary_subtract(X64 *x64, Storage ls, Storage rs) {
        Storage s = binary_simple(x64, SUBQ, ls, rs);

        if (s.where == REGISTER && s.value) {
            x64->op(NEGQ, RAX);
            return Storage(REGISTER);
        }
        else
            return s;
    }

    virtual Storage binary_multiply(X64 *x64, Storage ls, Storage rs) {
        switch (ls.where * rs.where) {
        case CONSTANT_CONSTANT:
            return Storage(CONSTANT, ls.value * rs.value);
        case CONSTANT_REGISTER:
            x64->op(IMUL3Q, RAX, RAX, ls.value);
            return Storage(REGISTER);
        case CONSTANT_STACK:
            x64->op(POPQ, RAX);
            x64->op(IMUL3Q, RAX, RAX, ls.value);
            return Storage(REGISTER);
        case CONSTANT_MEMORY:
            x64->op(IMUL3Q, RAX, rs.address, ls.value);
            return Storage(REGISTER);
        case STACK_CONSTANT:
            x64->op(POPQ, RAX);
            x64->op(IMUL3Q, RAX, RAX, rs.value);
            return Storage(REGISTER);
        case STACK_REGISTER:
            x64->op(POPQ, RBX);
            x64->op(IMUL2Q, RAX, RBX);
            return Storage(REGISTER);
        case STACK_STACK:
            x64->op(POPQ, RBX);
            x64->op(POPQ, RAX);
            x64->op(IMUL2Q, RAX, RBX);
            return Storage(REGISTER);
        case STACK_MEMORY:
            x64->op(POPQ, RAX);
            x64->op(IMUL2Q, RAX, rs.address);
            return Storage(REGISTER);
        case MEMORY_CONSTANT:
            x64->op(IMUL3Q, RAX, ls.address, rs.value);
            return Storage(REGISTER);
        case MEMORY_REGISTER:
            x64->op(IMUL2Q, RAX, ls.address);
            return Storage(REGISTER);
        case MEMORY_STACK:
            x64->op(POPQ, RAX);
            x64->op(IMUL2Q, RAX, ls.address);
            return Storage(REGISTER);
        case MEMORY_MEMORY:
            x64->op(MOVQ, RAX, ls.address);
            x64->op(IMUL2Q, RAX, rs.address);
            return Storage(REGISTER);
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Storage binary_divmod(X64 *x64, Storage ls, Storage rs) {
        switch (ls.where * rs.where) {
        case CONSTANT_CONSTANT:
            return Storage(CONSTANT, 0);  // careful
        case CONSTANT_REGISTER:
            x64->op(MOVQ, RBX, RAX);
            x64->op(MOVQ, RAX, ls.value);
        case CONSTANT_STACK:
            x64->op(MOVQ, RAX, ls.value);
            x64->op(POPQ, RBX);
        case CONSTANT_MEMORY:
            x64->op(MOVQ, RAX, ls.value);  // will div from mem
        case STACK_CONSTANT:
            x64->op(POPQ, RAX);
            x64->op(MOVQ, RBX, rs.value);
        case STACK_REGISTER:
            x64->op(MOVQ, RBX, RAX);
            x64->op(POPQ, RAX);
        case STACK_STACK:
            x64->op(POPQ, RBX);
            x64->op(POPQ, RAX);
        case STACK_MEMORY:
            x64->op(POPQ, RAX);  // will div from mem
        case MEMORY_CONSTANT:
            x64->op(MOVQ, RAX, ls.address);
            x64->op(MOVQ, RBX, rs.value);
        case MEMORY_REGISTER:
            x64->op(MOVQ, RBX, RAX);
            x64->op(MOVQ, RAX, ls.address);
        case MEMORY_STACK:
            x64->op(MOVQ, RAX, ls.address);
            x64->op(POPQ, RBX);
        case MEMORY_MEMORY:
            x64->op(MOVQ, RAX, ls.address);  // will div from mem
        default:
            throw INTERNAL_ERROR;
        }

        x64->op(CQO);
        
        if (rs.where == MEMORY)
            x64->op(IDIVQ, rs.address);
        else
            x64->op(IDIVQ, RBX);

        return Storage(REGISTER);
    }

    virtual Storage binary_shift(X64 *x64, ShiftOp opcode, Storage ls, Storage rs) {
        switch (ls.where * rs.where) {
        case CONSTANT_CONSTANT:
            return Storage(CONSTANT, opcode == SHLQ ? ls.value << rs.value : ls.value >> rs.value);
        case CONSTANT_REGISTER:
            x64->op(MOVQ, RCX, RAX);
            x64->op(MOVQ, RAX, ls.value);
            x64->op(opcode, RAX, CL);
            return Storage(REGISTER);
        case CONSTANT_STACK:
            x64->op(MOVQ, RAX, ls.value);
            x64->op(POPQ, RCX);
            x64->op(opcode, RAX, CL);
            return Storage(REGISTER);
        case CONSTANT_MEMORY:
            x64->op(MOVQ, RAX, ls.value);
            x64->op(MOVQ, RCX, rs.address);
            x64->op(opcode, RAX, CL);
            return Storage(REGISTER);
        case STACK_CONSTANT:
            x64->op(POPQ, RAX);
            x64->op(opcode, RAX, rs.value);
            return Storage(REGISTER);
        case STACK_REGISTER:
            x64->op(MOVQ, RCX, RAX);
            x64->op(POPQ, RAX);
            x64->op(opcode, RAX, CL);
            return Storage(REGISTER);
        case STACK_STACK:
            x64->op(POPQ, RCX);
            x64->op(POPQ, RAX);
            x64->op(opcode, RAX, CL);
            return Storage(REGISTER);
        case STACK_MEMORY:
            x64->op(POPQ, RAX);
            x64->op(MOVQ, RCX, rs.address);
            x64->op(opcode, RAX, CL);
            return Storage(REGISTER);
        case MEMORY_CONSTANT:
            x64->op(MOVQ, RAX, ls.address);
            x64->op(opcode, RAX, rs.value);
            return Storage(REGISTER);
        case MEMORY_REGISTER:
            x64->op(MOVQ, RCX, RAX);
            x64->op(MOVQ, RAX, ls.address);
            x64->op(opcode, RAX, CL);
            return Storage(REGISTER);
        case MEMORY_STACK:
            x64->op(POPQ, RCX);
            x64->op(MOVQ, RAX, ls.address);
            x64->op(opcode, RAX, CL);
            return Storage(REGISTER);
        case MEMORY_MEMORY:
            x64->op(MOVQ, RAX, ls.address);
            x64->op(MOVQ, RCX, rs.address);
            x64->op(opcode, RAX, CL);
            return Storage(REGISTER);
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Storage binary_exponent(X64 *x64, Storage ls, Storage rs) {
        switch (rs.where) {
        case CONSTANT:
            x64->op(MOVQ, RCX, rs.value);
            break;
        case REGISTER:
            x64->op(MOVQ, RCX, RAX);
            break;
        case STACK:
            x64->op(POPQ, RCX);
            break;
        case MEMORY:
            x64->op(MOVQ, RCX, rs.address);
            break;
        default:
            throw INTERNAL_ERROR;
        }

        switch (ls.where) {
        case CONSTANT:
            x64->op(MOVQ, RBX, ls.value);
            break;
        case STACK:
            x64->op(POPQ, RBX);
            break;
        case MEMORY:
            x64->op(MOVQ, RBX, ls.address);
            break;
        default:
            throw INTERNAL_ERROR;
        }
            
        exponentiation_by_squaring(x64);
        
        return Storage(REGISTER);
    }

    virtual Storage binary_compare(X64 *x64, BitSetOp opcode, Storage ls, Storage rs) {
        Storage s = binary_simple(x64, CMPQ, ls, rs);
        
        if (s.where == CONSTANT) {
            bool holds = (
                (s.value < 0 && (opcode == SETNE || opcode == SETL || opcode == SETLE)) ||
                (s.value > 0 && (opcode == SETNE || opcode == SETG || opcode == SETGE)) ||
                (s.value == 0 && (opcode == SETE || opcode == SETLE || opcode == SETGE))
            );
            
            return Storage(CONSTANT, holds ? 1 : 0);
        }
        else if (s.where == REGISTER) {
            // Actually in the flags only yet, but must negate the condition if value is 1
            if (s.value)
                opcode = (BitSetOp)((int)opcode ^ 1);
                
            x64->op(opcode, AL);
            return Storage(REGISTER);
        }
        else
            throw INTERNAL_ERROR;
    }

    virtual Storage assign_binary(X64 *x64, BinaryOp opcode, Storage ls, Storage rs) {
        switch (rs.where) {
        case CONSTANT:
            x64->op(opcode, ls.address, rs.value);
            return ls;
        case REGISTER:
            x64->op(opcode, ls.address, RAX);
            return ls;
        case STACK:
            x64->op(POPQ, RAX);
            x64->op(opcode, ls.address, RAX);
            return ls;
        case MEMORY:
            x64->op(MOVQ, RAX, rs.address);
            x64->op(opcode, ls.address, RAX);
            return ls;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Storage assign_shift(X64 *x64, ShiftOp opcode, Storage ls, Storage rs) {
        switch (rs.where) {
        case CONSTANT:
            x64->op(opcode, ls.address, rs.value);
            return ls;
        case REGISTER:
            x64->op(MOVQ, RCX, RAX);
            x64->op(opcode, ls.address, CL);
            return ls;
        case STACK:
            x64->op(POPQ, RCX);
            x64->op(opcode, ls.address, CL);
            return ls;
        case MEMORY:
            x64->op(MOVQ, RCX, rs.address);
            x64->op(opcode, ls.address, CL);
            return ls;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual StorageWhere complexity() {
        if (is_unary(operation))
            return left->complexity() == CONSTANT ? CONSTANT : REGISTER;
        else
            return left->complexity() == CONSTANT && right->complexity() == CONSTANT ? CONSTANT : REGISTER;
    }

    virtual Storage compile(X64 *x64) {
        Storage ls = left->compile(x64);
        
        if (is_assignment(operation) && ls.where != MEMORY) {
            std::cerr << "Integer lvalue not on the frame!\n";
            throw INTERNAL_ERROR;
        }

        Storage rs;

        if (right) {
            StorageWhere c = right->complexity();

            if (ls.where == REGISTER && c != CONSTANT && c != MEMORY) {
                store(left->ts, ls, Storage(STACK), x64);
                ls = Storage(STACK);
            }
            
            rs = right->compile(x64);
            //store(right->ts, rs, Storage(REGISTER), x64);
        }
        
        switch (operation) {
        case COMPLEMENT:
            return unary(x64, NOTQ, ls);
        case NEGATE:
            return unary(x64, NEGQ, ls);
        case ADD:
            return binary_simple(x64, ADDQ, ls, rs);
        case SUBTRACT:
            return binary_subtract(x64, ls, rs);
        case MULTIPLY:
            return binary_multiply(x64, ls, rs);
        case DIVIDE: {
            Storage s = binary_divmod(x64, ls, rs);
            if (s.where == CONSTANT)
                s.value = ls.value / rs.value;
            return s;
        }
        case MODULO: {
            Storage s = binary_divmod(x64, ls, rs);
            if (s.where == CONSTANT)
                s.value = ls.value % rs.value;
            else if (s.where == REGISTER)
                x64->op(MOVQ, EAX, EDX);
            return s;
        }
        case OR:
            return binary_simple(x64, ORQ, ls, rs);
        case XOR:
            return binary_simple(x64, XORQ, ls, rs);
        case AND:
            return binary_simple(x64, ANDQ, ls, rs);
        case SHIFT_LEFT:
            return binary_shift(x64, SHLQ, ls, rs);
        case SHIFT_RIGHT:
            return binary_shift(x64, SHRQ, ls, rs);
        case EXPONENT:
            return binary_exponent(x64, ls, rs);
        case EQUAL:
            return binary_compare(x64, SETE, ls, rs);
        case NOT_EQUAL:
            return binary_compare(x64, SETNE, ls, rs);
        case LESS:
            return binary_compare(x64, SETL, ls, rs);
        case GREATER:
            return binary_compare(x64, SETG, ls, rs);
        case LESS_EQUAL:
            return binary_compare(x64, SETLE, ls, rs);
        case GREATER_EQUAL:
            return binary_compare(x64, SETGE, ls, rs);
        case INCOMPARABLE:
            return Storage(CONSTANT, 0);
        case ASSIGN:
            switch (rs.where) {
            case CONSTANT:
                x64->op(MOVQ, ls.address, rs.value);
                return ls;
            case REGISTER:
                x64->op(MOVQ, ls.address, RAX);
                return ls;
            case STACK:
                x64->op(POPQ, ls.address);
                return ls;
            case MEMORY:
                x64->op(MOVQ, RAX, rs.address);
                x64->op(MOVQ, ls.address, RAX);
                return ls;
            default:
                throw INTERNAL_ERROR;
            }
        case ASSIGN_ADD:
            return assign_binary(x64, ADDQ, ls, rs);
        case ASSIGN_SUBTRACT:
            return assign_binary(x64, SUBQ, ls, rs);
        case ASSIGN_MULTIPLY:
            binary_multiply(x64, ls, rs);
            x64->op(MOVQ, ls.address, RAX);
            return ls;
        case ASSIGN_DIVIDE:
            binary_divmod(x64, ls, rs);
            x64->op(MOVQ, ls.address, RAX);
            return ls;
        case ASSIGN_MODULO:
            binary_divmod(x64, ls, rs);
            x64->op(MOVQ, ls.address, RDX);
            return ls;
        case ASSIGN_EXPONENT:
            binary_exponent(x64, ls, rs);
            x64->op(MOVQ, ls.address, RAX);
            return ls;
        case ASSIGN_OR:
            return assign_binary(x64, ORQ, ls, rs);
        case ASSIGN_XOR:
            return assign_binary(x64, XORQ, ls, rs);
        case ASSIGN_AND:
            return assign_binary(x64, ANDQ, ls, rs);
        case ASSIGN_SHIFT_LEFT:
            return assign_shift(x64, SHLQ, ls, rs);
        case ASSIGN_SHIFT_RIGHT:
            return assign_shift(x64, SHRQ, ls, rs);
        default:
            std::cerr << "Unknown integer arithmetic operator!\n";
            throw INTERNAL_ERROR;
        }
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


Value *make_function_definition_value(Value *ret, Value *head, Value *body, FunctionScope *fn_scope) {
    return new FunctionDefinitionValue(ret, head, body, fn_scope);
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
