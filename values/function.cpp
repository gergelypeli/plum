
// Represents the argument list of a function
class FunctionHeadValue: public Value {
public:
    FunctionHeadScope *head_scope;
    
    FunctionHeadValue(FunctionHeadScope *s)
        :Value(BOGUS_TS) {
        head_scope = s;
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

    virtual Storage compile(X64 *, Regs) {
        //std::cerr << "XXX Body offset is " << body_scope->offset << "\n";
        return Storage(MEMORY, Address(RBP, body_scope->offset));
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
        
        body->compile_and_store(x64, regs, Storage());
        
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
        Value *cv = convertible(var_ts, v);
        
        if (!cv) {
            std::cerr << "Argument type mismatch, " << v->ts << " is not a " << var_ts << "!\n";
            return false;
        }
        
        items[i] = std::unique_ptr<Value>(cv);
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
    
    virtual Storage compile(X64 *x64, Regs regs) {
        std::cerr << "Compiling call of " << function->name << "...\n";
        TypeSpec ret_ts = function->get_return_typespec();
        unsigned ret_size = round_up(ret_ts.measure());
        
        if (ret_size)
            x64->op(SUBQ, RSP, ret_size);
        
        unsigned passed_size = 0;
        
        if (pivot) {
            Storage s = pivot->compile(x64, regs);
            pivot->ts.store(s, Storage(STACK), x64);
            passed_size += round_up(pivot->ts.measure());
        }
        
        for (auto &item : items) {
            Storage s = item->compile(x64, regs);
            item->ts.store(s, Storage(STACK), x64);
            passed_size += round_up(item->ts.measure());
        }

        if (function->is_sysv && passed_size > 0)
            sysv_prologue(x64, passed_size);
        
        x64->op(CALL, function->x64_label);
        
        if (function->is_sysv && ret_size > 0)
            sysv_epilogue(x64, passed_size);
        
        for (int i = items.size() - 1; i >= 0; i--)
            items[i]->ts.store(Storage(STACK), Storage(), x64);
            
        if (pivot)
            pivot->ts.store(Storage(STACK), Storage(), x64);
            
        std::cerr << "Compiled call of " << function->name << ".\n";
        return ret_size ? Storage(STACK) : Storage();
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

    virtual Storage compile(X64 *x64, Regs regs) {
        // TODO: destructors
        Storage s = value->compile(x64, regs);
        
        Declaration *decl = return_scope->contents[0].get();
        Variable *anon = dynamic_cast<Variable *>(decl);
        int ret_offset = return_scope->offset + anon->offset;
        Storage ret_storage(MEMORY, Address(RBP, ret_offset));
        value->ts.store(s, ret_storage, x64);
        
        x64->op(JMP, rollback_declaration->get_rollback_label());
        return Storage();
    }
};

