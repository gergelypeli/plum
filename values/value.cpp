
struct ArgInfo {
    const char *name;
    TypeSpec *context;
    Scope *scope;
    std::unique_ptr<Value> *target;
};
    
typedef std::vector<ArgInfo> ArgInfos;

bool check_arguments(Args &args, Kwargs &kwargs, const ArgInfos &arg_infos);

// Values

class Value {
public:
    TypeSpec ts;
    Token token;
    //Marker marker;
        
    Value(TypeSpec t)
        :ts(t) {
    }

    virtual ~Value() {
    }

    virtual Value *set_token(Token t) {
        token = t;
        return this;
    }

    //virtual Value *set_marker(Marker m) {
    //    marker = m;
    //    return this;
    //}
    
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
    
    // DeclarationValue invokes these methods if this expression appears in a declaration.
    // In clode blocks first declare_impure is called, which may or may not return a Variable.
    // If not, then declare_pure is called, which must return any Declaration.
    // Data blocks invoke only declare_pure, and if that returns NULL, that's a semantic error.

    virtual Variable *declare_impure(std::string name, Scope *scope) {
        if (ts == VOID_TS)
            return NULL;
            
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
};


class UnwindStack {
public:
    std::vector<Value *> stack;
    
    virtual void push(Value *v) {
        stack.push_back(v);
    }
    
    virtual void pop(Value *v) {
        if (v != stack.back())
            throw INTERNAL_ERROR;
            
        stack.pop_back();
    }
    
    virtual void initiate(Declaration *last, X64 *x64) {
        for (int i = stack.size() - 1; i >= 0; i--) {
            Scope *s = stack[i]->unwind(x64);
            
            if (s) {
                if (s != last->outer_scope)
                    throw INTERNAL_ERROR;
                    
                last->jump_to_finalization(x64);
                return;
            }
        }
        
        throw INTERNAL_ERROR;
    }
};


void unwind_destroy_var(TypeSpec &ts, Storage s, X64 *x64) {
    if (s.where == ALIAS) {
        // Load the address, and destroy the result there
        Register reg = RAX;  // FIXME: is this okay to clobber this register?
        Storage t = Storage(MEMORY, Address(reg, 0));
        ts.store(s, t, x64);
        ts.destroy(t, x64);
    }
    else
        ts.destroy(s, x64);
}


class IdentityValue: public Value {
public:
    std::unique_ptr<Value> pivot;

    IdentityValue(Value *p)
        :Value(p->ts) {
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
    
    VariableValue(Variable *v, Value *p, TypeMatch &match)
        :Value(typesubst(v->var_ts, match).lvalue()) {
        variable = v;
        pivot.reset(p);
        reg = NOREG;
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
            return clob.add(reg);
        }
        else
            return clob;
    }
    
    virtual Storage compile(X64 *x64) {
        Storage s;
        
        if (pivot)
            s = pivot->compile(x64);
        else
            s = Storage(MEMORY, Address(RBP, 0));
        
        Storage t = variable->get_storage(s);
        
        if (t.where == ALIAS) {
            x64->op(MOVQ, reg, t.address);
            t = Storage(MEMORY, Address(reg, 0));
        }
        
        return t;    
    }
};


class RoleValue: public Value {
public:
    Variable *variable;
    std::unique_ptr<Value> pivot;
    Register reg;
    
    RoleValue(Variable *v, Value *p)
        :Value(v->var_ts.rvalue().unprefix(role_type).prefix(borrowed_type)) {
        variable = v;
        pivot.reset(p);
        reg = NOREG;
    }
    
    virtual Regs precompile(Regs preferred) {
        Regs clob = pivot ? pivot->precompile(preferred) : Regs();
            
        if (!variable->xxx_is_allocated)
            throw INTERNAL_ERROR;
            
        return clob;
    }
    
