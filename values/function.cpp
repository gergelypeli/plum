
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
    SingletonVariable *ston_var;
    TypeMatch match;
    Scope *outer_scope;

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
        ston_var = NULL;
        fn_scope = NULL;
        outer_scope = NULL;
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        // For later
        outer_scope = scope;
        
        fn_scope = new FunctionScope();
        
        // This is temporary, to allow lookups in the result/head scopes
        scope->add(fn_scope);
        fn_scope->enter();

        Scope *rs = fn_scope->add_result_scope();
        rs->enter();
        
        for (auto &arg : args) {
            Value *r = typize(arg.get(), rs);
        
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

        rs->leave();

        Scope *ss = fn_scope->add_self_scope();
        ss->enter();
        
        if (scope->type == DATA_SCOPE) {
            pivot_ts = scope->pivot_type_hint();
            
            if (pivot_ts.has_meta(singleton_metatype)) {
                // Singleton method
                
                if (type == INITIALIZER_FUNCTION) {
                    pivot_ts = pivot_ts.prefix(initializable_type);  // called by the runtime only
                    TypeSpec self_ts = pivot_ts.reprefix(initializable_type, partial_type);
                    ston_var = new PartialSingletonVariable("$", self_ts);
                }
                else {
                    ston_var = new SingletonVariable("$", pivot_ts);
                }
                
                ss->add(ston_var);
            }
            else if (pivot_ts != NO_TS && pivot_ts != ANY_TS) {
                if (type == INITIALIZER_FUNCTION) {
                    pivot_ts = pivot_ts.prefix(initializable_type);
                    TypeSpec self_ts = pivot_ts.reprefix(initializable_type, partial_type);
                    self_var = new PartialVariable("$", NO_TS, self_ts);
                }
                else {
                    // Records have a rvalue pivot type, but the self argument must be treated
                    // as lvalue, so the members will also be lvalue-s. But not in classes,
                    // as an lvalue self would be awkward.
                    TypeSpec self_ts = pivot_ts;  // (pivot_ts[0] == ptr_type ? pivot_ts : pivot_ts.lvalue());
                    self_var = new Variable("$", NO_TS, self_ts);
                }
                
                ss->add(self_var);
            }
        }

        ss->leave();

        // TODO: why do we store this in the fn scope?
        Expr *e = kwargs["may"].get();
        if (e) {
            TypeSpec TREENUMMETA_TS = { treenumeration_metatype };
            Value *v = typize(e, fn_scope, &TREENUMMETA_TS);
            
            if (v) {
                TreenumerationType *t = NULL;
                TreenumerationDefinitionValue *tdv = ptr_cast<TreenumerationDefinitionValue>(v);
                
                if (tdv) {
                    Declaration *d = tdv->declare("<may>", fn_scope);
                    
                    if (!d)
                        throw INTERNAL_ERROR;
                    
                    t = ptr_cast<TreenumerationType>(d);
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
        hs->enter();
        
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
        
        hs->leave();
        
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
            
            import_name = encode_ascii(sl->utext);
            
            if (import_name.size() == 0) {
                std::cerr << "Function import name is not ASCII: " << import_expr->token << "\n";
                return false;
            }
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
        
        // This was temporary
        fn_scope->leave();
        scope->remove(fn_scope);
        
        return true;
    }

    virtual bool define_code() {
        std::cerr << "Completing definition of function body " << function->name << ".\n";
        fn_scope->enter();
        CodeScope *bs = fn_scope->add_body_scope();
        bs->enter();

        if (deferred_body_expr) {
            PartialInfo *pi = NULL;
            TypeSpec ats;
            
            if (fn_scope->self_scope->contents.size() > 0) {
                Declaration *d = fn_scope->self_scope->contents.back().get();
                
                PartialVariable *pv = ptr_cast<PartialVariable>(d);
                
                if (pv) {
                    pi = pv->partial_info.get();
                    ats = pv->alloc_ts.unprefix(partial_type);
                }

                PartialSingletonVariable *ps = ptr_cast<PartialSingletonVariable>(d);
                
                if (ps) {
                    pi = ps->partial_info.get();
                    ats = ps->alloc_ts.unprefix(partial_type);
                }
            }

            if (pi) {
                // Must do this only after the class definition is completed
                if (ats.has_meta(singleton_metatype)) {
                    // Singleton initializer
                    SingletonType *st = ptr_cast<SingletonType>(ats[0]);
                    if (!st)
                        throw INTERNAL_ERROR;
                    
                    pi->set_member_names(st->get_member_names());
                }
                else if (ats[0] == ptr_type) {
                    ClassType *ct = ptr_cast<ClassType>(ats[1]);
                    if (!ct)
                        throw INTERNAL_ERROR;
                    
                    pi->set_member_names(ct->get_member_names());
                }
                else {
                    RecordType *rt = ptr_cast<RecordType>(ats[0]);
                    if (!rt)
                        throw INTERNAL_ERROR;
                    
                    pi->set_member_names(rt->get_member_names());
                }
            }
        
            // The body is in a separate CodeScope, but instead of a dedicated CodeValue,
            // we'll handle its compilation.
            // Disallow fallthrough in nonvoid functions.
            bool is_void = (fn_scope->result_scope->contents.size() == 0);
            TypeSpec *ctx = (is_void ? &VOID_CODE_TS : &WHATEVER_CODE_TS);
            Value *bv = typize(deferred_body_expr, bs, ctx);
            body.reset(bv);
            
            if (pi) {
                if (!pi->is_complete()) {
                    std::cerr << "Not all members initialized!\n";
                    return false;
                }
            }
        }

        bs->be_taken();
        bs->leave();

        fn_scope->leave();

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

        if (!function) {
            std::cerr << "Nameless function!\n";
            throw INTERNAL_ERROR;
        }
            
        if (exception_type_value)
            exception_type_value->compile(x64);  // to compile treenum definitions

        unsigned frame_size = fn_scope->get_frame_size();
        int associated_role_offset = 0;
        
        if (function->associated_role) {
            associated_role_offset = function->associated_role->offset.concretize(match);
            std::cerr << "Function body of " << function->name << " has associated role offset " << associated_role_offset << ".\n";
        }
        
        Storage self_storage = self_var ? self_var->get_local_storage() : Storage();

        std::string fqn = fn_scope->outer_scope->fully_qualify(function->name);
        x64->code_label_local(function->get_label(x64), fqn);
            
        if (fqn == ".Main.start") {
            Label dummy;
            x64->code_label_global(dummy, "start");
        }
        
        x64->op(PUSHQ, RBP);
        x64->op(MOVQ, RBP, RSP);
        x64->op(SUBQ, RSP, frame_size);
        
        // If the result is nonsimple, we'll return it at the given location pointed by RAX
        Storage ras = fn_scope->get_result_alias_storage();
        if (ras.where == MEMORY)
            x64->op(MOVQ, ras.address, RAX);
        
        // If this function has Code arguments, them raising an exception will be raised here
        Storage fes = fn_scope->get_forwarded_exception_storage();
        if (fes.where == MEMORY)
            x64->op(MOVQ, fes.address, NO_EXCEPTION);
        
        // Overriding functions get the self argument point to the role data area
        if (associated_role_offset)
            x64->op(SUBQ, self_storage.address, associated_role_offset);
        
        x64->unwind->push(this);
        body->compile_and_store(x64, Storage());
        x64->unwind->pop(this);
        
        x64->op(NOP);
        x64->op(MOVQ, RDX, NO_EXCEPTION);

        fn_scope->body_scope->finalize_contents(x64);
        
        if (may_be_aborted) {
            Label ok;
            x64->op(CMPQ, RDX, RETURN_EXCEPTION);
            x64->op(JNE, ok);
            x64->op(MOVQ, RDX, NO_EXCEPTION);  // caught
            x64->code_label(ok);
        }

        // If the caller reuses the self pointer, it must continue pointing to the role data
        if (associated_role_offset)
            x64->op(ADDQ, self_storage.address, associated_role_offset);
        
        x64->op(ADDQ, RSP, frame_size);
        
        if (fes.where == MEMORY)
            x64->op(MOVQ, RDX, fes.address);
            
        x64->op(CMPQ, RDX, NO_EXCEPTION);  // ZF => OK
        x64->op(POPQ, RBP);
        x64->op(RET);
        
        return Storage();
    }

    virtual Scope *unwind(X64 *x64) {
        may_be_aborted = true;
        return fn_scope->body_scope;  // stop unwinding here, and start destroying scoped variables
    }

    virtual Declaration *declare(std::string name, Scope *scope) {
        if (scope->type != DATA_SCOPE) {
            std::cerr << "Functions must be declared in data scopes!\n";
            return NULL;
        }
        
        if (type == FINALIZER_FUNCTION && name != "<anonymous>") {
            std::cerr << "Finalizer must be anonymous!\n";
            return NULL;
        }
        
        std::vector<TypeSpec> arg_tss;
        std::vector<std::string> arg_names;
        std::vector<TypeSpec> result_tss;
        bool has_code_arg = false;

        for (auto &d : fn_scope->head_scope->contents) {
            // FIXME: with an (invalid here) nested declaration this can be a CodeScope, too
            Allocable *v = ptr_cast<Allocable>(d.get());
            
            if (v) {
                arg_tss.push_back(v->alloc_ts);  // FIXME
                arg_names.push_back(v->name);
                
                if (v->alloc_ts[0] == code_type)
                    has_code_arg = true;
            }
        }

        // Not returned, but must be processed
        for (auto &d : fn_scope->self_scope->contents) {
            // FIXME: with an (invalid here) nested declaration this can be a CodeScope, too
            Allocable *v = ptr_cast<Allocable>(d.get());
            
            if (v) {
            }
            else
                throw INTERNAL_ERROR;
            
        }
        
        for (auto &d : fn_scope->result_scope->contents) {
            Allocable *v = ptr_cast<Allocable>(d.get());
            
            if (v) {
                result_tss.push_back(v->alloc_ts);  // FIXME
            }
            else
                throw INTERNAL_ERROR;
        }

        // Preparing hidden local variables
        StorageWhere simple_where = (
            result_tss.size() == 0 ? NOWHERE :
            result_tss.size() == 1 ? result_tss[0].where(AS_VALUE) :
            STACK
        );
        
        if (simple_where != NOWHERE && simple_where != REGISTER && simple_where != SSEREGISTER)
            fn_scope->make_result_alias_storage();

        if (has_code_arg)
            fn_scope->make_forwarded_exception_storage();
        
        std::cerr << "Making function " << pivot_ts << " " << name << ".\n";
        
        if (import_name.size())
            function = new SysvFunction(import_name, name, pivot_ts, type, arg_tss, arg_names, result_tss, fn_scope->get_exception_type(), fn_scope);
        else
            function = new Function(name, pivot_ts, type, arg_tss, arg_names, result_tss, fn_scope->get_exception_type(), fn_scope);

        scope->add(function);
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
    std::vector<unsigned> pushed_sizes;
    bool pushed_pivots;
    
    TypeSpec pivot_ts;
    std::vector<TypeSpec> arg_tss;
    std::vector<TypeSpec> res_tss;
    std::vector<std::string> arg_names;
    
    bool has_code_arg;
    bool is_static;
    Label static_label;
    
        
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
        is_static = false;
        pushed_pivots = false;
        
        if (pivot_ts.has_meta(module_metatype)) {
            // Module pivots are only symbolic, they are not pushed or popped.
            pivot_ts = {};
        }
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
        unsigned reg_index = 0;
        unsigned sse_index = 0;
        
        // For pushed pivots, ignore the pushed value, only use the pushed alias
        unsigned stack_offset = passed_size - (pushed_pivots ? pushed_sizes[0] : 0);  // reverse argument order for SysV!

        for (unsigned i = (pushed_pivots ? 1 : 0); i < pushed_tss.size(); i++) {
            // Must move raw values so it doesn't count as a copy
            stack_offset -= pushed_sizes[i];
            
            StorageWhere pushed_where = pushed_tss[i].where(AS_VALUE);
            
            if (pushed_where == NOWHERE)
                ;  // happens for singleton pivots
            else if (pushed_where == SSEREGISTER)
                x64->op(MOVSD, sses[sse_index++], Address(RSP, stack_offset));
            else if (pushed_sizes[i] == ADDRESS_SIZE)
                x64->op(MOVQ, regs[reg_index++], Address(RSP, stack_offset));
            else
                x64->op(LEA, regs[reg_index++], Address(RSP, stack_offset));
        }
        
        x64->runtime->call_sysv_got(function->get_label(x64));

        //x64->runtime->dump("Returned from SysV.");

        switch (res_total) {
        case 0:
            break;  // We return simple values in RAX and XMM0 like SysV.
        case ADDRESS_SIZE:
            x64->op(MOVQ, Address(RSP, passed_size), RAX);  // simple records, like String
            break;
        default:
            throw INTERNAL_ERROR;
        }
        
        // Raised exception in RDX.
        if (function->exception_type)
            x64->op(CMPQ, RDX, NO_EXCEPTION);
    }
    
    virtual void call_static(X64 *x64, unsigned passed_size) {
        if (res_total)
            x64->op(LEA, RAX, Address(RSP, passed_size));
            
        x64->op(CALL, (is_static ? static_label : function->get_label(x64)));
    }

    virtual void call_virtual(X64 *x64, unsigned passed_size) {
        int vti = function->virtual_index;
        
        if (!pivot)
            throw INTERNAL_ERROR;

        TypeSpec pts = pivot->ts.rvalue();
        
        if (pts[0] != ptr_type)
            throw INTERNAL_ERROR;

        if (res_total)
            x64->op(LEA, RAX, Address(RSP, passed_size));
            
        x64->op(MOVQ, R10, Address(RSP, passed_size - POINTER_SIZE));  // self pointer
        x64->op(MOVQ, R10, Address(R10, CLASS_VT_OFFSET));  // VMT pointer
        x64->op(CALL, Address(R10, vti * ADDRESS_SIZE));
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

    virtual void push_pivot(TypeSpec arg_ts, Value *arg_value, X64 *x64) {
        Storage s = arg_value->compile(x64);
        Storage t;
        StorageWhere where = arg_ts.where(AS_ARGUMENT);
        
        if (where != NOWHERE) {
            where = stacked(where);
            t = Storage(where);
        }

        if (s.where == STACK && t.where == ALISTACK) {
            // This is possible with record pivot arguments, handle it specially
            x64->op(PUSHQ, RSP);
            
            pushed_tss.push_back(arg_ts);
            pushed_storages.push_back(s);
            pushed_sizes.push_back(arg_ts.measure_where(STACK));
            
            pushed_tss.push_back(arg_ts);
            pushed_storages.push_back(t);
            pushed_sizes.push_back(arg_ts.measure_where(ALISTACK));
            
            pushed_pivots = true;
        }
        else {
            // Borrow pivot reference, if possible
            if ((s.where == BREGISTER || s.where == BSTACK || s.where == BMEMORY) && t.where == STACK)
                t.where = BSTACK;
            
            arg_ts.store(s, t, x64);
            
            pushed_tss.push_back(arg_ts);
            pushed_storages.push_back(t);  // For unwinding
            pushed_sizes.push_back(arg_ts.measure_where(where));
        }
    }
    
    virtual void push_arg(TypeSpec arg_ts, Value *arg_value, X64 *x64) {
        if (arg_ts[0] == code_type) {
            CodeScopeValue *csv = ptr_cast<CodeScopeValue>(arg_value);
            if (!csv)
                throw INTERNAL_ERROR;
            
            Label begin, skip;
            x64->op(JMP, skip);

            csv->compile_evaluable(begin, x64);
            
            x64->code_label(skip);
            x64->op(LEA, R10, Address(begin, 0));
            x64->op(PUSHQ, R10);
            
            pushed_tss.push_back(arg_ts);
            pushed_storages.push_back(Storage(STACK));
            pushed_sizes.push_back(ADDRESS_SIZE);
        }
        else {
            StorageWhere where = stacked(arg_ts.where(AS_ARGUMENT));
            Storage t(where);

            if (arg_value) {
                // Specified argument
                Storage s = arg_value->compile(x64);

                // Borrow argument reference, if possible
                if ((s.where == BREGISTER || s.where == BSTACK || s.where == BMEMORY) && t.where == STACK)
                    t.where = BSTACK;

                arg_ts.store(s, t, x64);
            }
            else {
                // Optional argument initialized to default value
                arg_ts.store(Storage(), t, x64);
            }

            pushed_tss.push_back(arg_ts);
            pushed_storages.push_back(t);  // For unwinding
            pushed_sizes.push_back(arg_ts.measure_where(where));
        }
    }

    virtual void pop_arg(X64 *x64) {
        TypeSpec arg_ts = pushed_tss.back();
        pushed_tss.pop_back();
        
        Storage s = pushed_storages.back();
        pushed_storages.pop_back();
        
        pushed_sizes.pop_back();
        
        if (arg_ts[0] == code_type) {
            x64->op(ADDQ, RSP, ADDRESS_SIZE);
        }
        else {
            arg_ts.store(s, Storage(), x64);
        }
    }

    virtual void check_static_cast(X64 *x64) {
        // Figure out if the pivot is in the form of $.foo bar baz
        // We need the virtual offset of bar baz within the VT of foo.
        
        Value *p = pivot.get();
        int virtual_offset = 0;
        
        while (true) {
            RoleValue *rv = ptr_cast<RoleValue>(p);
            
            if (!rv)
                break;  // non-role, so no static cast used
                
            Role *r = rv->role;
            
            if (rv->is_static()) {
                // The initial role is a static cast, so we already have the virtual offset
                // within it.
                int vi = virtual_offset + function->virtual_index;
                VirtualEntry *ve = r->alloc_ts.get_virtual_table()[vi];
                static_label = ve->get_virtual_entry_label(r->alloc_ts.match(), x64);
                is_static = true;
                break;
            }
            else {
                // If role selections were collapsed, the role refers a shadow role,
                // whose virtual offset is the absolute offset computed by summing all
                // outer role offsets, so we can use it in a single step. The static
                // role selection is never collapsed, so we're still summing the relative
                // offsets within it.
                    
                virtual_offset += r->virtual_offset;
                p = rv->pivot.get();
            }
        }
    }

    virtual Storage compile(X64 *x64) {
        //std::cerr << "Compiling call of " << function->name << "...\n";
        bool is_void = (res_tss.size() == 0);
        StorageWhere simple_where = NOWHERE;

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

        if (res_tss.size() == 1) {
            simple_where = res_tss[0].where(AS_VALUE);
            
            if (simple_where == REGISTER || simple_where == SSEREGISTER)
                res_total = 0;
        }

        if (res_total)
            x64->op(SUBQ, RSP, res_total);

        x64->unwind->push(this);
        
        if (pivot_ts.size()) {
            push_pivot(pivot_ts, pivot.get(), x64);
            //std::cerr << "Calling " << function->name << " with pivot " << function->get_pivot_typespec() << "\n";
            
            check_static_cast(x64);
        }
        
        for (unsigned i = 0; i < values.size(); i++)
            push_arg(arg_tss[i], values[i].get(), x64);

        unsigned passed_size = 0;
        for (unsigned &s : pushed_sizes)
            passed_size += s;

        if (function->prot == SYSV_FUNCTION)
            call_sysv(x64, passed_size);
        else if (function->virtual_index >= 0 && !is_static)
            call_virtual(x64, passed_size);
        else
            call_static(x64, passed_size);

        if (function->exception_type || has_code_arg) {
            Label noex;
            
            x64->op(JE, noex);  // Expect ZF if OK
            
            // Reraise exception in RDX (also exceptions forwarded back from a Code argument)
            x64->unwind->initiate(raising_dummy, x64);  // unwinds ourselves, too
            
            x64->code_label(noex);
        }

        x64->unwind->pop(this);
        
        for (int i = values.size() - 1; i >= 0; i--)
            pop_arg(x64);
            
        if (pivot_ts.size()) {
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
        else if (res_tss.size() == 1) {
            if (simple_where == REGISTER)
                return Storage(REGISTER, RAX);
            else if (simple_where == SSEREGISTER)
                return Storage(SSEREGISTER, XMM0);
            else
                return Storage(stacked(res_tss[0].where(AS_ARGUMENT)));
        }
        else
            return Storage(STACK);  // Multiple result values
    }

    virtual Scope *unwind(X64 *x64) {
        //std::cerr << "Unwinding function call " << function->name << " would wipe " << pushed_tss.size() << " arguments.\n";
        for (int i = pushed_tss.size() - 1; i >= 0; i--)
            pushed_tss[i].store(pushed_storages[i], Storage(), x64);
        
        // This area is uninitialized
        if (res_total)
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
    FunctionScope *fn_scope;
    
    FunctionReturnValue(OperationType o, Value *v, TypeMatch &m)
        :Value(WHATEVER_TS) {
        if (v)
            throw INTERNAL_ERROR;
            
        match = m;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (kwargs.size() != 0) {
            std::cerr << "Whacky :return!\n";
            return false;
        }

        fn_scope = scope->get_function_scope();
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
        StorageWhere simple_where = (
            result_vars.size() == 0 ? NOWHERE :
            result_vars.size() == 1 ? result_vars[0]->alloc_ts.where(AS_VALUE) :
            STACK
        );
            
        if (simple_where == NOWHERE)
            ;
        else if (simple_where == REGISTER) {
            values[0]->compile_and_store(x64, Storage(REGISTER, RAX));
        }
        else if (simple_where == SSEREGISTER) {
            values[0]->compile_and_store(x64, Storage(SSEREGISTER, XMM0));
        }
        else {
            // Since we store each result in a variable, upon an exception we must
            // destroy the already set ones before unwinding!
        
            Storage ras = fn_scope->get_result_alias_storage();
            if (ras.where != MEMORY)
                throw INTERNAL_ERROR;
            
            // This must be the result base for unwinding, too
            Storage r = Storage(MEMORY, Address(RAX, 0));
        
            x64->unwind->push(this);
        
            for (unsigned i = 0; i < values.size(); i++) {
                Storage var_storage = result_vars[i]->get_storage(match, r);
                var_storages.push_back(var_storage);
                TypeSpec var_ts = result_vars[i]->alloc_ts;
            
                Storage s = values[i]->compile(x64);
                Storage t = var_storage;

                x64->op(MOVQ, RAX, ras.address);
                var_ts.create(s, t, x64);
            }

            x64->unwind->pop(this);
        }
        
        x64->op(MOVQ, RDX, RETURN_EXCEPTION);
        x64->unwind->initiate(dummy, x64);
        
        return Storage();
    }
    
    virtual Scope *unwind(X64 *x64) {
        Storage ras = fn_scope->get_result_alias_storage();
        if (ras.where == MEMORY)
            x64->op(MOVQ, RAX, ras.address);  // load result base
            
        for (int i = var_storages.size() - 1; i >= 0; i--)
            result_vars[i]->alloc_ts.destroy(var_storages[i], x64);
            
        return NULL;
    }
};
