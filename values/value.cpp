
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
    
    virtual Declaration *make_exception_dummy(Type *exception_type, Scope *scope) {
        TryScope *try_scope = scope->get_try_scope();
        
        if (!try_scope) {
            std::cerr << "Exception " << exception_type->name << " not caught!\n";
            return NULL;
        }
        
        if (!try_scope->set_exception_type(exception_type)) {
            std::cerr << "Exception " << exception_type->name << " is not the caught one!\n";
            return NULL;
        }
        
        // Insert declaration dummy here, destroy variables before it if we get an exception
        Declaration *dummy = new Declaration;
        scope->add(dummy);

        return dummy;
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
    
    // DeclarationValue invokes these methods if this expression appears in a declaration.
    // In clode blocks first declare_impure is called, which may or may not return a Variable.
    // If not, then declare_pure is called, which must return any Declaration.
    // Data blocks invoke only declare_pure, and if that returns NULL, that's a semantic error.

    virtual Variable *declare_impure(std::string name, Scope *scope) {
        if (ts == VOID_TS) {
            std::cerr << "Can't declare " << name << " as Void!\n";
            return NULL;
        }
            
        return new Variable(name, NO_TS, scope->variable_type_hint(ts.rvalue()));
    }
    
    virtual Declaration *declare_pure(std::string name, Scope *scope) {
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
            reg = preferred.get_any();
            clob = clob.add(reg);
        }
        
        return clob;
    }
    
    virtual Storage compile(X64 *x64) {
        Storage t;
        
        if (pivot) {
            Storage s = pivot->compile(x64);
            
            if (pivot->ts.rvalue()[0] == reference_type) {
                // FIXME: technically we must borrow a reference here, or the container
                // may be destroyed before accessing this variable!
                
                pivot->ts.rvalue().store(s, Storage(REGISTER, reg), x64);
                x64->decref(reg);
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
#include "function.cpp"
