/*
// Represents the argument list of a function
class FunctionHeadValue: public Value {
public:
    Scope *head_scope;
    
    FunctionHeadValue(Scope *s)
        :Value(BOGUS_TS) {
        head_scope = s;
    }
    
    virtual Regs precompile(Regs) {
        return Regs();
    }
    
    virtual Storage compile(X64 *) {
        return head_scope->get_storage();
    }
};


// Represents the local variable list of a function
class FunctionBodyValue: public Value {
public:
    Scope *body_scope;
    
    FunctionBodyValue(Scope *s)
        :Value(BOGUS_TS) {
        body_scope = s;
    }

    virtual Regs precompile(Regs) {
        return Regs();
    }

    virtual Storage compile(X64 *) {
        //std::cerr << "XXX Body offset is " << body_scope->offset << "\n";
        return body_scope->get_storage();
    }
};
*/

// The value of a :function control
class FunctionDefinitionValue: public Value {
public:
    std::unique_ptr<Value> result;
    std::unique_ptr<Value> head;
    std::unique_ptr<Value> body;
    FunctionScope *fn_scope;
    
    Function *function;  // If declared with a name, which is always, for now
        
    FunctionDefinitionValue(TypeSpec fn_ts, Value *r, Value *h, Value *b, FunctionScope *f)
        :Value(fn_ts) {
        result.reset(r);
        head.reset(h);
        body.reset(b);
        fn_scope = f;
    }
    
    void set_function(Function *f) {
        function = f;
    }

    void get_interesting_stuff(std::vector<TypeSpec> &arg_tss, std::vector<std::string> &arg_names, TypeSpec &result_ts) {
        for (auto &d : fn_scope->head_scope->contents) {
            Variable *v = dynamic_cast<Variable *>(d.get());
            arg_tss.push_back(v->var_ts.rvalue());
            arg_names.push_back(v->name);
        }
        
        if (fn_scope->result_scope->contents.size()) {
            Variable *v = dynamic_cast<Variable *>(fn_scope->result_scope->contents.back().get());
            result_ts = v->var_ts.rvalue();
        }
        else
            result_ts = VOID_TS;
    }

    virtual Regs precompile(Regs) {
        body->precompile();
        return Regs();
    }
    
    virtual Storage compile(X64 *x64) {
        //fn_scope->allocate();  // Hm, do we call all allocate-s in one step?
        
        unsigned frame_size = fn_scope->get_frame_size();
        Label epilogue_label = fn_scope->get_epilogue_label();

        if (function)
            x64->code_label_export(function->x64_label, function->name, 0, true);
        else {
            std::cerr << "Nameless function!\n";
            throw TYPE_ERROR;
        }
        
        x64->op(PUSHQ, RBP);
        x64->op(MOVQ, RBP, RSP);
        x64->op(SUBQ, RSP, frame_size);
        
        body->compile_and_store(x64, Storage());
        
        // TODO: destructors
        //x64->code_label(fn_scope->body_scope->get_rollback_label());
        Storage s(MEMORY, Address(RBP, 0));
        fn_scope->finalize_scope(s, x64);
        
        x64->code_label(epilogue_label);
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
    
    virtual Regs precompile(Regs) {
        if (pivot)
            pivot->precompile();
        
        for (auto &item : items)
            item->precompile();
        
        return Regs::all();  // assume everything is clobbered
    }
    
    virtual Storage compile(X64 *x64) {
        //std::cerr << "Compiling call of " << function->name << "...\n";
        TypeSpec ret_ts = function->get_return_typespec();
        unsigned ret_size = stack_size(ret_ts.measure());
        
        if (ret_size)
            x64->op(SUBQ, RSP, ret_size);
        
        unsigned passed_size = 0;
        
        if (pivot) {
            Storage s = pivot->compile(x64);
            pivot->ts.store(s, Storage(STACK), x64);
            passed_size += stack_size(pivot->ts.measure());
        }
        
        for (auto &item : items) {
            Storage s = item->compile(x64);
            item->ts.store(s, Storage(STACK), x64);
            passed_size += stack_size(item->ts.measure());
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
            
        //std::cerr << "Compiled call of " << function->name << ".\n";
        // FIXME: basic types mustn't be returned as STACK anymore, implement!
        return ret_size ? Storage(STACK) : Storage();
    }
};


class FunctionReturnValue: public Value {
public:
    Variable *result_var;
    Declaration *marker;
    std::unique_ptr<Value> value;
    
    FunctionReturnValue(Variable *r, Declaration *m, Value *v)
        :Value(VOID_TS) {
        result_var = r;
        marker = m;
        value.reset(v);
    }

    virtual Regs precompile(Regs) {
        value->precompile();
        return Regs();  // We won't return
    }

    virtual Storage compile(X64 *x64) {
        Storage fn_storage(MEMORY, Address(RBP, 0));
        Storage s = value->compile(x64);
        Storage t = result_var->get_storage(fn_storage);

        value->ts.store(s, t, x64);

        // TODO: is this proper stack unwinding?
        marker->finalize(UNWINDING_FINALIZATION, fn_storage, x64);
        
        return Storage();
    }
};

