
class StringBufferValue: public Value {
public:
    int length;
    
    StringBufferValue(int l)
        :Value(STRING_TS) {
        length = l;
    }

    virtual Regs precompile(Regs preferred) {
        return Regs().add(RAX);
    }

    virtual Storage compile(X64 *x64) {
        Label alloc_array = x64->once->compile(compile_array_alloc, CHARACTER_TS);
        
        x64->op(MOVQ, RAX, length);
        x64->op(CALL, alloc_array);
        x64->op(PUSHQ, RAX);
        
        return Storage(STACK);
    }
};


class StringStreamificationValue: public GenericValue {
public:
    StringStreamificationValue(Value *p, TypeMatch &match)
        :GenericValue(STRING_LVALUE_TS, VOID_TS, p) {
    }
    
    virtual Regs precompile(Regs preferred) {
        left->precompile(preferred);
        right->precompile(preferred);
            
        return Regs::all();  // We're Void
    }
    
    virtual Storage compile(X64 *x64) {
        compile_and_store_both(x64, Storage(STACK), Storage(ALISTACK));

        STRING_TS.streamify(false, x64);

        right->ts.store(rs, Storage(), x64);
        left->ts.store(ls, Storage(), x64);
        
        return Storage();
    }
};


class CharacterStreamificationValue: public GenericValue {
public:

    CharacterStreamificationValue(Value *p, TypeMatch &match)
        :GenericValue(STRING_LVALUE_TS, VOID_TS, p) {
    }
    
    virtual Regs precompile(Regs preferred) {
        left->precompile(preferred);
        right->precompile(preferred);
            
        return Regs::all();  // We're Void
    }
    
    virtual Storage compile(X64 *x64) {
        compile_and_store_both(x64, Storage(STACK), Storage(ALISTACK));

        CHARACTER_TS.streamify(false, x64);
        
        right->ts.store(rs, Storage(), x64);
        left->ts.store(ls, Storage(), x64);
        
        return Storage();
    }
};


class EnumStreamificationValue: public GenericValue {
public:

    EnumStreamificationValue(Value *p, TypeMatch &match)
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


class OptionStreamificationValue: public GenericValue {
public:
    TypeSpec some_ts;

    OptionStreamificationValue(Value *p, TypeMatch &match)
        :GenericValue(STRING_LVALUE_TS, VOID_TS, p) {
        some_ts = match[1];
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


Value *interpolate(std::string text, Expr *expr, Scope *scope) {
    std::vector<std::string> fragments = brace_split(text);
    
    if (expr->args.size() > 0) {
        std::cerr << "String interpolation must use keyword arguments only!\n";
        throw TYPE_ERROR;
    }

    // We must scope ourselves
    CodeScope *code_scope = new CodeScope;
    scope->add(code_scope);
    //scope = s;
    
    CodeBlockValue *block = new CodeBlockValue(NULL);
    
    DeclarationValue *dv = make_declaration_by_value("<interpolated>", new StringBufferValue(100), code_scope);
    Variable *interpolated_var = dv->get_var();
    block->add_statement(dv);

    for (auto &kv : expr->kwargs) {
        std::string keyword = kv.first;
        Expr *expr = kv.second.get();
        Value *keyword_value = typize(expr, code_scope);
        DeclarationValue *decl_value = make_declaration_by_value(keyword, keyword_value, code_scope);
        block->add_statement(decl_value);
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
            pivot = make_string_literal_value(fragment);
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

        block->add_statement(streamify);
        identifier = !identifier;
    }

    TypeMatch match;  // kinda unnecessary
    Value *ret = make_variable_value(interpolated_var, NULL, match);
    //ret = new StringReallocValue(ret, match);
    ret = ret->lookup_inner("realloc");  // FIXME: missing check, but at least no arguments
    block->add_statement(ret, true);
    
    return make_code_scope_value(block, code_scope);
}


