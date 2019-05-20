
// Values

class Value {
public:
    TypeSpec ts;
    Token token;
    bool lvalue_needed;
        
    Value(TypeSpec t)
        :ts(t) {
        lvalue_needed = false;
    }

    virtual ~Value() {
    }

    virtual Value *set_token(Token t) {
        token = t;
        return this;
    }

    virtual void set_context_ts(TypeSpec *c) {
        // Generally we don't need it, only in controls
    }

    bool check_arguments(Args &args, Kwargs &kwargs, const ArgInfos &arg_infos, bool is_function_call = false) {
        // FIXME: shouldn't this be a proper method?
        return ::check_arguments(args, kwargs, arg_infos, is_function_call);
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        return check_arguments(args, kwargs, ArgInfos());
    }
    
    virtual void need_lvalue() {
        if (ts[0] != lvalue_type && ts[0] != uninitialized_type && ts[0] != partial_type)
            throw INTERNAL_ERROR;
            
        lvalue_needed = true;
    }
    
    virtual Regs precompile(Regs) {
        std::cerr << "This Value shouldn't have been precompiled!\n";
        throw INTERNAL_ERROR;
    }

    virtual Regs precompile_tail() {
        // Use to precompile a Value with no preference with the result location, and
        // allowing all borrows, since there's no subsequent sibling argument, or
        // this Value is stored onto the stack anyway.
        return precompile(Regs::all());
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
    
    virtual Storage compile_lvalue(X64 *x64) {
        Storage s = compile(x64);
        
        if (s.where == MEMORY) {
            if (s.address.base == RBP) {
                if (s.address.index != NOREG)
                    throw INTERNAL_ERROR;
                
                // Stack-local addresses are handled in compile time
                return s;
            }
            else if (s.address.base == RSP) {
                // RecordPreinitializer and FunctionCall can handle this, but they can
                // store RBP-relative values and fix later, while we can't.
                throw INTERNAL_ERROR;
            }
            else {
                // Borrowed dynamic addresses will be stored, and used as an ALISTACK
                x64->op(LEA, R10, s.address);
                x64->op(PUSHQ, 0);
                x64->op(PUSHQ, R10);
                
                return Storage(ALISTACK);
            }
        }
        else if (s.where == ALIAS) {
            if (s.address.base == RBP && s.address.index == NOREG) {
                // Stack-local addresses are handled in compile time
                return s;
            }
            else
                throw INTERNAL_ERROR;
        }
        else if (s.where == ALISTACK) {
            return s;
        }
        else
            throw INTERNAL_ERROR;
    }
    
    virtual Declaration *declare(std::string name, Scope *scope) {
        std::cerr << "Can't declare " << name << " with neither a type nor a metatype!\n";
        return NULL;
    }
    
    virtual void escape_statement_variables() {
        // Not needed in most Value classes
    }
    
    virtual Scope *unwind(X64 *x64) {
        std::cerr << "This Value can't be unwound!\n";
        throw INTERNAL_ERROR;
    }
    
    virtual bool define_data() {
        return true;
    }

    virtual bool define_code() {
        return true;
    }
    
    virtual Value *lookup_inner(std::string name, Scope *scope) {
        return ts.lookup_inner(name, this, scope);
    }

    virtual void streamify(X64 *x64) {
        return ts.streamify(x64);
    }
};


class Raiser {
public:
    RaisingDummy *raising_dummy;
    TreenumerationType *raised_type;
    bool am_implicit_matcher;
    
    Raiser() {
        raising_dummy = NULL;
        raised_type = NULL;
        am_implicit_matcher = false;
    }
    
    void be_implicit_matcher() {
        am_implicit_matcher = true;
    }
    
    virtual void make_raising_dummy(Scope *scope) {
        raising_dummy = new RaisingDummy;
        scope->add(raising_dummy);
    }
    
    virtual bool check_raise(TreenumerationType *exception_type, Scope *scope) {
        TryScope *try_scope = scope->get_try_scope();
        
        if (!try_scope) {
            std::cerr << "Exception " << exception_type->name << " not caught!\n";
            return false;
        }
        
        if (!try_scope->set_exception_type(exception_type, am_implicit_matcher)) {
            return false;
        }
        
        raised_type = exception_type;
        
        make_raising_dummy(scope);

        return true;
    }
    
