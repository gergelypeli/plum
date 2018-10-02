
class TypeDefinitionValue: public Value {
public:
    TypeDefinitionValue()
        :Value(HYPERTYPE_TS) {
    }

    virtual Regs precompile(Regs) {
        return Regs();
    }

    virtual Storage compile(X64 *) {
        return Storage();
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

    virtual Declaration *declare(std::string name, ScopeType st) {
        if (st == DATA_SCOPE || st == CODE_SCOPE || st == MODULE_SCOPE) {
            return new TreenumerationType(name, keywords, parents);
        }
        else
            return NULL;
    }
};


class RoleDefinitionValue: public TypeDefinitionValue {
public:
    std::unique_ptr<Value> value;
    TypeSpec role_ts, pivot_ts;
    //DataScope *original_scope;
    //Role *role;
    
    RoleDefinitionValue(Value *pivot, TypeMatch &tm)
        :TypeDefinitionValue() {
        //original_scope = NULL;
        //role = NULL;
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
        
        //original_scope = ct->get_inner_scope();

        pivot_ts = scope->pivot_type_hint();
        value.reset(v);
        
        return true;
    }
    /*
    virtual bool complete_definition() {
        std::cerr << "Completing role " << role->name << " definition.\n";
        
        // The role's inner scope is filled by declarations, but they should be checked
        return role->complete_role();
    }
    */
    virtual Declaration *declare(std::string name, ScopeType st) {
        Role *role = new Role(name, pivot_ts, role_ts, false);
        return role;
    }
};


class BaseRoleDefinitionValue: public RoleDefinitionValue {
public:
    BaseRoleDefinitionValue(Value *pivot, TypeMatch &tm)
        :RoleDefinitionValue(pivot, tm) {
    }

    virtual Declaration *declare(std::string name, ScopeType st) {
        Role *role = new Role(name, pivot_ts, role_ts, true);
        return role;
    }
};


class ImportDefinitionValue: public TypeDefinitionValue {
public:
    ImportScope *import_scope;

    ImportDefinitionValue()
        :TypeDefinitionValue() {
        import_scope = NULL;
    }

    virtual bool check_identifier(Expr *e) {
        if (e->type != Expr::IDENTIFIER) {
            std::cerr << "Not an identifier imported!\n";
            return false;
        }
        
        import_scope->add(e->text);
        return true;
    }
    

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() != 1 || kwargs.size() > 1) {
            std::cerr << "Whacky Import!\n";
            return false;
        }
        
        Expr *e = args[0].get();
        
        if (e->type != Expr::IDENTIFIER) {
            std::cerr << "Import expects an identifier for the module name!\n";
            return false;
        }
        
        ModuleScope *source_scope = import_module(e->text, scope);
        
        if (!source_scope)
            return false;

        ModuleScope *target_scope = scope->get_module_scope();

        import_scope = new ImportScope(source_scope, target_scope);
        
        if (kwargs.size() == 1) {
            if (!kwargs["for"]) {
                std::cerr << "Whacky Import!\n";
                return false;
            }
            
            Expr *f = kwargs["for"].get();
            bool ok = true;
            
            if (f->type == Expr::TUPLE) {
                for (auto &x : f->args)
                    ok = ok && check_identifier(x.get());
            }
            else
                ok = check_identifier(f);
        }
        
        import_scope->leave();
        
        return true;
    }
    
    virtual Declaration *declare(std::string name, ScopeType st) {
        if (st == MODULE_SCOPE || st == DATA_SCOPE) {
            if (name != "<anonymous>") {
                std::cerr << "Import declaration must be anonymous!\n";
                return NULL;
            }
            
            return import_scope;
        }
        else {
            std::cerr << "Import declaration must be in an data scope!\n";
            return NULL;
        }
    }
};


class ScopedTypeDefinitionValue: public TypeDefinitionValue {
public:
    Type *defined_type;
    std::vector<Expr *> deferred_exprs;
    DataScope *inner_scope;
    std::unique_ptr<DataBlockValue> data_value;

    ScopedTypeDefinitionValue()
        :TypeDefinitionValue() {
        defined_type = NULL;
        inner_scope = NULL;
    }

    void setup_inner(Type *t, TypeSpec pts) {
        inner_scope = t->make_inner_scope(pts);
    }

    void defer_as(Kwargs &kwargs) {
        // Type bodies may refer to their own type name, so they must be deferred
        Expr *as = kwargs["as"].get();
        
        if (as) {
            if (as->type == Expr::TUPLE)
                for (auto &e : as->args)
                    deferred_exprs.push_back(e.get());
            else
                deferred_exprs.push_back(as);
        }
    }