    virtual Storage compile(X64 *x64) {
        // Store a VT pointer and a data pointer onto the stack
        Storage s;
        
        if (pivot) {
            Label vtl = pivot->ts.get_virtual_table_label();
            int vti = variable->virtual_index;
            
            x64->op(LEARIP, RBX, vtl);
            x64->op(ADDQ, RBX, vti * 4);  // virtual table stores 32-bit relative offsets
            x64->op(PUSHQ, RBX);
            
            s = pivot->compile(x64);
        }
        else
            throw INTERNAL_ERROR;  // TODO: allow function Role-s?
            //s = Storage(MEMORY, Address(RBP, 0));
        
        Storage t = variable->get_storage(s);
        variable->var_ts.store(t, Storage(ALISTACK), x64);
        
        return Storage(STACK);
    }
};


#include "generic.cpp"
#include "block.cpp"
#include "literal.cpp"
#include "type.cpp"
#include "function.cpp"
#include "integer.cpp"
#include "boolean.cpp"
#include "array.cpp"
#include "string.cpp"
#include "reference.cpp"
#include "record.cpp"
#include "multi.cpp"
#include "control.cpp"
#include "stream.cpp"
#include "iterator.cpp"


TypeSpec get_typespec(Value *value) {
    return value ? value->ts : NO_TS;
}


Value *make_variable_value(Variable *decl, Value *pivot, TypeMatch &match) {
    return new VariableValue(decl, pivot, match);
}


Value *make_role_value(Variable *decl, Value *pivot) {
    return new RoleValue(decl, pivot);
}


Value *make_function_call_value(Function *decl, Value *pivot, TypeMatch &match) {
    return new FunctionCallValue(decl, pivot, match);
}


Value *make_type_value(TypeSpec ts) {
    return new TypeValue(ts);
}


Value *make_code_block_value(TypeSpec *context) {
    return new CodeBlockValue(context);
}


Value *make_multi_value() {
    return new MultiValue();
}


Value *make_eval_value(std::string en) {
    return new EvalValue(en);
}


Value *make_yield_value(EvalScope *es) {
    return new YieldValue(es);
}


Value *make_declaration_value(std::string name, TypeSpec *context) {
    return new DeclarationValue(name, context);
}


Value *make_basic_value(TypeSpec ts, int number) {
    return new BasicValue(ts, number);
}


Value *make_string_literal_value(std::string text) {
    return new StringLiteralValue(text);
}


Value *make_code_scope_value(Value *value, CodeScope *code_scope) {
    return new CodeScopeValue(value, code_scope);
}


Value *make_scalar_conversion_value(Value *p) {
    return new ScalarConversionValue(p);
}


Value *make_void_conversion_value(Value *p) {
    return new VoidConversionValue(p);
}


Value *peek_void_conversion_value(Value *v) {
    VoidConversionValue *vcv = dynamic_cast<VoidConversionValue *>(v);
    
    return vcv ? vcv->orig.get() : v;
}


Value *make_boolean_conversion_value(Value *p) {
    return new BooleanConversionValue(p);
}


Value *make_implementation_conversion_value(ImplementationType *imt, Value *p, TypeMatch &match) {
    return new ImplementationConversionValue(imt, p, match);
}


Value *make_boolean_not_value(Value *p) {
    TypeMatch match;
    
    if (!typematch(BOOLEAN_TS, p, match))
        throw INTERNAL_ERROR;
        
    return new BooleanOperationValue(COMPLEMENT, p, match);
}


Value *make_null_reference_value(TypeSpec ts) {
    return new NullReferenceValue(ts);
}


Value *make_unicode_character_value() {
    return new UnicodeCharacterValue();
}


Value *make_integer_definition_value() {
    return new IntegerDefinitionValue();
}


Value *make_enumeration_definition_value() {
    return new EnumerationDefinitionValue();
}


Value *make_treenumeration_definition_value() {
    return new TreenumerationDefinitionValue();
}


Value *make_record_definition_value() {
    return new RecordDefinitionValue();
}


Value *make_record_initializer_value(TypeMatch &match) {
    return new RecordInitializerValue(match);
}


Value *make_record_preinitializer_value(TypeSpec ts) {
    return new RecordPreinitializerValue(ts);
}


Value *make_record_postinitializer_value(Value *v) {
    return new RecordPostinitializerValue(v);
}


Value *make_class_definition_value() {
    return new ClassDefinitionValue();
}


Value *make_interface_definition_value() {
    return new InterfaceDefinitionValue();
}


Value *make_implementation_definition_value() {
    return new ImplementationDefinitionValue();
}


Value *make_identity_value(Value *v) {
    return new IdentityValue(v);
}


Value *make_equality_value(bool no, Value *v) {
    return new EqualityValue(no, v);
}


Value *make_comparison_value(BitSetOp bs, Value *v) {
    return new ComparisonValue(bs, v);
}


DeclarationValue *make_declaration_by_value(std::string name, Value *v, Scope *scope) {
    DeclarationValue *dv = new DeclarationValue(name);
    bool ok = dv->use(v, scope);
    if (!ok)
        throw INTERNAL_ERROR;
    return dv;
}


Value *make_declaration_by_type(std::string name, TypeSpec ts, Scope *scope) {
    Value *v = new TypeValue(ts.prefix(type_type));
    return make_declaration_by_value(name, v, scope);
}


bool unpack_value(Value *v, std::vector<TypeSpec> &tss) {
    return v->unpack(tss);
}


void unprefix_value(Value *v) {
    v->ts = TypeSpec(v->ts.begin() + 1);
}
