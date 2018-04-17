
// The value of a :Function control
class FunctionDefinitionValue: public Value {
public:
    std::vector<std::unique_ptr<Value>> results;
    std::unique_ptr<DataBlockValue> head;
    std::unique_ptr<Value> body;
    std::unique_ptr<Value> exception_type_value;
    FunctionScope *fn_scope;
    Expr *deferred_body_expr;
    bool may_be_aborted;
    TypeSpec pivot_ts;
    Variable *self_var;
    TypeMatch match;
    RoleScope *role_scope;

    FunctionType type;
    std::string import_name;
    Function *function;  // If declared with a name, which is always, for now
        
    FunctionDefinitionValue(Value *r, TypeMatch &tm)
        :Value(HYPERTYPE_TS) {
        match = tm;
        type = GENERIC_FUNCTION;
        function = NULL;
        deferred_body_expr = NULL;
        may_be_aborted = false;
        self_var = NULL;
        role_scope = NULL;
        fn_scope = NULL;
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        // For later
        role_scope = ptr_cast<RoleScope>(scope);
        
        fn_scope = new FunctionScope();
        scope->add(fn_scope);

        Scope *rs = fn_scope->add_result_scope();
        
        for (auto &arg : args) {
            Value *r = typize(arg.get(), scope);
        
            if (!r->ts.is_meta()) {
                std::cerr << "Function result expression is not a type!\n";
                return false;
            }

            TypeSpec var_ts = ptr_cast<TypeValue>(r)->represented_ts;

            if (!var_ts.has_meta(value_metatype)) {
                std::cerr << "Function result expression is not a value type!\n";
                return false;
            }
            
            results.push_back(std::unique_ptr<Value>(r));

            // Add internal result variable
            Variable *decl = new Variable("<result>", NO_TS, var_ts);
            rs->add(decl);
        }

        Scope *ss = fn_scope->add_self_scope();
        if (scope->type == DATA_SCOPE) {
            pivot_ts = scope->pivot_type_hint();
            
            if (pivot_ts != NO_TS && pivot_ts != ANY_TS) {
                if (type == INITIALIZER_FUNCTION) {
                    pivot_ts = pivot_ts.prefix(initializable_type);
                    TypeSpec partial_ts = pivot_ts.reprefix(initializable_type, partial_type);
                    self_var = new PartialVariable("$", NO_TS, partial_ts);
                }
                else
                    self_var = new Variable("$", NO_TS, pivot_ts);
                
                ss->add(self_var);
                
                if (role_scope) {
                    // Function overrides must pretend they take the role pivot type
                    TypeMatch tm = pivot_ts.match();
                    role_scope->get_role()->compute_match(tm);
                    pivot_ts = tm[0].prefix(weakref_type);
                }
            }
        }

        // TODO: why do we store this in the fn scope?
        Expr *e = kwargs["may"].get();
        if (e) {
            TypeSpec TREENUMMETA_TS = { treenumeration_metatype };
            Value *v = typize(e, fn_scope, &TREENUMMETA_TS);
            
            if (v) {
                TreenumerationType *t = NULL;
                TreenumerationDefinitionValue *tdv = ptr_cast<TreenumerationDefinitionValue>(v);
                
                if (tdv) {
                    Declaration *ed = tdv->declare("<may>", scope->type);
                    t = ptr_cast<TreenumerationType>(ed);
                    fn_scope->add(t);
                }
                
                TypeValue *tpv = ptr_cast<TypeValue>(v);
                if (tpv) {
                    t = ptr_cast<TreenumerationType>(tpv->represented_ts[0]);
                }
                
                if (t) {
                    fn_scope->set_exception_type(t);
                }
                else
                    throw INTERNAL_ERROR;
            }
            else
                throw INTERNAL_ERROR;
                
            exception_type_value.reset(v);
        }
                
        Scope *hs = fn_scope->add_head_scope();
        Expr *h = kwargs["from"].get();
        head.reset(new DataBlockValue(hs));

        if (h) {
            if (h->type == Expr::TUPLE) {
                for (auto &expr : h->args)
                    if (!head->check_statement(expr.get()))
                        return false;
            }
            else {
                if (!head->check_statement(h))
                    return false;
            }
        }
        
        deferred_body_expr = kwargs["as"].get();
        std::cerr << "Deferring definition of function body.\n";

        Expr *import_expr = kwargs["import"].get();

        if (import_expr) {
            Value *i = typize(import_expr, scope, &STRING_TS);
            StringLiteralValue *sl = ptr_cast<StringLiteralValue>(i);
            
            if (!sl) {
                std::cerr << "Function import is not a string literal!\n";
                return false;
            }
            
            import_name = sl->text;
        }
        
        if (deferred_body_expr && import_expr) {
            std::cerr << "Can't specify function body and import!\n";
            return false;
        }

        if (!deferred_body_expr && !import_expr) {
            std::cerr << "Must specify function body or import!\n";
            return false;
        }
        
        // TODO: warn for invalid keywords!
        
        return true;
    }

