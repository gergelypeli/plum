
class RoleCreator {
protected:
    virtual bool extend_main_role(Associable *base_role, Associable *main_role) {
        // Find the topmost base role
        while (base_role->has_base_role())
            base_role = base_role->get_head_role();
    
        if (base_role->has_main_role()) {
            // Inherited a main role
            Associable *bm_role = base_role->shadow_associables[0].release();
            
            if (main_role->alloc_ts == bm_role->alloc_ts) {
                // Repeated main role type, nothing to do, just put the old one back
                std::cerr << "Explicit main role has the same type as the inherited one.\n";
                base_role->set_head_role(bm_role);
                delete main_role;
                return true;
            }
            else {
                // Derived main role type, check in the base role chain
                Associable *curr_role = main_role;
            
                while (true) {
                    //std::cerr << "XXX " << curr_role->alloc_ts << "\n";
                    Associable *next_role = (curr_role->has_base_role() ? curr_role->get_head_role() : NULL);
                    
                    if (!next_role) {
                        std::cerr << "Main role " << main_role->alloc_ts << " is not derived from " << bm_role->alloc_ts << "!\n";
                        return false;
                    }
                    
                    //std::cerr << "Let's see: " << next_role->alloc_ts << " vs " << bm_role->alloc_ts << "\n";
                    
                    if (next_role->alloc_ts == bm_role->alloc_ts) {
                        // Found base, replace with the inherited one
                        std::cerr << "Subrole " << next_role->name << " will be replaced by inherited " << bm_role->name << "\n";
                        bm_role->inherit_as = AS_BASE;
                        
                        curr_role->set_head_role(bm_role);
                        
                        base_role->set_head_role(main_role);
                        
                        return true;
                    }
                        
                    curr_role = next_role;
                }
            }
        }
        else {
            // This is the first main role
            std::cerr << "Adding initial main role\n";
            base_role->insert_head_role(main_role);
            return true;
        }
    }

    virtual Associable *add_head_role(Scope *is, std::string name, TypeSpec main_ts, TypeSpec base_ts, InheritAs ia) {
        Associable *base_role = NULL, *main_role = NULL;
        bool is_explicit = (ia != AS_BASE);
        
        if (base_ts != NO_TS) {
            std::string base_name = (is_explicit ? name : BASE_ROLE_NAME);
            InheritAs base_as = (is_explicit ? ia : AS_BASE);
            is_explicit = false;
            base_role = new Role(base_name, base_ts, base_as);
        }
            
        if (main_ts != NO_TS) {
            std::string main_name = (is_explicit ? name : MAIN_ROLE_NAME);
            InheritAs main_as = (is_explicit ? ia : AS_MAIN);
            main_role = new Role(main_name, main_ts, main_as);
        }

        if (main_role && base_role) {
            if (!extend_main_role(base_role, main_role))
                return NULL;
                
            is->add(base_role);
            return base_role;
        }
        else if (base_role) {
            is->add(base_role);
            return base_role;
        }
        else if (main_role) {
            is->add(main_role);
            return main_role;
        }
        else {
            std::cerr << "Main or base role type expected!\n";
            return NULL;
        }
    }
};


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
        
        TypeSpec rts = ptr_cast<TypeValue>(value)->represented_ts;
        
        if (!rts.has_meta(meta_type))
            return NO_TS;
            
        return rts;
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
            t->make_inner_scope()->leave();
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
            t->make_inner_scope()->leave();
            scope->add(t);
            return t;
        }
        else
            return NULL;
    }
};

/*
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
            Declaration *d = new Lself(name);
            scope->add(d);
            return d;
        }
        else
            return NULL;
    }
};
*/

class RoleDefinitionValue: public TypeDefinitionValue, public RoleCreator {
public:
    InheritAs inherit_as;
    TypeSpec implemented_ts, inherited_ts, required_ts;
    Associable *provider_associable;
    bool is_concrete;
    