    virtual void raise(std::string keyword, X64 *x64) {
        if (!raising_dummy) {
            std::cerr << "Oops, a Raiser::check_raise was not called somewhere!\n";
            throw INTERNAL_ERROR;
        }

        //x64->runtime->log("XXX raising " + keyword);
        x64->op(MOVQ, RDX, raised_type->get_keyword_index(keyword));
        x64->unwind->initiate(raising_dummy, x64);
    }
};

/*
class GenericLvalue {
public:
    bool lvalue_needed;
    Storage rvalue_storage;
    
    GenericLvalue() {
        lvalue_needed = false;
    }
    
    virtual void need_lvalue() {
        lvalue_needed = true;
    }
};
*/

class ContainedLvalue {
public:
    Storage rvalue_storage;
    bool use_lvalue;
    bool may_borrow;
    
    ContainedLvalue() {
        may_borrow = false;
    }
    
    virtual Regs precompile_contained_lvalue(Regs preferred, bool lvalue_needed, TypeSpec ts) {
        // All contained values are on the heap
        use_lvalue = lvalue_needed;
        Regs borrowing_requirements = Regs::heapvars();
        
        if (use_lvalue)
            return borrowing_requirements;
        else {
            if ((preferred & borrowing_requirements) == borrowing_requirements)
                may_borrow = true;
            
            // May need to load the value
            rvalue_storage = ts.optimal_value_storage(preferred);
            return rvalue_storage.regs();
        }
    }
    
    virtual Storage compile_contained_lvalue(Address addr, Register container_ref, TypeSpec ts, X64 *x64) {
        if (use_lvalue) {
            // Lvalue
            
            if (container_ref == NOREG) {
                // Borrow contained value as well
                return Storage(MEMORY, addr);
            }
            else {
                // Return ALISTACK
                x64->op(PUSHQ, container_ref);
                x64->op(LEA, R10, addr);
                x64->op(PUSHQ, R10);
                return Storage(ALISTACK);
            }
        }
        else {
            // Rvalue
            
            if (container_ref == NOREG) {
                if (may_borrow) {
                    // Borrow contained value as well
                    return Storage(MEMORY, addr);
                }
                else {
                    // Load value
                    return ts.store(Storage(MEMORY, addr), rvalue_storage, x64);
                }
            }
            else {
                // Load value and decref container
                // NOTE: rvalue_storage may be using the container_ref register, because
                // we're to silly to take care. So override the optimal storage and
                // use STACK instead, that always works.
                
                if (rvalue_storage.regs() & Regs(container_ref))
                    rvalue_storage = Storage(STACK);
                    
                Storage t = ts.store(Storage(MEMORY, addr), rvalue_storage, x64);
                x64->runtime->decref(container_ref);
                return t;
            }
        }
    }
};


class PassValue: public Value {
public:
    PassValue(Value *p, TypeMatch tm)
        :Value(VOID_TS) {
    }

    virtual Regs precompile(Regs preferred) {
        return Regs();
    }

    virtual Storage compile(X64 *x64) {
        return Storage();
    }
};


class CastValue: public Value {
public:
    std::unique_ptr<Value> pivot;

    CastValue(Value *p, TypeSpec ts)
        :Value(ts) {
        pivot.reset(p);
    }

    virtual void need_lvalue() {
        pivot->need_lvalue();
    }

    virtual Regs precompile(Regs preferred) {
        return pivot->precompile(preferred);
    }

    virtual Storage compile(X64 *x64) {
        return pivot->compile(x64);
    }
};


class PtrCastValue: public Value {
public:
    std::unique_ptr<Value> value;
    
    PtrCastValue(Value *p, TypeMatch tm)
        :Value(NO_TS) {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!check_arguments(args, kwargs, { { "ref", &ANYID_REF_TS, scope, &value } }))
            return false;

        ts = value->ts.rvalue().reprefix(ref_type, ptr_type);
        return true;
    }