    virtual bool complete_definition() {
        std::cerr << "Completing definition of function body " << function->name << ".\n";
        Scope *bs = fn_scope->add_body_scope();

        if (deferred_body_expr) {
            PartialVariable *pv = NULL;
            
            if (fn_scope->self_scope->contents.size() > 0)
                pv = ptr_cast<PartialVariable>(fn_scope->self_scope->contents.back().get());
                
            if (pv) {
                // Must do this only after the class definition is completed
                if (pv->alloc_ts[1] == weakref_type) {
                    // TODO: this should also work for records
                    ClassType *ct = ptr_cast<ClassType>(pv->alloc_ts[2]);
                    if (!ct)
                        throw INTERNAL_ERROR;
                    
                    pv->set_member_names(ct->get_member_names());
                }
                else {
                    RecordType *rt = ptr_cast<RecordType>(pv->alloc_ts[1]);
                    if (!rt)
                        throw INTERNAL_ERROR;
                    
                    pv->set_member_names(rt->get_member_names());
                }
            }
        
            // The body is in a separate CodeScope, but instead of a dedicated CodeValue,
            // we'll handle its compilation.
            Value *bv = typize(deferred_body_expr, bs, &VOID_CODE_TS);
            body.reset(bv);
            
            if (fn_scope->result_scope->contents.size()) {
                // TODO: this is a very lame check for a mandatory :return, but we should
                // probably have a NORETURN type for this.

                FunctionReturnValue *rv = ptr_cast<FunctionReturnValue>(bv);
                
                if (!rv) {
                    CodeBlockValue *cbv = ptr_cast<CodeBlockValue>(bv);
                    if (!cbv)
                        throw INTERNAL_ERROR;
                    
                    Value *last_statement = cbv->statements.back().get();
                    CodeScopeValue *csv = ptr_cast<CodeScopeValue>(last_statement);
                    if (!csv)
                        throw INTERNAL_ERROR;
                    
                    rv = ptr_cast<FunctionReturnValue>(csv->value.get());
                    
                    if (!rv) {
                        std::cerr << "Non-Void function " << function->name << " does not end with a :return control: " << token << "\n";
                        return false;
                    }
                }
            }
            
            if (pv) {
                if (!pv->is_complete()) {
                    std::cerr << "Not all members initialized!\n";
                    return false;
                }
            }
        }

        return true;
    }

    virtual Regs precompile(Regs) {
        if (body)
            body->precompile();
            
        return Regs();
    }
    