    RoleDefinitionValue(Value *pivot, TypeMatch &tm, InheritAs ia = AS_ROLE)
        :TypeDefinitionValue() {
        inherit_as = ia;
        is_concrete = false;
        provider_associable = NULL;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        DataScope *ds = ptr_cast<DataScope>(scope);
        is_concrete = ds->is_virtual_scope() && !ds->is_abstract_scope();
        Expr *implemented_expr = NULL, *inherited_expr = NULL, *provider_expr = NULL, *required_expr = NULL;

        ExprInfos eis = {
            { "", &implemented_expr },
            { "by", &inherited_expr },
            { "of", &provider_expr },
            { "require", &required_expr }
        };
        
        if (!check_exprs(args, kwargs, eis)) {
            std::cerr << "Whacky role!\n";
            return false;
        }

        if (provider_expr) {
            if (implemented_expr || inherited_expr || required_expr) {
                std::cerr << "Whacky role aliasing!\n";
                return false;
            }
        }
        else if (required_expr) {
            if (implemented_expr || inherited_expr) {
                std::cerr << "Whacky role requiring!\n";
                return false;
            }
        }
        else {
            if (!implemented_expr && !inherited_expr) {
                std::cerr << "Neither inherited nor implemented type specified!\n";
                return false;
            }
        }
        
        if (implemented_expr) {
            // Implementation
            Value *v = typize(implemented_expr, scope, NULL);
    
            if (!v->ts.is_meta()) {
                std::cerr << "Implemented type name expected!\n";
                return false;
            }

            implemented_ts = ptr_cast<TypeValue>(v)->represented_ts;
        
            if (!implemented_ts.has_meta(abstract_metatype)) {
                std::cerr << "Implemented abstract name expected!\n";
                return false;
            }
            
            delete v;
        }
        
        if (inherited_expr) {
            // Inheritance
            if (!is_concrete) {
                std::cerr << "Inheritance is only allowed in Class scope!\n";
                return false;
            }
            
            Value *v = typize(inherited_expr, scope, NULL);
    
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

        if (required_expr) {
            // Require
            
            if (!is_concrete) {
                std::cerr << "Require is only allowed in Class scope!\n";
                return false;
            }
            
            Value *v = typize(required_expr, scope, NULL);
    
            if (!v->ts.is_meta()) {
                std::cerr << "Required type name expected!\n";
                return false;
            }

            required_ts = ptr_cast<TypeValue>(v)->represented_ts;
        
            if (!required_ts.has_meta(class_metatype)) {
                std::cerr << "Required class name expected!\n";
                return false;
            }
            
            delete v;
        }
        
        if (provider_expr) {
            // Aliasing
            
            if (!is_concrete) {
                std::cerr << "Aliasing is only allowed in Class scope!\n";
                return false;
            }
            
            if (provider_expr->type != Expr::IDENTIFIER) {
                std::cerr << "Aliased role identifier expected!\n";
                return false;
            }
            
            std::string name = provider_expr->text;
            
            for (auto &d : ds->contents) {
                Associable *able = ptr_cast<Associable>(d.get());
            
                if (able) {
                    Associable *a = able->lookup_associable(name);
                
                    if (a) {
                        provider_associable = a;
                        break;
                    }
                }
            }

            if (!provider_associable) {
                std::cerr << "Unknown provider role!\n";
                return false;
            }
        }
        
        return true;
    }

    virtual Declaration *declare(std::string name, Scope *scope) {
        if (scope->type == DATA_SCOPE) {
            if (provider_associable) {
                Declaration *d = new Provision(name, provider_associable);
                scope->add(d);
                return d;
            }
            else if (required_ts != NO_TS) {
                Associable *a = add_head_role(scope, name, NO_TS, required_ts, inherit_as);
                a->require();
                return a;
            }
            else
                return add_head_role(scope, name, implemented_ts, inherited_ts, inherit_as);
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


class GlobalDefinitionValue: public TypeDefinitionValue {
public:
    TypeSpec main_ts, class_ts;
    GlobalVariable *gvar;
    
    GlobalDefinitionValue(Value *null, TypeMatch &tm)
        :TypeDefinitionValue() {
        gvar = NULL;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        Expr *main_expr = NULL, *class_expr = NULL;
        
        ExprInfos eis = {
            { "", &main_expr },
            { "by", &class_expr }
        };
        
        if (!check_exprs(args, kwargs, eis)) {
            std::cerr << "Whacky global!\n";
            return false;
        }

        if (!main_expr || !class_expr) {
            std::cerr << "Global declaration needs an interface and a class type!\n";
            return false;
        }

        main_ts = typize_typespec(main_expr, scope, abstract_metatype);
        if (main_ts == NO_TS)
            return false;
            
        class_ts = typize_typespec(class_expr, scope, class_metatype);
        if (class_ts == NO_TS)
            return false;
            
        return true;
    }

    virtual bool define_data() {
        // Runs after declare
        // TODO: maybe allow selecting the initializer?
        Function *initializer_function = NULL;
        Scope *izs = ptr_cast<ClassType>(class_ts[0])->get_initializer_scope();

        if (izs->contents.size())
            initializer_function = ptr_cast<Function>(izs->contents[0].get());
        
        if (!initializer_function) {
            std::cerr << "No initializer in global variable class!\n";
            return false;
        }
        
        gvar->set_initializer_function(initializer_function);
        
        return true;
    }

    virtual Declaration *declare(std::string name, Scope *scope) {
        if (scope->type == MODULE_SCOPE || scope->type == DATA_SCOPE) {
            gvar = new GlobalVariable(name, main_ts.prefix(ptr_type), class_ts);
            scope->add(gvar);
            return gvar;
        }
        else
            return NULL;
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

    virtual Type *define(Type *dt, Scope *s) {
        defined_type = dt;
        s->add(defined_type);

        defined_type->make_inner_scope();
        defined_type->make_initializer_scope();
        defined_type->make_lvalue_scope();
        
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

        if (!define_data_prehook()) {
            defined_type->get_inner_scope()->leave();
            return false;
        }

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
            return define(t, scope);
        }
        else
            return NULL;
    }
};


class AbstractDefinitionValue: public ScopedTypeDefinitionValue {
public:
    Expr *base_expr;

    AbstractDefinitionValue()
        :ScopedTypeDefinitionValue() {
        base_expr = NULL;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        Expr *as_expr = NULL;
        
        ExprInfos eis = {
            { "", &base_expr },
            { "as", &as_expr }
        };
        
        if (!check_exprs(args, kwargs, eis)) {
            std::cerr << "Whacky abstract!\n";
            return false;
        }

        defer_as(as_expr);
            
        std::cerr << "Deferring abstract definition.\n";
        return true;
    }

    virtual bool define_data_prehook() {
        Scope *is = defined_type->get_inner_scope();
        
        if (base_expr) {
            TypeSpec base_ts = typize_typespec(base_expr, is, abstract_metatype);
        
            if (base_ts == NO_TS) {
                std::cerr << "Base abstract name expected!\n";
                return false;
            }

            is->add(new Role("", base_ts, AS_BASE));
        }

        return true;
    }

    virtual Declaration *declare(std::string name, Scope *scope) {
        if (scope->type == DATA_SCOPE || scope->type == CODE_SCOPE || scope->type == MODULE_SCOPE) {
            Type *t = new AbstractType(name, Metatypes {});
            return define(t, scope);
        }
        else
            return NULL;
    }
};


class ClassDefinitionValue: public ScopedTypeDefinitionValue, public RoleCreator {
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
            return define(t, scope);
        }
        else
            return NULL;
    }

    
    virtual bool define_data_prehook() {
        Scope *is = defined_type->get_inner_scope();
        TypeSpec main_ts, base_ts;
        
        if (main_expr) {
            main_ts = typize_typespec(main_expr, is, abstract_metatype);
        
            if (main_ts == NO_TS) {
                std::cerr << "Main abstract name expected!\n";
                return false;
            }
        }
        
        if (base_expr) {
            base_ts = typize_typespec(base_expr, is, class_metatype);
        
            if (base_ts == NO_TS) {
                std::cerr << "Base class name expected!\n";
                return false;
            }
        }

        if (base_expr || main_expr)
            return add_head_role(is, "", main_ts, base_ts, AS_BASE);
        else
            return true;
    }
};


// TODO: allow users define Interfaces?
class InterfaceDefinitionValue: public ScopedTypeDefinitionValue {
public:
    Expr *base_expr;

    InterfaceDefinitionValue()
        :ScopedTypeDefinitionValue() {
        base_expr = NULL;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        Expr *as_expr = NULL;
        
        ExprInfos eis = {
            { "", &base_expr },
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

    virtual bool define_data_prehook() {
        Scope *is = defined_type->get_inner_scope();
        
        if (base_expr) {
            TypeSpec base_ts = typize_typespec(base_expr, is, interface_metatype);
        
            if (base_ts == NO_TS) {
                std::cerr << "Base interface name expected!\n";
                return false;
            }

            is->add(new Implementation("", base_ts, AS_BASE));
        }

        return true;
    }

    virtual Declaration *declare(std::string name, Scope *scope) {
        if (scope->type == DATA_SCOPE || scope->type == CODE_SCOPE || scope->type == MODULE_SCOPE) {
            Type *t = new InterfaceType(name, Metatypes {});
            return define(t, scope);
        }
        else
            return NULL;
    }
};


class ImplementationDefinitionValue: public TypeDefinitionValue {
public:
    InheritAs inherit_as;
    TypeSpec interface_ts;
    bool is_concrete;
    
    ImplementationDefinitionValue(Value *pivot, TypeMatch &tm, InheritAs ia = AS_AUTO)
        :TypeDefinitionValue() {
        inherit_as = ia;
        is_concrete = false;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        DataScope *ds = ptr_cast<DataScope>(scope);
        is_concrete = ds->is_virtual_scope() && !ds->is_abstract_scope();
        Expr *interface_expr = NULL;

        ExprInfos eis = {
            { "", &interface_expr }
        };
        
        if (!check_exprs(args, kwargs, eis)) {
            std::cerr << "Whacky implementation!\n";
            return false;
        }

        if (interface_expr) {
            // Implementation
            Value *v = typize(interface_expr, scope, NULL);
    
            if (!v->ts.is_meta()) {
                std::cerr << "Implemented type name expected!\n";
                return false;
            }

            interface_ts = ptr_cast<TypeValue>(v)->represented_ts;
        
            if (!interface_ts.has_meta(interface_metatype)) {
                std::cerr << "Implemented interface name expected!\n";
                return false;
            }
            
            delete v;
        }
        else
            return false;
        
        return true;
    }

    virtual Declaration *declare(std::string name, Scope *scope) {
        if (scope->type == DATA_SCOPE) {
            Declaration *d = new Implementation(name, interface_ts, inherit_as);
            scope->add(d);
            return d;
        }
        else
            return NULL;
    }
};


// TODO: this is not strictly a type definition

class FunctorDefinitionValue: public TypeDefinitionValue {
public:
    ClassType *class_type;
    std::vector<Value *> with_values;
    std::vector<Variable *> with_vars;
    
    Value *fdv;
    TypeMatch match;
    
    FunctorDefinitionValue(Value *p, TypeMatch tm)
        :TypeDefinitionValue() {
        match = tm;
        fdv = NULL;
        class_type = NULL;
    }
    
    virtual bool check_with(Expr *e, CodeScope *cs, DataScope *is) {
        Value *v = typize(e, cs, NULL);
        if (!v)
            return false;
            
        CreateValue *cv = ptr_cast<CreateValue>(v);
        if (!cv)
            return false;
            
        DeclarationValue *dv = ptr_cast<DeclarationValue>(cv->left.get());
        if (!dv)
            return false;
            
        Variable *var = new Variable(dv->var->name, dv->var->alloc_ts);
        is->add(var);
            
        with_vars.push_back(var);
        with_values.push_back(cv->right.release());
        
        delete v;
        
        return true;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        Expr *main_expr = NULL, *with_expr = NULL, *to_expr = NULL, *from_expr = NULL, *do_expr = NULL;
        
        ExprInfos eis = {
            { "", &main_expr },
            { "with", &with_expr },
            { "to", &to_expr },
            { "from", &from_expr },
            { "do", &do_expr },  // TODO: raise?
        };
        
        if (!check_exprs(args, kwargs, eis)) {
            std::cerr << "Whacky functor!\n";
            return false;
        }

        // Create Class
        class_type = new ClassType("<functor>", {});
        scope->get_data_scope()->add(class_type);
        
        TypeSpec pivot_ts = { ptr_type, class_type };
        class_type->make_inner_scope();
        DataScope *is = class_type->get_inner_scope();
        
        // Add main role
        TypeSpec main_ts = typize_typespec(main_expr, scope, abstract_metatype);
        
        if (main_ts == NO_TS) {
            std::cerr << "Functor abstract name expected!\n";
            return false;
        }

        ts = main_ts.prefix(ref_type);

        Associable *main_role = new Role(MAIN_ROLE_NAME, main_ts, AS_MAIN);
        is->add(main_role);
        
        // Typize the with block, using a temporary code scope to collect state variables
        CodeScope *cs = new CodeScope;
        scope->add(cs);
        cs->be_taken();  // we're taking care of killing it
        cs->enter();
        
        if (with_expr == NULL)
            ;  // Okay not to have state variables
        else if (with_expr->type == Expr::TUPLE) {
            for (auto &e : with_expr->args) {
                if (!check_with(e.get(), cs, is))
                    return false;
            }
        }
        else {
            if (!check_with(with_expr, cs, is))
                return false;
        }
        
        cs->leave();
        scope->remove(cs);
        
        Args fake_args;
        fake_args.push_back(std::unique_ptr<Expr>(to_expr));
        Kwargs fake_kwargs;
        fake_kwargs["from"] = std::unique_ptr<Expr>(from_expr);
        fake_kwargs["as"] = std::unique_ptr<Expr>(do_expr);
        
        fdv = make<FunctionDefinitionValue>((Value *)NULL, match);
        
        if (!fdv->check(fake_args, fake_kwargs, is))
            return false;
            
        fake_args[0].release();
        fake_kwargs["from"].release();
        fake_kwargs["as"].release();
            
        Declaration *decl = fdv->declare(MAIN_ROLE_NAME + QUALIFIER_NAME + "do", is);
        
        main_role->check_associated(decl);

        class_type->complete_type();

        fdv->define_code();

        is->leave();
        
        return true;
    }
    
    virtual Regs precompile(Regs preferred) {
        for (auto wv : with_values)
            wv->precompile(preferred);
            
        fdv->precompile(preferred);
        
        return Regs::all();
    }
    
    virtual Storage compile(X64 *x64) {
        // TODO: compile functions using Once?
        Label skip;
        
        x64->op(JMP, skip);
        
        fdv->compile(x64);
        
        x64->code_label(skip);
        
        Storage ps = preinitialize_class({ class_type }, x64);
        
        if (ps.where != REGISTER)
            throw INTERNAL_ERROR;
            
        Storage cs = Storage(MEMORY, Address(ps.reg, 0));
        
        // FIXME: handle exceptions!
        for (unsigned i = 0; i < with_vars.size(); i++) {
            Storage s = with_values[i]->compile(x64);
            Storage t = with_vars[i]->get_storage(match, cs);
            with_vars[i]->alloc_ts.create(s, t, x64);
        }
        
        return ps;
    }
};
