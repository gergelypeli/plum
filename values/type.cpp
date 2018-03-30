
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

    virtual Declaration *declare(std::string name, ScopeType st) {
        if (st == ARGUMENT_SCOPE) {
            if (!represented_ts.has_meta(attribute_metatype) && !represented_ts.has_meta(value_metatype))
                return NULL;
            
            if (represented_ts[0] == code_type)
                return new Evaluable(name, pivot_ts, represented_ts);
            else
                return new Variable(name, pivot_ts, represented_ts);
        }
        else {
            if (!represented_ts.has_meta(value_metatype))
                return NULL;

            return new Variable(name, pivot_ts, represented_ts.lvalue());
        }
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
    
    virtual void escape_statement_variables() {
        orig->escape_statement_variables();
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
        
        // NOTE: ots may be Lvalue, but this is necessary for records, where the pivot type
        // must be Lvalue in order to match any implemented method!
        
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

