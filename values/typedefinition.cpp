
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
    
    virtual Declaration *declare(std::string name, Scope *scope) {
        if (scope->type == DATA_SCOPE || scope->type == CODE_SCOPE || scope->type == MODULE_SCOPE) {
            Declaration *d = new IntegerType(name, size, is_not_signed);
            scope->add(d);
            return d;
        }
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

    
    virtual Declaration *declare(std::string name, Scope *scope) {
        if (scope->type == DATA_SCOPE || scope->type == CODE_SCOPE || scope->type == MODULE_SCOPE) {
            Type *t = new EnumerationType(name, keywords);
            t->make_inner_scope({ t })->leave();
            scope->add(t);
            return t;
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

    virtual Declaration *declare(std::string name, Scope *scope) {
        if (scope->type == DATA_SCOPE || scope->type == CODE_SCOPE || scope->type == MODULE_SCOPE || scope->type == FUNCTION_SCOPE) {
            Type *t = new TreenumerationType(name, keywords, parents);
            t->make_inner_scope({ t })->leave();
            scope->add(t);
            return t;
        }
        else
            return NULL;
    }
};


class LselfDefinitionValue: public TypeDefinitionValue {
public:
    LselfDefinitionValue(Value *pivot, TypeMatch &tm)
        :TypeDefinitionValue() {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() != 0 || kwargs.size() != 0) {
            std::cerr << "Whacky lself!\n";
            return false;
        }

        return true;
    }

    virtual Declaration *declare(std::string name, Scope *scope) {
        if (scope->type == DATA_SCOPE) {
            Declaration *d = new Lself(name, NO_TS);
            scope->add(d);
            return d;
        }
        else
            return NULL;
    }
};


class RoleDefinitionValue: public TypeDefinitionValue {
public:
    InheritAs inherit_as;
    TypeSpec pivot_ts;
    TypeSpec inherited_ts;
    TypeSpec implemented_ts;
    
    RoleDefinitionValue(Value *pivot, TypeMatch &tm, InheritAs ia = AS_ROLE)
        :TypeDefinitionValue() {
        inherit_as = ia;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() > 1 || kwargs.size() > 1) {
            std::cerr << "Whacky role definition!\n";
            return false;
        }

        if (args.size() == 1) {
            // Inheritance
            
            DataScope *ds = ptr_cast<DataScope>(scope);
            if (!ds->is_virtual_scope()) {
                std::cerr << "Inheritance is only allowed in Class scope!\n";
                return false;
            }
            
            Value *v = typize(args[0].get(), scope, NULL);
    
            if (!v->ts.is_meta()) {
                std::cerr << "Inherited type name expected!\n";
                return false;
            }

            inherited_ts = ptr_cast<TypeValue>(v)->represented_ts;
        
            if (!inherited_ts.has_meta(class_metatype)) {
                std::cerr << "Inherited class name expected!\n";
                return false;
            }
            
            delete v;
        }

        if (kwargs.size() == 1) {
            // Implementation
            
            Value *v = typize(kwargs["as"].get(), scope, NULL);
    
            if (!v->ts.is_meta()) {
                std::cerr << "Implemented type name expected!\n";
                return false;
            }

            implemented_ts = ptr_cast<TypeValue>(v)->represented_ts;
        
            if (!implemented_ts.has_meta(interface_metatype)) {
                std::cerr << "Implemented interface name expected!\n";
                return false;
            }
            
            delete v;
        }
        
        if (inherited_ts != NO_TS && implemented_ts != NO_TS) {
            std::cerr << "Oops, can't handle implementation and inheritance at once yet!\n";
            return false;
        }
        
        if (inherited_ts == NO_TS && implemented_ts == NO_TS) {
            std::cerr << "Neither inherited nor implemented type specified!\n";
            return false;
        }
        
        pivot_ts = scope->pivot_type_hint();
        
        return true;
    }

    virtual Declaration *declare(std::string name, Scope *scope) {
        if (scope->type == DATA_SCOPE) {
            Declaration *d;
            
            if (inherited_ts != NO_TS)
                d = new Role(name, pivot_ts, inherited_ts, inherit_as);
            else if (implemented_ts != NO_TS)
                d = new Implementation(name, pivot_ts, implemented_ts, inherit_as);
            else
                throw INTERNAL_ERROR;
                
            scope->add(d);
            return d;
        }
        else
            return NULL;
    }
};


