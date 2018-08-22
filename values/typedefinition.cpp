
class TypeDefinitionValue: public Value {
public:
    std::vector<Expr *> deferred_exprs;
    DataScope *inner_scope;
    std::unique_ptr<DataBlockValue> data_value;

    TypeDefinitionValue()
        :Value(HYPERTYPE_TS) {
        inner_scope = NULL;
    }

    void setup_inner(Type *t, TypeSpec pts) {
        inner_scope = t->make_inner_scope(pts);
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
            if (!t->complete_type())
                return false;
            
        return data_value->complete_definition();
    }
};


class IntegerDefinitionValue: public TypeDefinitionValue {
public:
    int size;
    bool is_not_signed;
    std::unique_ptr<Value> bs, iu;

    IntegerDefinitionValue()
        :TypeDefinitionValue() {
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
            BasicValue *bv = ptr_cast<BasicValue>(bs.get());
            
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
            BasicValue *bv = ptr_cast<BasicValue>(iu.get());
            
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

    virtual Declaration *declare(std::string name, ScopeType st) {
        if (st == DATA_SCOPE || st == CODE_SCOPE || st == MODULE_SCOPE)
            return new IntegerType(name, size, is_not_signed);
        else
            return NULL;
    }
};


class EnumerationDefinitionValue: public TypeDefinitionValue {
public:
    std::vector<std::string> keywords;
    Label stringifications_label;

    EnumerationDefinitionValue()
        :TypeDefinitionValue() {
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

    virtual Storage compile(X64 *) {
        return Storage();
    }
    
    virtual Declaration *declare(std::string name, ScopeType st) {
        if (st == DATA_SCOPE || st == CODE_SCOPE || st == MODULE_SCOPE) {
            return new EnumerationType(name, keywords);
        }
        else
            return NULL;
    }
};


class TreenumerationDefinitionValue: public TypeDefinitionValue {
public:
    std::vector<std::string> keywords;
    std::vector<unsigned> parents;

    TreenumerationDefinitionValue()
        :TypeDefinitionValue() {
    }

    virtual unsigned add_keyword(std::string kw, unsigned parent) {
        for (auto &k : keywords)
            if (k == kw)
                return 0;
                
        unsigned x = keywords.size();
        keywords.push_back(kw);
        parents.push_back(parent);
        
        return x;
    }
    
