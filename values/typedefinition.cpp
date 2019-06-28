#include "../plum.h"


bool RoleCreator::extend_main_role(Associable *base_role, Associable *main_role) {
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

Associable *RoleCreator::add_head_role(Scope *is, std::string name, TypeSpec main_ts, TypeSpec base_ts, InheritAs ia, bool isa) {
    Associable *base_role = NULL, *main_role = NULL;
    bool is_explicit = (ia != AS_BASE);
    
    if (base_ts != NO_TS) {
        if (is_explicit)
            base_role = new Role(name, RVALUE_PIVOT, base_ts, ia, isa);
        else
            base_role = new Role(BASE_ROLE_NAME, RVALUE_PIVOT, base_ts, AS_BASE, true);
            
        is_explicit = false;
    }
        
    if (main_ts != NO_TS) {
        if (is_explicit)
            main_role = new Role(name, RVALUE_PIVOT, main_ts, ia, isa);
        else
            main_role = new Role(MAIN_ROLE_NAME, RVALUE_PIVOT, main_ts, AS_MAIN, true);
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



TypeDefinitionValue::TypeDefinitionValue()
    :Value(HYPERTYPE_TS) {
}

Regs TypeDefinitionValue::precompile(Regs) {
    return Regs();
}

Storage TypeDefinitionValue::compile(Cx *) {
    return Storage();
}

TypeSpec TypeDefinitionValue::typize_typespec(Expr *expr, Scope *scope, MetaType *meta_type) {
    Value *value = typize(expr, scope, NULL);

    if (!value->ts.is_meta())
        return NO_TS;
    
    TypeSpec rts = ptr_cast<TypeValue>(value)->represented_ts;
    
    if (!rts.has_meta(meta_type))
        return NO_TS;
        
    return rts;
}




IntegerDefinitionValue::IntegerDefinitionValue()
    :TypeDefinitionValue() {
    size = 0;
    is_not_signed = false;
}

bool IntegerDefinitionValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
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

Declaration *IntegerDefinitionValue::declare(std::string name, Scope *scope) {
    if (scope->type == DATA_SCOPE || scope->type == CODE_SCOPE || scope->type == MODULE_SCOPE) {
        Declaration *d = new IntegerType(name, size, is_not_signed);
        scope->add(d);
        return d;
    }
    else
        return NULL;
}




EnumerationDefinitionValue::EnumerationDefinitionValue()
    :TypeDefinitionValue() {
}

bool EnumerationDefinitionValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
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


Declaration *EnumerationDefinitionValue::declare(std::string name, Scope *scope) {
    if (scope->type == DATA_SCOPE || scope->type == CODE_SCOPE || scope->type == MODULE_SCOPE) {
        Type *t = new EnumerationType(name, keywords);
        t->make_inner_scope()->leave();
        scope->add(t);
        return t;
    }
    else
        return NULL;
}




TreenumerationDefinitionValue::TreenumerationDefinitionValue()
    :TypeDefinitionValue() {
}

unsigned TreenumerationDefinitionValue::add_keyword(std::string kw, unsigned parent) {
    for (auto &k : keywords)
        if (k == kw)
            return 0;
            
    unsigned x = keywords.size();
    keywords.push_back(kw);
    parents.push_back(parent);
    
    return x;
}

bool TreenumerationDefinitionValue::parse_level(Args &args, unsigned parent) {
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

bool TreenumerationDefinitionValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    if (args.size() == 0 || kwargs.size() != 0) {
        std::cerr << "Whacky treenumeration!\n";
        return false;
    }

    add_keyword("", 0);
    
    if (!parse_level(args, 0))
        return false;
    
    return true;
}

Declaration *TreenumerationDefinitionValue::declare(std::string name, Scope *scope) {
    if (scope->type == DATA_SCOPE || scope->type == CODE_SCOPE || scope->type == MODULE_SCOPE || scope->type == FUNCTION_SCOPE) {
        Type *t = new TreenumerationType(name, keywords, parents);
        t->make_inner_scope()->leave();
        scope->add(t);
        return t;
    }
    else
        return NULL;
}




RoleDefinitionValue::RoleDefinitionValue(Value *pivot, TypeMatch &tm, InheritAs ia, bool isa)
    :TypeDefinitionValue() {
    inherit_as = ia;
    is_concrete = false;
    is_autoconv = isa;
}

bool RoleDefinitionValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    DataScope *ds = ptr_cast<DataScope>(scope);
    is_concrete = ds->is_virtual_scope() && !ds->is_abstract_scope();
    std::unique_ptr<Expr> implemented_expr, inherited_expr;

    ExprInfos eis = {
        { "", &implemented_expr },
        { "by", &inherited_expr }
    };
    
    if (!check_exprs(args, kwargs, eis)) {
        std::cerr << "Whacky role!\n";
        return false;
    }

    if (!implemented_expr && !inherited_expr) {
        std::cerr << "Neither inherited nor implemented type specified!\n";
        return false;
    }
    
    if (inherit_as == AS_REQUIRE && implemented_expr && inherited_expr) {
        std::cerr << "Required role must be either an Abstract or a Class!\n";
        return false;
    }
    
    if (implemented_expr) {
        // Implementation
        Value *v = typize(implemented_expr.get(), scope, NULL);

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
        
        Value *v = typize(inherited_expr.get(), scope, NULL);

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

    return true;
}

Declaration *RoleDefinitionValue::declare(std::string name, Scope *scope) {
    if (scope->type == DATA_SCOPE) {
        return add_head_role(scope, name, implemented_ts, inherited_ts, inherit_as, is_autoconv);
    }
    else
        return NULL;
}



PlainRoleDefinitionValue::PlainRoleDefinitionValue(Value *pivot, TypeMatch &tm)
    :RoleDefinitionValue(pivot, tm, AS_ROLE, false) {
}



AutoRoleDefinitionValue::AutoRoleDefinitionValue(Value *pivot, TypeMatch &tm)
    :RoleDefinitionValue(pivot, tm, AS_ROLE, true) {
}



RequireRoleDefinitionValue::RequireRoleDefinitionValue(Value *pivot, TypeMatch &tm)
    :RoleDefinitionValue(pivot, tm, AS_REQUIRE, false) {
}




ProvideDefinitionValue::ProvideDefinitionValue(Value *pivot, TypeMatch &tm)
    :TypeDefinitionValue() {
    provider_associable = NULL;
}

bool ProvideDefinitionValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    DataScope *ds = ptr_cast<DataScope>(scope);
    std::unique_ptr<Expr> provider_expr;

    ExprInfos eis = {
        { "", &provider_expr }
    };
    
    if (!check_exprs(args, kwargs, eis)) {
        std::cerr << "Whacky provide!\n";
        return false;
    }

    if (provider_expr) {
        if (provider_expr->type != Expr::IDENTIFIER) {
            std::cerr << "Provided role identifier expected!\n";
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
            std::cerr << "Unknown provided role!\n";
            return false;
        }
    }
    else {
        // Use the base role as provider
        // Not nice, but we'll dig into the inner scope to find the base role
        provider_associable = ptr_cast<Associable>(scope->contents[0].get());

        if (!provider_associable || !provider_associable->is_baseconv()) {
            std::cerr << "No base role to provide!\n";
            return false;
        }
    }
    
    return true;
}

Declaration *ProvideDefinitionValue::declare(std::string name, Scope *scope) {
    if (scope->type == DATA_SCOPE) {
        Declaration *d = new Provision(name, provider_associable);
        scope->add(d);
        return d;
    }
    else
        return NULL;
}




ImportDefinitionValue::ImportDefinitionValue()
    :TypeDefinitionValue() {
    import_scope = NULL;
}

bool ImportDefinitionValue::check_identifier(Expr *e) {
    if (e->type != Expr::IDENTIFIER) {
        std::cerr << "Not an identifier imported!\n";
        return false;
    }
    
    import_scope->add(e->text);
    return true;
}


bool ImportDefinitionValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    std::unique_ptr<Expr> name_expr, for_expr;
    
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
            ok = check_identifier(for_expr.get());
    }
    
    import_scope->leave();
    
    return true;
}