    virtual Storage compile(X64 *x64) {
        if (!body)
            return Storage();
            
        if (exception_type_value)
            exception_type_value->compile(x64);  // to compile treenum definitions
    
        unsigned frame_size = fn_scope->get_frame_size();
        //Label epilogue_label = fn_scope->get_epilogue_label();

        int containing_role_offset = (role_scope ? role_scope->get_role()->compute_offset(match) : 0);
        if (containing_role_offset)
            std::cerr << "Function body of " << function->name << " has containing role offset " << containing_role_offset << ".\n";
        
        Storage self_storage = self_var ? self_var->get_local_storage() : Storage();

        if (function) {
            if (function->name == "start")
                x64->code_label_global(function->get_label(x64), function->name);
            else
                x64->code_label_local(function->get_label(x64), function->name);
        }
        else {
            std::cerr << "Nameless function!\n";
            throw INTERNAL_ERROR;
        }
        
        x64->op(PUSHQ, RBP);
        x64->op(MOVQ, RBP, RSP);
        x64->op(SUBQ, RSP, frame_size);
        x64->op(MOVB, EXCEPTION_ADDRESS, NO_EXCEPTION);
        x64->op(MOVQ, RESULT_ALIAS_ADDRESS, RAX);
        
        if (containing_role_offset)
            x64->op(SUBQ, self_storage.address, containing_role_offset);
        
        x64->unwind->push(this);
        body->compile_and_store(x64, Storage());
        x64->unwind->pop(this);
        
        x64->op(NOP);

        fn_scope->body_scope->finalize_contents(x64);
        
        if (may_be_aborted) {
            Label ok;
            x64->op(CMPB, EXCEPTION_ADDRESS, RETURN_EXCEPTION);
            x64->op(JNE, ok);
            x64->op(MOVB, EXCEPTION_ADDRESS, NO_EXCEPTION);
            x64->code_label(ok);
        }

        if (containing_role_offset)
            x64->op(ADDQ, self_storage.address, containing_role_offset);
        
        x64->op(MOVB, BL, EXCEPTION_ADDRESS);
        x64->op(ADDQ, RSP, frame_size);
        x64->op(CMPB, BL, 0);  // ZF => OK
        x64->op(POPQ, RBP);
        x64->op(RET);
        
        return Storage();
    }

    virtual Scope *unwind(X64 *x64) {
        may_be_aborted = true;
        return fn_scope->body_scope;  // stop unwinding here, and start destroying scoped variables
    }

    virtual Declaration *declare(std::string name, ScopeType st) {
        if (st != DATA_SCOPE) {
            std::cerr << "Can't declare functions in this scope!\n";
            return NULL;
        }
        
        if (type == FINALIZER_FUNCTION && name != "<anonymous>") {
            std::cerr << "Finalizer must be anonymous!\n";
            return NULL;
        }
        
        std::vector<TypeSpec> arg_tss;
        std::vector<std::string> arg_names;
        std::vector<TypeSpec> result_tss;

        for (auto &d : fn_scope->head_scope->contents) {
            // FIXME: with an (invalid here) nested declaration this can be a CodeScope, too
            Allocable *v = ptr_cast<Allocable>(d.get());
            
            if (v) {
                arg_tss.push_back(v->alloc_ts);  // FIXME
                arg_names.push_back(v->name);
            }
        }

        // Not returned, but must be processed
        for (auto &d : fn_scope->self_scope->contents) {
            // FIXME: with an (invalid here) nested declaration this can be a CodeScope, too
            Allocable *v = ptr_cast<Allocable>(d.get());
            
            if (v) {
            }
        }
        
        for (auto &d : fn_scope->result_scope->contents) {
            Allocable *v = ptr_cast<Allocable>(d.get());
            
            if (v) {
                result_tss.push_back(v->alloc_ts);  // FIXME
            }
            else
                throw INTERNAL_ERROR;
        }
        
        std::cerr << "Making function " << pivot_ts << " " << name << ".\n";
        
        if (import_name.size())
            function = new SysvFunction(import_name, name, pivot_ts, type, arg_tss, arg_names, result_tss, fn_scope->get_exception_type());
        else
            function = new Function(name, pivot_ts, type, arg_tss, arg_names, result_tss, fn_scope->get_exception_type());

        if (role_scope) {
            if (!function->set_role_scope(role_scope))
                return NULL;
        }

        return function;
    }
};


class InitializerDefinitionValue: public FunctionDefinitionValue {
public:
    InitializerDefinitionValue(Value *r, TypeMatch &match)
        :FunctionDefinitionValue(r, match) {
        type = INITIALIZER_FUNCTION;
    }
};


class FinalizerDefinitionValue: public FunctionDefinitionValue {
public:
    FinalizerDefinitionValue(Value *r, TypeMatch &match)
        :FunctionDefinitionValue(r, match) {
        type = FINALIZER_FUNCTION;
    }
};


