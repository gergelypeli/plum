#include "../plum.h"


// Values

Value::Value(TypeSpec t)
    :ts(t) {
}

Value::~Value() {
}

Value *Value::set_token(Token t) {
    token = t;
    return this;
}

void Value::set_context_ts(TypeSpec *c) {
    // Generally we don't need it, only in controls
}

bool Value::check_arguments(Args &args, Kwargs &kwargs, const ArgInfos &arg_infos, bool is_function_call) {
    // FIXME: shouldn't this be a proper method?
    return ::check_arguments(args, kwargs, arg_infos, is_function_call);
}

bool Value::check(Args &args, Kwargs &kwargs, Scope *scope) {
    return check_arguments(args, kwargs, ArgInfos());
}

Regs Value::precompile(Regs) {
    std::cerr << "This Value shouldn't have been precompiled!\n";
    throw INTERNAL_ERROR;
}

Regs Value::precompile_tail() {
    // Use to precompile a Value with no preference with the result location, and
    // allowing all borrows, since there's no subsequent sibling argument, or
    // this Value is stored onto the stack anyway.
    return precompile(Regs::all());
}

Storage Value::compile(X64 *) {
    std::cerr << "This Value shouldn't have been compiled!\n";
    throw INTERNAL_ERROR;
}

void Value::compile_and_store(X64 *x64, Storage t) {
    Storage s = compile(x64);
    //std::cerr << "Compiled and storing a " << ts << " from " << s << " to " << t << ".\n";
    ts.store(s, t, x64);
}