    virtual Regs precompile(Regs preferred) {
        return value->precompile(preferred);
    }

    virtual Storage compile(X64 *x64) {
        return value->compile(x64);
    }
};


class VariableValue: public Value {
public:
    Variable *variable;
    std::unique_ptr<Value> pivot;
    Register unalias_reg;
    TypeMatch match;
    TypeSpec pts;
    //bool is_rvalue_record;
    //bool is_single_record;
    Storage value_storage;
    Regs borrows_allowed;
    bool ralias_needed;
    
    VariableValue(Variable *v, Value *p, Scope *scope, TypeMatch &tm)
        :Value(v->get_typespec(tm)) {
        variable = v;
        pivot.reset(p);
        unalias_reg = NOREG;
        match = tm;
        ralias_needed = false;
        
        if (pivot) {
            pts = pivot->ts.rvalue();

            // Sanity check
            if (pts[0] == ref_type)
                throw INTERNAL_ERROR;  // member variables are accessed by pointers only
            /*
            //is_rvalue = (pivot->ts[0] != lvalue_type && pts[0] != ptr_type && !pts.has_meta(singleton_metatype));
            if (pts.has_meta(record_metatype)) {
                if (ptr_cast<RecordType>(pts[0])->is_single) {
                    is_single_record = true;
                }
                else if (pivot->ts[0] != lvalue_type) {
                    is_rvalue_record = true;
                    ts = ts.rvalue();
                }
            }
            */
        }
    }
    
    virtual void need_lvalue() {
        Value::need_lvalue();
        
        // The pivot type of member variables is not lvalue, so they can work with
        // non-lvalue containers. So if the member turns out to be needed as an lvalue,
        // we must warn the container so it can clobber the appropriate flags.
        
        if (pivot && pts[0] != ptr_type)
            pivot->need_lvalue();
    }
    
    virtual void need_ralias() {
        // This is an optimization so that rvalue ALIAS-es are not value copied,
        // but returned as ALIAS, because the parent VariableValue can handle it.
        // We can't borrow an ALIAS, since if loaded into a MEMORY, it may be spilled
        // to the stack, but may point to the stack itself, which is relocatable.
        // But that's suboptimal for members of ALIAS variables, so make a little effort.
        
        ralias_needed = true;
    }
    
    virtual Regs precompile(Regs preferred) {
        borrows_allowed = preferred & (Regs::stackvars() | Regs::heapvars());
        Regs clob;
        
        if (pivot) {
            clob = pivot->precompile(preferred);
            
            if (!lvalue_needed) {
                // Try some optimization
                VariableValue *vvp = ptr_cast<VariableValue>(pivot.get());
                
                if (vvp)
                    vvp->need_ralias();
            }
        }
        
        if (variable->where == NOWHERE)
            throw INTERNAL_ERROR;

        unalias_reg = preferred.has_gpr() ? preferred.get_gpr() : RAX;
        clob = clob | unalias_reg;

        if (lvalue_needed) {
            if (pivot) {
                // The clobbered flags were already set by the pivot value, above
            }
            else {
                clob = clob | variable->borrowing_requirements();
            }
        }
        else {
            // Can't make all decisions now, allocate, and we'll see
            value_storage = ts.optimal_value_storage(preferred);
            clob = clob | value_storage.regs();
        }
        
        return clob;
    }
    