    virtual bool complete_definition() {
        if (!defined_type || !inner_scope)
            throw INTERNAL_ERROR;
            
        std::cerr << "Completing definition of " << defined_type->name << ".\n";

        data_value.reset(new DataBlockValue(inner_scope));

        for (Expr *expr : deferred_exprs)
            if (!data_value->check_statement(expr))
                return false;

        // Must complete records/classes before compiling method bodies
        if (!defined_type->complete_type())
            return false;
            
        if (!data_value->complete_definition())
            return false;
        
        std::cerr << "Completed definition of " << defined_type->name << ".\n";
        inner_scope->leave();

        return true;
    }

    virtual Regs precompile(Regs preferred) {
        return data_value->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        return data_value->compile(x64);
    }
};


class RecordDefinitionValue: public ScopedTypeDefinitionValue {
public:
    RecordDefinitionValue()
        :ScopedTypeDefinitionValue() {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (kwargs.size() != 1 || !kwargs["as"]) {
            std::cerr << "Whacky record!\n";
            return false;
        }

        defined_type = new RecordType("<anonymous>", Metatypes {});
        TypeSpec rts = { defined_type };

        setup_inner(defined_type, rts);
        
        //inner_scope->set_meta_scope(record_metatype->get_inner_scope());

        defer_as(kwargs);
            
        std::cerr << "Deferring record definition.\n";
        return true;
    }

    virtual Declaration *declare(std::string name, ScopeType st) {
        if (st == DATA_SCOPE || st == CODE_SCOPE || st == MODULE_SCOPE) {
            defined_type->set_name(name);
            return defined_type;
        }
        else
            return NULL;
    }
};


class SingletonDefinitionValue: public ScopedTypeDefinitionValue {
public:
    SingletonDefinitionValue()
        :ScopedTypeDefinitionValue() {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (kwargs.size() != 1 || !kwargs["as"]) {
            std::cerr << "Whacky singleton!\n";
            return false;
        }

        defined_type = new SingletonType("<anonymous>");
        TypeSpec sts = { defined_type };

        setup_inner(defined_type, sts);

        defer_as(kwargs);
            
        std::cerr << "Deferring singleton definition.\n";
        return true;
    }

    virtual Declaration *declare(std::string name, ScopeType st) {
        if (st == MODULE_SCOPE) {
            defined_type->set_name(name);
            return defined_type;
        }
        else
            return NULL;
    }
};


class ClassDefinitionValue: public ScopedTypeDefinitionValue {
public:
    ClassDefinitionValue()
        :ScopedTypeDefinitionValue() {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (kwargs.size() != 1 || !kwargs["as"]) {
            std::cerr << "Whacky class!\n";
            return false;
        }

        defined_type = new ClassType("<anonymous>", Metatypes {});
        TypeSpec cts = { ptr_type, defined_type };

        setup_inner(defined_type, cts);

        defer_as(kwargs);
            
        std::cerr << "Deferring class definition.\n";
        return true;
    }

    virtual Declaration *declare(std::string name, ScopeType st) {
        if (st == DATA_SCOPE || st == CODE_SCOPE || st == MODULE_SCOPE) {
            defined_type->set_name(name);
            return defined_type;
        }
        else
            return NULL;
    }
};


class InterfaceDefinitionValue: public ScopedTypeDefinitionValue {
public:
    InterfaceDefinitionValue()
        :ScopedTypeDefinitionValue() {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() != 0 || kwargs.size() != 1) {
            std::cerr << "Whacky interface!\n";
            return false;
        }

        defined_type = new InterfaceType("<anonymous>", Metatypes {});

        setup_inner(defined_type, ANY_TS);
        //inner_scope->set_meta_scope(_metatype->get_inner_scope());

        defer_as(kwargs);
            
        std::cerr << "Deferring interface definition.\n";
        return true;
    }

    virtual Declaration *declare(std::string name, ScopeType st) {
        if (st == DATA_SCOPE || st == CODE_SCOPE || st == MODULE_SCOPE) {
            defined_type->set_name(name);
            return defined_type;
        }
        else
            return NULL;
    }
};


class ImplementationDefinitionValue: public TypeDefinitionValue {
public:
    TypeSpec pivot_ts;
    TypeSpec interface_ts;
    
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
        
        pivot_ts = scope->pivot_type_hint();
        interface_ts = ptr_cast<TypeValue>(v)->represented_ts;  // NOTE: May still contain Some types
        
        if (!interface_ts.has_meta(interface_metatype)) {
            std::cerr << "Implementation needs an interface type name!\n";
            return false;
        }

        return true;
    }

    virtual Declaration *declare(std::string name, ScopeType st) {
        if (st == DATA_SCOPE) {
            return new Implementation(name, pivot_ts, interface_ts);
        }
        else
            return NULL;
    }
};


class LselfDefinitionValue: public TypeDefinitionValue {
public:
    LselfDefinitionValue()
        :TypeDefinitionValue() {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() != 0 || kwargs.size() != 0) {
            std::cerr << "Whacky lself!\n";
            return false;
        }

        return true;
    }

    virtual Declaration *declare(std::string name, ScopeType st) {
        if (st == DATA_SCOPE) {
            return new Lself(name, NO_TS);
        }
        else
            return NULL;
    }
};