Storage Value::compile_lvalue(X64 *x64) {
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

Declaration *Value::declare(std::string name, Scope *scope) {
    std::cerr << "Can't declare " << name << " with neither a type nor a metatype!\n";
    return NULL;
}

void Value::escape_statement_variables() {
    // Not needed in most Value classes
}

CodeScope *Value::unwind(X64 *x64) {
    std::cerr << "This Value can't be unwound!\n";
    throw INTERNAL_ERROR;
}

bool Value::define_data() {
    return true;
}

bool Value::define_code() {
    return true;
}

Value *Value::lookup_inner(std::string name, Scope *scope) {
    return ts.lookup_inner(name, this, scope);
}

void Value::streamify(X64 *x64) {
    return ts.streamify(x64);
}




Raiser::Raiser() {
    raising_dummy = NULL;
    raised_type = NULL;
    am_implicit_matcher = false;
}

void Raiser::be_implicit_matcher() {
    am_implicit_matcher = true;
}

void Raiser::make_raising_dummy(Scope *scope) {
    raising_dummy = new RaisingDummy(EXCEPTION_UNWOUND);
    scope->add(raising_dummy);
}

bool Raiser::check_raise(TreenumerationType *exception_type, Scope *scope) {
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

void Raiser::raise(std::string keyword, X64 *x64) {
    if (!raising_dummy) {
        std::cerr << "Oops, a Raiser::check_raise was not called somewhere!\n";
        throw INTERNAL_ERROR;
    }

    //x64->runtime->log("XXX raising " + keyword);
    x64->op(MOVQ, RDX, raised_type->get_keyword_index(keyword));
    x64->unwind->initiate(raising_dummy, x64);
}

void Raiser::drop_and_raise(TypeSpec left_ts, Storage ls, std::string keyword, X64 *x64) {
    int old_stack_usage = x64->accounting->mark();
    left_ts.store(ls, Storage(), x64);
    
    raise(keyword, x64);
    
    x64->accounting->rewind(old_stack_usage);
}


void Raiser::drop_two_and_raise(TypeSpec right_ts, Storage rs, TypeSpec left_ts, Storage ls, std::string keyword, X64 *x64) {
    int old_stack_usage = x64->accounting->mark();
    right_ts.store(rs, Storage(), x64);
    left_ts.store(ls, Storage(), x64);
    
    raise(keyword, x64);
    
    x64->accounting->rewind(old_stack_usage);
}




RvalueCastValue::RvalueCastValue(Value *v)
    :Value(v->ts.rvalue()) {
    pivot.reset(v);
    ralias_needed = false;

    if (v->ts[0] != lvalue_type && v->ts[0] != uninitialized_type && v->ts[0] != partial_type)
        throw INTERNAL_ERROR;
}

void RvalueCastValue::need_ralias() {
    // This is an optimization so that rvalue ALIAS-es are not value copied,
    // but returned as ALIAS, because the parent VariableValue can handle it.
    // We can't borrow an ALIAS, since if loaded into a MEMORY, it may be spilled
    // to the stack, but may point to the stack itself, which is relocatable.
    // But that's suboptimal for members of ALIAS variables, so make a little effort.
    
    ralias_needed = true;
}

Regs RvalueCastValue::precompile(Regs preferred) {
    GenericLvalue *glv = ptr_cast<GenericLvalue>(pivot.get());
    
    if (glv)
        glv->need_rvalue();
    else
        throw INTERNAL_ERROR;

    borrows_allowed = preferred & (Regs::stackvars() | Regs::heapvars());

    Regs clob = pivot->precompile(preferred);
    
    // We'd need one register to handle ALIAS, and two for ALISTACK pivots
    clob.reserve_gpr(2);
    unalias_reg = clob.get_gpr();
    container_reg = (clob & ~Regs(unalias_reg)).get_gpr();
    
    // May need to load the value
    // Overwriting unalias_reg in one step is fine, but killing container_reg is not.
    rvalue_storage = ts.optimal_value_storage(preferred & ~Regs(container_reg));
    
    return clob | rvalue_storage.regs();
}

Storage RvalueCastValue::compile(X64 *x64) {
    Storage s = pivot->compile(x64);
    
    switch (s.where) {
    case MEMORY:
        if (s.address.base == RSP)
            throw INTERNAL_ERROR;
        else {
            // The base may be NOREG for static locations
            Regs borrow_needed = (s.address.base == RBP ? Regs::stackvars() : Regs::heapvars());
            
            if (borrow_needed & borrows_allowed)
                return s;
            else
                return ts.store(s, rvalue_storage, x64);
        }
    case ALIAS:
        if (s.address.base != RBP)
            throw INTERNAL_ERROR;
        else if (ralias_needed) {
            // A bit of optimization for VariableValue parents
            return s;
        }
        else {
            // An ALIAS may point to stack-local addresses, so we can't borrow it.

            x64->op(MOVQ, unalias_reg, s.address);

            Storage x(MEMORY, Address(unalias_reg, s.value));
            ts.store(x, rvalue_storage, x64);
            
            return rvalue_storage;
        }
    case ALISTACK:
        // Can't borrow, as the container may be freed, so make a value copy.
        // Assume that the container is always non-NULL, as there should be no reason
        // to return ALISTACK if not.
        
        x64->op(POPQ, unalias_reg);
        x64->op(POPQ, container_reg);

        s = Storage(MEMORY, Address(unalias_reg, s.value));
        ts.store(s, rvalue_storage, x64);
        
        x64->runtime->decref(container_reg);
        
        return rvalue_storage;
    default:
        return s;
    }
}




GenericLvalue::GenericLvalue() {
    rvalue_needed = false;
}


void GenericLvalue::need_rvalue() {
    rvalue_needed = true;
}




ContainedLvalue::ContainedLvalue()
    :GenericLvalue() {
}

Regs ContainedLvalue::precompile_contained_lvalue() {
    return !rvalue_needed ? Regs::heapvars() : Regs();
}

Storage ContainedLvalue::compile_contained_lvalue(Address addr, Register container_ref, TypeSpec ts, X64 *x64) {
    if (container_ref == NOREG) {
        // Return borrowable contained value as well
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



PassValue::PassValue(Value *p, TypeMatch tm)
    :Value(VOID_TS) {
}

Regs PassValue::precompile(Regs preferred) {
    return Regs();
}

Storage PassValue::compile(X64 *x64) {
    return Storage();
}




CastValue::CastValue(Value *p, TypeSpec ts)
    :Value(ts) {
    pivot.reset(p);
}

void CastValue::need_rvalue() {
    ptr_cast<GenericLvalue>(pivot.get())->need_rvalue();
}

Regs CastValue::precompile(Regs preferred) {
    return pivot->precompile(preferred);
}

Storage CastValue::compile(X64 *x64) {
    return pivot->compile(x64);
}




PtrCastValue::PtrCastValue(Value *p, TypeMatch tm)
    :Value(NO_TS) {
}

bool PtrCastValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    if (!check_arguments(args, kwargs, { { "ref", &ANYID_REF_TS, scope, &value } }))
        return false;

    ts = value->ts.rvalue().reprefix(ref_type, ptr_type);
    return true;
}

Regs PtrCastValue::precompile(Regs preferred) {
    return value->precompile(preferred);
}

Storage PtrCastValue::compile(X64 *x64) {
    return value->compile(x64);
}




VariableValue::VariableValue(Variable *v, Value *p, Scope *scope, TypeMatch &tm)
    :Value(v->get_typespec(tm)) {
    variable = v;
    pivot.reset(p);
    record_reg = NOREG;
    match = tm;
    
    if (pivot) {
        pts = pivot->ts.rvalue();

        // Sanity check
        if (pts[0] == ref_type)
            throw INTERNAL_ERROR;  // member variables are accessed by pointers only
            
        if (pts[0] == ptr_type) {
            // The only Ptr whose member variables can be accessed is $, which should not
            // be an lvalue.
            if (pivot->ts[0] == lvalue_type)
                throw INTERNAL_ERROR;
        }
        else {
            // Members of rvalue records are rvalues themselves
            if (pivot->ts[0] != lvalue_type)
                ts = ts.rvalue();
        }
    }
}

void VariableValue::need_rvalue() {
    if (ts[0] != lvalue_type && ts[0] != uninitialized_type && ts[0] != partial_type)
        throw INTERNAL_ERROR;
        
    GenericLvalue::need_rvalue();

    if (pivot && pivot->ts[0] == lvalue_type) {
        // The pivot type of member variables is not lvalue, so they can work with
        // non-lvalue containers. So if the member turns out to be needed as an rvalue,
        // we must warn the container so it won't clobber the appropriate flags.
        // Also, accessing the members of a Ptr $ doesn't need lvalue pivot either.
        
        GenericLvalue *glv = ptr_cast<GenericLvalue>(pivot.get());
    
        if (glv)
            glv->need_rvalue();
    }
    
    // Try some optimization by asking RvalueCastValue not to unalias
    RvalueCastValue *rcv = ptr_cast<RvalueCastValue>(pivot.get());
    
    if (rcv)
        rcv->need_ralias();
}

Regs VariableValue::precompile(Regs preferred) {
    Regs clob;
    
    if (pivot) {
        clob = pivot->precompile(preferred);
        
        clob.reserve_gpr(1);
        record_reg = clob.get_gpr();
        member_storage = ts.optimal_value_storage(preferred);
    }
    
    if (variable->where == NOWHERE)
        throw INTERNAL_ERROR;

    if (!rvalue_needed) {
        if (ts[0] == uninitialized_type) {
            // This must be handled specially.
            // First, the Variable we point to is never Uninitialized, more like Lvalue,
            // so it cannot tell when it is initialized, and when assigned. So we check it.
            // Second, the CREATE operation returns an lvalue to the freshly created variable.
            // If it happens to be really used as an lvalue, it would clobber something.
            // Since CREATE relies on its pivot argument to set the necessary clobber flags,
            // the initialization must be considered clobbering a location, even if technically
            // it does not overwrite any meaningful data.
            
            if (pivot) {
                // Uninitialized member.
                // The Partial prefix is cast off the $ for the sake of member lookup.
                // And it is also prefixed by Lvalue for records.
                
                if (pivot->ts[0] == lvalue_type) {
                    // Uninitialized member of a record
                    // NOTE: we assume record initialization happens on the stack!
                    clob = clob | Regs::stackvars();
                }
                else if (pivot->ts[0] == ptr_type) {
                    // Uninitialized member of a class
                    clob = clob | Regs::heapvars();
                }
                else
                    throw INTERNAL_ERROR;
            }
            else {
                // Uninitialized local variable, it's on the stack
                clob = clob | Regs::stackvars();
            }
        }
        else if (ts[0] == lvalue_type) {
            if (pivot) {
                if (pivot->ts[0] == lvalue_type) {
                    // Lvalue member of an lvalue record, the pivot already clobbered
                }
                else if (pivot->ts[0] == ptr_type) {
                    // Lvalue member of a class, clobber the heap
                    clob = clob | Regs::heapvars();
                }
                else
                    throw INTERNAL_ERROR;
            }
            else {
                // Ask the local variable itself
                clob = clob | variable->borrowing_requirements();
            }
        }
        else {
            // It's OK to be a non-lvalue variable, then rvalue_needed is not set
        }
    }

    return clob;
}

Storage VariableValue::compile(X64 *x64) {
    Storage t;
    
    if (pivot) {
        Storage s = pivot->compile(x64);
        
        if (pts[0] == ptr_type) {
            // The only member variables accessible by a Ptr are self variables.
            // Since $ is a rvalue argument, we can borrow the reference.
            
            if (s.where == MEMORY) {
                x64->op(MOVQ, record_reg, s.address);  // borrow $
                s = Storage(MEMORY, Address(record_reg, 0));
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

    if (t.where == STACK) {
        // Must extract the member from a STACK value
        
        // We can save this step for single member records
        if (ts.measure_stack() != pts.measure_stack()) {
            // A store from an RSP-relative address to a STACK is invalid
            x64->op(MOVQ, record_reg, RSP);
            Storage ms = variable->get_storage(match, Storage(MEMORY, Address(record_reg, 0)));
        
            // If the optimal storage is REGISTER, this may overwrite unalias_reg, that's OK
            ts.store(ms, member_storage, x64);
            pts.destroy(Storage(MEMORY, Address(RSP, 0)), x64);
            
            if (member_storage.where == STACK) {
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
            
            t = member_storage;
        }
    }

    return t;
}




SelfVariableValue::SelfVariableValue(Variable *pv, Value *p, Scope *scope, TypeMatch &match, SelfInfo *si)
    :VariableValue(pv, p, scope, match) {
    self_info = si;
}

Value *SelfVariableValue::lookup_inner(std::string name, Scope *scope) {
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




PartialVariableValue::PartialVariableValue(Variable *pv, Value *p, Scope *scope, TypeMatch &match, PartialInfo *pi)
    :VariableValue(pv, p, scope, match) {
    partial_info = pi;
    
    // First, it's not assignable, second, it may already have an lvalue_type inside it
    ts = ts.rvalue();
}

Value *PartialVariableValue::lookup_inner(std::string name, Scope *scope) {
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




EvaluableValue::EvaluableValue(Evaluable *e)
    :Value(e->alloc_ts) { //uncodify(typesubst(e->alloc_ts, tm))) {
    evaluable = e;
}

std::vector<Variable *> EvaluableValue::get_dvalue_variables() {
    return evaluable->get_dvalue_variables();
}

Regs EvaluableValue::precompile(Regs preferred) {
    return Regs();
}

Storage EvaluableValue::compile(X64 *x64) {
    return evaluable->get_local_storage();
}




TypeSpec uncodify(TypeSpec ts) {
    ts = ts.unprefix(code_type);
    
    if (ts[0] == tuple0_type)
        ts = VOID_TS;
    else if (ts[0] == tuple1_type)
        ts = ts.unprefix(tuple1_type);
        
    return ts;
}

EvaluateValue::EvaluateValue(Value *p, TypeMatch &tm)
    :Value(NO_TS) { //uncodify(typesubst(e->alloc_ts, tm))) {
}

bool EvaluateValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
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

Regs EvaluateValue::precompile(Regs preferred) {
    for (auto &a : arg_values)
        a->precompile_tail();  // will be pushed
        
    return Regs::all();
}

void EvaluateValue::destroy_arguments(X64 *x64) {
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

Storage EvaluateValue::compile(X64 *x64) {
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

CodeScope *EvaluateValue::unwind(X64 *x64) {
    destroy_arguments(x64);
    
    return NULL;
}




RoleValue::RoleValue(Associable *a, Value *p, TypeMatch &tm)
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

Regs RoleValue::precompile(Regs preferred) {
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

Storage RoleValue::compile(X64 *x64) {
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

