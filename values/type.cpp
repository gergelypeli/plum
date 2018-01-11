
void compile_virtual_table(X64 *x64, std::string name, Label label, Scope *inner_scope) {
    std::vector<Function *> vt = inner_scope->get_virtual_table();
    
    if (vt.size() > 0) {
        x64->data_align();
        x64->data_label_export(label, "vmt_" + name, 0, false);
    
        for (auto f : vt) {
            if (f)
                x64->data_reference(f->x64_label);
            else
                x64->data_qword(0);  // data references are now 64-bit absolute addresses
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
        
        if (is_heap_type(ts[0]))
            ts = ts.prefix(reference_type);
        
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


class TypeDefinitionValue: public Value {
public:
    std::vector<Expr *> deferred_exprs;
    DataScope *inner_scope;
    std::unique_ptr<DataBlockValue> data_value;

    TypeDefinitionValue(TypeSpec t)
        :Value(t) {
        inner_scope = NULL;
    }

    void setup_inner(Type *t, TypeSpec pts) {
        inner_scope = t->make_inner_scope(pts);
        //scope->add(inner_scope);
        //inner_scope->set_pivot_type_hint(pts);
    }

    void defer_as(Kwargs &kwargs) {
        Expr *as = kwargs["as"].get();
        
        if (as) {
            if (as->type == Expr::TUPLE)
                for (auto &e : as->args)
                    deferred_exprs.push_back(e.get());
            else
                deferred_exprs.push_back(as);
        }
    }
    
    bool complete_as() {
        // Type bodies may refer to their own type name, so they must be deferred
        data_value.reset(new DataBlockValue(inner_scope));

        for (Expr *expr : deferred_exprs)
            if (!data_value->check_statement(expr))
                return false;

        return true;
    }
    
    bool complete(Type *t = NULL) {
        // Must complete records/classes before compiling method bodies
        if (t)
            t->complete_type();
            
        return data_value->complete_definition();
    }
};


class IntegerDefinitionValue: public TypeDefinitionValue {
public:
    int size;
    bool is_not_signed;
    std::unique_ptr<Value> bs, iu;

    IntegerDefinitionValue()
        :TypeDefinitionValue(METATYPE_TS) {
        size = 0;
        is_not_signed = false;
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        ArgInfos infos = {
            { "bytes", &INTEGER_TS, scope, &bs },
            { "is_unsigned", &BOOLEAN_TS, scope, &iu }
        };
        
        if (!check_arguments(args, kwargs, infos))
            return false;
            
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


class EnumerationDefinitionValue: public TypeDefinitionValue {
public:
    std::vector<std::string> keywords;
    Label stringifications_label;
    std::string declname;

    EnumerationDefinitionValue()
        :TypeDefinitionValue(METATYPE_TS) {
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
            
        x64->data_label_export(stringifications_label, declname + "_sfy", 0, false);
        
        for (auto &label : labels)
            x64->data_reference(label);  // 64-bit absolute
        
        return Storage();
    }

    virtual Variable *declare_impure(std::string name, Scope *scope) {
        return NULL;
    }

    virtual Declaration *declare_pure(std::string name, Scope *scope) {
        declname = name;
        return new EnumerationType(name, keywords, stringifications_label);
    }
};


class TreenumerationDefinitionValue: public TypeDefinitionValue {
public:
    std::vector<std::string> keywords;
    std::vector<unsigned> tails;
    Label stringifications_label;
    Label tails_label;
    std::string declname;

    TreenumerationDefinitionValue()
        :TypeDefinitionValue(METATYPE_TS) {
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
            
        x64->data_label_export(stringifications_label, declname + "_sfy", 0, false);
        
        for (auto &label : labels)
            x64->data_reference(label);  // 64-bit absolute

        x64->data_label_export(tails_label, declname + "_tails", 0, false);
        
        for (unsigned tail : tails)
            x64->data_byte(tail);
        
        return Storage();
    }

    virtual Variable *declare_impure(std::string name, Scope *scope) {
        return NULL;
    }

    virtual Declaration *declare_pure(std::string name, Scope *scope) {
        declname = name;
        return new TreenumerationType(name, keywords, stringifications_label, tails_label);
    }
};


class RecordDefinitionValue: public TypeDefinitionValue {
public:
    RecordType *record_type;
    
    RecordDefinitionValue()
        :TypeDefinitionValue(METATYPE_TS) {
        record_type = NULL;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (kwargs.size() != 1 || !kwargs["as"]) {
            std::cerr << "Whacky record!\n";
            return false;
        }

        record_type = new RecordType("<anonymous>", 0);
        TypeSpec rts = { record_type };

        setup_inner(record_type, rts.lvalue());
        
        inner_scope->set_meta_scope(record_metatype->get_inner_scope(rts.begin()));

        defer_as(kwargs);
            
        std::cerr << "Deferring record definition.\n";
        return true;
    }

    virtual bool complete_definition() {
        std::cerr << "Completing record definition.\n";
        if (!complete_as())
            return false;

        inner_scope->add(make_record_compare());

        return complete(record_type);
    }
    
    virtual Regs precompile(Regs preferred) {
        return data_value->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
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


class ClassDefinitionValue: public TypeDefinitionValue {
public:
    ClassType *class_type;
    Label virtual_table_label;
    
    ClassDefinitionValue()
        :TypeDefinitionValue(METATYPE_TS) {
        class_type = NULL;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (kwargs.size() != 1 || !kwargs["as"]) {
            std::cerr << "Whacky class!\n";
            return false;
        }

        class_type = new ClassType("<anonymous>", virtual_table_label);
        TypeSpec cts = { reference_type, class_type };

        setup_inner(class_type, cts);
        inner_scope->be_virtual_scope();
        inner_scope->set_meta_scope(class_metatype->get_inner_scope(cts.begin()));

        defer_as(kwargs);
            
        std::cerr << "Deferring class definition.\n";
        return true;
    }

    virtual bool complete_definition() {
        std::cerr << "Completing class " << class_type->name << " definition.\n";
        if (!complete_as())
            return false;

        inner_scope->reserve(8);  // VMT pointer

        return complete(class_type);
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


class InterfaceDefinitionValue: public TypeDefinitionValue {
public:
    InterfaceType *interface_type;
    
    InterfaceDefinitionValue()
        :TypeDefinitionValue(METATYPE_TS) {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() != 0 || kwargs.size() != 1) {
            std::cerr << "Whacky interface!\n";
            return false;
        }

        interface_type = new InterfaceType("<anonymous>", 0);

        setup_inner(interface_type, ANY_TS);
        //inner_scope->set_meta_scope(_metatype->get_inner_scope());

        defer_as(kwargs);
            
        std::cerr << "Deferring interface definition.\n";
        return true;
    }

    virtual bool complete_definition() {
        std::cerr << "Completing interface definition.\n";
        if (!complete_as())
            return false;
            
        for (auto &c : inner_scope->contents) {
            if (dynamic_cast<FunctionScope *>(c.get()))
                continue;
            else if (dynamic_cast<Function *>(c.get()))
                continue;
            
            std::cerr << "Not a function in an interface!\n";
            return false;
        }
        
        return complete(interface_type);
    }
    
    virtual Regs precompile(Regs preferred) {
        return data_value->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        return data_value->compile(x64);
    }

    virtual Variable *declare_impure(std::string name, Scope *scope) {
        return NULL;
    }

    virtual Declaration *declare_pure(std::string name, Scope *scope) {
        interface_type->set_name(name);
        return interface_type;
    }
};


class ImplementationDefinitionValue: public TypeDefinitionValue {
public:
    TypeSpec interface_ts;
    ImplementationType *implementation_type;
    
    ImplementationDefinitionValue()
        :TypeDefinitionValue(METATYPE_TS) {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() != 1 || kwargs.size() > 1) {
            std::cerr << "Whacky implementation!\n";
            return false;
        }

        Value *v = typize(args[0].get(), scope, NULL);
        TypeMatch match;
        
        if (!typematch(ANY_TYPE_TS, v, match)) {
            std::cerr << "Implementation needs an interface type name!\n";
            return false;
        }
        
        TypeSpec implementor_ts = scope->pivot_type_hint();
        interface_ts = match[1];  // NOTE: May still contain Some types
        implementation_type = new ImplementationType("<anonymous>", implementor_ts, interface_ts);

        setup_inner(implementation_type, implementor_ts);
        //inner_scope->set_meta_scope(_metatype->get_inner_scope());
        //implementation_type->set_inner_scope(inner_scope);  // for preview only

        defer_as(kwargs);
            
        std::cerr << "Deferring implementation definition.\n";
        return true;
    }

    virtual bool complete_definition() {
        std::cerr << "Completing implementation definition.\n";
        if (!complete_as())
            return false;
            
        InterfaceType *interface_type = dynamic_cast<InterfaceType *>(interface_ts[0]);
        
        if (!interface_type) {
            std::cerr << "Implementation needs an interface type name!\n";
            return false;
        }
        
        // NOTE: this is kinda weird, but correct.
        // If a parametric type implements an interface with the same type parameter
        // used, we can't concretize that here yet. So the fake_match, despite being
        // a replacement, may still have Same types. When getting the argument types
        // from the interface definition, the substitution will replace Same types
        // with Same types. But the functions in the implementation will be similarly
        // parametrized, so the comparison should compare Same to Same, and succeed.
        TypeMatch fake_match = type_parameters_to_match(interface_ts);
        TypeMatch empty_match;

        for (auto &c : inner_scope->contents) {
            if (dynamic_cast<FunctionScope *>(c.get()))
                continue;
        
            Function *f = dynamic_cast<Function *>(c.get());
            
            if (!f) {
                std::cerr << "Not a function in an implementation!\n";
                return false;
            }
            
            //std::cerr << "Checking imp fun: " << f->name << "\n";
            //std::cerr << "XXX " << interface_type->member_functions.size() << "\n";
            
            bool found = false;
            
            for (Function *iff : interface_type->member_functions) {
                if (iff->name != f->name)
                    continue;
            
                if (iff->get_argument_tss(fake_match) != f->get_argument_tss(empty_match)) {
                    std::cerr << "Mismatching " << f->name << " implementation argument types: " << f->get_argument_tss(empty_match)[0] << " should be " << iff->get_argument_tss(fake_match)[0] << "!\n";
                    return false;
                }
                
                if (iff->get_argument_names() != f->get_argument_names()) {
                    std::cerr << "Mismatching implementation argument names!\n";
                    return false;
                }

                if (iff->get_result_tss(fake_match) != f->get_result_tss(empty_match)) {
                    std::cerr << "Mismatching implementation result types!\n";
                    return false;
                }
                
                found = true;
                break;
            }
            
            if (!found) {
                std::cerr << "Invalid implementation function name: " << f->name << "!\n";
                return false;
            }
        }

        return complete(implementation_type);
    }
    
    virtual Regs precompile(Regs preferred) {
        return data_value->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        return data_value->compile(x64);
    }

    virtual Variable *declare_impure(std::string name, Scope *scope) {
        return NULL;
    }

    virtual Declaration *declare_pure(std::string name, Scope *scope) {
        implementation_type->set_name(name);
        return implementation_type;
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
    ImplementationType *implementation_type;
    std::unique_ptr<Value> orig;
    
    ImplementationConversionValue(ImplementationType *imt, Value *o, TypeMatch &match)
        :Value(o->ts.prefix(imt)) {
        implementation_type = imt;
        orig.reset(o);
        std::cerr << "XXX Created implementation conversion " << implementation_type->name << " as " << ts << ".\n";;
    }
    
    virtual Regs precompile(Regs preferred) {
        return orig->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        return orig->compile(x64);
    }
};

