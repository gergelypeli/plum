
// The value of a :Function control
class FunctionDefinitionValue: public Value {
public:
    std::vector<std::unique_ptr<Value>> results;
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
        
        for (auto &arg : args) {
            Value *r = typize(arg.get(), scope);
            TypeMatch match;
        
            if (!typematch(ANY_TYPE_TS, r, match)) {
                std::cerr << "Function result expression is not a type!\n";
                return false;
            }
            
            results.push_back(std::unique_ptr<Value>(r));

            // Add internal result variable
            TypeSpec var_ts = r->ts.unprefix(type_type);
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
        body.reset(b ? typize(b, bs, &VOID_TS) : NULL);
        
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
        std::vector<TypeSpec> result_tss;

        for (auto &d : fn_scope->head_scope->contents) {
            // FIXME: with an (invalid here) nested declaration this can be a CodeScope, too
            Variable *v = dynamic_cast<Variable *>(d.get());
            
            if (v) {
                arg_tss.push_back(v->var_ts);  // FIXME
                arg_names.push_back(v->name);
                v->be_argument();
            }
        }

        // Not returned, but must be processed
        for (auto &d : fn_scope->self_scope->contents) {
            // FIXME: with an (invalid here) nested declaration this can be a CodeScope, too
            Variable *v = dynamic_cast<Variable *>(d.get());
            
            if (v) {
                v->be_argument();
            }
        }
        
        for (auto &d : fn_scope->result_scope->contents) {
            Variable *v = dynamic_cast<Variable *>(d.get());
            
            if (v) {
                result_tss.push_back(v->var_ts);  // FIXME
                v->be_argument();
            }
            else
                throw INTERNAL_ERROR;
        }
            
        function = new Function(name, scope->pivot_type_hint(), arg_tss, arg_names, result_tss);
        
        return function;
    }
};


// The value of calling a function
class FunctionCallValue: public Value {
public:
    Function *function;
    std::unique_ptr<Value> pivot;
    std::vector<std::unique_ptr<Value>> items;  // FIXME
    std::vector<Variable *> result_variables;
    Register reg;
    