    virtual Storage compile(X64 *x64) {
        Storage t;
        
        if (pivot) {
            Storage s = pivot->compile(x64);
            
            if (pts[0] == ptr_type) {
                // The only member variable accessible by a Ptr are self variables, borrow.
                // Don't disguise this as an ALIAS, we know it points to the heap.
                
                if (s.where == MEMORY) {
                    x64->op(MOVQ, unalias_reg, s.address);
                    s = Storage(MEMORY, Address(unalias_reg, 0));
                    t = variable->get_storage(match, s);  // [unalias_reg + x]
                }
                else
                    throw INTERNAL_ERROR;
            }
            else {
                if (s.where == REGISTER || s.where == SSEREGISTER)
                    t = s;  // optimized record, no need to extract
                else if (s.where == STACK)
                    t = s;  // we'll do the member extraction later
                else if (s.where == MEMORY || s.where == ALIAS)
                    t = variable->get_storage(match, s);
                else if (s.where == ALISTACK) {
                    x64->op(ADDQ, Address(RSP, 0), variable->get_offset(match));
                    t = s;
                }
                else
                    throw INTERNAL_ERROR;
            }
        }
        else {
            t = variable->get_local_storage();
        }

        // If the parent can handle lvalues, it can handle any storages
        if (lvalue_needed) {
            if (t.where == MEMORY || t.where == ALIAS || t.where == ALISTACK)
                return t;
            else
                throw INTERNAL_ERROR;
        }
            
        // Otherwise we must either borrow a MEMORY, or do a value copy

        if (pivot) {
            // The pivot did all the necessary borrowing checks
            
            if (t.where == REGISTER || t.where == SSEREGISTER)
                return t;
            else if (t.where == STACK) {
                // Must extract the member from a STACK value
                
                // We can save this step for single member records
                if (ts.measure_stack() != pts.measure_stack()) {
                    // A store from an RSP-relative address to a STACK is invalid
                    x64->op(MOVQ, unalias_reg, RSP);
                    Storage ms = variable->get_storage(match, Storage(MEMORY, Address(unalias_reg, 0)));
                
                    // If the optimal storage is REGISTER, this may overwrite unalias_reg, that's OK
                    ts.store(ms, value_storage, x64);
                    pts.destroy(Storage(MEMORY, Address(RSP, 0)), x64);
                    
                    if (value_storage.where == STACK) {
                        // We must shift up the member (the member size is smaller)
                        int offset = pts.measure_stack();
                        int length = ts.measure_stack();
                        
                        for (int i = 0; i < length; i += ADDRESS_SIZE)
                            x64->op(POPQ, Address(RSP, offset - ADDRESS_SIZE));  // POP [RSP]!
                            
                        x64->op(ADDQ, RSP, offset - length);
                    }
                    else {
                        // Just drop the stack value
                        x64->op(ADDQ, RSP, pts.measure_stack());
                    }
                    
                    t = value_storage;
                }
                
                return t;  // return value copy
            }
            else if (t.where == MEMORY)
                return t;  // return borrowable location
            else if (t.where == ALIAS) {
                // We must have asked for this with need_ralias
                
                if (!ralias_needed) {
                    // Do a value copy, as an ALIAS can't be borrowed
                    x64->op(MOVQ, unalias_reg, t.address);
                    t = Storage(MEMORY, Address(unalias_reg, t.value));  // [unalias_reg + x]
                    t = ts.store(t, value_storage, x64);
                }
                
                return t;
            }
            else
                throw INTERNAL_ERROR;  // ALISTACK is invalid for rvalues
        }
        else {
            if (t.where == NOWHERE)
                return t;  // for GlobalNamespace and Unit
            else if (t.where == MEMORY) {
                // Possibly borrowable, check for the requirements
                Regs borrows_required = variable->borrowing_requirements();
                        
                if ((borrows_allowed & borrows_required) != borrows_required) {
                    // Make a value copy if the location may be clobbered
                    t = ts.store(t, value_storage, x64);
                }
                
                return t;
            }
            else if (t.where == ALIAS) {
                // An ALIAS can't be borrowed
                
                if (!ralias_needed) {
                    // Do a value copy
                    x64->op(MOVQ, unalias_reg, t.address);
                    t = Storage(MEMORY, Address(unalias_reg, t.value));  // [unalias_reg + x]
                    t = ts.store(t, value_storage, x64);
                }
                
                return t;
            }
            else
                throw INTERNAL_ERROR;
        }
    }
};


class SelfVariableValue: public VariableValue {
public:
    SelfInfo *self_info;
    
    SelfVariableValue(Variable *pv, Value *p, Scope *scope, TypeMatch &match, SelfInfo *si)
        :VariableValue(pv, p, scope, match) {
        self_info = si;
    }
    