// The value of calling a function
class FunctionCallValue: public Value, public Raiser {
public:
    Function *function;
    std::unique_ptr<Value> pivot;
    std::vector<std::unique_ptr<Value>> values;
    Register reg;
    
    unsigned res_total;
    std::vector<TypeSpec> pushed_tss;
    std::vector<Storage> pushed_storages;
    
    TypeSpec pivot_ts;
    std::vector<TypeSpec> arg_tss;
    std::vector<TypeSpec> res_tss;
    std::vector<std::string> arg_names;
    
    bool has_code_arg;
    Label *current_except_label;
    bool is_static;
        
    FunctionCallValue(Function *f, Value *p, TypeMatch &m)
        :Value(NO_TS) {
        function = f;
        pivot.reset(p);

        pivot_ts = function->get_pivot_typespec(m);
        res_tss = function->get_result_tss(m);
        arg_tss = function->get_argument_tss(m);
        arg_names = function->get_argument_names();
        
        if (res_tss.size() == 0)
            ts = pivot_ts != NO_TS ? pivot_ts : VOID_TS;
        else if (res_tss.size() == 1)
            ts = res_tss[0];
        else if (res_tss.size() > 1)
            ts = MULTI_TS;
            
        res_total = 0;
        has_code_arg = false;
        current_except_label = NULL;
        is_static = false;
    }

    virtual void be_static() {
        is_static = true;
    }

    virtual bool unpack(std::vector<TypeSpec> &tss) {
        if (res_tss.size() > 1) {
            tss = res_tss;
            return true;
        }
        else
            return false;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        ArgInfos infos;

        // Separate loop, so reallocations won't screw us
        for (unsigned i = 0; i < arg_tss.size(); i++)
            values.push_back(NULL);
        
        for (unsigned i = 0; i < arg_tss.size(); i++) {
            if (arg_tss[i][0] == code_type)
                has_code_arg = true;
                
            infos.push_back(ArgInfo { arg_names[i].c_str(), &arg_tss[i], scope, &values[i] });
        }

        bool ok = check_arguments(args, kwargs, infos);
        if (!ok)
            return false;
        
        for (unsigned i = 0; i < values.size(); i++) {
            if (!values[i]) {
                TypeSpec arg_ts = arg_tss[i];
                
                if (arg_ts[0] != ovalue_type) {
                    std::cerr << "Argument " << i << " not supplied: " << arg_ts << "!\n";
                    return false;
                }

                std::cerr << "Argument " << i << " is omitted.\n";

                if (arg_ts.where(AS_ARGUMENT) == ALIAS)
                    throw INTERNAL_ERROR;
            }
        }

        if (function->exception_type) {
            if (!check_raise(function->exception_type, scope))
                return false;
        }
        else if (has_code_arg) {
            // We won't raise anything, just continue unwinding for something already raised
            make_raising_dummy(scope);
        }

        return true;
    }

    virtual void call_sysv(X64 *x64, unsigned passed_size) {
        if (arg_tss.size() > 5) {
            std::cerr << "Oops, too many arguments to a SysV function!\n";
            throw INTERNAL_ERROR;
        }
        
        Register regs[] = { RDI, RSI, RDX, RCX, R8, R9 };
        SseRegister sses[] = { XMM0, XMM1, XMM2, XMM3, XMM4, XMM5 };
        
        std::vector<bool> is_floats;
        
        if (pivot_ts != NO_TS)
            is_floats.push_back(pivot_ts == FLOAT_TS);
            
        for (unsigned i = 0; i < arg_tss.size(); i++)
            is_floats.push_back(arg_tss[i] == FLOAT_TS);
        
        unsigned reg_index = 0;
        unsigned sse_index = 0;
        unsigned stack_offset = is_floats.size() * ADDRESS_SIZE;  // reverse order for SysV
        
        for (bool is_float : is_floats) {
            // Must move raw values so it doesn't count as a copy
            stack_offset -= ADDRESS_SIZE;
            
            if (is_float)
                x64->op(MOVSD, sses[sse_index++], Address(RSP, stack_offset));
            else
                x64->op(MOVQ, regs[reg_index++], Address(RSP, stack_offset));
        }
        
        x64->runtime->call_sysv_got(function->get_label(x64));

        bool is_void = res_tss.size() == 0;
        
        if (!is_void) {
            // Must move raw values
            
            if (res_tss[0] == FLOAT_TS)
                x64->op(MOVSD, Address(RSP, passed_size), XMM0);
            else
                x64->op(MOVQ, Address(RSP, passed_size), RAX);
        }
        else if (function->exception_type) {
            if (function->exception_type != errno_exception_type)
                throw INTERNAL_ERROR;
                
            // AL contains the potential exception
            x64->op(CMPB, RAX, 0);  // set ZF if no exception
            x64->op(LEA, RBX, Address(RAX, ERRNO_TREENUM_OFFSET));
        }
    }
    
