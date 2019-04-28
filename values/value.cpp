
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
        lvalue_needed = true;
    }
    
    virtual Regs precompile(Regs) {
        std::cerr << "This Value shouldn't have been precompiled!\n";
        throw INTERNAL_ERROR;
    }

    virtual Regs precompile() {
        return precompile(Regs::all());  // no particular preference
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

    virtual Storage compile_and_alias(X64 *x64, TemporaryAlias *ta) {
        return ta->process(compile(x64), x64);
    }

    virtual Declaration *declare(std::string name, Scope *scope) {
        std::cerr << "Can't declare " << name << " with neither a type nor a metatype!\n";
        return NULL;
    }
    
    virtual bool unpack(std::vector<TypeSpec> &tss) {
        return false;
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


class Aliaser {
public:
    TemporaryAlias *temporary_alias;
    
    Aliaser() {
        temporary_alias = NULL;
    }
    
    bool check_alias(Scope *scope) {
        temporary_alias = new TemporaryAlias;
        scope->add(temporary_alias);
        return true;
    }

    TemporaryAlias *get_alias() {
        return temporary_alias;
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
    RetroScope *retro_scope;
    std::unique_ptr<Value> pivot;
    Register unalias_reg;
    TypeMatch match;
    TypeSpec pts;
    bool is_rvalue_record;
    bool is_single_record;
    Unborrow *unborrow;
    
    VariableValue(Variable *v, Value *p, Scope *scope, TypeMatch &tm)
        :Value(v->get_typespec(tm)) {
        variable = v;
        pivot.reset(p);
        unalias_reg = NOREG;
        match = tm;
        is_rvalue_record = false;
        is_single_record = false;
        unborrow = NULL;
        
        if (pivot) {
            pts = pivot->ts.rvalue();

            // Sanity check
            if (pts[0] == ref_type)
                throw INTERNAL_ERROR;  // member variables are accessed by pointers only
            else if (pts[0] == ptr_type) {
                unborrow = new Unborrow(pts.unprefix(ptr_type));  // may or may not be used
                scope->add(unborrow);
            }
                
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
            
            retro_scope = NULL;
        }
        else {
            retro_scope = scope->get_retro_scope();
        }
    }
    
    virtual void need_lvalue() {
        Value::need_lvalue();
        
        // The pivot type of member variables is not lvalue, so they can work with
        // non-lvalue containers. So if the member turns out to be needed as an lvalue,
        // we must warn the container so it can clobber the appropriate storages.
        
        if (pivot)
            pivot->need_lvalue();
    }
    
    virtual Regs precompile(Regs preferred) {
        Regs clob = pivot ? pivot->precompile(preferred) : Regs();
            
        if (variable->where == NOWHERE)
            throw INTERNAL_ERROR;

        unalias_reg = preferred.get_any();
        clob = clob | unalias_reg;

        if (pivot) {
            if (variable->where == ALIAS)
                throw INTERNAL_ERROR;
        }
        else {
            if (lvalue_needed)
                clob = clob | variable->lvalue_would_clobber();
        }
        
        return clob;
    }
    
    virtual Storage compile(X64 *x64) {
        Storage t;
        
        if (pivot) {
            // Rvalue containers may be on the STACK, so we must extract our variable
            // and discard it all.
            if (is_rvalue_record)
                x64->op(SUBQ, RSP, ts.measure_stack());

            Storage s = pivot->compile(x64);
            
            if (pts[0] == ptr_type) {
                // The only member variable accessible by a Ptr are self variables, borrow.
                if (s.where == MEMORY) {
                    x64->op(MOVQ, unalias_reg, s.address);
                    s = Storage(MEMORY, Address(unalias_reg, 0));
                    return variable->get_storage(match, s);  // [unalias_reg + x]
                }
                else
                    throw INTERNAL_ERROR;
            }
            else if (is_single_record) {
                // Selecting the single member of a record is a no-op
                // This also assumes that there are no such things as custom record finalizers
                return s;
            }
            else if (is_rvalue_record) {
                Storage r;
                
                if (s.where == STACK) {
                    r = variable->get_storage(match, Storage(MEMORY, Address(RSP, 0)));
                    t = Storage(MEMORY, Address(RSP, pts.measure_stack()));
                }
                else if (s.where == MEMORY) {
                    r = variable->get_storage(match, s);
                    t = Storage(MEMORY, Address(RSP, 0));
                }
                else
                    throw INTERNAL_ERROR;
                    
                ts.create(r, t, x64);
                pts.store(s, Storage(), x64);
                
                return Storage(STACK);
            }
            else {
                Storage t = variable->get_storage(match, s);
                
                if (t.where == ALIAS && !lvalue_needed) {
                    // NOTE: this address base is unborrowable, so use a zero offset
                    // This may become a convention soon.
                    x64->op(MOVQ, unalias_reg, t.address);
                    x64->op(ADDQ, unalias_reg, t.value);
                    t = Storage(MEMORY, Address(unalias_reg, 0));  // [unalias_reg]
                }
                
                return t;
            }
        }
        else {
            Storage t = variable->get_local_storage();

            if (retro_scope) {
                // In a retro scope we're running in a stack frame that is at a fixed offset
                // from the enclosing stack frame, so all local variable addresses
                // must be adjusted (even for the ones declared inside the same retro scope).
                int retro_offset = retro_scope->get_frame_offset();
                
                if (t.where == NOWHERE)
                    ;
                else if (t.where == MEMORY || t.where == BMEMORY || t.where == ALIAS)
                    t.address.offset -= retro_offset;
                else
                    throw INTERNAL_ERROR;
            }

            //if (t.where == MEMORY && ts[0] != lvalue_type) {
                // Local variables that are not lvalues are constant during the
                // function execution, so they can be borrowed even if the subsequent
                // sibling arguments clobber everything that moves. Tell this to the parent.

            //    t.where = BMEMORY;
            //}

            if (t.where == ALIAS && !lvalue_needed) {
                // NOTE: this address base is unborrowable, so use a zero offset
                // This may become a convention soon.
                x64->op(MOVQ, unalias_reg, t.address);
                x64->op(ADDQ, unalias_reg, t.value);
                t = Storage(MEMORY, Address(unalias_reg, 0));  // [unalias_reg]
            }
            
            return t;
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


class EvaluableValue: public Value, public Raiser {
public:
    Evaluable *evaluable;
    std::vector<Variable *> dvalue_variables;
    std::vector<std::unique_ptr<Value>> arg_values;
    std::vector<StorageWhere> retro_wheres;
    unsigned result_stack_size;
    FunctionScope *fn_scope;
    
    EvaluableValue(Evaluable *e, Value *p, TypeMatch &tm)
        :Value(typesubst(e->alloc_ts, tm).unprefix(code_type)) {
        evaluable = e;
        
        dvalue_variables = e->get_dvalue_variables();
        result_stack_size = 0;
        
        if (p)
            throw INTERNAL_ERROR;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        ArgInfos infos;
        TSs arg_tss;
        
        arg_tss.reserve(dvalue_variables.size());  // prevent reallocations
        arg_values.reserve(dvalue_variables.size());  // prevent reallocations
                
        for (auto v : dvalue_variables) {
            arg_values.push_back(NULL);
            
            // Must keep these, as we hold a pointer to each
            arg_tss.push_back(v->get_typespec(TypeMatch()).unprefix(dvalue_type));
            
            infos.push_back(ArgInfo {
                v->name.c_str(),
                &arg_tss.back(),
                scope,
                &arg_values.back()
            });
        }

        if (!check_arguments(args, kwargs, infos))
            return false;
            
        // Must insert dummy after evaluating arguments
        if (!check_raise(code_break_exception_type, scope))
            return false;

        fn_scope = scope->get_function_scope();
            
        return true;
    }
    
    virtual Regs precompile(Regs preferred) {
        if (evaluable->where == NOWHERE)
            throw INTERNAL_ERROR;
    
        for (auto &a : arg_values)
            a->precompile(preferred);
            
        return Regs::all();
    }

    virtual void destroy_arguments(X64 *x64) {
        for (int i = dvalue_variables.size() - 1; i >= 0; i--) {
            Variable *dvalue_var = dvalue_variables[i];
            Storage dvalue_storage = dvalue_var->get_local_storage();
            TypeSpec dvalue_ts = dvalue_var->get_typespec(TypeMatch());
            TypeSpec retro_ts = dvalue_ts.unprefix(dvalue_type);
            StorageWhere retro_where = retro_wheres[i];

            // Must use RBX to load the ALIAS-es to, because RAX is a valid result storage
            Register reg = RBX;
            x64->op(MOVQ, reg, dvalue_storage.address);

            if (retro_where == MEMORY || retro_where == BMEMORY) {
                // rvalue retro, destroy value

                Storage t(retro_where, Address(reg, 0));
                retro_ts.destroy(t, x64);
            }
            else if (retro_where == ALIAS) {
                // lvalue retro, no need to clear address
            }
            else
                throw INTERNAL_ERROR;
        }
    }
    
    virtual Storage compile(X64 *x64) {
        x64->unwind->push(this);
        
        for (unsigned i = 0; i < arg_values.size(); i++) {
            Storage s = arg_values[i]->compile(x64);
            
            Variable *dvalue_var = dvalue_variables[i];
            Storage dvalue_storage = dvalue_var->get_local_storage();
            TypeSpec dvalue_ts = dvalue_var->get_typespec(TypeMatch());
            TypeSpec retro_ts = dvalue_ts.unprefix(dvalue_type);
            StorageWhere retro_where = retro_ts.where(AS_ARGUMENT);

            Register reg = (Regs::all() & ~s.regs()).get_any();
            x64->op(MOVQ, reg, dvalue_storage.address);
            
            if (retro_where == MEMORY) {
                // rvalue retro, initialize the value
                if (s.where == BMEMORY)
                    retro_where = BMEMORY;  // allow borrowing
                    
                Storage t(retro_where, Address(reg, 0));
                retro_ts.create(s, t, x64);
            }
            else if (retro_where == ALIAS) {
                // lvalue retro, initialize the address
                
                if (s.where == MEMORY || s.where == BMEMORY) {
                    x64->op(LEA, R10, s.address);
                }
                else if (s.where == ALIAS) {
                    x64->op(MOVQ, R10, s.address);
                    
                    if (s.value)
                        x64->op(ADDQ, R10, s.value);
                }
                else
                    throw INTERNAL_ERROR;

                x64->op(MOVQ, Address(reg, 0), R10);
            }
            else
                throw INTERNAL_ERROR;

            retro_wheres.push_back(retro_where);
        }

        Storage es = evaluable->get_local_storage();

        StorageWhere where = ts.where(AS_VALUE);
        Storage t = (
            where == NOWHERE ? Storage() :
            where == REGISTER ? Storage(REGISTER, RAX) :
            where == SSEREGISTER ? Storage(SSEREGISTER, XMM0) :
            Storage(STACK)
        );
        
        if (t.where == STACK) {
            result_stack_size = ts.measure_stack();
            x64->op(SUBQ, RSP, result_stack_size);
        }

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
        
        return t;
    }

    virtual Scope *unwind(X64 *x64) {
        destroy_arguments(x64);
        
        if (result_stack_size)
            x64->op(ADDQ, RSP, result_stack_size);
            
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
        if (clob.has_any())
            reg = clob.get_any();
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
            case BMEMORY:
                pivot->ts.store(s, Storage(BREGISTER, reg), x64);
                x64->op(ADDQ, reg, offset);
                return Storage(BREGISTER, reg);
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
            case BMEMORY:
                pivot->ts.store(s, Storage(BREGISTER, reg), x64);
                x64->op(MOVQ, R10, Address(reg, CLASS_VT_OFFSET));
                x64->op(MOVQ, R10, Address(R10, virtual_index * ADDRESS_SIZE));
                x64->op(ADDQ, reg, R10);
                return Storage(BREGISTER, reg);
            default:
                throw INTERNAL_ERROR;
            }
        }
    }
};

