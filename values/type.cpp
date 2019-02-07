
class TypeValue: public Value {
public:
    TypeSpec pivot_ts;
    TypeSpec represented_ts;
    
    TypeValue(Type *mt, TypeSpec ts)
        :Value(TypeSpec { mt }) {
        represented_ts = ts;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() != 0 || kwargs.size() != 0) {
            std::cerr << "Whacky type name!\n";
            return false;
        }

        pivot_ts = scope->pivot_type_hint();
        return true;
    }
    
    virtual Regs precompile(Regs preferred) {
        return Regs();
    }
    
    virtual Storage compile(X64 *x64) {
        return Storage();
    }

    virtual Declaration *declare(std::string name, Scope *scope) {
        if (scope->type == ARGUMENT_SCOPE) {
            if (!represented_ts.has_meta(attribute_metatype) && !represented_ts.has_meta(value_metatype)) {
                std::cerr << "Invalid type for argument declaration: " << represented_ts << "!\n";
                return NULL;
            }
            
            Declaration *d;
            
            if (represented_ts[0] == code_type)
                d = new Evaluable(name, pivot_ts, represented_ts);
            else
                d = new Variable(name, pivot_ts, represented_ts);
                
            scope->add(d);
            return d;
        }
        else if (scope->type == CODE_SCOPE || scope->type == DATA_SCOPE || scope->type == SINGLETON_SCOPE) {
            if (!represented_ts.has_meta(value_metatype)) {
                std::cerr << "Invalid type for variable declaration: " << represented_ts << "!\n";
                return NULL;
            }

            Declaration *d = new Variable(name, pivot_ts, represented_ts.lvalue());
            scope->add(d);
            return d;
        }
        else {
            std::cerr << "Variables must be declared in code or data scopes!\n";
            return NULL;
        }
    }
};


class VoidConversionValue: public Value {
public:
    std::unique_ptr<Value> orig;
    
    VoidConversionValue(Value *o)
        :Value(VOID_TS) {
        orig.reset(o);
        set_token(o->token);
        //marker = orig->marker;
    }
    
    virtual Regs precompile(Regs preferred) {
        return orig->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        Storage s = orig->compile(x64);
        Storage t = Storage();
        orig->ts.store(s, t, x64);
        return t;
    }
    
    virtual void escape_statement_variables() {
        orig->escape_statement_variables();
    }
};


class ImplementationConversionValue: public Value {
public:
    TypeMatch match;
    Implementation *implementation;
    std::unique_ptr<Value> orig;
    
    ImplementationConversionValue(Implementation *imt, Value *o, TypeMatch tm)
        :Value(o->ts) {
        match = tm;  // the implementor type's match
        implementation = imt;
        orig.reset(o);
        
        //if (o->ts[0] == lvalue_type)
        //    ts = ts.lvalue();
    }
    
    virtual void streamify(X64 *x64) {
        Associable *sable = implementation->autoconv_streamifiable(match);
        
        if (!sable) {
            std::cerr << "Unstreamifiable implementation!\n";
            throw TYPE_ERROR;  // TODO: return value?
        }

        // Allow the implementation do this as it likes
        sable->streamify(match, x64);
    }
    
    virtual Value *lookup_inner(std::string name, Scope *scope) {
        //Scope *implementor_scope = implementation->outer_scope;
        ts = orig->ts;  // implementor_scope->pivot_type_hint();
        std::string implementing_name = implementation->prefix + name;
        
        // In the case of nested interfaces (such as Iterable in Iterator) the ts
        // is a concrete type, while the implementation of the nested interface methods
        // are in the enclosing interface, not in the concrete type scope. So looking
        // up by ts won't find it, and we must look up directly in the implementor scope.
        //std::cerr << "Looking up implementation " << ts << " " << implementing_name << "\n";
        //Value *value = implementor_scope->lookup(implementing_name, this, scope);
        
        Value *value = implementation->associating_scope->lookup(implementing_name, this, scope);

        if (!value)
            std::cerr << "Oops, missing implementation " << implementing_name << "?\n";

        return value;
    }
    
    virtual Regs precompile(Regs preferred) {
        return orig->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        return orig->compile(x64);
    }
};