    virtual bool parse_level(Args &args, unsigned parent) {
        for (unsigned i = 0; i < args.size(); i++) {
            Expr *e = args[i].get();

            if (e->type == Expr::IDENTIFIER) {
                if (e->args.size() != 0 || e->kwargs.size() != 0) {
                    std::cerr << "Whacky treenum symbol!\n";
                    return false;
                }

                unsigned x = add_keyword(e->text, parent);

                if (!x)
                    return false;
            }
            else if (e->type == Expr::INITIALIZER) {
                if (!e->pivot || e->pivot->type != Expr::IDENTIFIER || e->kwargs.size() != 0) {
                    std::cerr << "Whacky treenum subtree!\n";
                    return false;
                }

                unsigned x = add_keyword(e->pivot->text, parent);

                if (!x)
                    return false;

                if (!parse_level(e->args, x))
                    return false;
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

        add_keyword("", 0);
        
        if (!parse_level(args, 0))
            return false;
        
        return true;
    }

    virtual Regs precompile(Regs) {
        return Regs();
    }

    virtual Storage compile(X64 *) {
        return Storage();
    }
    
    virtual Declaration *declare(std::string name, ScopeType st) {
        if (st == DATA_SCOPE || st == CODE_SCOPE || st == MODULE_SCOPE) {
            return new TreenumerationType(name, keywords, parents);
        }
        else
            return NULL;
    }
};


class RecordDefinitionValue: public TypeDefinitionValue {
public:
    RecordType *record_type;
    
    RecordDefinitionValue()
        :TypeDefinitionValue() {
        record_type = NULL;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (kwargs.size() != 1 || !kwargs["as"]) {
            std::cerr << "Whacky record!\n";
            return false;
        }

        record_type = new RecordType("<anonymous>", Metatypes {});
        TypeSpec rts = { record_type };

        setup_inner(record_type, rts);
        
        inner_scope->set_meta_scope(record_metatype->get_inner_scope());

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

    virtual Declaration *declare(std::string name, ScopeType st) {
        if (st == DATA_SCOPE || st == CODE_SCOPE || st == MODULE_SCOPE) {
            record_type->set_name(name);
            inner_scope->set_name(name);
            return record_type;
        }
        else
            return NULL;
    }
};


class SingletonDefinitionValue: public TypeDefinitionValue {
public:
    SingletonType *singleton_type;
    
    SingletonDefinitionValue()
        :TypeDefinitionValue() {
        singleton_type = NULL;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (kwargs.size() != 1 || !kwargs["as"]) {
            std::cerr << "Whacky singleton!\n";
            return false;
        }

        singleton_type = new SingletonType("<anonymous>");
        TypeSpec sts = { singleton_type };

        setup_inner(singleton_type, sts);

        defer_as(kwargs);
            
        std::cerr << "Deferring singleton definition.\n";
        return true;
    }

    virtual bool complete_definition() {
        std::cerr << "Completing singleton " << singleton_type->name << " definition.\n";
        if (!complete_as())
            return false;

        return complete(singleton_type);
    }
    
    virtual Regs precompile(Regs preferred) {
        return data_value->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        return data_value->compile(x64);
    }

    virtual Declaration *declare(std::string name, ScopeType st) {
        if (st == MODULE_SCOPE) {
            singleton_type->set_name(name);
            inner_scope->set_name(name);
            return singleton_type;
        }
        else
            return NULL;
    }
};


class ClassDefinitionValue: public TypeDefinitionValue {
public:
    ClassType *class_type;
    
    ClassDefinitionValue()
        :TypeDefinitionValue() {
        class_type = NULL;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (kwargs.size() != 1 || !kwargs["as"]) {
            std::cerr << "Whacky class!\n";
            return false;
        }

        class_type = new ClassType("<anonymous>", Metatypes {});
        TypeSpec cts = { weakref_type, class_type };

        setup_inner(class_type, cts);

        defer_as(kwargs);
            
        std::cerr << "Deferring class definition.\n";
        return true;
    }

    virtual bool complete_definition() {
        std::cerr << "Completing class " << class_type->name << " definition.\n";
        if (!complete_as())
            return false;

        return complete(class_type);
    }
    
    virtual Regs precompile(Regs preferred) {
        return data_value->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        return data_value->compile(x64);
    }

    virtual Declaration *declare(std::string name, ScopeType st) {
        if (st == DATA_SCOPE || st == CODE_SCOPE || st == MODULE_SCOPE) {
            class_type->set_name(name);
            inner_scope->set_name(name);
            return class_type;
        }
        else
            return NULL;
    }
};


class RoleDefinitionValue: public Value {
public:
    std::unique_ptr<Value> value;
    TypeSpec role_ts, pivot_ts;
    DataScope *original_scope;
    Role *role;
    
    RoleDefinitionValue(Value *pivot, TypeMatch &tm)
        :Value(HYPERTYPE_TS) {
        original_scope = NULL;
        role = NULL;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() > 1 || kwargs.size() != 0) {
            std::cerr << "Whacky role definition!\n";
            return false;
        }

        // TODO: check for Class definition scope!
        Value *v = typize(args[0].get(), scope, NULL);
    
        if (!v->ts.is_meta()) {
            std::cerr << "Class type name expected!\n";
            return false;
        }

        role_ts = ptr_cast<TypeValue>(v)->represented_ts;
        ClassType *ct = ptr_cast<ClassType>(role_ts[0]);
        
        if (!ct) {
            std::cerr << "Class type name expected!\n";
            return false;
        }
        
        original_scope = ct->get_inner_scope();

        pivot_ts = scope->pivot_type_hint();
        value.reset(v);
        
        return true;
    }

    //virtual bool complete_definition() {
    //    std::cerr << "Completing role " << role->name << " definition.\n";
        
        // The role's inner scope is filled by declarations, but they should be checked
    //    return role->complete_role();
    //}
    
    virtual Regs precompile(Regs preferred) {
        return Regs();
    }
    
    virtual Storage compile(X64 *x64) {
        return Storage();
    }

    virtual Declaration *declare(std::string name, ScopeType st) {
        role = new Role(name, pivot_ts, role_ts, original_scope);
        return role;
    }
};


class BaseRoleDefinitionValue: public RoleDefinitionValue {
public:
    BaseRoleDefinitionValue(Value *pivot, TypeMatch &tm)
        :RoleDefinitionValue(pivot, tm) {
    }

    virtual Declaration *declare(std::string name, ScopeType st) {
        role = new BaseRole(name, pivot_ts, role_ts, original_scope);
        return role;
    }
};


class InterfaceDefinitionValue: public TypeDefinitionValue {
public:
    InterfaceType *interface_type;
    
    InterfaceDefinitionValue()
        :TypeDefinitionValue() {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() != 0 || kwargs.size() != 1) {
            std::cerr << "Whacky interface!\n";
            return false;
        }

        interface_type = new InterfaceType("<anonymous>", Metatypes {});

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
            if (ptr_cast<FunctionScope>(c.get()))
                continue;
            else if (ptr_cast<Function>(c.get()))
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

    virtual Declaration *declare(std::string name, ScopeType st) {
        if (st == DATA_SCOPE || st == CODE_SCOPE || st == MODULE_SCOPE) {
            interface_type->set_name(name);
            inner_scope->set_name(name);
            return interface_type;
        }
        else
            return NULL;
    }
};


class ImplementationDefinitionValue: public TypeDefinitionValue {
public:
    TypeSpec interface_ts;
    ImplementationType *implementation_type;
    
    ImplementationDefinitionValue()
        :TypeDefinitionValue() {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() != 1 || kwargs.size() > 1) {
            std::cerr << "Whacky implementation!\n";
            return false;
        }

        Value *v = typize(args[0].get(), scope, NULL);
        TypeMatch match;
        
        if (!v->ts.is_meta()) {
            std::cerr << "Implementation needs an interface type name!\n";
            return false;
        }
        
        TypeSpec implementor_ts = scope->pivot_type_hint();
        interface_ts = ptr_cast<TypeValue>(v)->represented_ts;  // NOTE: May still contain Some types
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
            
        InterfaceType *interface_type = ptr_cast<InterfaceType>(interface_ts[0]);
        
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
        TypeMatch iftm = interface_ts.match();
        TypeMatch empty_match;

        for (auto &c : inner_scope->contents) {
            if (ptr_cast<FunctionScope>(c.get()))
                continue;
        
            Function *f = ptr_cast<Function>(c.get());
            
            if (!f) {
                std::cerr << "Not a function in an implementation!\n";
                return false;
            }
            
            //std::cerr << "Checking imp fun: " << f->name << "\n";
            //std::cerr << "XXX " << interface_type->member_functions.size() << "\n";
            
            bool found = false;
            
            for (Function *iff : interface_type->member_functions) {
                if (f->does_implement(empty_match, iff, iftm)) {
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                std::cerr << "Invalid implementation of function: " << interface_type->name << "." << f->name << "!\n";
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

    virtual Declaration *declare(std::string name, ScopeType st) {
        if (st == DATA_SCOPE || st == CODE_SCOPE || st == MODULE_SCOPE) {
            implementation_type->set_name(name);
            inner_scope->set_name(name);
            return implementation_type;
        }
        else
            return NULL;
    }
};

