
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

    virtual Variable *declare_impure(std::string name, Scope *scope) {
        TypeSpec var_ts = scope->variable_type_hint(ts.unprefix(type_type));
            
        return new Variable(name, scope->pivot_type_hint(), var_ts);
    }
    
    virtual Declaration *declare_pure(std::string name, Scope *scope) {
        return declare_impure(name, scope);
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

    virtual Variable *declare_impure(std::string name, Scope *scope) {
        return NULL;
    }

    virtual Declaration *declare_pure(std::string name, Scope *scope) {
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
        
        DataScope *fake_scope = new DataScope;
        fake_scope->set_pivot_type_hint(VOID_TS);  // TODO: any better idea?
        scope->add(fake_scope);
        
        for (auto &a : args) {
            std::unique_ptr<Value> kwv;
            kwv.reset(typize(a.get(), fake_scope, &INTEGER_TS));
            
            DeclarationValue *dv = declaration_value_cast(kwv.get());

            if (!dv) {
                std::cerr << "Not a declaration in an enumeration definition!\n";
                return false;
            }
            
            // Can't check the dv ts in pure blocks anymore...
            //if (dv->ts != INTEGER_TS) {
            //    std::cerr << "Not an integer declaration in an enumeration definition: " << dv->ts << "!\n";
            //    return false;
            //}
            
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

    virtual Variable *declare_impure(std::string name, Scope *scope) {
        return NULL;
    }

    virtual Declaration *declare_pure(std::string name, Scope *scope) {
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

    Expr *make_equality(std::vector<std::string> member_names) {
        Expr *expr = NULL;
        
        for (auto &member_name : member_names) {
            Expr *c = mkexpr("is_equal", mkexpr(member_name, mkexpr("$")), mkexpr(member_name, mkexpr("other")));
        
            if (expr)
                expr = mkexpr("logical and", expr, c);
            else
                expr = c;
        }
    
        Expr *fn = mkctrl("Function", mkexpr("Boolean"),
            "from", mkdecl("other", mkexpr("<datatype>")),
            "as", mkctrl("return", expr)
        );

        return mkdecl("is_equal", fn);
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() == 0 || kwargs.size() != 0) {
            std::cerr << "Whacky record!\n";
            return false;
        }

        record_type.reset(new RecordType("<anonymous>"));

        inner_scope = new DataScope;
        scope->add(inner_scope);
        inner_scope->set_pivot_type_hint(TypeSpec { record_type.get() });
        inner_scope->set_meta_scope(record_metatype->get_inner_scope());
        
        for (auto &a : args) {
            Value *v = typize(a.get(), inner_scope);
            values.push_back(std::unique_ptr<Value>(v));
        }

        std::vector<std::string> member_names;
        
        for (auto &item : inner_scope->contents) {
            Variable *var = dynamic_cast<Variable *>(item.get());
            
            if (var)
                member_names.push_back(var->name);
        }

        // TODO: this should be a fallback if the user didn't define his own
        Value *eq = typize(make_equality(member_names), inner_scope);
        values.push_back(std::unique_ptr<Value>(eq));

        record_type->set_inner_scope(inner_scope);

        return true;
    }
    
    virtual Regs precompile(Regs) {
        for (auto &v : values)
            v->precompile(Regs());

        return Regs();
    }
    
    virtual Storage compile(X64 *x64) {
        for (auto &v : values)
            v->compile(x64);
            
        return Storage();
    }

    virtual Variable *declare_impure(std::string name, Scope *scope) {
        return NULL;
    }

    virtual Declaration *declare_pure(std::string name, Scope *scope) {
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

