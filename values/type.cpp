
class TypeValue: public DeclarableValue {
public:
    TypeValue(TypeSpec ts)
        :DeclarableValue(ts) {
    }
    
    virtual Regs precompile(Regs) {
        return Regs();
    }
    
    virtual Storage compile(X64 *) {
        return Storage();
    }

    virtual Declaration *declare(std::string name) {
        TypeSpec var_ts = ts.unprefix(type_type);
        
        if (dynamic_cast<HeapType *>(var_ts[0]))
            var_ts = var_ts.prefix(reference_type);
            
        return new Variable(name, VOID_TS, var_ts.lvalue());
    }
};


class EnumerationDefinitionValue: public DeclarableValue {
public:
    std::vector<std::string> keywords;

    EnumerationDefinitionValue()
        :DeclarableValue(TypeSpec { type_type, enumeration_metatype }) {
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        for (auto &a : args) {
            Value *kwv = typize(a.get(), scope);
            
            DeclarationValue *dv = declaration_value_cast(kwv);
            if (!dv) {
                std::cerr << "Not a declaration in an Enumeration definition!\n";
                return false;
            }
            
            if (get_typespec(kwv)[0] != uncertain_type) {
                std::cerr << "Not an uncertain declaration in an Enumeration definition!\n";
                return false;
            }
            
            keywords.push_back(declaration_get_name(dv));
        }
        
        if (kwargs.size() > 0) {
            std::cerr << "Keyword arguments in an Enumeration definition!\n";
            return false;
        }
        
        return true;
    }
    
    virtual Regs precompile(Regs) {
        return Regs();
    }
    
    virtual Storage compile(X64 *) {
        return Storage();
    }

    virtual Declaration *declare(std::string name) {
        return new EnumerationType(name, keywords);
    }
};


class VoidConversionValue: public Value {
public:
    std::unique_ptr<Value> orig;
    
    VoidConversionValue(Value *o)
        :Value(VOID_TS) {
        orig.reset(o);
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
    }
    
    virtual Regs precompile(Regs preferred) {
        return orig->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        Storage s = orig->compile(x64);
        return orig->ts.boolval(s, x64, false);
    }
};