Declaration *ImportDefinitionValue::declare(std::string name, Scope *scope) {
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




GlobalDefinitionValue::GlobalDefinitionValue(Value *null, TypeMatch &tm)
    :TypeDefinitionValue() {
    gvar = NULL;
}

bool GlobalDefinitionValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    std::unique_ptr<Expr> main_expr, class_expr;
    
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

    main_ts = typize_typespec(main_expr.get(), scope, abstract_metatype);
    if (main_ts == NO_TS)
        return false;
        
    class_ts = typize_typespec(class_expr.get(), scope, class_metatype);
    if (class_ts == NO_TS)
        return false;
        
    return true;
}

bool GlobalDefinitionValue::define_data() {
    // Runs after declare
    // TODO: maybe allow selecting the initializer?
    Function *initializer_function = NULL;
    Scope *is = ptr_cast<ClassType>(class_ts[0])->get_inner_scope();

    for (auto &d : is->contents) {
        Function *f = ptr_cast<Function>(d.get());
        
        if (f && f->type == INITIALIZER_FUNCTION) {
            initializer_function = f;
            break;
        }
    }
    
    if (!initializer_function) {
        std::cerr << "No initializer in global variable class!\n";
        return false;
    }
    
    gvar->set_initializer_function(initializer_function);
    
    return true;
}

