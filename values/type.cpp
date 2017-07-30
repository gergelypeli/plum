
class TypeValue: public Value {
public:
    TypeValue(TypeSpec ts)
        :Value(ts) {
    }
    
    virtual Regs precompile(Regs) {
        return Regs();
    }
    
    virtual Storage compile(X64 *) {
        return Storage();
    }

    virtual Variable *declare(std::string name, Scope *scope) {
        TypeSpec var_ts = ts.unprefix(type_type);
        
        if (dynamic_cast<HeapType *>(var_ts[0]))
            var_ts = var_ts.prefix(reference_type);
            
        Variable *v = new Variable(name, VOID_TS, var_ts.lvalue());
        scope->add(v);
        return v;
    }
};


class IntegerDefinitionValue: public Value {
public:
    int size;
    bool is_not_signed;
    std::unique_ptr<Value> bs, iu;

    IntegerDefinitionValue()
        :Value(METATYPE_TS) {
        size = 0;
        is_not_signed = false;
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() > 0) {
            std::cerr << "Positional arguments in an integer definition!\n";
            return false;
        }

        Expr *b = kwargs["bytes"].get();
        bs.reset(typize(b, scope, &INTEGER_TS));
        if (!bs) {
            std::cerr << "Missing bytes keyword argument in integer definition!\n";
            return false;
        }
        else {
            BasicValue *bv = dynamic_cast<BasicValue *>(bs.get());
            
            if (!bv) {
                std::cerr << "Nonconstant bytes keyword argument in integer definition!\n";
                return false;
            }
            
            size = bv->number;
        }
        
        Expr *u = kwargs["is_unsigned"].get();
        iu.reset(typize(u, scope, &BOOLEAN_TS));
        if (!iu) {
            std::cerr << "Missing is_unsigned keyword argument in integer definition!\n";
            return false;
        }
        else {
            BasicValue *bv = dynamic_cast<BasicValue *>(iu.get());
            
            if (!bv) {
                std::cerr << "Nonconstant is_unsigned keyword argument in integer definition!\n";
                return false;
            }
            
            is_not_signed = (bool)bv->number;
        }
        
        return true;
    }
    
    virtual Regs precompile(Regs) {
        return Regs();
    }
    
    virtual Storage compile(X64 *) {
        return Storage();
    }

    virtual Variable *declare(std::string name, Scope *scope) {
        scope->add(new IntegerType(name, size, is_not_signed));
        return NULL;
    }
};


class EnumerationDefinitionValue: public Value {
public:
    std::vector<std::string> keywords;

    EnumerationDefinitionValue()
        :Value(METATYPE_TS) {
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() == 1 || kwargs.size() != 0) {
            std::cerr << "Whacky enumeration!\n";
            return false;
        }
        
        for (auto &a : args) {
            Value *kwv = typize(a.get(), scope);
            DeclarationValue *dv = declaration_value_cast(kwv);

            if (!dv) {
                std::cerr << "Not a declaration in an enumeration definition!\n";
                return false;
            }
            
            if (dv->ts != UNCERTAIN_TS) {
                std::cerr << "Not an uncertain declaration in an enumeration definition!\n";
                return false;
            }
            
            keywords.push_back(declaration_get_name(dv));
        }
        
        if (kwargs.size() > 0) {
            std::cerr << "Keyword arguments in an enumeration definition!\n";
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

    virtual Variable *declare(std::string name, Scope *scope) {
        scope->add(new EnumerationType(name, keywords));
        return NULL;
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

