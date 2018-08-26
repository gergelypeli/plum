
class SingletonType: public Type {
public:
    std::vector<Allocable *> member_allocables;
    std::vector<std::string> member_names;
    Function *initializer_function;
    Function *finalizer_function;

    SingletonType(std::string name)
        :Type(name, {}, singleton_metatype) {
        initializer_function = NULL;
        finalizer_function = NULL;
    }

    virtual Value *matched(TypeSpec result_ts) {
        return make<TypeValue>(this, result_ts);
    }

    virtual bool complete_type() {
        for (auto &c : inner_scope->contents) {
            Allocable *v = ptr_cast<Allocable>(c.get());
            
            if (v) {
                member_allocables.push_back(v);
                member_names.push_back(v->name);
            }
            
            Function *f = ptr_cast<Function>(c.get());
            
            if (f && f->type == INITIALIZER_FUNCTION) {
                if (initializer_function) {
                    std::cerr << "Multiple singleton initializers!\n";
                    return false;
                }
                    
                initializer_function = f;
            }

            if (f && f->type == FINALIZER_FUNCTION) {
                if (finalizer_function) {
                    std::cerr << "Multiple singleton finalizers!\n";
                    return false;
                }
                    
                finalizer_function = f;
            }
        }
        
        std::cerr << "Singleton " << name << " has " << member_allocables.size() << " member variables.\n";
        return true;
    }

    virtual Allocation measure(TypeMatch tm) {
        return inner_scope->get_size(tm);
    }

    virtual StorageWhere where(TypeMatch tm, AsWhat as_what) {
        return NOWHERE;
    }

    virtual DataScope *make_inner_scope() {
        return new SingletonScope;
    }
    
    virtual DataScope *make_inner_scope(TypeSpec pts) {
        return Type::make_inner_scope(pts);  // Thanks, C++!
    }

    virtual std::vector<std::string> get_member_names() {
        return member_names;
    }

    // No initializers are accessible from the language, done by the runtime itself
    Label compile_initializer(X64 *x64) {
        Label label;
        x64->code_label_local(label, name + "_initializer");  // FIXME: ambiguous name!

        if (initializer_function)
            x64->op(CALL, initializer_function->get_label(x64));  // no arguments

        x64->op(RET);
        return label;
    }

    Label compile_finalizer(X64 *x64) {
        Label label;
        x64->code_label_local(label, name + "_finalizer");  // FIXME: ambiguous name!

        if (finalizer_function)
            x64->op(CALL, finalizer_function->get_label(x64));  // no arguments

        TypeSpec ts = { this };
        TypeMatch tm = ts.match();
        SingletonScope *ss = ptr_cast<SingletonScope>(inner_scope.get());
        Storage s(MEMORY, Address(x64->runtime->application_label, ss->offset.concretize()));

        for (auto &var : member_allocables)  // FIXME: reverse!
            var->destroy(tm, s, x64);

        x64->op(RET);
        return label;
    }
};


class ModuleType: public Type {
public:
    ModuleScope *module_scope;
    std::vector<SingletonType *> singleton_types;

    ModuleType(std::string name, ModuleScope *ms)
        :Type(name, {}, module_metatype) {
        module_scope = ms;
    }
    
    virtual void store(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        if (s.where != NOWHERE || t.where != NOWHERE) {
            std::cerr << "Invalid module store from " << s << " to " << t << "!\n";
            throw INTERNAL_ERROR;
        }
    }

    virtual Value *lookup_inner(TypeMatch tm, std::string n, Value *v) {
        return module_scope->lookup(n, v);
    }
        
    virtual bool complete_type() {
        for (auto &c : module_scope->contents) {
            SingletonType *s = ptr_cast<SingletonType>(c.get());
            
            if (s)
                singleton_types.push_back(s);
        }
        
        std::cerr << "Module " << name << " has " << singleton_types.size() << " singletons.\n";
        return true;
    }

    virtual void collect_initializer_labels(std::vector<Label> &labels, X64 *x64) {
        for (SingletonType *s : singleton_types)
            labels.push_back(s->compile_initializer(x64));
    }

    virtual void collect_finalizer_labels(std::vector<Label> &labels, X64 *x64) {
        for (SingletonType *s : singleton_types)
            labels.push_back(s->compile_finalizer(x64));
    }
};
