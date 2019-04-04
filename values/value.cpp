
// Values

class Value {
public:
    TypeSpec ts;
    Token token;
        
    Value(TypeSpec t)
        :ts(t) {
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

    bool check_arguments(Args &args, Kwargs &kwargs, const ArgInfos &arg_infos) {
        // FIXME: shouldn't this be a proper method?
        return ::check_arguments(args, kwargs, arg_infos);
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        return check_arguments(args, kwargs, ArgInfos());
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
        
        x64->op(MOVQ, RDX, raised_type->get_keyword_index(keyword));
        x64->unwind->initiate(raising_dummy, x64);
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
    std::unique_ptr<Value> pivot;
    Register reg;
    TypeMatch match;
    TypeSpec pts;
    bool is_rvalue_record;
    bool is_single_record;
    Unborrow *unborrow;
    
    VariableValue(Variable *v, Value *p, Scope *scope, TypeMatch &tm)
        :Value(v->get_typespec(tm)) {
        variable = v;
        pivot.reset(p);
        reg = NOREG;
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
        }
    }
    
    virtual Regs precompile(Regs preferred) {
        Regs clob = pivot ? pivot->precompile(preferred) : Regs();
            
        if (variable->where == NOWHERE)
            throw INTERNAL_ERROR;

        if (pivot) {
            if (variable->where == ALIAS)
                throw INTERNAL_ERROR;
                
            if (pts[0] == ptr_type) {
                reg = preferred.get_any();
                clob = clob | reg;
            }
        }
        else {
            if (variable->where == ALIAS) {
                reg = preferred.get_any();
                //std::cerr << "Alias variable " << variable->name << " loaded to " << reg << "\n";
                clob = clob | reg;
            }
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
                if (s.where == BMEMORY) {
                    // We can borrow a reference to the container for free
                    pts.store(s, Storage(BREGISTER, reg), x64);
                }
                else {
                    // Or do that for money
                    pts.store(s, Storage(REGISTER, reg), x64);
                    x64->op(MOVQ, unborrow->get_address(), reg);
                }
                
                s = Storage(MEMORY, Address(reg, 0));
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
            
            if (s.where != MEMORY)
                throw INTERNAL_ERROR;
            
            t = variable->get_storage(match, s);
        }
        else {
            t = variable->get_local_storage();

            if (t.where == ALIAS) {
                x64->op(MOVQ, reg, t.address);
                t = Storage(MEMORY, Address(reg, 0));
            }
            
            if (variable->name == "$" && ts[0] == ptr_type) {
                // Try borrowing on the self argument, guaranteed not to change
                if (t.where != MEMORY)
                    throw INTERNAL_ERROR;
                    
                // Let the parent borrow this instead of making a reference copy
                t.where = BMEMORY;
            }
        }
        
        return t;    
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
            role_value_be_static(ptr_cast<RoleValue>(rv));
            
            Function *iff = f->implemented_function;
            Value *v = iff->matched(rv, scope, tm);  // Must make sure it matches
            //std::cerr << "XXX " << v << "\n";
            if (!v)
                throw INTERNAL_ERROR;

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
    std::vector<std::unique_ptr<Value>> arg_values;
    std::vector<Storage> arg_storages;
    FunctionScope *fn_scope;
    
    EvaluableValue(Evaluable *e, Value *p, TypeMatch &tm)
        :Value(typesubst(e->alloc_ts, tm).unprefix(code_type)) {
        evaluable = e;
        
        if (p)
            throw INTERNAL_ERROR;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        ArgInfos infos;
        TSs arg_tss;
        
        arg_tss.reserve(evaluable->arg_variables.size());  // prevent reallocations
        arg_values.reserve(evaluable->arg_variables.size());  // prevent reallocations
                
        for (auto v : evaluable->arg_variables) {
            arg_values.push_back(NULL);
            
            // Must keep these, as we hold a pointer to each
            arg_tss.push_back(v->alloc_ts.unprefix(dvalue_type));
            
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
    
    virtual Storage compile(X64 *x64) {
        x64->unwind->push(this);
        
        for (unsigned i = 0; i < arg_values.size(); i++) {
            Storage s = arg_values[i]->compile(x64);
            Register reg = (Regs::all() & ~s.regs()).get_any();
            
            Storage x = evaluable->arg_variables[i]->get_local_storage();
            if (x.where != ALIAS)
                throw INTERNAL_ERROR;
                
            arg_storages.push_back(x);
            x64->op(MOVQ, reg, x.address);
            Storage t(MEMORY, Address(reg, 0));
            
            evaluable->arg_variables[i]->alloc_ts.create(s, t, x64);
        }
    
        Storage es = evaluable->get_local_storage();
        
        if (ts != VOID_TS)
            x64->op(SUBQ, RSP, ts.measure_stack());
            
        x64->op(MOVQ, R10, es.address);  // Needs RBP
        x64->op(PUSHQ, RBP);
        x64->op(MOVQ, RBP, Address(RBP, 0));  // Restore caller RBP
        
        x64->op(CALL, R10);
        
        x64->op(POPQ, RBP);

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

        x64->code_label(noex);

        x64->unwind->pop(this);

        // FIXME: these must be destroyed even if the body raises an exception!
        for (int i = arg_storages.size() - 1; i >= 0; i--) {
            Storage x = arg_storages[i];
            Register reg = RAX;
            x64->op(MOVQ, reg, x.address);
            Storage t(MEMORY, Address(reg, 0));
            
            evaluable->arg_variables[i]->alloc_ts.destroy(t, x64);
        }
        
        return ts == VOID_TS ? Storage() : Storage(STACK);
    }

    virtual Scope *unwind(X64 *x64) {
        for (int i = arg_storages.size() - 1; i >= 0; i--) {
            Storage x = arg_storages[i];
            Register reg = RAX;
            x64->op(MOVQ, reg, x.address);
            Storage t(MEMORY, Address(reg, 0));
            
            evaluable->arg_variables[i]->alloc_ts.destroy(t, x64);
        }
        
        if (ts != VOID_TS)
            x64->op(ADDQ, RSP, ts.measure_stack());
            
        return NULL;
    }
};


class RoleValue: public Value {
public:
    Associable *associable;
    std::unique_ptr<Value> pivot;
    Register reg;
    TypeMatch match;
    bool am_static;
    
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
        am_static = false;
    }
    
    virtual void be_static() {
        am_static = true;
    }
    
    virtual bool is_static() {
        return am_static;
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

