
class TypeValue: public Value {
public:
    std::unique_ptr<Value> value;
    
    TypeValue(TypeSpec ts)
        :Value(ts) {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() > 1 || kwargs.size() != 0) {
            std::cerr << "Whacky type name!\n";
            return false;
        }
        
        if (args.size() == 0)
            return true;
            
        // Only allow explicit type context for value types
        if (ts[1]->type != VALUE_TYPE)
            return false;
            
        ts = ts.unprefix(ts[0]);
        
        //if (heap_type_cast(ts[0]))
        //    ts = ts.prefix(reference_type);
        
        Value *v = typize(args[0].get(), scope, &ts);
        TypeMatch match;
        
        if (!typematch(ts, v, match)) {
            std::cerr << "Explicit type conversion failed!\n";
            return false;
        }
            
        value.reset(v);
        return true;
    }
    
    virtual Regs precompile(Regs preferred) {
        if (value)
            return value->precompile(preferred);
        else
            return Regs();
    }
    
    virtual Storage compile(X64 *x64) {
        if (value)
            return value->compile(x64);
        else
            return Storage();
    }

    virtual Variable *declare_arg(std::string name, Scope *scope) {
        if (!value && ts[1]->type != GENERIC_TYPE && ts[1]->type != VALUE_TYPE)
            return NULL;
            
        TypeSpec t = value ? ts : ts.unprefix(ts[0]);
        
        if (t[0] == code_type)
            return new Evaluable(name, scope->pivot_type_hint(), t);
        else
            return new Variable(name, scope->pivot_type_hint(), scope->variable_type_hint(t));
    }

    virtual Variable *declare_impure(std::string name, Scope *scope) {
        if (!value && ts[1]->type != VALUE_TYPE)
            return NULL;

        TypeSpec t = value ? ts : ts.unprefix(ts[0]);
        
        return new Variable(name, scope->pivot_type_hint(), scope->variable_type_hint(t));
    }
    
    virtual Declaration *declare_pure(std::string name, Scope *scope) {
        return declare_impure(name, scope);
    }
};


class VoidConversionValue: public Value {
public:
    std::unique_ptr<Value> orig;
    
    VoidConversionValue(Value *o)
        :Value(VOID_TS) {
        orig.reset(o);
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
};


class BooleanConversionValue: public Value {
public:
    std::unique_ptr<Value> orig;
    
    BooleanConversionValue(Value *o)
        :Value(BOOLEAN_TS) {
        orig.reset(o);
        //marker = orig->marker;
    }
    
    virtual Regs precompile(Regs preferred) {
        return orig->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        Storage s = orig->compile(x64);
        return orig->ts.boolval(s, x64, false);
    }
};


class ImplementationConversionValue: public Value {
public:
    std::unique_ptr<Value> orig;
    
    ImplementationConversionValue(ImplementationType *imt, Value *o, TypeMatch &tm)
        :Value(TypeSpec { imt }) {
        TypeSpec ifts = imt->get_interface_ts(tm);
        TypeSpec ots = o->ts;  // FIXME: if that's an ICV, too, then parse the orig ts from it!
        
        ts.insert(ts.end(), ifts.begin(), ifts.end());
        ts.insert(ts.end(), ots.begin(), ots.end());
            
        orig.reset(o);
        //std::cerr << "XXX Created implementation conversion " << implementation_type->name << " as " << ts << ".\n";;
    }
    
    virtual Regs precompile(Regs preferred) {
        return orig->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        return orig->compile(x64);
    }
};

