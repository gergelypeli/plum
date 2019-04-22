
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
    //TypeSpec pivot_ts;
    Variable *self_var;
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
        //ston_var = NULL;
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
            Variable *decl = new Variable("<result>", var_ts);
            rs->add(decl);
        }

        rs->leave();

        Scope *ss = fn_scope->add_self_scope();
        ss->enter();
        
        if (scope->type == DATA_SCOPE) {
            TypeSpec pivot_ts = scope->get_pivot_ts();
            
            if (pivot_ts != NO_TS && pivot_ts != ANY_TS) {
                if (type == INITIALIZER_FUNCTION) {
                    self_var = new PartialVariable("$", pivot_ts.prefix(partial_type));
                }
                else if (type == LVALUE_FUNCTION) {
                    self_var = new SelfVariable("$", pivot_ts.lvalue());
                }
                else {
                    // Overriding functions have a different pivot type than the overridden
                    // one, although they can be called on those as well.
                    self_var = new SelfVariable("$", pivot_ts);
                }
                
                ss->add(self_var);
            }
        }

        ss->leave();

        Args fake_args;
        Expr *raise_expr = NULL, *from_expr = NULL, *import_expr = NULL, *as_expr = NULL;
        
        ExprInfos eis = {
            { "from", &from_expr },
            { "raise", &raise_expr },
            { "import", &import_expr },
            { "as", &as_expr }
        };
        
        if (!check_exprs(fake_args, kwargs, eis)) {
            std::cerr << "Whacky function!\n";
            return false;
        }

        // TODO: why do we store this in the fn scope?
        if (raise_expr) {
            TypeSpec TREENUMMETA_TS = { treenumeration_metatype };
            Value *v = typize(raise_expr, fn_scope, &TREENUMMETA_TS);
            
            if (v) {
                TreenumerationType *t = NULL;
                TreenumerationDefinitionValue *tdv = ptr_cast<TreenumerationDefinitionValue>(v);
                
                if (tdv) {
                    Declaration *d = tdv->declare("<raise>", fn_scope);
                    
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
        
        head.reset(new DataBlockValue(hs));

        if (from_expr) {
            if (from_expr->type == Expr::TUPLE) {
                for (auto &expr : from_expr->args)
                    if (!head->check_statement(expr.get()))
                        return false;
            }
            else {
                if (!head->check_statement(from_expr))
                    return false;
            }
        }
        
        hs->leave();
        
        if (as_expr) {
            std::cerr << "Deferring definition of function body.\n";
            deferred_body_expr = as_expr;
        }

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

        //if (!deferred_body_expr && !import_expr) {
        //    std::cerr << "Must specify function body or import!\n";
        //    return false;
        //}
        
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
            // Must do these only after the class definition is completed
            SelfInfo *si = NULL;
            PartialInfo *pi = NULL;
            TypeSpec ats;
            
            if (fn_scope->self_scope->contents.size() > 0) {
                Declaration *d = fn_scope->self_scope->contents.back().get();

                SelfVariable *sv = ptr_cast<SelfVariable>(d);
                
                if (sv) {
                    si = sv->self_info.get();
                }
                
                PartialVariable *pv = ptr_cast<PartialVariable>(d);
                
                if (pv) {
                    pi = pv->partial_info.get();
                    ats = pv->alloc_ts;
                }
            }

            if (si) {
                Function *f = function->implemented_function;
                
                if (f && f->type == GENERIC_FUNCTION) {
                    // Add $ .orig syntax for overriding an already implemented function
                    si->add_special(".orig", function);
                }
            }

            if (pi) {
                if (ats[0] != partial_type)
                    throw INTERNAL_ERROR;
                    
                Type *pit = (ats[1] == ptr_type ? ats[2] : ats[1]);
                PartialInitializable *pible = ptr_cast<PartialInitializable>(pit);
                
                if (!pible)
                    throw INTERNAL_ERROR;

                pi->set_member_names(pible->get_partial_initializable_names());
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
                    std::cerr << "Not all members initialized in " << token << "\n";
                    throw INTERNAL_ERROR;
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

    virtual void fix_arg(Declaration *d, X64 *x64) {
        // RSI - old stack bottom, RDI - old stack top, RDX - relocation difference
        // RBP points to the relocated stack frame
        Allocable *a = ptr_cast<Allocable>(d);
        Storage s = a->get_local_storage();
        
        if (s.where == ALIAS || s.where == RETRO) {
            x64->runtime->fix_address(s.address);
            x64->runtime->log("Fixed argument " + a->name + " of " + function->get_fully_qualified_name());
        }
    }
    
    virtual Storage compile(X64 *x64) {
        if (!function) {
            std::cerr << "Nameless function!\n";
            throw INTERNAL_ERROR;
        }
            
        if (exception_type_value)
            exception_type_value->compile(x64);  // to compile treenum definitions

        int associated_role_offset = 0;
        Associable *assoc = function->associated;
        
        if (assoc) {
            if (assoc->is_requiring() || assoc->is_in_requiring()) {
                // Offset if unknown until runtime, will be put by a trampoline into R11,
                // we must store it for ourselves.
                std::cerr << "Function body of " << function->name << " has dynamic associated role offset.\n";
            }
            else {
                associated_role_offset = assoc->get_offset(match);
                std::cerr << "Function body of " << function->name << " has associated role offset " << associated_role_offset << ".\n";
            }
        }
        
        unsigned frame_size = fn_scope->get_frame_size();
        Storage self_storage = self_var ? self_var->get_local_storage() : Storage();
        std::string fqn = fn_scope->outer_scope->fully_qualify(function->name);
            
        if (import_name.size()) {
            return Storage();
        }
        else if (function->is_abstract()) {
            std::string msg = "Abstract function " + fqn + " was called";
            x64->runtime->die(msg);
            return Storage();
        }
        else if (!body) {
            if (function->type != INITIALIZER_FUNCTION && function->type != FINALIZER_FUNCTION)
                throw INTERNAL_ERROR;
                
            x64->code_label_local(function->get_label(x64), fqn);
            x64->op(RET);
            return Storage();
        }
        
        Label start_label = function->get_label(x64);
        Label end_label;
        x64->code_label_local(start_label, fqn);
        
        x64->op(PUSHQ, RBP);
        x64->op(MOVQ, RBP, RSP);
        x64->op(SUBQ, RSP, frame_size);
        
        x64->accounting->start();
        
        // If the result is nonsimple, we'll return it at the given location pointed by RAX
        Storage ras = fn_scope->get_result_alias_storage();
        if (ras.where == MEMORY)
            x64->op(MOVQ, ras.address, RAX);
        
        // If this function has Code arguments, them raising an exception will be raised here
        Storage fes = fn_scope->get_forwarded_exception_storage();
        if (fes.where == MEMORY)
            x64->op(MOVQ, fes.address, NO_EXCEPTION);
        
        // Overriding functions get the self argument point to the role data area
        Storage aos = fn_scope->get_associated_offset_storage();
        if (aos.where == MEMORY) {
            x64->op(MOVQ, aos.address, R11);
            x64->op(SUBQ, self_storage.address, R11);
        }
        else if (associated_role_offset)
            x64->op(SUBQ, self_storage.address, associated_role_offset);
        
        x64->unwind->push(this);
        body->compile_and_store(x64, Storage());
        x64->unwind->pop(this);
        
        x64->op(NOP);
        x64->op(MOVQ, RDX, NO_EXCEPTION);

        fn_scope->body_scope->finalize_contents(x64);
        
        if (may_be_aborted) {
            Label ok, caught;
            x64->op(CMPQ, RDX, RETURN_EXCEPTION);
            x64->op(JE, caught);
            
            x64->op(CMPQ, RDX, NO_EXCEPTION);
            x64->op(JAE, ok);
            
            // Negative values mean yields, but that would be completely bogus.
            // Check this for debugging.
            x64->runtime->die("Unhandled yield in the function body!");
            
            x64->code_label(caught);
            x64->op(MOVQ, RDX, NO_EXCEPTION);  // caught
            
            x64->code_label(ok);
        }

        // If the caller reuses the self pointer, it must continue pointing to the role data
        if (aos.where == MEMORY) {
            x64->op(MOVQ, R11, aos.address);
            x64->op(ADDQ, self_storage.address, R11);
        }
        else if (associated_role_offset)
            x64->op(ADDQ, self_storage.address, associated_role_offset);

        int stack_usage = x64->accounting->stop();
        std::cerr << "XXX function " << function->get_fully_qualified_name() << " stack usage is " << stack_usage << " bytes\n";
        
        x64->op(ADDQ, RSP, frame_size);
        
        if (fes.where == MEMORY)
            x64->op(MOVQ, RDX, fes.address);
            
        x64->op(CMPQ, RDX, NO_EXCEPTION);  // ZF => OK
        x64->op(POPQ, RBP);
        x64->op(RET);

        x64->code_label(end_label);
        
        // Generate fixup at end_label.
        // RSI - old stack bottom, RDI - old stack top, RDX - relocation difference
        // RBP points to the relocated stack frame
        x64->runtime->log("Fixing arguments of " + function->get_fully_qualified_name());
        
        for (auto &d : fn_scope->self_scope->contents)
            fix_arg(d.get(), x64);
            
        for (auto &d : fn_scope->head_scope->contents)
            fix_arg(d.get(), x64);
        
        x64->op(RET);
        
        x64->runtime->add_func_info(fqn, start_label, end_label);
        
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

        DataScope *ds = ptr_cast<DataScope>(scope);
        
        if (ds->is_abstract_scope()) {
            if (type != GENERIC_FUNCTION && type != LVALUE_FUNCTION) {
                std::cerr << "Only generic functions can be defined in interfaces!\n";
                return NULL;
            }
            
            if (deferred_body_expr) {
                std::cerr << "Interface functions can't have a body!\n";
                return NULL;
            }
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
            
        std::cerr << "Making function " << name << ".\n";
        
        if (import_name.size())
            function = new SysvFunction(import_name, name, type, arg_tss, arg_names, result_tss, fn_scope->get_exception_type(), fn_scope);
        else
            function = new Function(name, type, arg_tss, arg_names, result_tss, fn_scope->get_exception_type(), fn_scope);

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


class ProcedureDefinitionValue: public FunctionDefinitionValue {
public:
    ProcedureDefinitionValue(Value *r, TypeMatch &match)
        :FunctionDefinitionValue(r, match) {
        type = LVALUE_FUNCTION;
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
    std::vector<Storage> pushed_stackfixes;

    TypeSpec pivot_ts;
    Storage pivot_alias_storage;
    
    std::vector<TypeSpec> arg_tss;
    std::vector<TypeSpec> res_tss;
    std::vector<std::string> arg_names;
    
    bool has_code_arg;
    Associable *static_role;
    
    FunctionCallValue(Function *f, Value *p, TypeMatch &m)
        :Value(NO_TS) {
        function = f;
        pivot.reset(p);

        function->get_parameters(pivot_ts, res_tss, arg_tss, arg_names, get_typespec(p), m);
        
        if (res_tss.size() == 0)
            ts = pivot_ts != NO_TS ? pivot_ts : VOID_TS;
        else if (res_tss.size() == 1)
            ts = res_tss[0];
        else if (res_tss.size() > 1)
            ts = MULTI_TS;
            
        res_total = 0;
        has_code_arg = false;
        static_role = NULL;
        
        if (pivot_ts.has_meta(module_metatype)) {
            // Module pivots are only symbolic, they are not pushed or popped.
            pivot_ts = {};
        }
    }

    virtual void be_static(Associable *sr) {
        static_role = sr;
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

        bool ok = check_arguments(args, kwargs, infos, true);
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

    virtual void call_static(X64 *x64, unsigned passed_size) {
        if (res_total)
            x64->op(LEA, RAX, Address(RSP, passed_size));

        Label label;
        
        if (static_role) {
            int vi = function->virtual_index;
            TypeSpec static_ts = static_role->alloc_ts;
            VirtualEntry *ve = static_ts.get_virtual_table().get(vi);
            label = ve->get_virtual_entry_label(static_ts.match(), x64);
        }
        else
            label = function->get_label(x64);
            
        x64->op(CALL, label);
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
        std::cerr << "Will invoke virtual method of " << pts << " #" << vti << " " << function->name << ".\n";
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

    virtual void push_arg(TypeSpec arg_ts, Value *arg_value, X64 *x64) {
        Storage t;
        unsigned size;
        Storage stackfix;
        
        if (arg_ts[0] == code_type) {
            // arg_value is a RetroScopeValue
            t = arg_value->compile(x64);
            size = ADDRESS_SIZE;
        }
        else if (arg_ts[0] == dvalue_type) {
            // arg_value is a DeclarationValue with a retro variable.
            // Must pass its address, even if it means passing the address of an ALIAS.
            // So this is another unnamed level of indirection.
            Storage s = arg_value->compile(x64);
            
            if (s.where != MEMORY && s.where != ALIAS)
                throw INTERNAL_ERROR;
                
            if (s.address.base != RBP)
                throw INTERNAL_ERROR;
                
            x64->op(LEA, R10, s.address);
            x64->op(PUSHQ, R10);
            
            // The storage is whatever
            size = ADDRESS_SIZE;
        }
        else {
            StorageWhere where = arg_ts.where(AS_ARGUMENT);
        
            if (where != NOWHERE) {  // happens for singleton pivots
                where = stacked(where);
                t = Storage(where);
            }

            size = arg_ts.measure_where(where);

            if (arg_value) {
                // Specified argument
                Storage s = arg_value->compile(x64);

                // Borrow argument reference, if possible
                if ((s.where == BREGISTER || s.where == BSTACK || s.where == BMEMORY) && t.where == STACK)
                    t.where = BSTACK;

                // Pushing a stack relative address onto the stack is becoming illegal.
                // Unless this is done in the last step before invoking a function, when
                // all subsequent arguments were evaluated (potentially moving the stack).
                // So only store the RBP-relative offset onto the stack, and remember to add
                // the current RBP value to them just before invoking the function.
                if (t.where == ALISTACK) {
                    // Necessary to return the original storage in void functions
                    if (arg_value == pivot.get())
                        pivot_alias_storage = s;
                
                    if (s.where == MEMORY && s.address.base == RBP) {
                        // Will add RBP
                        x64->op(PUSHQ, s.address.offset);
                        stackfix = s;
                    }
                    else if (s.where == MEMORY && s.address.base == RSP) {
                        // Will add RBP
                        x64->op(LEA, R10, s.address);
                        x64->op(SUBQ, R10, RBP);
                        x64->op(PUSHQ, R10);
                        stackfix = s;
                    }
                    else if (s.where == ALIAS) {
                        // Will add ALIAS address
                        x64->op(PUSHQ, s.value);
                        stackfix = s;
                    }
                    else
                        arg_ts.store(s, t, x64);
                }
                else
                    arg_ts.store(s, t, x64);
            }
            else {
                // Optional argument initialized to default value
                arg_ts.store(Storage(), t, x64);
            }
        }

        pushed_tss.push_back(arg_ts);
        pushed_storages.push_back(t);  // For unwinding
        pushed_sizes.push_back(size);
        pushed_stackfixes.push_back(stackfix);
    }

    virtual void pop_arg(X64 *x64) {
        TypeSpec arg_ts = pushed_tss.back();
        Storage s = pushed_storages.back();

        pushed_tss.pop_back();
        pushed_storages.pop_back();
        pushed_sizes.pop_back();
        pushed_stackfixes.pop_back();
        
        if (arg_ts[0] == code_type) {
            // TODO: code_type should take care of itself!
            x64->op(ADDQ, RSP, ADDRESS_SIZE);
        }
        else if (arg_ts[0] == dvalue_type) {
            x64->op(ADDQ, RSP, ADDRESS_SIZE);
        }
        else {
            arg_ts.store(s, Storage(), x64);
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
            push_arg(pivot_ts, pivot.get(), x64);
            //std::cerr << "Calling " << function->name << " with pivot " << function->get_pivot_typespec() << "\n";
        }
        
        for (unsigned i = 0; i < values.size(); i++)
            push_arg(arg_tss[i], values[i].get(), x64);

        unsigned passed_size = 0;
        for (unsigned &s : pushed_sizes)
            passed_size += s;

        int stackfix_offset = passed_size;
        
        for (unsigned i = 0; i < pushed_stackfixes.size(); i++) {
            stackfix_offset -= pushed_sizes[i];
            Storage sf = pushed_stackfixes[i];
            
            if (sf.where == MEMORY) {
                std::cerr << "XXX stackfix for " << function->get_fully_qualified_name() << " argument " << i << "\n";
                x64->op(ADDQ, Address(RSP, stackfix_offset), RBP);
            }
            else if (sf.where == ALIAS) {
                std::cerr << "XXX stackfix for " << function->get_fully_qualified_name() << " argument " << i << "\n";
                x64->op(MOVQ, R10, sf.address);
                x64->op(ADDQ, Address(RSP, stackfix_offset), R10);
            }
        }

        if (function->virtual_index != 0 && !static_role)
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
            if (is_void) {
                // Return pivot argument (an ALIAS must be returned as received)
                
                Storage s = pushed_storages[0];
            
                if (s.where == ALISTACK) {
                    // A general register can only be used to store heap addresses.
                    // If the pivot was specified using a stack relative address, we need
                    // to return it as we got it.
                    s = pivot_alias_storage;
                    
                    if (s.where == MEMORY) {
                        if (s.address.base == RBP || s.address.base == RSP)
                            x64->op(ADDQ, RSP, ADDRESS_SIZE);
                        else {
                            x64->op(POPQ, RAX);
                            s = Storage(MEMORY, Address(RAX, 0));
                        }
                    }
                    else if (s.where == ALIAS)
                        x64->op(ADDQ, RSP, ADDRESS_SIZE);
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
        for (int i = pushed_tss.size() - 1; i >= 0; i--) {
            if (pushed_tss[i][0] == code_type)
                x64->op(ADDQ, RSP, ADDRESS_SIZE);
            else
                pushed_tss[i].store(pushed_storages[i], Storage(), x64);
        }
        
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

