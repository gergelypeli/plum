
class StringBufferValue: public Value {
public:
    int length;
    
    StringBufferValue(int l)
        :Value(STRING_TS) {
        length = l;
    }

    virtual Regs precompile(Regs preferred) {
        return Regs(RAX);
    }

    virtual Storage compile(X64 *x64) {
        Label alloc_array = x64->once->compile(compile_array_alloc, CHARACTER_TS);
        
        x64->op(MOVQ, RAX, length);
        x64->op(CALL, alloc_array);
        x64->op(PUSHQ, RAX);
        
        return Storage(STACK);
    }
};


class GenericStreamificationValue: public GenericValue {
public:

    GenericStreamificationValue(Value *p, TypeMatch &match)
        :GenericValue(STRING_LVALUE_TS, VOID_TS, p) {
    }
    
    virtual Regs precompile(Regs preferred) {
        left->precompile(preferred);
        right->precompile(preferred);
            
        return Regs::all();  // We're Void
    }
    
    virtual Storage compile(X64 *x64) {
        compile_and_store_both(x64, Storage(STACK), Storage(ALISTACK));

        left->ts.streamify(false, x64);
        
        right->ts.store(rs, Storage(), x64);
        left->ts.store(ls, Storage(), x64);
        
        return Storage();
    }
};


// This is a simplified CodeBlockValue that allows a nonvoid result
class InterpolationValue: public Value {
public:
    std::vector<std::unique_ptr<Value>> statements;

    InterpolationValue()
        :Value(STRING_TS) {
    }

    virtual void add_statement(Value *value) {
        statements.push_back(std::unique_ptr<Value>(value));
        value->complete_definition();
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob;
        
        for (unsigned i = 0; i < statements.size(); i++)
            clob = clob | statements[i]->precompile(preferred);
            
        return clob;
    }

    virtual Storage compile(X64 *x64) {
        for (unsigned i = 0; i < statements.size() - 1; i++)
            statements[i]->compile_and_store(x64, Storage());
        
        return statements.back()->compile(x64);
    }
};


CreateValue *make_initialization_by_value(std::string name, Value *v, Scope *scope) {
    DeclarationValue *dv = new DeclarationValue(name);
    dv->fix_bare(v->ts, scope);
    
    TypeMatch tm = { dv->ts, dv->ts.unprefix(uninitialized_type) };
    CreateValue *cv = new CreateValue(dv, tm);
    cv->use(v);
    
    return cv;
}


Value *interpolate(std::string text, Expr *expr, Scope *scope) {
    std::vector<std::string> fragments = brace_split(text);
    
    if (expr->args.size() > 0) {
        std::cerr << "String interpolation must use keyword arguments only!\n";
        throw TYPE_ERROR;
    }

    // We must scope ourselves
    CodeScope *code_scope = new CodeScope;
    scope->add(code_scope);
    
    InterpolationValue *interpolation = new InterpolationValue;
    
    CreateValue *cv = make_initialization_by_value("<interpolated>", new StringBufferValue(100), code_scope);
    Variable *interpolated_var = ptr_cast<Variable>(cv->get_decl());
    interpolation->add_statement(cv);

    for (auto &kv : expr->kwargs) {
        std::string keyword = kv.first;
        Expr *expr = kv.second.get();
        Value *keyword_value = typize(expr, code_scope);
        CreateValue *decl_value = make_initialization_by_value(keyword, keyword_value, code_scope);
        interpolation->add_statement(decl_value);
    }

    bool pseudo_only = (expr->kwargs.size() > 0);
    bool identifier = false;
    
    for (auto &fragment : fragments) {
        Value *pivot;
        
        if (identifier) {
            // For explicit keywords, we only look up in the innermost scope.
            // For identifiers, we look up outer scopes, but we don't need to look
            // in inner scopes, because that would need a pivot value, which we don't have.
            
            for (Scope *s = code_scope; s; s = s->outer_scope) {
                pivot = s->lookup(fragment, NULL);
        
                if (pivot)
                    break;
                else if (pseudo_only)
                    break;  // Look up only pseudo variables in this scope
            }
            
            if (!pivot) {
                std::cerr << "Cannot interpolate undefined {" << fragment << "}!\n";
                throw TYPE_ERROR;
            }
        }
        else {
            pivot = make<StringLiteralValue>(fragment);
        }

        TypeMatch match;
        if (!typematch(STREAMIFIABLE_TS, pivot, match)) {
            std::cerr << "Cannot interpolate unstreamifiable " << pivot->ts << "!\n";
            throw TYPE_ERROR;
        }

        Value *streamify = lookup_fake("streamify", pivot, expr->token, code_scope, NULL, interpolated_var);
        if (!streamify) {
            std::cerr << "Cannot interpolate badly streamifiable " << pivot->ts << "!\n";
            throw TYPE_ERROR;
        }

        interpolation->add_statement(streamify);
        identifier = !identifier;
    }

    TypeMatch match;  // kinda unnecessary
    Value *ret = make<VariableValue>(interpolated_var, (Value *)NULL, match);
    ret = ret->lookup_inner("realloc");  // FIXME: missing check, but at least no arguments
    interpolation->add_statement(ret);
    
    return make<CodeScopeValue>(interpolation, code_scope);
}