    FunctionCallValue(Function *f, Value *p)
        :Value(BOGUS_TS) {
        function = f;
        pivot.reset(p);

        std::vector<TypeSpec> &res_tss = function->get_result_tss();
        
        if (res_tss.size() == 0)
            ts = f->get_pivot_typespec();
        else if (res_tss.size() == 1)
            ts = res_tss[0];
        else if (res_tss.size() > 1)
            ts = MULTI_TS;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        std::vector<TypeSpec> &res_tss = function->get_result_tss();
        
        for (auto &res_ts : res_tss) {
            result_variables.push_back(NULL);
            
            if (res_ts.where(true) == ALIAS) {
                Variable *result_variable = new Variable("<result>", VOID_TS, res_ts);
                scope->add(result_variable);
                result_variables.back() = result_variable;
            }
        }

        std::vector<TypeSpec> &arg_tss = function->get_argument_tss();
        std::vector<std::string> &arg_names = function->get_argument_names();
        
        bool ok = check_arguments(args, kwargs, scope, arg_tss, arg_names, items);
        if (!ok)
            return false;
        
        for (unsigned i = 0; i < items.size(); i++) {
            if (!items[i]) {
                TypeSpec arg_ts = arg_tss[i];
                
                if (arg_ts[0] != ovalue_type) {
                    std::cerr << "Argument " << i << " not supplied: " << arg_ts << "!\n";
                    return false;
                }

                std::cerr << "Argument " << i << " is omitted.\n";

                if (arg_ts.where(true) == ALIAS) {
                    // We can't just initialize an optional ALIAS, because it needs an
                    // allocated MEMORY storage first. So let's allocate it now.
                    
                    //DeclarationValue *dv = new DeclarationValue("<dummy>");
                    //Value *right = new TypeValue(arg_ts.unprefix(ovalue_type).prefix(type_type));
                    //dv->use(right, scope);
                    Value *dv = make_declaration_by_type("<dummy>", arg_ts.unprefix(ovalue_type), scope);
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
            reg = preferred.get_any();
        
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

    virtual Storage ret_res(TypeSpec res_ts, X64 *x64) {
        // Return a result from the stack in its native storage
        Storage s, t;
        
        switch (res_ts.where(true)) {
        case MEMORY:
            switch (res_ts.where(false)) {
            case REGISTER:
                s = Storage(STACK);
                t = Storage(REGISTER, reg);
                break;
            default:
                throw INTERNAL_ERROR;
            }
            break;
        case ALIAS:
            switch (res_ts.where(false)) {
            case MEMORY:
                // Pop the address into a MEMORY with a dynamic base register
                s = Storage(ALISTACK);
                t = Storage(MEMORY, Address(reg, 0));
                break;
            default:
                throw INTERNAL_ERROR;
            }
            break;
        default:
            throw INTERNAL_ERROR;
        }
        
        res_ts.store(s, t, x64);
        return t;
    }
    
    virtual Storage compile(X64 *x64) {
        //std::cerr << "Compiling call of " << function->name << "...\n";
        std::vector<TypeSpec> &arg_tss = function->get_argument_tss();
        std::vector<TypeSpec> &res_tss = function->get_result_tss();

        bool is_void = res_tss.size() == 0;
        
        for (unsigned i = 0; i < res_tss.size(); i++) {
            TypeSpec res_ts = res_tss[i];
            StorageWhere res_where = res_ts.where(true);
        
            if (res_where == ALIAS) {
                // pass an alias to the allocated result variable
                Storage s = result_variables[i]->get_storage(Storage(MEMORY, Address(RBP, 0)));
                res_ts.store(s, Storage(ALISTACK), x64);
            }
            else if (res_where == MEMORY) {
                // Must skip some place for uninitialized data
                x64->op(SUBQ, RSP, res_ts.measure(STACK));
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
            passed_size += push_arg(arg_tss[i], items[i].get(), x64);
            
        if (function->is_sysv && passed_size > 0)
            sysv_prologue(x64, passed_size);
        
        x64->op(CALL, function->x64_label);
        
        if (function->is_sysv && !is_void)
            sysv_epilogue(x64, passed_size);
        
        for (int i = items.size() - 1; i >= 0; i--)
            pop_arg(arg_tss[i], x64);
            
        if (pivot) {
            TypeSpec pivot_ts = function->get_pivot_typespec();
            
            if (is_void)
                return ret_res(pivot_ts, x64);
            
            pop_arg(pivot_ts, x64);
        }
            
        //std::cerr << "Compiled call of " << function->name << ".\n";
        if (is_void)
            return Storage();
        else if (res_tss.size() == 1)
            return ret_res(res_tss[0], x64);
        else
            return Storage(STACK);  // Multiple result values
    }
    
    virtual Variable *declare_dirty(std::string name, Scope *scope) {
        DeclarationValue *pivot_dv = declaration_value_cast(pivot.get());
        
        if (function->get_result_tss().size() == 0 && pivot_dv) {
            Variable *var = declaration_get_var(pivot_dv);
            
            if (var->name == "<new>") {
                var->name = name;
                return var;
            }
        }
            
        return NULL;
    }
};


class FunctionReturnValue: public Value {
public:
    std::vector<Variable *> result_vars;
    std::vector<std::unique_ptr<Value>> values;
    Declaration *dummy;
    
    FunctionReturnValue(OperationType o, Value *v, TypeMatch &m)
        :Value(VOID_TS) {
        if (v)
            throw INTERNAL_ERROR;
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
        
        result_vars = fn_scope->get_result_variables();

        if (result_vars.size() != args.size()) {
            std::cerr << "Wrong number of :return values!\n";
            return false;
        }

        for (unsigned i = 0; i < args.size(); i++) {
            Variable *result_var = result_vars[i];
            Expr *expr = args[i].get();
            
            TypeSpec result_ts = result_var->var_ts;
            Value *r = typize(expr, scope, &result_ts);
            TypeMatch match;
        
            if (!typematch(result_ts, r, match)) {
                std::cerr << "A :return control with incompatible value!\n";
                std::cerr << "Type " << get_typespec(r) << " is not " << result_ts << "!\n";
                return false;
            }

            values.push_back(std::unique_ptr<Value>(r));
        }

        // We must insert this after all potential declarations inside the result expression,
        // because we must finalize those variables upon return.
        dummy = new Declaration;
        scope->add(dummy);
        
        return true;
    }

    virtual Regs precompile(Regs) {
        for (auto &v : values)
            v->precompile();
            
        return Regs();  // We won't return
    }

    virtual Storage compile(X64 *x64) {
        Storage fn_storage(MEMORY, Address(RBP, 0));

        for (unsigned i = 0; i < values.size(); i++) {
            Storage s = values[i]->compile(x64);
            Storage t = result_vars[i]->get_storage(fn_storage);
            
            if (t.where == ALIAS) {
                // Load the address, and store the result there
                Register reg = (Regs::all() & ~s.regs()).get_any();
                Storage m = Storage(MEMORY, Address(reg, 0));
                result_vars[i]->var_ts.store(t, m, x64);
                t = m;
            }
                
            values[i]->ts.store(s, t, x64);
        }

        // TODO: is this proper stack unwinding?
        dummy->finalize(UNWINDING_FINALIZATION, fn_storage, x64);
        
        return Storage();
    }
};

