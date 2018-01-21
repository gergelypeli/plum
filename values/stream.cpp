
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
        Label ss_label = x64->once(compile_string_streamification);

        compile_and_store_both(x64, Storage(STACK), Storage(ALISTACK));
        
        x64->op(CALL, ss_label);
        
        right->ts.store(rs, Storage(), x64);
        left->ts.store(ls, Storage(), x64);
        
        return Storage();
    }
    
    static void compile_string_streamification(X64 *x64) {
        // RAX - target array, RBX - tmp, RCX - size, RDX - source array, RDI - alias
        x64->op(MOVQ, RDI, Address(RSP, ADDRESS_SIZE));  // alias to the stream reference
        x64->op(MOVQ, RDX, Address(RSP, ADDRESS_SIZE + ALIAS_SIZE));  // reference to the string
        
        x64->op(MOVQ, RAX, Address(RDI, 0));
        x64->op(MOVQ, RBX, x64->array_length_address(RDX));

        x64->op(MOVQ, RCX, 2);
        x64->preappend_array_RAX_RBX_RCX();
        
        x64->op(MOVQ, Address(RDI, 0), RAX);  // RDI no longer needed

        x64->op(LEA, RDI, x64->array_elems_address(RAX));
        x64->op(ADDQ, RDI, x64->array_length_address(RAX));
        x64->op(ADDQ, RDI, x64->array_length_address(RAX));  // Yes, added twice

        x64->op(LEA, RSI, x64->array_elems_address(RDX));
        x64->op(MOVQ, RCX, x64->array_length_address(RDX));
        x64->op(ADDQ, x64->array_length_address(RAX), RCX);
        x64->op(SHLQ, RCX, 1);
        
        x64->op(REPMOVSB);
        
        x64->op(RET);
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
        Label cs_label = x64->once(compile_character_streamification);

        compile_and_store_both(x64, Storage(STACK), Storage(ALISTACK));
        
        x64->op(CALL, cs_label);
        
        right->ts.store(rs, Storage(), x64);
        left->ts.store(ls, Storage(), x64);
        
        return Storage();
    }
    
    static void compile_character_streamification(X64 *x64) {
        // RAX - target array, RBX - tmp, RCX - size, RDX - source character, RDI - alias
        x64->op(MOVQ, RDI, Address(RSP, ADDRESS_SIZE));  // alias to the stream reference
        x64->op(MOVQ, RDX, Address(RSP, ADDRESS_SIZE + ALIAS_SIZE));  // the character

        x64->op(MOVQ, RAX, Address(RDI, 0));
        x64->op(MOVQ, RBX, 1);
        
        x64->op(MOVQ, RCX, 2);
        x64->preappend_array_RAX_RBX_RCX();
        
        x64->op(MOVQ, Address(RDI, 0), RAX);  // RDI no longer needed

        x64->op(LEA, RDI, x64->array_elems_address(RAX));
        x64->op(ADDQ, RDI, x64->array_length_address(RAX));
        x64->op(ADDQ, RDI, x64->array_length_address(RAX));  // Yes, added twice

        x64->op(MOVW, Address(RDI, 0), DX);
            
        x64->op(ADDQ, x64->array_length_address(RAX), 1);

        x64->op(RET);
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
        Label es_label = x64->once(compile_enum_streamification);

        compile_and_store_both(x64, Storage(STACK), Storage(ALISTACK));
        
        EnumerationType *t = dynamic_cast<EnumerationType *>(left->ts.rvalue()[0]);
        x64->op(LEARIP, RBX, t->stringifications_label);  // table start
        x64->op(CALL, es_label);
        
        right->ts.store(rs, Storage(), x64);
        left->ts.store(ls, Storage(), x64);
        
        return Storage();
    }
    
    static void compile_enum_streamification(X64 *x64) {
        // RAX - target array, RBX - table start, RCX - size, RDX - source enum, RDI - alias
        x64->op(MOVQ, RDI, Address(RSP, ADDRESS_SIZE));  // alias to the stream reference
        x64->op(MOVQ, RDX, Address(RSP, ADDRESS_SIZE + ALIAS_SIZE));  // the enum

        // Find the string for this enum value
        x64->op(ANDQ, RDX, 0xFF);
        x64->op(MOVQ, RDX, Address(RBX, RDX, ADDRESS_SIZE, 0));
            
        x64->op(MOVQ, RAX, Address(RDI, 0));
        x64->op(MOVQ, RBX, x64->array_length_address(RDX));
        x64->op(MOVQ, RCX, CHARACTER_SIZE);
        
        x64->preappend_array_RAX_RBX_RCX();
        
        x64->op(MOVQ, Address(RDI, 0), RAX);  // RDI no longer needed

        x64->op(LEA, RDI, x64->array_elems_address(RAX));
        x64->op(ADDQ, RDI, x64->array_length_address(RAX));
        x64->op(ADDQ, RDI, x64->array_length_address(RAX));  // Yes, added twice

        x64->op(LEA, RSI, x64->array_elems_address(RDX));
        x64->op(MOVQ, RCX, x64->array_length_address(RDX));
        x64->op(ADDQ, x64->array_length_address(RAX), RCX);
        x64->op(IMUL3Q, RCX, RCX, CHARACTER_SIZE);
        
        x64->op(REPMOVSB);
        
        x64->op(RET);
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
    
    //Marker marker = scope->mark();
    CodeBlockValue *block = new CodeBlockValue(NULL);
    //block->set_marker(marker);
    
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
