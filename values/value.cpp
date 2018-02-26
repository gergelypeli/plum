
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

    virtual Value *set_context_ts(TypeSpec *c) {
        // Generally we don't need it, only in controls
        return this;
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
        
            TypeSpec var_ts = (ts[0]->type == ATTRIBUTE_TYPE ? ts.reprefix(ts[0], lvalue_type) : ts.lvalue());
        
            return new Variable(name, NO_TS, var_ts);
        }
        else
            return NULL;
    }
    
    virtual bool unpack(std::vector<TypeSpec> &tss) {
        return false;
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
        x64->op(MOVB, EXCEPTION_ADDRESS, raised_type->get_keyword_index(keyword));
        x64->unwind->initiate(raising_dummy, x64);
    }
};


void unwind_destroy_var(TypeSpec &ts, Storage s, X64 *x64) {
    if (s.where == ALIAS) {
        std::cerr << "ALIAS vars are no longer used since records are returned by value!\n";
        throw INTERNAL_ERROR;
        // Load the address, and destroy the result there
        //Register reg = RAX;  // FIXME: is this okay to clobber this register?
        //Storage t = Storage(MEMORY, Address(reg, 0));
        //ts.store(s, t, x64);
        //ts.destroy(t, x64);
    }
    else
        ts.destroy(s, x64);
}


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


class EqualityValue: public Value {
public:
    bool no;
    std::unique_ptr<Value> value;

    EqualityValue(bool n, Value *v)
        :Value(BOOLEAN_TS) {
        no = n;
        value.reset(v);
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        return value->check(args, kwargs, scope);
    }
    