class BaseDefinitionValue: public RoleDefinitionValue {
public:
    BaseDefinitionValue(Value *pivot, TypeMatch &tm)
        :RoleDefinitionValue(pivot, tm, AS_BASE) {
    }
};


class AutoDefinitionValue: public RoleDefinitionValue {
public:
    AutoDefinitionValue(Value *pivot, TypeMatch &tm)
        :RoleDefinitionValue(pivot, tm, AS_AUTO) {
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
        import_scope->enter();
        
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
    
    virtual Declaration *declare(std::string name, Scope *scope) {
        if (scope->type == MODULE_SCOPE || scope->type == DATA_SCOPE) {
            if (name != "<anonymous>") {
                std::cerr << "Import declaration must be anonymous!\n";
                return NULL;
            }
            
            scope->add(import_scope);
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
    std::vector<Expr *> meta_exprs;
    std::vector<Expr *> data_exprs;
    std::unique_ptr<DataBlockValue> block_value;

    ScopedTypeDefinitionValue()
        :TypeDefinitionValue() {
        defined_type = NULL;
    }

    void defer(Expr *e) {
        if (is_typedefinition(e))
            meta_exprs.push_back(e);
        else
            data_exprs.push_back(e);
    }

    void defer_as(Kwargs &kwargs) {
        // Type bodies may refer to their own type name, so they must be deferred
        Expr *as = kwargs["as"].get();
        
        if (as) {
            if (as->type == Expr::TUPLE)
                for (auto &e : as->args)
                    defer(e.get());
            else
                defer(as);
        }
    }

    virtual Type *define(Type *dt, TypeSpec pts, Scope *s) {
        defined_type = dt;
        s->add(defined_type);

        defined_type->make_inner_scope(pts);
        block_value.reset(new DataBlockValue(defined_type->get_inner_scope()));
        
        for (Expr *expr : meta_exprs)
            if (!block_value->check_statement(expr))
                return NULL;
        
        defined_type->get_inner_scope()->leave();
        
        return defined_type;
    }

    virtual bool define_data() {
        if (!defined_type)
            throw INTERNAL_ERROR;

        std::cerr << "Completing definition of " << defined_type->name << ".\n";
        defined_type->get_inner_scope()->enter();

        for (Expr *expr : data_exprs)
            if (!block_value->check_statement(expr))
                return false;

        // Must complete records/classes before compiling method bodies
        if (!defined_type->complete_type())
            return false;
            
        if (!block_value->define_data())
            return false;
        
        std::cerr << "Completed definition of " << defined_type->name << ".\n";
        defined_type->get_inner_scope()->leave();

        return true;
    }

    virtual bool define_code() {
        defined_type->get_inner_scope()->enter();
        bool ok = block_value->define_code();
        defined_type->get_inner_scope()->leave();
        
        return ok;
    }

    virtual Regs precompile(Regs preferred) {
        return block_value->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        return block_value->compile(x64);
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

        defer_as(kwargs);
            
        std::cerr << "Deferring record definition.\n";
        return true;
    }

    virtual Declaration *declare(std::string name, Scope *scope) {
        if (scope->type == DATA_SCOPE || scope->type == CODE_SCOPE || scope->type == MODULE_SCOPE) {
            Type *t = new RecordType(name, Metatypes {});
            return define(t, { t }, scope);
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

        defer_as(kwargs);
            
        std::cerr << "Deferring singleton definition.\n";
        return true;
    }

    virtual Declaration *declare(std::string name, Scope *scope) {
        if (scope->type == MODULE_SCOPE) {
            Type *t = new SingletonType(name);
            return define(t, { t }, scope);
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

        defer_as(kwargs);
            
        std::cerr << "Deferring class definition.\n";
        return true;
    }

    virtual Declaration *declare(std::string name, Scope *scope) {
        if (scope->type == DATA_SCOPE || scope->type == CODE_SCOPE || scope->type == MODULE_SCOPE) {
            Type *t = new ClassType(name, Metatypes {});
            return define(t, { ptr_type, t }, scope);
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

        defer_as(kwargs);
            
        std::cerr << "Deferring interface definition.\n";
        return true;
    }

    virtual Declaration *declare(std::string name, Scope *scope) {
        if (scope->type == DATA_SCOPE || scope->type == CODE_SCOPE || scope->type == MODULE_SCOPE) {
            Type *t = new InterfaceType(name, Metatypes {});
            return define(t, ANY_TS, scope);
        }
        else
            return NULL;
    }
};