    virtual void call_static(X64 *x64, unsigned passed_size) {
        x64->op(LEA, RAX, Address(RSP, passed_size));
        x64->op(CALL, function->get_label(x64));
    }

    virtual void call_virtual(X64 *x64, unsigned passed_size) {
        int vti = function->virtual_index;
        
        if (!pivot)
            throw INTERNAL_ERROR;

        TypeSpec pts = pivot->ts.rvalue();
        
        if (pts[0] != weakref_type)  // Was: borrowed_type
            throw INTERNAL_ERROR;
            
        x64->op(LEA, RAX, Address(RSP, passed_size));
        x64->op(MOVQ, RBX, Address(RSP, passed_size - REFERENCE_SIZE));  // self pointer
        x64->op(MOVQ, RBX, Address(RBX, 0));  // VMT pointer
        x64->op(CALL, Address(RBX, vti * ADDRESS_SIZE));
        std::cerr << "Will invoke virtual method of " << pts << " #" << vti << ".\n";
    }
    
    virtual Regs precompile(Regs preferred) {
        if (pivot)
            pivot->precompile();
        
        for (auto &value : values)
            if (value)
                value->precompile();
        
        if (ts != VOID_TS)
            reg = preferred.get_any();
        
        return Regs::all();  // assume everything is clobbered
    }

    virtual int push_pivot(TypeSpec arg_ts, Value *arg_value, X64 *x64) {
        Storage s = arg_value->compile(x64);
        
        StorageWhere where = stacked(arg_ts.where(AS_PIVOT));
        Storage t(where);

        if (s.where == STACK && t.where == ALISTACK) {
            // This is possible with record pivot arguments, handle it specially
            x64->op(PUSHQ, RSP);
            
            pushed_tss.push_back(arg_ts);
            pushed_storages.push_back(s);
            
            pushed_tss.push_back(arg_ts);
            pushed_storages.push_back(t);
            
            return arg_ts.measure_where(STACK) + arg_ts.measure_where(ALISTACK);
        }
        else {
            arg_ts.store(s, t, x64);
            
            pushed_tss.push_back(arg_ts);
            pushed_storages.push_back(t);  // For unwinding
            
            return arg_ts.measure_where(where);
        }
    }
    
    virtual int push_arg(TypeSpec arg_ts, Value *arg_value, X64 *x64) {
        if (arg_ts[0] == code_type) {
            TypeSpec rts = arg_ts.unprefix(code_type);
            Label begin, skip, end, except;
            
            x64->op(JMP, skip);
            x64->code_label(begin);
            current_except_label = &except;  // for unwinding
            
            if (arg_value) {
                Storage s = arg_value->compile(x64);
                
                switch (s.where) {
                case NOWHERE:
                    break;
                case CONSTANT:
                case FLAGS:
                case REGISTER:
                case MEMORY:
                    rts.create(s, Storage(MEMORY, Address(RSP, 2 * ADDRESS_SIZE)), x64);
                    break;
                case STACK:
                    rts.create(s, Storage(MEMORY, Address(RSP, 2 * ADDRESS_SIZE + rts.measure_stack())), x64);
                    break;
                default:
                    throw INTERNAL_ERROR;
                }
            }
            
            x64->op(MOVB, BL, 0);
            
            x64->code_label(end);
            x64->op(CMPB, BL, 0);  // ZF => OK
            x64->op(RET);
            
            // Jump here if the code above raise an exception, and report CODE_BREAK
            x64->code_label(except);
            x64->op(MOVB, BL, code_break_exception_type->get_keyword_index("CODE_BREAK"));
            x64->op(JMP, end);
            
            x64->code_label(skip);
            x64->op(LEARIP, RBX, begin);
            x64->op(PUSHQ, RBX);
            
            current_except_label = NULL;
            
            pushed_tss.push_back(arg_ts);
            pushed_storages.push_back(Storage(STACK));
            
            return ADDRESS_SIZE;
        }
        else {
            StorageWhere where = stacked(arg_ts.where(AS_ARGUMENT));
            Storage t(where);

            if (arg_value) {
                // Specified argument
                arg_value->compile_and_store(x64, t);
            }
            else {
                // Optional argument
                arg_ts.create(Storage(), t, x64);
            }

            pushed_tss.push_back(arg_ts);
            pushed_storages.push_back(t);  // For unwinding
        
            return arg_ts.measure_where(where);
        }
    }

