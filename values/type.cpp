
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

    virtual Variable *declare_impure(std::string name) {
        TypeSpec var_ts = ts.unprefix(type_type);
        
        if (dynamic_cast<HeapType *>(var_ts[0]))
            var_ts = var_ts.prefix(reference_type);
            
        return new Variable(name, VOID_TS, var_ts.nonrvalue());
    }
    
    virtual Declaration *declare_pure(std::string name, TypeSpec scope_ts) {
        TypeSpec var_ts = ts.unprefix(type_type);
        
        if (dynamic_cast<HeapType *>(var_ts[0]))
            var_ts = var_ts.prefix(reference_type);
            
        return new Variable(name, scope_ts, var_ts.nonrvalue());
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

    virtual Variable *declare_impure(std::string name) {
        return NULL;
    }

    virtual Declaration *declare_pure(std::string name, TypeSpec scope_ts) {
        return new IntegerType(name, size, is_not_signed);
    }
};


class EnumerationDefinitionValue: public Value {
public:
    std::vector<std::string> keywords;
    Label stringifications_label;

    EnumerationDefinitionValue()
        :Value(METATYPE_TS) {
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() == 1 || kwargs.size() != 0) {
            std::cerr << "Whacky enumeration!\n";
            return false;
        }
        
        Scope *fake_scope = new DataScope;
        scope->add(fake_scope);
        
        for (auto &a : args) {
            std::unique_ptr<Value> kwv;
            kwv.reset(typize(a.get(), fake_scope, &INTEGER_TS));
            
            DeclarationValue *dv = declaration_value_cast(kwv.get());

            if (!dv) {
                std::cerr << "Not a declaration in an enumeration definition!\n";
                return false;
            }
            
            if (dv->ts != INTEGER_LVALUE_TS) {
                std::cerr << "Not an integer declaration in an enumeration definition!\n";
                return false;
            }
            
            keywords.push_back(declaration_get_name(dv));
        }
        
        return true;
    }
    
    virtual Regs precompile(Regs) {
        return Regs();
    }
    
    virtual Storage compile(X64 *x64) {
        // This storage method is quite verbose (32 bytes overhead for each string!),
        // but will be fine for a while.
        std::vector<Label> labels;
        
        for (auto &keyword : keywords) 
            labels.push_back(x64->data_heap_string(decode_utf8(keyword)));
            
        x64->data_label_export(stringifications_label, "enum_stringifications", 0, false);
        
        for (auto &label : labels)
            x64->data_reference(label);  // 32-bit relative
        
        return Storage();
    }

    virtual Variable *declare_impure(std::string name) {
        return NULL;
    }

    virtual Declaration *declare_pure(std::string name, TypeSpec scope_type) {
        return new EnumerationType(name, keywords, stringifications_label);
    }
};


class RecordDefinitionValue: public Value {
public:
    DataScope *inner_scope;
    std::unique_ptr<RecordType> record_type;
    std::vector<std::unique_ptr<Value>> values;
    
    RecordDefinitionValue()
        :Value(METATYPE_TS) {
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() == 1 || kwargs.size() != 0) {
            std::cerr << "Whacky record!\n";
            return false;
        }


        inner_scope = new DataScope;
        scope->add(inner_scope);

        record_type.reset(new RecordType("<anonymous>", inner_scope));
        inner_scope->set_scope_type(TypeSpec { record_type.get() });
        
        for (auto &a : args) {
            Value *v = typize(a.get(), inner_scope);
            
            DeclarationValue *dv = declaration_value_cast(v);

            if (!dv) {
                std::cerr << "Not a declaration in a record definition!\n";
                return false;
            }
            
            values.push_back(std::unique_ptr<Value>(v));
        }

        return true;
    }
    
    virtual Regs precompile(Regs) {
        return Regs();
    }
    
    virtual Storage compile(X64 *x64) {
        return Storage();
    }

    virtual Variable *declare_impure(std::string name) {
        return NULL;
    }

    virtual Declaration *declare_pure(std::string name, TypeSpec scope_ts) {
        record_type->set_name(name);
        return record_type.release();
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