    virtual Value *lookup_inner(std::string name, Scope *scope) {
        //std::cerr << "XXX self inner lookup " << name << "\n";
        Identifier *i = self_info->get_special(name);
        
        if (i) {
            // FIXME: this seems to be fairly special for calling base functions
            //std::cerr << "XXX special\n";
            Function *f = ptr_cast<Function>(i);  // The current function
            Role *r = ptr_cast<Role>(f->associated);
            TypeMatch tm = ts.match();
            Value *rv = r->matched(this, scope, tm);
            
            Function *iff = f->implemented_function;
            Value *v = iff->matched(rv, scope, tm);  // Must make sure it matches
            //std::cerr << "XXX " << v << "\n";
            if (!v)
                throw INTERNAL_ERROR;
                
            function_call_be_static(v, r);

            return v;
        }
    
        return VariableValue::lookup_inner(name, scope);
    }
};


class PartialVariableValue: public VariableValue {
public:
    PartialInfo *partial_info;
    
    PartialVariableValue(Variable *pv, Value *p, Scope *scope, TypeMatch &match, PartialInfo *pi)
        :VariableValue(pv, p, scope, match) {
        partial_info = pi;
        
        // First, it's not assignable, second, it may already have an lvalue_type inside it
        ts = ts.rvalue();
    }

    virtual Value *lookup_inner(std::string name, Scope *scope) {
        //std::cerr << "XXX partial inner lookup " << name << "\n";
        std::string::size_type dotpos = name.find('.');
        
        if (dotpos != std::string::npos) {
            // Handle lookup of base initializers in the form of "role.init"
            std::string role_name = name.substr(0, dotpos + 1);  // include trailing dot
            std::string method_name = name.substr(dotpos + 1);   // just method name
            
            Value *role_value = VariableValue::lookup_inner(role_name, scope);
            if (!role_value)
                return NULL;

            // This is a lookup of an initializer, but this time we need to supply
            // a pivot value, so the inner lookup mechanism is used, and ClassType will
            // get it right.
            return role_value->lookup_inner(method_name, scope);
        }
        
        return VariableValue::lookup_inner(name, scope);
    }
};

class EvaluableValue: public Value {
public:
    Evaluable *evaluable;
    
    EvaluableValue(Evaluable *e)
        :Value(e->alloc_ts) { //uncodify(typesubst(e->alloc_ts, tm))) {
        evaluable = e;
    }
    
    virtual std::vector<Variable *> get_dvalue_variables() {
        return evaluable->get_dvalue_variables();
    }

    virtual Regs precompile(Regs preferred) {
        return Regs();
    }

    virtual Storage compile(X64 *x64) {
        return evaluable->get_local_storage();
    }
};


class EvaluateValue: public Value, public Raiser {
public:
    std::vector<Variable *> dvalue_variables;
    std::unique_ptr<Value> evaluable_value;
    std::vector<std::unique_ptr<Value>> arg_values;
    //std::vector<StorageWhere> retro_wheres;
    FunctionScope *fn_scope;
    
    static TypeSpec uncodify(TypeSpec ts) {
        ts = ts.unprefix(code_type);
        
        if (ts[0] == tuple0_type)
            ts = VOID_TS;
        else if (ts[0] == tuple1_type)
            ts = ts.unprefix(tuple1_type);
            
        return ts;
    }
    
    EvaluateValue(Value *p, TypeMatch &tm)
        :Value(NO_TS) { //uncodify(typesubst(e->alloc_ts, tm))) {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() != 1) {
            std::cerr << "Whacky :evaluate!\n";
            return false;
        }
        
        Value *v = typize(args[0].get(), scope, NULL);
        evaluable_value.reset(v);
        
        EvaluableValue *ev = ptr_cast<EvaluableValue>(v);
        
        if (!ev) {
            std::cerr << "Invalid :evaluate argument!\n";
            return false;
        }
        
        dvalue_variables = ev->get_dvalue_variables();

        TypeSpec tuple_ts = evaluable_value->ts.unprefix(code_type);
        TSs res_tss;
        tuple_ts.unpack_tuple(res_tss);
        
