
class TypeValue: public Value {
public:
    Type *type;
    MetaType *meta_type;
    Metatypes param_metatypes;
    TypeSpec represented_ts;
    
    TypeValue(Type *t, MetaType *mt, Metatypes pmts)
        :Value(TypeSpec { mt }) {
        type = t;
        meta_type = mt;
        param_metatypes = pmts;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (kwargs.size() > 0) {
            return false;
        }
        
        unsigned pc = param_metatypes.size();
        
        if (args.size() != pc) {
            std::cerr << "Type " << type->name << " requires " << pc << " parameters!\n";
            return false;
        }
        
        represented_ts.push_back(type);
        
        for (unsigned i = 0; i < pc; i++) {
            TypeSpec meta_ts = { param_metatypes[i] };
            Value *value = typize(args[i].get(), scope, &meta_ts);
    
            if (!value->ts.is_meta()) {
                std::cerr << "Type " << type->name << " parameter " << (i + 1) << " is not a type!\n";
                return false;
            }
        
            TypeSpec rts = ptr_cast<TypeValue>(value)->represented_ts;
        
            if (!rts.has_meta(param_metatypes[i])) {
                std::cerr << "Type " << type->name << " parameter " << (i + 1) << " is not a " << param_metatypes[i]->name << "!\n";
                return false;
            }
        
            represented_ts.insert(represented_ts.end(), rts.begin(), rts.end());
        }

        return true;
    }
    
    virtual Regs precompile(Regs preferred) {
        return Regs();
    }
    
    virtual Storage compile(X64 *x64) {
        return Storage();
    }

    virtual Declaration *declare(std::string name, Scope *scope) {
        if (scope->type == ARGUMENT_SCOPE) {
            if (!represented_ts.has_meta(argument_metatype)) {
                std::cerr << "Invalid type for argument declaration: " << represented_ts << "!\n";
                return NULL;
            }
            
            Declaration *d;
            
            if (represented_ts[0] == code_type)
                d = new Evaluable(name, represented_ts);
            else
                d = new Variable(name, represented_ts);
                
            scope->add(d);
            return d;
        }
        else if (scope->type == CODE_SCOPE || scope->type == DATA_SCOPE) {
            if (!represented_ts.has_meta(value_metatype)) {
                std::cerr << "Invalid type for variable declaration: " << represented_ts << "!\n";
                return NULL;
            }

            Declaration *d = new Variable(name, represented_ts.lvalue());
            scope->add(d);
            return d;
        }
        else {
            std::cerr << "Variables must be declared in code or data scopes!\n";
            return NULL;
        }
    }
};


// Describes a type, that happens to be a tuple
class TupleTypeValue: public TypeValue {
public:
    TupleTypeValue()
        :TypeValue(NULL, tuple_metatype, {}) {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (kwargs.size() > 0) {
            std::cerr << "Sorry, no keywords in tuple types now!\n";
            return false;
        }
        
        TSs tss;
        
        for (auto &a : args) {
            Value *value = typize(a.get(), scope, NULL);
            
            if (!value->ts.is_meta()) {
                std::cerr << "Not a type name in a tuple type definition!\n";
                return false;
            }
        
            TypeSpec rts = ptr_cast<TypeValue>(value)->represented_ts;
        
            if (!rts.has_meta(argument_metatype)) {
                std::cerr << "Not an argument type name in a tuple type definition!\n";
                return false;
            }
            
            tss.push_back(rts);
        }
        
        represented_ts.pack_tuple(tss);
        return true;
    }

    virtual Declaration *declare(std::string name, Scope *scope) {
        std::cerr << "No identifier can be declared as a tuple type!\n";
        return NULL;
    }
};


// Describes a tuple, that happens to contain types
class TypeTupleValue: public Value {
public:
    std::vector<std::unique_ptr<Value>> values;
    std::vector<TypeSpec> represented_tss;

    static TypeSpec metas(std::vector<std::unique_ptr<Value>> &vs) {
        TupleType *tt = TupleType::get(std::vector<std::string>(vs.size(), ""));
        TypeSpec mts = { tt };
        
        for (auto &v : vs) {
            mts.insert(mts.end(), v->ts.begin(), v->ts.end());
        }
        
        return mts;
    }

    TypeTupleValue(std::vector<std::unique_ptr<Value>> vs)
        :Value(metas(vs)), values(std::move(vs)) {
        for (auto &v : values)
            represented_tss.push_back(ptr_cast<TypeValue>(v.get() )->represented_ts);
    }
    
    /*
    static TypeSpec metas(std::vector<TypeSpec> rtss) {
        TupleType *tt = TupleType::get(std::vector<std::string>(rtss.size(), ""));
        TypeSpec mts = { tt };
        
        for (auto &rts : rtss)
            mts.push_back(rts.get_meta());
            
        return mts;
    }

    TypeTupleValue(std::vector<TypeSpec> tss)
        :Value(metas(tss)) {
        represented_tss = tss;
    }
    */
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        // This class was created after checking the arguments
        throw INTERNAL_ERROR;
    }
    
    virtual Storage compile(X64 *x64) {
        throw INTERNAL_ERROR;
    }
};


class VoidConversionValue: public Value {
public:
    std::unique_ptr<Value> orig;
    
    VoidConversionValue(Value *o)
        :Value(VOID_TS) {
        orig.reset(o);
        set_token(o->token);
        //marker = orig->marker;
    }
    
    virtual Regs precompile(Regs preferred) {
        return orig->precompile_tail();
    }
    
    virtual Storage compile(X64 *x64) {
        Storage s = orig->compile(x64);
        Storage t = Storage();
        orig->ts.store(s, t, x64);
        return t;
    }
    
    virtual void escape_statement_variables() {
        orig->escape_statement_variables();
    }
};


class ImplementationConversionValue: public Value {
public:
    TypeMatch match;
    Implementation *implementation;
    std::unique_ptr<Value> orig;
    
    ImplementationConversionValue(Implementation *imt, Value *o, TypeMatch tm)
        :Value(o->ts) {
        match = tm;  // the implementor type's match
        implementation = imt;
        orig.reset(o);
        
        //if (o->ts[0] == lvalue_type)
        //    ts = ts.lvalue();
    }
    
    virtual void streamify(X64 *x64) {
        Associable *sable = implementation->autoconv_streamifiable(match);
        
        if (!sable) {
            std::cerr << "Unstreamifiable implementation!\n";
            throw TYPE_ERROR;  // TODO: return value?
        }

        // Allow the implementation do this as it likes
        sable->streamify(match, x64);
    }
    
    virtual Value *lookup_inner(std::string name, Scope *scope) {
        //Scope *implementor_scope = implementation->outer_scope;
        ts = orig->ts;  // implementor_scope->get_pivot_ts();
        std::string implementing_name = implementation->prefix + name;
        
        // In the case of nested interfaces (such as Iterable in Iterator) the ts
        // is a concrete type, while the implementation of the nested interface methods
        // are in the enclosing interface, not in the concrete type scope. So looking
        // up by ts won't find it, and we must look up directly in the implementor scope.
        //std::cerr << "Looking up implementation " << ts << " " << implementing_name << "\n";
        //Value *value = implementor_scope->lookup(implementing_name, this, scope);
        
        Value *value = implementation->outer_scope->lookup(implementing_name, this, scope);

        if (!value)
            std::cerr << "Oops, missing implementation " << implementing_name << "?\n";

        return value;
    }

    virtual void need_lvalue() {
        orig->need_lvalue();
    }
    
    virtual Regs precompile(Regs preferred) {
        return orig->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        return orig->compile(x64);
    }
};

