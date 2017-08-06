
// The value of a :function control
class FunctionDefinitionValue: public Value {
public:
    std::unique_ptr<Value> result;
    std::unique_ptr<Value> head;
    std::unique_ptr<Value> body;
    FunctionScope *fn_scope;
    
    Function *function;  // If declared with a name, which is always, for now
        
    FunctionDefinitionValue(OperationType o, Value *r, TypeMatch &match)
        :Value(METATYPE_TS) {
        //result.reset(r);
        function = NULL;
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        fn_scope = new FunctionScope();
        scope->add(fn_scope);

        Scope *rs = fn_scope->add_result_scope();
        
        if (args.size() == 1) {
            Value *r = typize(args[0].get(), scope);
            TypeMatch match;
        
            if (!typematch(ANY_TYPE_TS, r, match)) {
                std::cerr << "Function result expression is not a type!\n";
                return false;
            }
            
            result.reset(r);

            // Add internal result variable
            TypeSpec var_ts = result->ts.unprefix(type_type);
            Variable *decl = new Variable("<result>", VOID_TS, var_ts);
            rs->add(decl);
        }

        Scope *ss = fn_scope->add_self_scope();
        if (scope->is_pure()) {
            TypeSpec pivot_ts = scope->pivot_type_hint();
            
            if (pivot_ts != VOID_TS)
                ss->add(new Variable("$", VOID_TS, pivot_ts));
        }

        Scope *hs = fn_scope->add_head_scope();
        Expr *h = kwargs["from"].get();
        head.reset(h ? typize(h, hs) : NULL);
        
        Scope *bs = fn_scope->add_body_scope();
        Expr *b = kwargs["as"].get();
        body.reset(b ? typize(b, bs) : NULL);
        
        return true;
    }

    virtual Regs precompile(Regs) {
        body->precompile();
        return Regs();
    }
    
    virtual Storage compile(X64 *x64) {
        unsigned frame_size = fn_scope->get_frame_size();
        Label epilogue_label = fn_scope->get_epilogue_label();

        if (function)
            x64->code_label_export(function->x64_label, function->name, 0, true);
        else {
            std::cerr << "Nameless function!\n";
            throw INTERNAL_ERROR;
        }
        
        x64->op(PUSHQ, RBP);
        x64->op(MOVQ, RBP, RSP);
        x64->op(SUBQ, RSP, frame_size);
        
        body->compile_and_store(x64, Storage());
        x64->op(NOP);
        
        Storage s(MEMORY, Address(RBP, 0));
        fn_scope->finalize_scope(s, x64);
        
        x64->code_label(epilogue_label);
        x64->op(ADDQ, RSP, frame_size);
        x64->op(POPQ, RBP);
        x64->op(RET);
        
        return Storage();
    }

    virtual Variable *declare_impure(std::string name, Scope *scope) {
        return NULL;
    }
    
    virtual Declaration *declare_pure(std::string name, Scope *scope) {
        std::vector<TypeSpec> arg_tss;
        std::vector<std::string> arg_names;
        TypeSpec result_ts;

        for (auto &d : fn_scope->head_scope->contents) {
            // FIXME: with an (invalid here) nested declaration this can be a CodeScope, too
            Variable *v = dynamic_cast<Variable *>(d.get());
            
            if (v) {
                arg_tss.push_back(v->var_ts);  // FIXME
                arg_names.push_back(v->name);
                v->be_argument();
            }
        }

        for (auto &d : fn_scope->self_scope->contents) {
            // FIXME: with an (invalid here) nested declaration this can be a CodeScope, too
            Variable *v = dynamic_cast<Variable *>(d.get());
            
            if (v) {
                v->be_argument();
            }
        }
        
        if (fn_scope->result_scope->contents.size()) {
            Variable *v = dynamic_cast<Variable *>(fn_scope->result_scope->contents.back().get());
            
            if (v) {
                result_ts = v->var_ts;  // FIXME
                v->be_argument();
            }
            else
                throw INTERNAL_ERROR;
        }
        else
            result_ts = VOID_TS;
            
        function = new Function(name, scope->pivot_type_hint(), arg_tss, arg_names, result_ts);
        
        return function;
    }
};


// The value of calling a function
class FunctionCallValue: public Value {
public:
    Function *function;
    std::unique_ptr<Value> pivot;
    std::vector<std::unique_ptr<Value>> items;  // FIXME
    Variable *result_variable;
    Register reg;
    
