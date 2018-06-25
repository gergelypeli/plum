
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

    virtual Declaration *declare(std::string name, ScopeType st) {
        if (st == CODE_SCOPE) {
            if (ts == VOID_TS) {
                std::cerr << "Can't declare " << name << " as Void!\n";
                return NULL;
            }
        
            TypeSpec var_ts = (
                ts[0]->upper_type == attribute_metatype ? ts.reprefix(ts[0], lvalue_type) :
                ts.lvalue()
            );
        
            return new Variable(name, NO_TS, var_ts);
        }
        else
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
    
    virtual bool complete_definition() {
        return true;
    }
    
    virtual Value *lookup_inner(std::string name) {
        return ts.lookup_inner(name, this);
    }
};


class Raiser {
public:
    RaisingDummy *raising_dummy;
    TreenumerationType *raised_type;
    
    Raiser() {
        raising_dummy = NULL;
        raised_type = NULL;
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
        
        if (!try_scope->set_exception_type(exception_type)) {
            std::cerr << "Exception " << exception_type->name << " is not the caught one!\n";
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


class VariableValue: public Value {
public:
    Variable *variable;
    std::unique_ptr<Value> pivot;
    Register reg;
    TypeMatch match;
    TypeSpec pts;
    bool is_rvalue;
    
    VariableValue(Variable *v, Value *p, TypeMatch &tm)
        :Value(v->get_typespec(tm)) {
        variable = v;
        pivot.reset(p);
        reg = NOREG;
        match = tm;
        is_rvalue = false;
        
        if (pivot) {
            pts = pivot->ts.rvalue();

            if (pts[0] == ref_type)
                throw INTERNAL_ERROR;  // variables are accessed by weak references only
                
            is_rvalue = (pivot->ts[0] != lvalue_type && pts[0] != weakref_type && !pts.has_meta(module_metatype));
            
            if (is_rvalue)
                ts = ts.rvalue();
        }
    }
    
    virtual Regs precompile(Regs preferred) {
        Regs clob = pivot ? pivot->precompile(preferred) : Regs();
            
        if (variable->where == NOWHERE)
            throw INTERNAL_ERROR;

        if (pivot) {
            if (variable->where == ALIAS)
                throw INTERNAL_ERROR;
                
            if (pts[0] == weakref_type) {
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
            // Rvalue containers are on the STACK, so we must extract our variable
            // and discard it all.
            if (is_rvalue)
                x64->op(SUBQ, RSP, ts.measure_stack());

            Storage s = pivot->compile(x64);
            
            if (pts[0] == weakref_type) {
                // FIXME: technically we must borrow a reference here, or the container
                // may be destroyed before accessing this variable!
                
                pts.store(s, Storage(REGISTER, reg), x64);
                x64->runtime->decweakref(reg);
                s = Storage(MEMORY, Address(reg, 0));
            }
            else if (is_rvalue) {
                if (s.where != STACK)
                    throw INTERNAL_ERROR;
                    
                s = variable->get_storage(match, Storage(MEMORY, Address(RSP, 0)));
                t = Storage(MEMORY, Address(RSP, pts.measure_stack()));

                ts.create(s, t, x64);
                pts.store(Storage(STACK), Storage(), x64);
                
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
        }
        
        return t;    
    }
};


class PartialVariableValue: public VariableValue {
public:
    PartialVariable *partial_variable;
    
    PartialVariableValue(PartialVariable *pv, Value *p, TypeMatch &match)
        :VariableValue(pv, p, match) {
        partial_variable = pv;
        
        // First, it's not assignable, second, it may already have an lvalue_type inside it
        ts = ts.rvalue();
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
            
        x64->op(MOVQ, RBX, es.address);  // Needs RBP
        x64->op(PUSHQ, RBP);
        x64->op(MOVQ, RBP, Address(RBP, 0));  // Restore caller RBP
        
        x64->op(CALL, RBX);
        
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
    Role *role;
    std::unique_ptr<Value> pivot;
    Register reg;
    TypeMatch match;
    
    RoleValue(Role *r, Value *p, TypeMatch &tm)
        :Value(typesubst(r->alloc_ts, tm).prefix(weakref_type)) {  // Was: borrowed_type
        role = r;
        pivot.reset(p);
        reg = NOREG;
        match = tm;
    }
    
    virtual Regs precompile(Regs preferred) {
        Regs clob = pivot->precompile(preferred);
            
        if (role->where == NOWHERE)
            throw INTERNAL_ERROR;
            
        return clob;
    }
    
    virtual Storage compile(X64 *x64) {
        Storage s = pivot->compile(x64);
        int offset = role->compute_offset(match);  // May step multiple roles
        
        switch (s.where) {
        case REGISTER:
            x64->runtime->decweakref(s.reg);
            x64->op(ADDQ, s.reg, offset);
            x64->runtime->incweakref(s.reg);
            return s;
        case STACK:
            x64->op(POPQ, RBX);
            x64->runtime->decweakref(RBX);
            x64->op(ADDQ, RBX, offset);
            x64->runtime->incweakref(RBX);
            x64->op(PUSHQ, RBX);
            return s;
        case MEMORY:
            x64->op(MOVQ, RBX, s.address);
            x64->op(ADDQ, RBX, offset);
            x64->runtime->incweakref(RBX);
            x64->op(PUSHQ, RBX);
            return Storage(STACK);
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class ModuleValue: public Value {
public:
    ModuleScope *module_scope;
    
    ModuleValue(ModuleScope *ms, TypeSpec mts)
        :Value(mts) {
        module_scope = ms;
    }
    
    virtual Regs precompile(Regs preferred) {
        return Regs();
    }
    
    virtual Storage compile(X64 *x64) {
        if (!module_scope)
            throw INTERNAL_ERROR;
            
        int module_offset = module_scope->offset.concretize();
        return Storage(MEMORY, Address(x64->runtime->application_label, module_offset));
    }
};


class PartialModuleValue: public ModuleValue {
public:
    PartialModuleIdentifier *partial_module;
    
    PartialModuleValue(PartialModuleIdentifier *pm, ModuleScope *ms, TypeSpec mts)
        :ModuleValue(ms, mts) {
        partial_module = pm;
    }
};



#include "type.cpp"
#include "generic.cpp"
#include "equality.cpp"
#include "block.cpp"
#include "integer.cpp"
#include "boolean.cpp"
#include "iterator.cpp"
#include "container.cpp"
#include "array.cpp"
#include "circularray.cpp"
#include "rbtree.cpp"
#include "string.cpp"
#include "reference.cpp"
#include "record.cpp"
#include "multi.cpp"
#include "control.cpp"
#include "stream.cpp"
#include "class.cpp"
#include "literal.cpp"
#include "typedefinition.cpp"
#include "function.cpp"
#include "option.cpp"
#include "float.cpp"