Declaration *GlobalDefinitionValue::declare(std::string name, Scope *scope) {
    if (scope->type == MODULE_SCOPE || scope->type == DATA_SCOPE) {
        gvar = new GlobalVariable(name, main_ts.prefix(ptr_type), class_ts);
        scope->add(gvar);
        return gvar;
    }
    else
        return NULL;
}




ScopedTypeDefinitionValue::ScopedTypeDefinitionValue()
    :TypeDefinitionValue() {
    defined_type = NULL;
}

void ScopedTypeDefinitionValue::defer(std::unique_ptr<Expr> e) {
    // Expressions in type definitions may define other types, and such definitions
    // must take place in the first pass, while the definitions of the type details
    // in the second pass.
    
    if (is_typedefinition(e.get()))
        meta_args.push_back(std::move(e));
    else
        data_args.push_back(std::move(e));
}

void ScopedTypeDefinitionValue::defer_as(std::unique_ptr<Expr> as_expr) {
    // Type bodies may refer to their own type name, so they must be deferred
    if (as_expr) {
        if (as_expr->type == Expr::TUPLE) {
            for (auto &e : as_expr->args)
                defer(std::move(e));
        }
        else
            defer(std::move(as_expr));
    }
}

Type *ScopedTypeDefinitionValue::define(Type *dt, Scope *s) {
    defined_type = dt;
    s->add(defined_type);

    defined_type->make_inner_scope();
    
    block_value.reset(new DataBlockValue);
    
    Kwargs fake_kwargs;
    
    if (!block_value->check(meta_args, fake_kwargs, defined_type->get_inner_scope()))
        return NULL;
    
    defined_type->get_inner_scope()->leave();
    
    return defined_type;
}

bool ScopedTypeDefinitionValue::define_data_prehook() {
    return true;
}

bool ScopedTypeDefinitionValue::define_data() {
    if (!defined_type)
        throw INTERNAL_ERROR;

    std::cerr << "Completing definition of " << defined_type->name << ".\n";
    defined_type->get_inner_scope()->enter();

    if (!define_data_prehook()) {
        defined_type->get_inner_scope()->leave();
        return false;
    }

    Kwargs fake_kwargs;
    
    if (!block_value->check(data_args, fake_kwargs, defined_type->get_inner_scope()))
        return NULL;

    // Must complete records/classes before compiling method bodies
    if (!defined_type->complete_type())
        return false;
        
    if (!block_value->define_data())
        return false;
    
    std::cerr << "Completed definition of " << defined_type->name << ".\n";
    defined_type->get_inner_scope()->leave();

    return true;
}

bool ScopedTypeDefinitionValue::define_code() {
    defined_type->get_inner_scope()->enter();
    bool ok = block_value->define_code();
    defined_type->get_inner_scope()->leave();
    
    return ok;
}

Regs ScopedTypeDefinitionValue::precompile(Regs preferred) {
    return block_value->precompile_tail();
}

Storage ScopedTypeDefinitionValue::compile(Cx *cx) {
    return block_value->compile(cx);
}



RecordDefinitionValue::RecordDefinitionValue()
    :ScopedTypeDefinitionValue() {
}

bool RecordDefinitionValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    std::unique_ptr<Expr> as_expr;
    
    ExprInfos eis = {
        { "as", &as_expr }
    };
    
    if (!check_exprs(args, kwargs, eis)) {
        std::cerr << "Whacky record!\n";
        return false;
    }

    defer_as(std::move(as_expr));
    
    std::cerr << "Deferring record definition.\n";
    return true;
}

Declaration *RecordDefinitionValue::declare(std::string name, Scope *scope) {
    if (scope->type == DATA_SCOPE || scope->type == CODE_SCOPE || scope->type == MODULE_SCOPE) {
        Type *t = new RecordType(name, Metatypes {});
        return define(t, scope);
    }
    else
        return NULL;
}




