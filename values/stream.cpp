
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
        Label ss_label = x64->once->compile(compile_string_streamification);

        compile_and_store_both(x64, Storage(STACK), Storage(ALISTACK));
        
        x64->op(CALL, ss_label);
        
        right->ts.store(rs, Storage(), x64);
        left->ts.store(ls, Storage(), x64);
        
        return Storage();
    }
    
    static void compile_string_streamification(Label label, X64 *x64) {
        // RAX - target array, RBX - tmp, RCX - size, RDX - source array, RDI - alias
        Label preappend_array = x64->once->compile(compile_array_preappend, CHARACTER_TS);
        
        x64->code_label_local(label, "string_streamification");
        
        x64->op(MOVQ, RDI, Address(RSP, ADDRESS_SIZE));  // alias to the stream reference
        x64->op(MOVQ, RDX, Address(RSP, ADDRESS_SIZE + ALIAS_SIZE));  // reference to the string
        
        x64->op(MOVQ, RAX, Address(RDI, 0));
        x64->op(MOVQ, RBX, Address(RDX, ARRAY_LENGTH_OFFSET));

        x64->op(CALL, preappend_array);
        
        x64->op(MOVQ, Address(RDI, 0), RAX);  // RDI no longer needed

        x64->op(LEA, RDI, Address(RAX, ARRAY_ELEMS_OFFSET));
        x64->op(ADDQ, RDI, Address(RAX, ARRAY_LENGTH_OFFSET));
        x64->op(ADDQ, RDI, Address(RAX, ARRAY_LENGTH_OFFSET));  // Yes, added twice

        x64->op(LEA, RSI, Address(RDX, ARRAY_ELEMS_OFFSET));
        x64->op(MOVQ, RCX, Address(RDX, ARRAY_LENGTH_OFFSET));
        x64->op(ADDQ, Address(RAX, ARRAY_LENGTH_OFFSET), RCX);
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
        Label cs_label = x64->once->compile(compile_character_streamification);

        compile_and_store_both(x64, Storage(STACK), Storage(ALISTACK));
        
        x64->op(CALL, cs_label);
        
        right->ts.store(rs, Storage(), x64);
        left->ts.store(ls, Storage(), x64);
        
        return Storage();
    }
    
    static void compile_character_streamification(Label label, X64 *x64) {
        // RAX - target array, RBX - tmp, RCX - size, RDX - source character, RDI - alias
        Label preappend_array = x64->once->compile(compile_array_preappend, CHARACTER_TS);

        x64->code_label_local(label, "character_streamification");

        x64->op(MOVQ, RDI, Address(RSP, ADDRESS_SIZE));  // alias to the stream reference
        x64->op(MOVQ, RDX, Address(RSP, ADDRESS_SIZE + ALIAS_SIZE));  // the character

        x64->op(MOVQ, RAX, Address(RDI, 0));
        x64->op(MOVQ, RBX, 1);
        
        x64->op(CALL, preappend_array);
        
        x64->op(MOVQ, Address(RDI, 0), RAX);  // RDI no longer needed

        x64->op(LEA, RDI, Address(RAX, ARRAY_ELEMS_OFFSET));
        x64->op(ADDQ, RDI, Address(RAX, ARRAY_LENGTH_OFFSET));
        x64->op(ADDQ, RDI, Address(RAX, ARRAY_LENGTH_OFFSET));  // Yes, added twice

        x64->op(MOVW, Address(RDI, 0), DX);
            
        x64->op(ADDQ, Address(RAX, ARRAY_LENGTH_OFFSET), 1);

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
        Label es_label = x64->once->compile(compile_enum_streamification);

        compile_and_store_both(x64, Storage(STACK), Storage(ALISTACK));
        
        EnumerationType *t = dynamic_cast<EnumerationType *>(left->ts.rvalue()[0]);
        x64->op(LEARIP, RBX, t->get_stringifications_label(x64));  // table start
        x64->op(CALL, es_label);
        
        right->ts.store(rs, Storage(), x64);
        left->ts.store(ls, Storage(), x64);
        
        return Storage();
    }
    
    static void compile_enum_streamification(Label label, X64 *x64) {
        // RAX - target array, RBX - table start, RCX - size, RDX - source enum, RDI - alias
        Label preappend_array = x64->once->compile(compile_array_preappend, CHARACTER_TS);

        x64->code_label_local(label, "enum_streamification");

        x64->op(MOVQ, RDI, Address(RSP, ADDRESS_SIZE));  // alias to the stream reference
        x64->op(MOVQ, RDX, Address(RSP, ADDRESS_SIZE + ALIAS_SIZE));  // the enum

        // Find the string for this enum value
        x64->op(ANDQ, RDX, 0xFF);
        x64->op(MOVQ, RDX, Address(RBX, RDX, ADDRESS_SIZE, 0));
            
        x64->op(MOVQ, RAX, Address(RDI, 0));
        x64->op(MOVQ, RBX, Address(RDX, ARRAY_LENGTH_OFFSET));

        x64->op(CALL, preappend_array);
        
        x64->op(MOVQ, Address(RDI, 0), RAX);  // RDI no longer needed

        x64->op(LEA, RDI, Address(RAX, ARRAY_ELEMS_OFFSET));
        x64->op(ADDQ, RDI, Address(RAX, ARRAY_LENGTH_OFFSET));
        x64->op(ADDQ, RDI, Address(RAX, ARRAY_LENGTH_OFFSET));  // Yes, added twice

        x64->op(LEA, RSI, Address(RDX, ARRAY_ELEMS_OFFSET));
        x64->op(MOVQ, RCX, Address(RDX, ARRAY_LENGTH_OFFSET));
        x64->op(ADDQ, Address(RAX, ARRAY_LENGTH_OFFSET), RCX);
        x64->op(IMUL3Q, RCX, RCX, CHARACTER_SIZE);
        
        x64->op(REPMOVSB);
        
        x64->op(RET);
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
        Label os_label = x64->once->compile(compile_option_streamification, some_ts);

        compile_and_store_both(x64, Storage(STACK), Storage(ALISTACK));
        
        x64->op(CALL, os_label);
        
        right->ts.store(rs, Storage(), x64);
        left->ts.store(ls, Storage(), x64);
        
        return Storage();
    }
    
    static void compile_option_streamification(Label label, TypeSpec some_ts, X64 *x64) {
        Label string_streamification_label = x64->once->compile(StringStreamificationValue::compile_string_streamification);
        Label some;
        
        x64->code_label_local(label, "x_option_streamification");

        x64->op(CMPQ, Address(RSP, ADDRESS_SIZE + ALIAS_SIZE), 0);  // the flag
        x64->op(JNE, some);
        
        // `none
        Label none_label = x64->data_heap_string(decode_utf8("`none"));
        x64->op(LEARIP, RBX, none_label);
        x64->op(PUSHQ, RBX);
        x64->op(PUSHQ, Address(RSP, ADDRESS_SIZE + ADDRESS_SIZE));
        x64->op(CALL, string_streamification_label);
        x64->op(ADDQ, RSP, 16);
        x64->op(RET);

        // `some
        x64->code_label(some);
        Label some_label = x64->data_heap_string(decode_utf8("`some"));
        x64->op(LEARIP, RBX, some_label);
        x64->op(PUSHQ, RBX);
        x64->op(PUSHQ, Address(RSP, ADDRESS_SIZE + ADDRESS_SIZE));
        x64->op(CALL, string_streamification_label);
        x64->op(ADDQ, RSP, 16);
        x64->op(RET);
        
        // FIXME: print the some value, too!
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
