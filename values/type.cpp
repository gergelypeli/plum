#include "../plum.h"


TypeValue::TypeValue(Type *t, MetaType *mt, Metatypes pmts)
    :Value(TypeSpec { mt }) {
    type = t;
    meta_type = mt;
    param_metatypes = pmts;
}

bool TypeValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
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

Regs TypeValue::precompile(Regs preferred) {
    return Regs();
}

Storage TypeValue::compile(Cx *cx) {
    return Storage();
}

Declaration *TypeValue::declare(std::string name, Scope *scope) {
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


// Describes a type, that happens to be a tuple

TupleTypeValue::TupleTypeValue()
    :TypeValue(NULL, tuple_metatype, {}) {
}

bool TupleTypeValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
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

Declaration *TupleTypeValue::declare(std::string name, Scope *scope) {
    std::cerr << "No identifier can be declared as a tuple type!\n";
    return NULL;
}


// Describes a tuple, that happens to contain types

TypeTupleValue::TypeTupleValue(std::vector<std::unique_ptr<Value>> vs)
    :Value(metas(vs)), values(std::move(vs)) {
    for (auto &v : values)
        represented_tss.push_back(ptr_cast<TypeValue>(v.get() )->represented_ts);
}

TypeSpec TypeTupleValue::metas(std::vector<std::unique_ptr<Value>> &vs) {
    TupleType *tt = TupleType::get(std::vector<std::string>(vs.size(), ""));
    TypeSpec mts = { tt };
    
    for (auto &v : vs) {
        mts.insert(mts.end(), v->ts.begin(), v->ts.end());
    }
    
    return mts;
}

bool TypeTupleValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    // This class was created after checking the arguments
    throw INTERNAL_ERROR;
}

Storage TypeTupleValue::compile(Cx *cx) {
    throw INTERNAL_ERROR;
}




VoidConversionValue::VoidConversionValue(Value *o)
    :Value(VOID_TS) {
    orig.reset(o);
    set_token(o->token);
    //marker = orig->marker;
}

Regs VoidConversionValue::precompile(Regs preferred) {
    return orig->precompile_tail();
}

Storage VoidConversionValue::compile(Cx *cx) {
    Storage s = orig->compile(cx);
    Storage t = Storage();
    orig->ts.store(s, t, cx);
    return t;
}

void VoidConversionValue::escape_statement_variables() {
    orig->escape_statement_variables();
}




ImplementationConversionValue::ImplementationConversionValue(Implementation *imt, Value *o, TypeMatch tm)
    :Value(o->ts) {
    match = tm;  // the implementor type's match
    implementation = imt;
    orig.reset(o);
    
    //if (o->ts[0] == lvalue_type)
    //    ts = ts.lvalue();
}

void ImplementationConversionValue::streamify(Cx *cx) {
    Associable *sable = implementation->autoconv_streamifiable(match);
    
    if (!sable) {
        std::cerr << "Unstreamifiable implementation!\n";
        throw TYPE_ERROR;  // TODO: return value?
    }

    // Allow the implementation do this as it likes
    sable->streamify(match, cx);
}

Value *ImplementationConversionValue::lookup_inner(std::string name, Scope *scope) {
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

void ImplementationConversionValue::need_rvalue() {
    ptr_cast<GenericLvalue>(orig.get())->need_rvalue();
}

Regs ImplementationConversionValue::precompile(Regs preferred) {
    return orig->precompile(preferred);
}

Storage ImplementationConversionValue::compile(Cx *cx) {
    return orig->compile(cx);
}

