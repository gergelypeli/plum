
class StringStreamificationValue: public GenericValue {
public:
    StringStreamificationValue(Value *p, TypeMatch &match)
        :GenericValue(CHARACTER_ARRAY_REFERENCE_LVALUE_TS, VOID_TS, p) {
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
        x64->op(MOVQ, RDI, Address(RSP, 8));  // alias to the stream reference
        x64->op(MOVQ, RDX, Address(RSP, 16));  // reference to the string
        
        x64->op(MOVQ, RAX, Address(RDI, 0));
        x64->op(MOVQ, RBX, x64->array_length_address(RDX));

        x64->op(MOVQ, RCX, 2);
        x64->preappend_array_RAX_RBX_RCX();
        
        x64->op(MOVQ, Address(RDI, 0), RAX);  // RDI no longer needed

        x64->op(LEA, RDI, x64->array_items_address(RAX));
        x64->op(ADDQ, RDI, x64->array_length_address(RAX));
        x64->op(ADDQ, RDI, x64->array_length_address(RAX));  // Yes, added twice

        x64->op(LEA, RSI, x64->array_items_address(RDX));
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
        :GenericValue(CHARACTER_ARRAY_REFERENCE_LVALUE_TS, VOID_TS, p) {
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
        x64->op(MOVQ, RDI, Address(RSP, 8));  // alias to the stream reference
        x64->op(MOVQ, RDX, Address(RSP, 16));  // the character

        x64->op(MOVQ, RAX, Address(RDI, 0));
        x64->op(MOVQ, RBX, 1);
        
        x64->op(MOVQ, RCX, 2);
        x64->preappend_array_RAX_RBX_RCX();
        
        x64->op(MOVQ, Address(RDI, 0), RAX);  // RDI no longer needed

        x64->op(LEA, RDI, x64->array_items_address(RAX));
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
        :GenericValue(CHARACTER_ARRAY_REFERENCE_LVALUE_TS, VOID_TS, p) {
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
        x64->op(MOVQ, RDI, Address(RSP, 8));  // alias to the stream reference
        x64->op(MOVQ, RDX, Address(RSP, 16));  // the enum

        // Find the string for this enum value
        x64->op(ANDQ, RDX, 0xFF);
        x64->op(SHLQ, RDX, 2);  // 32-bit relative offsets are stored in our table
        x64->op(ADDQ, RBX, RDX);  // entry start
        x64->op(MOVSXQ, RDX, Address(RBX, 0));  // offset to string
        x64->op(ADDQ, RDX, RBX);  // absolute address of string
            
        x64->op(MOVQ, RAX, Address(RDI, 0));
        x64->op(MOVQ, RBX, x64->array_length_address(RDX));
        x64->op(MOVQ, RCX, 2);
        
        x64->preappend_array_RAX_RBX_RCX();
        
        x64->op(MOVQ, Address(RDI, 0), RAX);  // RDI no longer needed

        x64->op(LEA, RDI, x64->array_items_address(RAX));
        x64->op(ADDQ, RDI, x64->array_length_address(RAX));
        x64->op(ADDQ, RDI, x64->array_length_address(RAX));  // Yes, added twice

        x64->op(LEA, RSI, x64->array_items_address(RDX));
        x64->op(MOVQ, RCX, x64->array_length_address(RDX));
        x64->op(ADDQ, x64->array_length_address(RAX), RCX);
        x64->op(SHLQ, RCX, 1);
        
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
    //CodeScope *s = new CodeScope;
    //scope->add(s);
    //scope = s;
    
    Marker marker = scope->mark();
    CodeBlockValue *block = new CodeBlockValue(NULL);
    block->set_marker(marker);
    
    DeclarationValue *dv = make_declaration_by_value("<interpolated>", new StringBufferValue(100), scope);
    //DeclarationValue *dv = new DeclarationValue("<result>");
    //Value *initial_value = new StringBufferValue(100);
    //dv->use(initial_value, scope);
    Variable *v = dv->get_var();
    block->add_statement(dv);

    for (auto &kv : expr->kwargs) {
        std::string keyword = kv.first;
        Expr *expr = kv.second.get();
        Value *keyword_value = typize(expr, scope);
        //DeclarationValue *decl_value = new DeclarationValue(keyword);
        //decl_value->use(keyword_value, scope);
        DeclarationValue *decl_value = make_declaration_by_value(keyword, keyword_value, scope);
        block->add_statement(decl_value);
    }

    bool identifier = false;
    Expr streamify_expr(Expr::IDENTIFIER, expr->token, "streamify");
    streamify_expr.add_arg(new Expr(Expr::IDENTIFIER, expr->token, "<interpolated>"));
    
    for (auto &fragment : fragments) {
        Value *pivot;
        TypeMatch match;
        
        if (identifier) {
            // For explicit keywords, we only look up in the innermost scope.
            // For identifiers, we look up outer scopes, but we don't need to look
            // in inner scopes, because that would need a pivot value, which we don't have.
            
            for (Scope *s = scope; s; s = s->outer_scope) {
                pivot = s->lookup(fragment, NULL, match);
        
                if (pivot)
                    break;
                else if (expr->kwargs.size() > 0)
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

        Value *streamify = lookup("streamify", pivot, &streamify_expr, scope);
        if (!streamify) {
            std::cerr << "Cannot interpolate unstreamifiable " << pivot->ts << "!\n";
            throw TYPE_ERROR;
        }

        block->add_statement(streamify);
        identifier = !identifier;
    }

    Value *ret = make_variable_value(v, NULL);
    TypeMatch match;  // kinda unnecessary
    ret = new ArrayReallocValue(TWEAK, ret, match);
    block->add_statement(ret, true);
    
    return make_code_value(block);
}