    virtual void pop_arg(X64 *x64) {
        TypeSpec arg_ts = pushed_tss.back();
        pushed_tss.pop_back();
        
        Storage s = pushed_storages.back();
        pushed_storages.pop_back();
        
        if (arg_ts[0] == code_type) {
            x64->op(ADDQ, RSP, 8);
        }
        else {
            arg_ts.store(s, Storage(), x64);
        }
    }
    /*
    virtual Storage ret_res(TypeSpec res_ts, X64 *x64) {
        // Return a result from the stack in its native storage
        Storage s, t;
        
        switch (res_ts.where(AS_ARGUMENT)) {
        case MEMORY:
            s = Storage(STACK);
            
            switch (res_ts.where(AS_VALUE)) {
            case REGISTER:
                t = Storage(REGISTER, reg);
                break;
            case STACK:
                t = Storage(STACK);
                break;
            default:
                throw INTERNAL_ERROR;
            }
            break;
        case ALIAS:
            s = Storage(ALISTACK);
            
            switch (res_ts.where(AS_VARIABLE)) {
            case MEMORY:
                // Pop the address into a MEMORY with a dynamic base register
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
    */
    virtual Storage compile(X64 *x64) {
        //std::cerr << "Compiling call of " << function->name << "...\n";
        bool is_void = res_tss.size() == 0;
        
        for (unsigned i = 0; i < res_tss.size(); i++) {
            TypeSpec res_ts = res_tss[i];
            StorageWhere res_where = res_ts.where(AS_ARGUMENT);
        
            if (res_where == MEMORY) {
                // Must skip some place for uninitialized data
                int size = res_ts.measure_stack();
                res_total += size;
            }
            else
                throw INTERNAL_ERROR;
        }

        if (res_total)
            x64->op(SUBQ, RSP, res_total);

        x64->unwind->push(this);
        unsigned passed_size = 0;
        
        if (pivot) {
            passed_size += push_pivot(pivot_ts, pivot.get(), x64);
            //std::cerr << "Calling " << function->name << " with pivot " << function->get_pivot_typespec() << "\n";
        }
        
        for (unsigned i = 0; i < values.size(); i++)
            passed_size += push_arg(arg_tss[i], values[i].get(), x64);
            
        if (function->prot == SYSV_FUNCTION)
            call_sysv(x64, passed_size);
        else if (function->virtual_index >= 0 && !is_static)
            call_virtual(x64, passed_size);
        else
            call_static(x64, passed_size);
        
        if (function->exception_type || has_code_arg) {
            Label ex, noex;
            
            x64->op(JE, noex);  // Expect ZF if OK
            x64->op(MOVB, EXCEPTION_ADDRESS, BL);  // Expect BL if not OK
            
            x64->code_label(ex);
            x64->unwind->initiate(raising_dummy, x64);  // unwinds ourselves, too
            
            x64->code_label(noex);
            
            if (has_code_arg) {
                // Must continue unwinding if a Code argument raised something
                x64->op(CMPB, EXCEPTION_ADDRESS, 0);
                x64->op(JNE, ex);
            }
        }

        x64->unwind->pop(this);
        
        for (int i = values.size() - 1; i >= 0; i--)
            pop_arg(x64);
            
        if (pivot) {
            if (pushed_storages.size() == 2) {
                // A record pivot argument was handled specially, remove the second ALISTACK
                pop_arg(x64);
            }
            
            if (is_void) {
                // Return pivot argument
                Storage s = pushed_storages[0];
                
                if (s.where == ALISTACK) {
                    // Returninig ALISTACK is a bit evil, as the caller would need to
                    // allocate a register to do anything meaningful with it, so do that here
                    Storage t = Storage(MEMORY, Address(reg, 0));
                    s = pushed_tss[0].store(s, t, x64);
                }
                
                return s;
            }
            
            pop_arg(x64);
        }
            
        //std::cerr << "Compiled call of " << function->name << ".\n";
        if (is_void)
            return Storage();
        else if (res_tss.size() == 1)
            return Storage(stacked(res_tss[0].where(AS_ARGUMENT)));  // ret_res(res_tss[0], x64);
        else
            return Storage(STACK);  // Multiple result values
    }

