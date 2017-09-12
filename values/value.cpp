
// Values

class Value {
public:
    TypeSpec ts;
    Token token;
    Marker marker;
    
    Value(TypeSpec t)
        :ts(t) {
    }

    virtual Value *set_token(Token t) {
        token = t;
        return this;
    }

    virtual Value *set_marker(Marker m) {
        marker = m;
        return this;
    }
    
    virtual Value *set_context_ts(TypeSpec *c) {
        // Generally we don't need it, only in controls
        return this;
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        return (args.size() == 0 && kwargs.size() == 0);
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
    // declare_dirty is to get the declared automatic variable from a constructor call,
    // so we can save a copy to the final value.

    virtual Variable *declare_dirty(std::string name, Scope *scope) {
        return NULL;
    }
    
    virtual Variable *declare_impure(std::string name, Scope *scope) {
        if (ts == VOID_TS)
            return NULL;
            
        return new Variable(name, VOID_TS, scope->variable_type_hint(ts.rvalue()));
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


class VariableValue: public Value {
public:
    Variable *variable;
    std::unique_ptr<Value> pivot;
    Register reg;
    
    VariableValue(Variable *v, Value *p)
        :Value(v->var_ts) {
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


#include "generic.cpp"
#include "literal.cpp"
#include "block.cpp"
#include "type.cpp"
#include "function.cpp"
#include "integer.cpp"
#include "boolean.cpp"
#include "array.cpp"
#include "reference.cpp"
#include "record.cpp"
#include "multi.cpp"
#include "control.cpp"


TypeSpec get_typespec(Value *value) {
    return value ? value->ts : VOID_TS;
}


Value *make_variable_value(Variable *decl, Value *pivot) {
    return new VariableValue(decl, pivot);
}


Value *make_function_call_value(Function *decl, Value *pivot) {
    return new FunctionCallValue(decl, pivot);
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


Value *make_declaration_value(std::string name, TypeSpec *context) {
    return new DeclarationValue(name, context);
}


Value *make_basic_value(TypeSpec ts, int number) {
    return new BasicValue(ts, number);
}


Value *make_string_literal_value(std::string text) {
    return new StringLiteralValue(text);
}


Value *make_code_value(Value *value, Declaration *escape) {
    if (!value->marker.scope)
        return value;
        
    CodeScope *intruder = new CodeScope;
    
    if (value->marker.scope->intrude(intruder, value->marker, escape))
        return new CodeValue(value, intruder);
    else {
        delete intruder;
        return value;
    }
}


Value *make_scalar_conversion_value(Value *p) {
    return new ScalarConversionValue(p);
}


Value *make_void_conversion_value(Value *p) {
    return new VoidConversionValue(p);
}


Value *make_boolean_conversion_value(Value *p) {
    return new BooleanConversionValue(p);
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


Value *make_record_initializer_value(Variable *var) {
    return new RecordInitializerValue(var);
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