UnionDefinitionValue::UnionDefinitionValue()
    :ScopedTypeDefinitionValue() {
}

bool UnionDefinitionValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    std::unique_ptr<Expr> as_expr;
    
    ExprInfos eis = {
        { "as", &as_expr }
    };
    
    if (!check_exprs(args, kwargs, eis)) {
        std::cerr << "Whacky union!\n";
        return false;
    }

    defer_as(std::move(as_expr));
    
    std::cerr << "Deferring union definition.\n";
    return true;
}

Declaration *UnionDefinitionValue::declare(std::string name, Scope *scope) {
    if (scope->type == DATA_SCOPE || scope->type == CODE_SCOPE || scope->type == MODULE_SCOPE) {
        Type *t = new UnionType(name);
        return define(t, scope);
    }
    else
        return NULL;
}




AbstractDefinitionValue::AbstractDefinitionValue()
    :ScopedTypeDefinitionValue() {
    base_expr = NULL;
}

bool AbstractDefinitionValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    std::unique_ptr<Expr> as_expr;
    
    ExprInfos eis = {
        { "", &base_expr },
        { "as", &as_expr }
    };
    
    if (!check_exprs(args, kwargs, eis)) {
        std::cerr << "Whacky abstract!\n";
        return false;
    }

    defer_as(std::move(as_expr));
        
    std::cerr << "Deferring abstract definition.\n";
    return true;
}

bool AbstractDefinitionValue::define_data_prehook() {
    Scope *is = defined_type->get_inner_scope();
    
    if (base_expr) {
        TypeSpec base_ts = typize_typespec(base_expr.get(), is, abstract_metatype);
    
        if (base_ts == NO_TS) {
            std::cerr << "Base abstract name expected!\n";
            return false;
        }

        is->add(new Role("", RVALUE_PIVOT, base_ts, AS_BASE, true));
    }

    return true;
}

Declaration *AbstractDefinitionValue::declare(std::string name, Scope *scope) {
    if (scope->type == DATA_SCOPE || scope->type == CODE_SCOPE || scope->type == MODULE_SCOPE) {
        Type *t = new AbstractType(name, Metatypes {});
        return define(t, scope);
    }
    else
        return NULL;
}




ClassDefinitionValue::ClassDefinitionValue()
    :ScopedTypeDefinitionValue() {
}

bool ClassDefinitionValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    std::unique_ptr<Expr> as_expr;
    
    ExprInfos eis = {
        { "", &main_expr },
        { "by", &base_expr },
        { "as", &as_expr }
    };
    
    if (!check_exprs(args, kwargs, eis))
        return false;
    
    defer_as(std::move(as_expr));
        
    std::cerr << "Deferring class definition.\n";
    return true;
}

Declaration *ClassDefinitionValue::declare(std::string name, Scope *scope) {
    if (scope->type == DATA_SCOPE || scope->type == CODE_SCOPE || scope->type == MODULE_SCOPE) {
        Type *t = new ClassType(name, Metatypes {});
        return define(t, scope);
    }
    else
        return NULL;
}

bool ClassDefinitionValue::define_data_prehook() {
    Scope *is = defined_type->get_inner_scope();
    TypeSpec main_ts, base_ts;
    
    if (main_expr) {
        main_ts = typize_typespec(main_expr.get(), is, abstract_metatype);
    
        if (main_ts == NO_TS) {
            std::cerr << "Main abstract name expected!\n";
            return false;
        }
    }
    
    if (base_expr) {
        base_ts = typize_typespec(base_expr.get(), is, class_metatype);
    
        if (base_ts == NO_TS) {
            std::cerr << "Base class name expected!\n";
            return false;
        }
    }

    if (base_expr || main_expr)
        return add_head_role(is, "", main_ts, base_ts, AS_BASE, true);
    else
        return true;
}


// TODO: allow users define Interfaces?

InterfaceDefinitionValue::InterfaceDefinitionValue()
    :ScopedTypeDefinitionValue() {
    base_expr = NULL;
}

bool InterfaceDefinitionValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    std::unique_ptr<Expr> as_expr;
    
    ExprInfos eis = {
        { "", &base_expr },
        { "as", &as_expr }
    };
    
    if (!check_exprs(args, kwargs, eis)) {
        std::cerr << "Whacky interface!\n";
        return false;
    }

    defer_as(std::move(as_expr));
        
    std::cerr << "Deferring interface definition.\n";
    return true;
}