    virtual Regs precompile(Regs preferred) {
        return value->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        // Returns a boolean if the arguments were equal
        Storage s = value->compile(x64);

        if (!no)
            return s;
        
        switch (s.where) {
        case CONSTANT:
            return Storage(CONSTANT, !s.value);
        case FLAGS:
            return Storage(FLAGS, negate(s.bitset));
        case REGISTER:
            x64->op(CMPB, s.reg, 0);
            return Storage(FLAGS, SETE);
        case MEMORY:
            x64->op(CMPB, s.address, 0);
            return Storage(FLAGS, SETE);
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class ComparisonValue: public Value {
public:
    BitSetOp bitset;
    std::unique_ptr<Value> value;

    ComparisonValue(BitSetOp b, Value *v)
        :Value(BOOLEAN_TS) {
        bitset = b;
        value.reset(v);
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        return value->check(args, kwargs, scope);
    }
    
    virtual Regs precompile(Regs preferred) {
        return value->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        // Returns an integer representing the ordering of the arguments
        Storage s = value->compile(x64);

        switch (s.where) {
        case REGISTER:
            x64->op(CMPB, s.reg, 0);
            break;
        case MEMORY:
            x64->op(CMPB, s.address, 0);
            break;
        default:
            throw INTERNAL_ERROR;
        }

        return Storage(FLAGS, bitset);
    }
};


class EqualityMatcherValue: public Value, public Raiser {
public:
    std::unique_ptr<Value> value;

    EqualityMatcherValue(Value *v)
        :Value(VOID_TS) {
        value.reset(v);
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!check_raise(match_unmatched_exception_type, scope))
            return false;
            
        return value->check(args, kwargs, scope);
    }

    virtual Regs precompile(Regs preferred) {
        return value->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        Label equal;
        Storage s = value->compile(x64);

        switch (s.where) {
        case CONSTANT:
            if (s.value)
                x64->op(JMP, equal);
            break;
        case FLAGS:
            x64->op(branchize(s.bitset), equal);
            break;
        case REGISTER:
            x64->op(CMPB, s.reg, 0);
            x64->op(JNE, equal);
            break;
        case MEMORY:
            x64->op(CMPB, s.address, 0);
            x64->op(JNE, equal);
            break;
        default:
            throw INTERNAL_ERROR;
        }
        
        raise("UNMATCHED", x64);
        
        x64->code_label(equal);
        return Storage();
    }
};


class VariableValue: public Value {
public:
    Variable *variable;
    std::unique_ptr<Value> pivot;
    Register reg;
    TypeMatch match;
    
    VariableValue(Variable *v, Value *p, TypeMatch &tm)
        :Value(typesubst(v->var_ts, tm).lvalue()) {
        variable = v;
        pivot.reset(p);
        reg = NOREG;
        match = tm;
    }
    
    virtual Regs precompile(Regs preferred) {
        Regs clob = pivot ? pivot->precompile(preferred) : Regs();
            
        if (!variable->xxx_is_allocated)
            throw INTERNAL_ERROR;
            
        if (variable->where == ALIAS) {
            // Just a sanity check, aliases are function arguments, and must have no pivot
            if (pivot)
                throw INTERNAL_ERROR;
                
            reg = preferred.get_any();
            //std::cerr << "Alias variable " << variable->name << " loaded to " << reg << "\n";
            clob = clob.add(reg);
        }
        
        if (pivot && pivot->ts.rvalue()[0] == reference_type) {
            std::cerr << "This is a bit suspicious, strong reference as variable pivot?\n";
            throw INTERNAL_ERROR;
        }
        
        if (pivot && pivot->ts.rvalue()[0] == weakreference_type) {
            reg = preferred.get_any();
            clob = clob.add(reg);
        }
        
        return clob;
    }
    
    virtual Storage compile(X64 *x64) {
        Storage t;
        
        if (pivot) {
            Storage s = pivot->compile(x64);
            
            if (pivot->ts.rvalue()[0] == weakreference_type) {
                // FIXME: technically we must borrow a reference here, or the container
                // may be destroyed before accessing this variable!
                
                pivot->ts.rvalue().store(s, Storage(REGISTER, reg), x64);
                x64->decweakref(reg);
                s = Storage(MEMORY, Address(reg, 0));
            }
            
            t = variable->get_storage(match, s);
        }
        else {
            t = variable->get_local_storage();
        }

        if (t.where == ALIAS) {
            x64->op(MOVQ, reg, t.address);
            t = Storage(MEMORY, Address(reg, 0));
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
        
        // First, it's not assignable, second, it may already has an lvalue_type inside it
        ts = ts.rvalue();
    }
    
    virtual void be_initialized(std::string name) {
        partial_variable->be_initialized(name);
    }
    
    virtual bool is_initialized(std::string name) {
        return partial_variable->is_initialized(name);
    }
    
    virtual Variable *var_initialized(std::string name) {
        return partial_variable->var_initialized(name);
    }
    
    virtual bool is_complete() {
        return partial_variable->is_complete();
    }
};


class EvaluableValue: public Value, public Raiser {
public:
    Evaluable *evaluable;
    std::vector<std::unique_ptr<Value>> arg_values;
    std::vector<Storage> arg_storages;
    
    EvaluableValue(Evaluable *e, Value *p, TypeMatch &tm)
        :Value(typesubst(e->var_ts, tm).unprefix(code_type)) {
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
            arg_tss.push_back(v->var_ts.unprefix(dvalue_type));
            
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
            
        return true;
    }
    
    virtual Regs precompile(Regs preferred) {
        if (!evaluable->xxx_is_allocated)
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
            
            evaluable->arg_variables[i]->var_ts.create(s, t, x64);
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
        x64->op(MOVB, EXCEPTION_ADDRESS, BL);  // Expect BL if not OK
        x64->unwind->initiate(raising_dummy, x64);  // unwinds ourselves, too
        x64->code_label(noex);

        x64->unwind->pop(this);

        for (int i = arg_storages.size() - 1; i >= 0; i--) {
            Storage x = arg_storages[i];
            Register reg = RAX;
            x64->op(MOVQ, reg, x.address);
            Storage t(MEMORY, Address(reg, 0));
            
            evaluable->arg_variables[i]->var_ts.destroy(t, x64);
        }
        
        return ts == VOID_TS ? Storage() : Storage(STACK);
    }

    virtual Scope *unwind(X64 *x64) {
        for (int i = arg_storages.size() - 1; i >= 0; i--) {
            Storage x = arg_storages[i];
            Register reg = RAX;
            x64->op(MOVQ, reg, x.address);
            Storage t(MEMORY, Address(reg, 0));
            
            evaluable->arg_variables[i]->var_ts.destroy(t, x64);
        }
        
        if (ts != VOID_TS)
            x64->op(ADDQ, RSP, ts.measure_stack());
            
        return NULL;
    }
};


class RoleValue: public Value {
public:
    Variable *variable;
    std::unique_ptr<Value> pivot;
    Register reg;
    TypeMatch match;
    
    RoleValue(Variable *v, Value *p, TypeMatch &tm)
        :Value(v->var_ts.rvalue().unprefix(role_type).prefix(reference_type)) {  // Was: borrowed_type
        variable = v;
        pivot.reset(p);
        reg = NOREG;
        match = tm;
    }
    
    virtual Regs precompile(Regs preferred) {
        Regs clob = pivot ? pivot->precompile(preferred) : Regs();
            
        if (!variable->xxx_is_allocated)
            throw INTERNAL_ERROR;
            
        return clob;
    }
    
    virtual Storage compile(X64 *x64) {
        // Store a VT pointer and a data pointer onto the stack
        Storage t;
        
        if (pivot) {
            // FIXME: this seems to be bogus, this is the static type of the pivot,
            // not the dynamic type!
            Label vtl = pivot->ts.get_virtual_table_label(x64);
            int vti = variable->virtual_index;
            
            x64->op(LEARIP, RBX, vtl);
            x64->op(ADDQ, RBX, vti * 4);  // virtual table stores 32-bit relative offsets
            x64->op(PUSHQ, RBX);
            
            Storage s = pivot->compile(x64);
            t = variable->get_storage(match, s);
        }
        else
            throw INTERNAL_ERROR;  // TODO: allow function Role-s?
            //s = Storage(MEMORY, Address(RBP, 0));
        
        variable->var_ts.store(t, Storage(ALISTACK), x64);
        
        return Storage(STACK);
    }
};


#include "generic.cpp"
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
#include "map.cpp"
#include "literal.cpp"
#include "type.cpp"
#include "typedefinition.cpp"
#include "function.cpp"
#include "option.cpp"