        // TODO: not nice
        ts = (res_tss.size() == 1 ? res_tss[0] : tuple_ts);

        ArgInfos infos;
        TSs arg_tss;
        
        arg_tss.reserve(dvalue_variables.size());  // prevent reallocations
        arg_values.reserve(dvalue_variables.size());  // prevent reallocations
                
        for (auto v : dvalue_variables) {
            arg_values.push_back(NULL);
            
            // Must keep these, as we hold a pointer to each
            TypeSpec tuple_ts = v->get_typespec(TypeMatch()).unprefix(dvalue_type);
            TSs member_tss;
            tuple_ts.unpack_tuple(member_tss);
            
            // TODO: not nice
            arg_tss.push_back(member_tss.size() == 1 ? member_tss[0] : tuple_ts);
            
            infos.push_back(ArgInfo {
                v->name.c_str(),
                &arg_tss.back(),
                scope,
                &arg_values.back()
            });
        }

        Args fake_args;

        if (!check_arguments(fake_args, kwargs, infos))
            return false;
            
        // Must insert dummy after evaluating arguments
        if (!check_raise(code_break_exception_type, scope))
            return false;

        fn_scope = scope->get_function_scope();
            
        return true;
    }
    
    virtual Regs precompile(Regs preferred) {
        for (auto &a : arg_values)
            a->precompile_tail();  // will be pushed
            
        return Regs::all();
    }

    virtual void destroy_arguments(X64 *x64) {
        for (int i = dvalue_variables.size() - 1; i >= 0; i--) {
            Variable *dvalue_var = dvalue_variables[i];
            Storage dvalue_storage = dvalue_var->get_local_storage();
            TypeSpec dvalue_ts = dvalue_var->get_typespec(TypeMatch());
            TypeSpec tuple_ts = dvalue_ts.unprefix(dvalue_type);
            //StorageWhere retro_where = retro_wheres[i];

            // Must use RBX to load the ALIAS-es to, because RAX is a valid result storage
            Register reg = RBX;
            x64->op(MOVQ, reg, dvalue_storage.address);
            tuple_ts.destroy(Storage(MEMORY, Address(reg, 0)), x64);
        }
    }
    
    virtual Storage compile(X64 *x64) {
        x64->unwind->push(this);
        
        for (unsigned i = 0; i < arg_values.size(); i++) {
            arg_values[i]->compile_and_store(x64, Storage(STACK));
            
            Variable *dvalue_var = dvalue_variables[i];
            Storage dvalue_storage = dvalue_var->get_local_storage();
            if (dvalue_storage.where != ALIAS)
                throw INTERNAL_ERROR;
                
            Register reg = RBX;
            x64->op(MOVQ, reg, dvalue_storage.address);

            TypeSpec dvalue_ts = dvalue_var->get_typespec(TypeMatch());
            TypeSpec tuple_ts = dvalue_ts.unprefix(dvalue_type);
            
            if (tuple_ts.where(AS_ARGUMENT) != MEMORY)
                throw INTERNAL_ERROR;
            
            tuple_ts.create(Storage(STACK), Storage(MEMORY, Address(reg, 0)), x64);
        }

        Storage es = evaluable_value->compile(x64);
        if (es.where != MEMORY)
            throw INTERNAL_ERROR;

        StorageWhere where = ts.where(AS_VALUE);
        Storage t = (
            where == NOWHERE ? Storage() :
            where == REGISTER ? Storage(REGISTER, RAX) :
            where == SSEREGISTER ? Storage(SSEREGISTER, XMM0) :
            Storage(STACK)
        );
        
        x64->op(CALL, es.address);

        // This is like with function calls
        Label noex;
        x64->op(JE, noex);  // Expect ZF if OK

        // Store forwarded exception, and replace it with CODE_BREAK
        Storage fes = fn_scope->get_forwarded_exception_storage();
        if (fes.where != MEMORY)
            throw INTERNAL_ERROR;
            
        x64->op(MOVQ, fes.address, RDX);
        x64->op(MOVQ, RDX, code_break_exception_type->get_keyword_index("CODE_BREAK"));
        x64->unwind->initiate(raising_dummy, x64);  // unwinds ourselves, too
        
        x64->unwind->pop(this);
        
        x64->code_label(noex);
        destroy_arguments(x64);

        // We don't allocate space for the results on the stack, so we must
        // account for the result size manually, but only on success.
        if (t.where == STACK) {
            TypeSpec tuple_ts = evaluable_value->ts.unprefix(code_type);
            int result_stack_size = tuple_ts.measure_where(tuple_ts.where(AS_ARGUMENT));
            x64->accounting->adjust_stack_usage(result_stack_size);
        }
        
        return t;
    }

    virtual Scope *unwind(X64 *x64) {
        destroy_arguments(x64);
        
        return NULL;
    }
};


