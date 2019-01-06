
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
    
    virtual TypeSpec typize_typespec(Expr *expr, Scope *scope, MetaType *meta_type) {
        Value *value = typize(expr, scope, NULL);
    
        if (!value->ts.is_meta())
            return NO_TS;
        
        ts = ptr_cast<TypeValue>(value)->represented_ts;
        
        if (!ts.has_meta(meta_type))
            return NO_TS;
            
        return ts;
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
    TypeSpec role_ts;
    bool is_concrete;
    
    RoleDefinitionValue(Value *pivot, TypeMatch &tm, InheritAs ia = AS_ROLE)
        :TypeDefinitionValue() {
        inherit_as = ia;
        is_concrete = false;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        DataScope *ds = ptr_cast<DataScope>(scope);
        is_concrete = ds->is_virtual_scope() && !ds->is_abstract_scope();
        Expr *int_expr = NULL, *imp_expr = NULL;

        ExprInfos eis = {
            { "", &int_expr },
            { "by", &imp_expr }
        };
        
        if (!check_exprs(args, kwargs, eis)) {
            std::cerr << "Whacky role!\n";
            return false;
        }

        if (int_expr && imp_expr) {
            std::cerr << "Oops, can't handle implementation and inheritance at once yet!\n";
            return false;
        }
        else if (int_expr) {
            // Implementation
            Value *v = typize(int_expr, scope, NULL);
    
            if (!v->ts.is_meta()) {
                std::cerr << "Implemented type name expected!\n";
                return false;
            }

            role_ts = ptr_cast<TypeValue>(v)->represented_ts;
        
            if (!role_ts.has_meta(interface_metatype)) {
                std::cerr << "Implemented interface name expected!\n";
                return false;
            }
            
            delete v;
        }
        else if (imp_expr) {
            // Inheritance
            if (!is_concrete) {
                std::cerr << "Inheritance is only allowed in Class scope!\n";
                return false;
            }
            
            Value *v = typize(imp_expr, scope, NULL);
    
            if (!v->ts.is_meta()) {
                std::cerr << "Inherited type name expected!\n";
                return false;
            }

            role_ts = ptr_cast<TypeValue>(v)->represented_ts;
        
            if (!role_ts.has_meta(class_metatype)) {
                std::cerr << "Inherited class name expected!\n";
                return false;
            }
            
            delete v;
        }
        else {
            std::cerr << "Neither inherited nor implemented type specified!\n";
            return false;
        }
        
        pivot_ts = scope->pivot_type_hint();
        
        return true;
    }

    virtual Declaration *declare(std::string name, Scope *scope) {
        if (scope->type == DATA_SCOPE) {
            Declaration *d;
            
            if (is_concrete)
                d = new Role(name, pivot_ts, role_ts, inherit_as);
            else
                d = new Implementation(name, pivot_ts, role_ts, inherit_as);
                
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
        Expr *name_expr = NULL, *for_expr = NULL;
        
        ExprInfos eis = {
            { "", &name_expr },
            { "for", &for_expr }
        };
        
        if (!check_exprs(args, kwargs, eis)) {
            std::cerr << "Whacky Import!\n";
            return false;
        }

        if (!name_expr || name_expr->type != Expr::IDENTIFIER) {
            std::cerr << "Import expects an identifier for the module name!\n";
            return false;
        }
        
        ModuleScope *source_scope = import_module(name_expr->text, scope);
        
        if (!source_scope)
            return false;

        ModuleScope *target_scope = scope->get_module_scope();

        import_scope = new ImportScope(source_scope, target_scope);
        import_scope->enter();
        
        if (for_expr) {
            bool ok = true;
            
            if (for_expr->type == Expr::TUPLE) {
                for (auto &x : for_expr->args)
                    ok = ok && check_identifier(x.get());
            }
            else
                ok = check_identifier(for_expr);
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

    void defer_as(Expr *as_expr) {
        // Type bodies may refer to their own type name, so they must be deferred
        if (as_expr) {
            if (as_expr->type == Expr::TUPLE)
                for (auto &e : as_expr->args)
                    defer(e.get());
            else
                defer(as_expr);
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

    virtual bool define_data_prehook() {
        return true;
    }

    virtual bool define_data() {
        if (!defined_type)
            throw INTERNAL_ERROR;

        std::cerr << "Completing definition of " << defined_type->name << ".\n";
        defined_type->get_inner_scope()->enter();

        if (!define_data_prehook())
            return false;

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
        Expr *as_expr = NULL;
        
        ExprInfos eis = {
            { "as", &as_expr }
        };
        
        if (!check_exprs(args, kwargs, eis)) {
            std::cerr << "Whacky record!\n";
            return false;
        }

        defer_as(as_expr);
        
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
        Expr *as_expr = NULL;
        
        ExprInfos eis = {
            { "as", &as_expr }
        };
        
        if (!check_exprs(args, kwargs, eis)) {
            std::cerr << "Whacky singleton!\n";
            return false;
        }

        defer_as(as_expr);
            
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
    Expr *base_expr;
    Expr *main_expr;
    
    ClassDefinitionValue()
        :ScopedTypeDefinitionValue() {
        base_expr = NULL;
        main_expr = NULL;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        Expr *as_expr = NULL;
        
        ExprInfos eis = {
            { "", &main_expr },
            { "by", &base_expr },
            { "as", &as_expr }
        };
        
        if (!check_exprs(args, kwargs, eis))
            return false;
        
        defer_as(as_expr);
            
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

    virtual bool define_data_prehook() {
        Scope *is = defined_type->get_inner_scope();
        
        if (main_expr) {
            TypeSpec main_ts = typize_typespec(main_expr, is, interface_metatype);
        
            if (main_ts == NO_TS) {
                std::cerr << "Main interface name expected!\n";
                return false;
            }
            
            // Temporary name until @ becomes legal in identifiers
            is->add(new Role("main", is->pivot_type_hint(), main_ts, AS_BASE));
        }
        
        if (base_expr) {
            TypeSpec base_ts = typize_typespec(base_expr, is, class_metatype);
        
            if (base_ts == NO_TS) {
                std::cerr << "Base class name expected!\n";
                return false;
            }
        }

        return true;
    }
};


class InterfaceDefinitionValue: public ScopedTypeDefinitionValue {
public:
    InterfaceDefinitionValue()
        :ScopedTypeDefinitionValue() {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        Expr *as_expr = NULL;
        
        ExprInfos eis = {
            { "as", &as_expr }
        };
        
        if (!check_exprs(args, kwargs, eis)) {
            std::cerr << "Whacky interface!\n";
            return false;
        }

        defer_as(as_expr);
            
        std::cerr << "Deferring interface definition.\n";
        return true;
    }

    virtual Declaration *declare(std::string name, Scope *scope) {
        if (scope->type == DATA_SCOPE || scope->type == CODE_SCOPE || scope->type == MODULE_SCOPE) {
            Type *t = new InterfaceType(name, Metatypes {});
            TypeSpec pivot_ts = { ptr_type, t };
            return define(t, pivot_ts, scope);
        }
        else
            return NULL;
    }
};
