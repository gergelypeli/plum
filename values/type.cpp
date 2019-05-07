
class TypeValue: public Value {
public:
    TypeSpec pivot_ts;
    TypeSpec represented_ts;
    
    TypeValue(Type *mt, TypeSpec ts)
        :Value(TypeSpec { mt }) {
        represented_ts = ts;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() != 0 || kwargs.size() != 0) {
            std::cerr << "Whacky type name!\n";
            return false;
        }

        if (scope->type == ARGUMENT_SCOPE || scope->type == CODE_SCOPE || scope->type == DATA_SCOPE)
            pivot_ts = scope->get_pivot_ts();
            
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
        :TypeValue(tuple_metatype, NO_TS) {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        // TODO: limited support for now

        if (args.size() == 0 && kwargs.size() == 0) {
            // Empty tuple: {}
            represented_ts = { tuple0_type };
            return true;
        }
        else if (args.size() == 1 && kwargs.size() == 0) {
            // Single tuple: { T }
            Value *value = typize(args[0].get(), scope, NULL);
    
            if (!value->ts.is_meta()) {
                std::cerr << "Not a type name in a tuple type definition!\n";
                return false;
            }
        
            TypeSpec rts = ptr_cast<TypeValue>(value)->represented_ts;
        
            if (!rts.has_meta(argument_metatype)) {
                std::cerr << "Not an argument type name in a tuple type definition!\n";
                return false;
            }
            
            represented_ts = rts.prefix(tuple1_type);
            return true;
        }
        else {
            std::cerr << "Sorry, too complex tuple type for now!\n";
            return false;
        }
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
        return orig->precompile(preferred);
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