class RoleValue: public Value {
public:
    Associable *associable;
    std::unique_ptr<Value> pivot;
    Register reg;
    TypeMatch match;
    
    RoleValue(Associable *a, Value *p, TypeMatch &tm)
        :Value(NO_TS) {
        TypeSpec ats = typesubst(a->alloc_ts, tm);
        TypeSpec pts = p->ts.rvalue();
        
        if (pts[0] != ptr_type && pts[0] != ref_type)
            throw INTERNAL_ERROR;
            
        ts = ats.prefix(pts[0]);  // keep same Ref/Ptr

        associable = a;
        pivot.reset(p);
        reg = NOREG;
        match = tm;
    }
    
    virtual Regs precompile(Regs preferred) {
        Regs clob = pivot->precompile(preferred);
            
        if (associable->where == NOWHERE)
            throw INTERNAL_ERROR;
            
        // Get a working register just in case
        if (clob.has_gpr())
            reg = clob.get_gpr();
        else {
            reg = RAX;
            clob = clob | reg;
        }
            
        return clob;
    }
    
    virtual Storage compile(X64 *x64) {
        // The offset of the role can be determined in 3 ways:
        //   * for BASE and MAIN roles it is constant 0
        //   * for concrete roles, it is the offset they computed in allocate/relocate
        //   * for abstract roles, it can be retrieved from the VT at its virtual index
        
        Storage s = pivot->compile(x64);
        
        if (associable->inherit_as == AS_BASE || associable->inherit_as == AS_MAIN)
            return s;
        else if (!associable->is_abstract()) {
            int offset = associable->get_offset(match);
        
            //std::cerr << "XXX RoleValue for " << role->name << " has offset " << offset << "\n";
        
            switch (s.where) {
            case REGISTER:
                x64->op(ADDQ, s.reg, offset);
                return s;
            case STACK:
                x64->op(ADDQ, Address(RSP, 0), offset);
                return s;
            case MEMORY:
                pivot->ts.store(s, Storage(REGISTER, reg), x64);
                x64->op(ADDQ, reg, offset);
                return Storage(REGISTER, reg);
            default:
                throw INTERNAL_ERROR;
            }
        }
        else {
            int virtual_index = associable->virtual_index;
            if (virtual_index == 0)
                throw INTERNAL_ERROR;
                
            switch (s.where) {
            case REGISTER:
                x64->op(MOVQ, R10, Address(s.reg, CLASS_VT_OFFSET));
                x64->op(ADDQ, s.reg, Address(R10, virtual_index * ADDRESS_SIZE));
                return s;
            case STACK:
                x64->op(MOVQ, R10, Address(RSP, 0));
                x64->op(MOVQ, R10, Address(R10, CLASS_VT_OFFSET));
                x64->op(MOVQ, R10, Address(R10, virtual_index * ADDRESS_SIZE));
                x64->op(ADDQ, Address(RSP, 0), R10);
                return s;
            case MEMORY:
                pivot->ts.store(s, Storage(REGISTER, reg), x64);
                x64->op(MOVQ, R10, Address(reg, CLASS_VT_OFFSET));
                x64->op(MOVQ, R10, Address(R10, virtual_index * ADDRESS_SIZE));
                x64->op(ADDQ, reg, R10);
                return Storage(REGISTER, reg);
            default:
                throw INTERNAL_ERROR;
            }
        }
    }
};