bool InterfaceDefinitionValue::define_data_prehook() {
    Scope *is = defined_type->get_inner_scope();
    
    if (base_expr) {
        TypeSpec base_ts = typize_typespec(base_expr.get(), is, interface_metatype);
    
        if (base_ts == NO_TS) {
            std::cerr << "Base interface name expected!\n";
            return false;
        }

        is->add(new Implementation("", RVALUE_PIVOT, base_ts, AS_BASE));
    }

    return true;
}

Declaration *InterfaceDefinitionValue::declare(std::string name, Scope *scope) {
    if (scope->type == DATA_SCOPE || scope->type == CODE_SCOPE || scope->type == MODULE_SCOPE) {
        Type *t = new InterfaceType(name, Metatypes {});
        return define(t, scope);
    }
    else
        return NULL;
}




ImplementationDefinitionValue::ImplementationDefinitionValue(Value *pivot, TypeMatch &tm)
    :TypeDefinitionValue() {
}

bool ImplementationDefinitionValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    std::unique_ptr<Expr> interface_expr;

    ExprInfos eis = {
        { "", &interface_expr }
    };
    
    if (!check_exprs(args, kwargs, eis)) {
        std::cerr << "Whacky implementation!\n";
        return false;
    }

    if (interface_expr) {
        // Implementation
        Value *v = typize(interface_expr.get(), scope, NULL);

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

Declaration *ImplementationDefinitionValue::declare(std::string name, Scope *scope) {
    if (scope->type == DATA_SCOPE) {
        Declaration *d = new Implementation(name, RVALUE_PIVOT, interface_ts, AS_ROLE);
        scope->add(d);
        return d;
    }
    else
        return NULL;
}


// TODO: this is not strictly a type definition

FunctorDefinitionValue::FunctorDefinitionValue(Value *p, TypeMatch tm)
    :TypeDefinitionValue() {
    match = tm;
    fdv = NULL;
    class_type = NULL;
}

bool FunctorDefinitionValue::check_with(Expr *e, CodeScope *cs, DataScope *is) {
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

bool FunctorDefinitionValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    std::unique_ptr<Expr> main_expr, with_expr, to_expr, from_expr, do_expr;
    
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
    TypeSpec main_ts = typize_typespec(main_expr.get(), scope, abstract_metatype);
    
    if (main_ts == NO_TS) {
        std::cerr << "Functor abstract name expected!\n";
        return false;
    }

    ts = main_ts.prefix(ref_type);

    Associable *main_role = new Role(MAIN_ROLE_NAME, RVALUE_PIVOT, main_ts, AS_MAIN, true);
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
        if (!check_with(with_expr.get(), cs, is))
            return false;
    }
    
    cs->leave();
    scope->remove(cs);
    delete cs;
    
    Args fake_args;
    fake_args.push_back(std::move(to_expr));
    Kwargs fake_kwargs;
    fake_kwargs["from"] = std::move(from_expr);
    fake_kwargs["as"] = std::move(do_expr);
    
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

Regs FunctorDefinitionValue::precompile(Regs preferred) {
    for (auto wv : with_values)
        wv->precompile_tail();
        
    fdv->precompile_tail();
    
    return Regs::all();
}

void FunctorDefinitionValue::deferred_compile(Label label, Cx *cx) {
    fdv->compile(cx);
}

Storage FunctorDefinitionValue::compile(Cx *cx) {
    // Compile the body later, so it does not get inserted here
    cx->once->compile(this);
    
    Storage ps = preinitialize_class({ class_type }, cx);
    
    if (ps.where != REGISTER)
        throw INTERNAL_ERROR;
        
    cx->op(PUSHQ, ps.reg);
    Storage cs = Storage(MEMORY, Address(ps.reg, 0));
    
    // FIXME: handle exceptions!
    for (unsigned i = 0; i < with_vars.size(); i++) {
        with_values[i]->compile_and_store(cx, Storage(STACK));
        
        cx->op(MOVQ, ps.reg, Address(RSP, with_values[i]->ts.measure_stack()));
        Storage t = with_vars[i]->get_storage(match, cs);
        
        with_vars[i]->alloc_ts.create(Storage(STACK), t, cx);
    }
    
    cx->op(POPQ, ps.reg);
    
    return ps;
}