    FunctionCallValue(Function *f, Value *p)
        :Value(f->get_return_typespec()) {
        function = f;
        pivot.reset(p);
        result_variable = NULL;
        
        for (unsigned i = 0; i < function->get_argument_count(); i++)
            items.push_back(NULL);
            
        if (ts == VOID_TS)
            ts = f->get_pivot_typespec();
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
        
        TypeMatch match;
        
        if (!typematch(var_ts, v, match)) {
            std::cerr << "Argument type mismatch, " << get_typespec(v) << " is not a " << var_ts << "!\n";
            return false;
        }
        
        items[i] = std::unique_ptr<Value>(v);
        return true;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        TypeSpec ret_ts = function->get_return_typespec();
        
        if (ret_ts != VOID_TS && ret_ts.where(true) == ALIAS) {
            result_variable = new Variable("<result>", VOID_TS, ret_ts);
            scope->add(result_variable);
        }

        for (unsigned i = 0; i < args.size(); i++) {
            Expr *e = args[i].get();
            TypeSpec var_ts = function->get_argument_typespec(i);
            
            if (!check_arg(i, typize(e, scope, &var_ts)))
                return false;
        }
                
        for (auto &kv : kwargs) {
            unsigned i = function->get_argument_index(kv.first);
            if (i == (unsigned)-1) {
                std::cerr << "No argument named " << kv.first << "!\n";
                return false;
            }
            
            Expr *e = kv.second.get();
            TypeSpec var_ts = function->get_argument_typespec(i);
            
            if (!check_arg(i, typize(e, scope, &var_ts)))
                return false;
        }

        for (unsigned i = 0; i < items.size(); i++) {
            if (!items[i]) {
                TypeSpec arg_ts = function->get_argument_typespec(i);
                if (arg_ts[0] != ovalue_type) {
                    std::cerr << "Argument " << i << " not supplied: " << arg_ts << "!\n";
                    return false;
                }

                std::cerr << "Argument " << i << " is omitted.\n";

                if (arg_ts.where(true) == ALIAS) {
                    // We can't just initialize an optional ALIAS, because it needs an
                    // allocated MEMORY storage first. So let's allocate it now.
                    
                    DeclarationValue *dv = new DeclarationValue("<dummy>");
                    Value *right = new TypeValue(arg_ts.unprefix(ovalue_type).prefix(type_type));
                    dv->use(right, scope);
                    items[i].reset(dv);
                    
                    std::cerr << "Argument " << i << " is now a dummy.\n";
                }
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
    
    virtual Regs precompile(Regs preferred) {
        if (pivot)
            pivot->precompile();
        
        for (auto &item : items)
            if (item)
                item->precompile();
        
        if (ts != VOID_TS)
            reg = ts.where(false) == REGISTER ? preferred.get_gpr() : preferred.get_ptr();
        
        return Regs::all();  // assume everything is clobbered
    }
    
    virtual int push_arg(TypeSpec arg_ts, Value *arg_value, X64 *x64) {
        StorageWhere where = arg_ts.where(true);
        where = (where == MEMORY ? STACK : where == ALIAS ? ALISTACK : throw INTERNAL_ERROR);

        if (arg_value) {
            // Specified argument
            arg_value->compile_and_store(x64, Storage(where));
        }
        else {
            // Optional argument
            arg_ts.create(Storage(), Storage(where), x64);
        }
        
        return arg_ts.measure(where);
    }

    virtual void pop_arg(TypeSpec arg_ts, X64 *x64) {
        StorageWhere where = arg_ts.where(true);
        where = (where == MEMORY ? STACK : where == ALIAS ? ALISTACK : throw INTERNAL_ERROR);
        
        arg_ts.store(Storage(where), Storage(), x64);
    }
    
    virtual Storage compile(X64 *x64) {
        //std::cerr << "Compiling call of " << function->name << "...\n";
        TypeSpec ret_ts = function->get_return_typespec();
        bool is_void = ret_ts == VOID_TS;
        Storage ret_storage;
        
        if (!is_void) {
            StorageWhere ret_where = ret_ts.where(true);
        
            if (ret_where == ALIAS) {
                // pass an alias to the allocated result variable
                ret_storage = result_variable->get_storage(Storage(MEMORY, Address(RBP, 0)));
                ret_ts.store(ret_storage, Storage(ALISTACK), x64);
            }
            else if (ret_where == MEMORY) {
                // Must skip some place for uninitialized data
                x64->op(SUBQ, RSP, ret_ts.measure(STACK));
            }
            else
                throw INTERNAL_ERROR;
        }
        
        unsigned passed_size = 0;
        
        if (pivot) {
            passed_size += push_arg(function->get_pivot_typespec(), pivot.get(), x64);
            //std::cerr << "Calling " << function->name << " with pivot " << function->get_pivot_typespec() << "\n";
        }
        
        for (unsigned i = 0; i < items.size(); i++)
            passed_size += push_arg(function->get_argument_typespec(i), items[i].get(), x64);
            
        if (function->is_sysv && passed_size > 0)
            sysv_prologue(x64, passed_size);
        
        x64->op(CALL, function->x64_label);
        
        if (function->is_sysv && !is_void)
            sysv_epilogue(x64, passed_size);
        
        for (int i = items.size() - 1; i >= 0; i--)
            pop_arg(function->get_argument_typespec(i), x64);
            
        if (pivot) {
            TypeSpec pivot_ts = function->get_pivot_typespec();
            
            if (is_void) {
                // Return the pivot argument instead of nothing.
                // Choose a popped storage just like with normal return values.
                
                switch (pivot_ts.where(true)) {
                case MEMORY:
                    switch (pivot_ts.where(false)) {
                    case REGISTER:
                        pivot_ts.store(Storage(STACK), Storage(REGISTER, reg), x64);
                        return Storage(REGISTER, reg);
                    default:
                        throw INTERNAL_ERROR;
                    }
                case ALIAS:
                    switch (pivot_ts.where(false)) {
                    case MEMORY:
                        // Pop the address into a MEMORY with a dynamic base register
                        pivot_ts.store(Storage(ALISTACK), Storage(MEMORY, Address(reg, 0)), x64);
                        return Storage(MEMORY, Address(reg, 0));
                    default:
                        throw INTERNAL_ERROR;
                    }
                default:
                    throw INTERNAL_ERROR;
                }
            }
            
            pop_arg(pivot_ts, x64);
        }
            
        //std::cerr << "Compiled call of " << function->name << ".\n";
        if (is_void)
            return Storage();
        else if (result_variable) {
            // No need to keep anything on the stack, the result is in a temp variable
            // at an RBP relative address (popping an ALISTACK to such MEMORY is illegal).
            ret_ts.store(Storage(ALISTACK), Storage(), x64);
            return ret_storage;
        }
        else {
            // Return value in a non-STACK storage
            
            switch (ret_ts.where(false)) {
            case REGISTER:
                ret_ts.store(Storage(STACK), Storage(REGISTER, reg), x64);
                return Storage(REGISTER, reg);
            default:
                throw INTERNAL_ERROR;
            }
        }
    }
};


class FunctionReturnValue: public Value {
public:
    Variable *result_var;
    std::unique_ptr<Value> result;
    Declaration *dummy;
    
    FunctionReturnValue(OperationType o, Value *v, TypeMatch &m)
        :Value(VOID_TS) {
        result_var = NULL;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (kwargs.size() != 0) {
            std::cerr << "Whacky :return!\n";
            return false;
        }

        FunctionScope *fn_scope = scope->get_function_scope();
        if (!fn_scope) {
            std::cerr << "A :return control outside of a function!\n";
            return false;
        }
        
        result_var = fn_scope->get_result_variable();

        if (result_var) {
            if (args.size() == 0) {
                std::cerr << "A :return control without value in a nonvoid function!\n";
                return false;
            }
            
            TypeSpec result_ts = result_var->var_ts;
            Value *r = typize(args[0].get(), scope, &result_ts);
            TypeMatch match;
            
            if (!typematch(result_ts, r, match)) {
                std::cerr << "A :return control with incompatible value!\n";
                std::cerr << "Type " << get_typespec(r) << " is not " << result_ts << "!\n";
                return false;
            }

            result.reset(r);
        }
        else {
            if (args.size() > 0) {
                std::cerr << "A :return control with value in a void function!\n";
                return false;
            }
        }

        // We must insert this after all potential declarations inside the result expression,
        // because we must finalize those variables upon return.
        dummy = new Declaration;
        scope->add(dummy);
        
        return true;
    }

    virtual Regs precompile(Regs) {
        if (result)
            result->precompile();
            
        return Regs();  // We won't return
    }

    virtual Storage compile(X64 *x64) {
        Storage fn_storage(MEMORY, Address(RBP, 0));

        if (result) {
            Storage s = result->compile(x64);
            Storage t = result_var->get_storage(fn_storage);
            
            if (t.where == ALIAS) {
                Register reg = (Regs::all_ptrs() & ~s.regs()).get_ptr();
                Storage m = Storage(MEMORY, Address(reg, 0));
                result_var->var_ts.store(t, m, x64);
                t = m;
            }
                
            result->ts.store(s, t, x64);
        }

        // TODO: is this proper stack unwinding?
        dummy->finalize(UNWINDING_FINALIZATION, fn_storage, x64);
        
        return Storage();
    }
};

