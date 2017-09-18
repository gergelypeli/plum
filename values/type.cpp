
void compile_virtual_table(X64 *x64, std::string name, Label label, Scope *inner_scope) {
    std::vector<Function *> vt = inner_scope->get_virtual_table();
    
    if (vt.size() > 0) {
        x64->data_align();
        x64->data_label_export(label, "vmt_" + name, 0, false);
    
        for (auto f : vt) {
            if (f)
                x64->data_reference(f->x64_label);
            else
                x64->data_dword(0);  // data references are now 32-bit relative addresses
        }
    }
}


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
            
        ts = ts.unprefix(type_type);
        
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

    virtual Variable *declare_impure(std::string name, Scope *scope) {
        TypeSpec t = value ? ts : ts.unprefix(type_type);
        TypeSpec var_ts = scope->variable_type_hint(t);
            
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
        if (args.size() == 0 || kwargs.size() != 0) {
            std::cerr << "Whacky enumeration!\n";
            return false;
        }
        
        for (auto &a : args) {
            if (a->type != Expr::IDENTIFIER || a->args.size() > 0 || a->kwargs.size() > 0) {
                std::cerr << "Whacky enum symbol!\n";
                return false;
            }
            
            keywords.push_back(a->text);
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


class TreenumerationDefinitionValue: public Value {
public:
    std::vector<std::string> keywords;
    std::vector<unsigned> tails;
    Label stringifications_label;
    Label tails_label;

    TreenumerationDefinitionValue()
        :Value(METATYPE_TS) {
    }

    virtual unsigned add_keyword(std::string kw) {
        for (auto &k : keywords)
            if (k == kw)
                return 0;
                
        unsigned x = keywords.size();
        keywords.push_back(kw);
        tails.push_back(x);
        
        return x;
    }
    
    virtual bool parse_level(Args &args) {
        for (unsigned i = 0; i < args.size(); i++) {
            Expr *e = args[i].get();
            
            if (e->type == Expr::IDENTIFIER) {
                if (e->args.size() > 1 || e->kwargs.size() != 0) {
                    std::cerr << "Whacky treenum symbol!\n";
                    return false;
                }

                unsigned x = add_keyword(e->text);
                
                if (!x)
                    return false;
                    
                if (e->args.size() == 1) {
                    Expr *f = e->args[0].get();
                    
                    if (f->type != Expr::INITIALIZER || f->kwargs.size() != 0) {
                        std::cerr << "Whacky treenum subtree!\n";
                        return false;
                    }
                    
                    if (!parse_level(f->args))
                        return false;
                        
                    tails[x] = keywords.size() - 1;
                }
            }
            else {
                std::cerr << "Whacky treenum syntax!\n";
                return false;
            }
        }
        
        return true;
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() == 0 || kwargs.size() != 0) {
            std::cerr << "Whacky treenumeration!\n";
            return false;
        }

        add_keyword("");
        
        if (!parse_level(args))
            return false;
        
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
            
        x64->data_label_export(stringifications_label, "treenum_stringifications", 0, false);
        
        for (auto &label : labels)
            x64->data_reference(label);  // 32-bit relative

        x64->data_label_export(tails_label, "treenum_tails", 0, false);
        
        for (unsigned tail : tails)
            x64->data_byte(tail);
        
        return Storage();
    }

    virtual Variable *declare_impure(std::string name, Scope *scope) {
        return NULL;
    }

    virtual Declaration *declare_pure(std::string name, Scope *scope) {
        return new TreenumerationType(name, keywords, stringifications_label, tails_label);
    }
};


class RecordDefinitionValue: public Value {
public:
    DataScope *inner_scope;
    RecordType *record_type;
    std::unique_ptr<DataBlockValue> data_value;
    std::vector<Expr *> deferred_exprs;
    Label virtual_table_label;
    
    RecordDefinitionValue()
        :Value(METATYPE_TS) {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (kwargs.size() != 0) {
            std::cerr << "Whacky record!\n";
            return false;
        }

        record_type = new RecordType("<anonymous>", virtual_table_label);

        inner_scope = new DataScope;
        scope->add(inner_scope);
        inner_scope->set_pivot_type_hint(TypeSpec { record_type });
        inner_scope->set_meta_scope(record_metatype->get_inner_scope());

        for (auto &a : args)
            deferred_exprs.push_back(a.get());
            
        std::cerr << "Deferring record definition.\n";
        return true;
    }

    virtual bool complete_definition() {
        std::cerr << "Completing record definition.\n";
        data_value.reset(new DataBlockValue(inner_scope));
        
        for (Expr *expr : deferred_exprs)
            if (!data_value->check_statement(expr))
                return false;

        std::vector<std::string> member_names;
        
        for (auto &item : inner_scope->contents) {
            Variable *var = dynamic_cast<Variable *>(item.get());
            
            if (var && (var->var_ts[0] != lvalue_type || var->var_ts[1] != role_type))
                member_names.push_back(var->name);
        }

        // TODO: this should be a fallback if the user didn't define his own
        if (!data_value->check_statement(make_equality(member_names)))
            return false;

        if (!data_value->complete_definition())
            return false;

        record_type->set_inner_scope(inner_scope);

        return true;
        
    }
    
    virtual Regs precompile(Regs preferred) {
        return data_value->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        compile_virtual_table(x64, record_type->name, virtual_table_label, inner_scope);

        return data_value->compile(x64);
    }

    virtual Variable *declare_impure(std::string name, Scope *scope) {
        return NULL;
    }

    virtual Declaration *declare_pure(std::string name, Scope *scope) {
        record_type->set_name(name);
        return record_type;
    }
};


class ClassDefinitionValue: public Value {
public:
    DataScope *inner_scope;
    ClassType *class_type;
    std::unique_ptr<DataBlockValue> data_value;
    std::vector<Expr *> deferred_exprs;
    Label virtual_table_label;
    
    ClassDefinitionValue()
        :Value(METATYPE_TS) {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (kwargs.size() != 0) {
            std::cerr << "Whacky role!\n";
            return false;
        }

        class_type = new ClassType("<anonymous>", virtual_table_label);

        inner_scope = new DataScope;
        inner_scope->be_virtual_scope();
        
        scope->add(inner_scope);
        inner_scope->set_pivot_type_hint(TypeSpec { borrowed_type, class_type });
        inner_scope->set_meta_scope(record_metatype->get_inner_scope());

        for (auto &a : args)
            deferred_exprs.push_back(a.get());
            
        std::cerr << "Deferring class definition.\n";
        return true;
    }

    virtual bool complete_definition() {
        std::cerr << "Completing class definition.\n";
        data_value.reset(new DataBlockValue(inner_scope));
        
        for (Expr *expr : deferred_exprs)
            if (!data_value->check_statement(expr))
                return false;
        /*
        std::vector<std::string> member_names;
        
        for (auto &item : inner_scope->contents) {
            Variable *var = dynamic_cast<Variable *>(item.get());
            
            if (var)
                member_names.push_back(var->name);
        }

        // TODO: this should be a fallback if the user didn't define his own
        if (!data_value->check_statement(make_equality(member_names)))
            return false;
        */
        if (!data_value->complete_definition())
            return false;

        class_type->set_inner_scope(inner_scope);

        return true;
    }
    
    virtual Regs precompile(Regs preferred) {
        return data_value->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        compile_virtual_table(x64, class_type->name, virtual_table_label, inner_scope);

        return data_value->compile(x64);
    }

    virtual Variable *declare_impure(std::string name, Scope *scope) {
        return NULL;
    }

    virtual Declaration *declare_pure(std::string name, Scope *scope) {
        class_type->set_name(name);
        return class_type;
    }
};


class VoidConversionValue: public Value {
public:
    std::unique_ptr<Value> orig;
    
    VoidConversionValue(Value *o)
        :Value(VOID_TS) {
        orig.reset(o);
        marker = orig->marker;
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
        marker = orig->marker;
    }
    
    virtual Regs precompile(Regs preferred) {
        return orig->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        Storage s = orig->compile(x64);
        return orig->ts.boolval(s, x64, false);
    }
};