    virtual Scope *unwind(X64 *x64) {
        if (current_except_label) {
            // We're compiling a Code argument, so for exceptions we don't roll back the
            // previous arguments and leave, but just jump to the reporting of this one.
            x64->op(JMP, *current_except_label);
            return NULL;
        }
        
        for (int i = pushed_tss.size() - 1; i >= 0; i--)
            pushed_tss[i].store(pushed_storages[i], Storage(), x64);
        
        // This area is uninitialized
        x64->op(ADDQ, RSP, res_total);
        return NULL;
    }
};


class FunctionReturnValue: public Value {
public:
    std::vector<Variable *> result_vars;
    std::vector<std::unique_ptr<Value>> values;
    Declaration *dummy;
    std::vector<Storage> var_storages;
    TypeMatch match;
    
    FunctionReturnValue(OperationType o, Value *v, TypeMatch &m)
        :Value(VOID_TS) {
        if (v)
            throw INTERNAL_ERROR;
            
        match = m;
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

        ArgInfos infos;

        for (unsigned i = 0; i < result_vars.size(); i++)
            values.push_back(NULL);
        
        for (unsigned i = 0; i < result_vars.size(); i++)
            infos.push_back(ArgInfo { result_vars[i]->name.c_str(), &result_vars[i]->alloc_ts, scope, &values[i] });
            
        if (!check_arguments(args, kwargs, infos))
            return false;

        if (result_vars.size() != args.size()) {
            std::cerr << "Wrong number of :return values!\n";
            return false;
        }

        // We must insert this after all potential declarations inside the result expression,
        // because we must finalize those variables upon return.
        dummy = new RaisingDummy;
        scope->add(dummy);
        
        return true;
    }

    virtual Regs precompile(Regs) {
        for (auto &v : values)
            v->precompile();
            
        return Regs();  // We won't return
    }

    virtual Storage compile(X64 *x64) {
        // Since we store each result in a variable, upon an exception we must
        // destroy the already set ones before unwinding!
        
        Storage r = Storage(MEMORY, Address(RAX, 0));
        
        x64->unwind->push(this);
        
        for (unsigned i = 0; i < values.size(); i++) {
            Storage var_storage = result_vars[i]->get_storage(match, r);
            var_storages.push_back(var_storage);
            TypeSpec var_ts = result_vars[i]->alloc_ts;
            
            Storage s = values[i]->compile(x64);
            Storage t = var_storage;

            x64->op(MOVQ, RAX, RESULT_ALIAS_ADDRESS);
            var_ts.create(s, t, x64);
        }

        x64->unwind->pop(this);

        x64->op(MOVB, EXCEPTION_ADDRESS, RETURN_EXCEPTION);
        x64->unwind->initiate(dummy, x64);
        
        return Storage();
    }
    
    virtual Scope *unwind(X64 *x64) {
        x64->op(MOVQ, RAX, RESULT_ALIAS_ADDRESS);
        
        for (int i = var_storages.size() - 1; i >= 0; i--)
            result_vars[i]->alloc_ts.destroy(var_storages[i], x64);
            
        return NULL;
    }
};

