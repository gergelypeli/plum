
class StringBufferValue: public Value {
public:
    int length;
    
    StringBufferValue(int l)
        :Value(STRING_TS) {
        length = l;
    }

    virtual Regs precompile(Regs preferred) {
        return Regs::all();
    }

    virtual Storage compile(X64 *x64) {
        Label alloc_array = x64->once->compile(compile_array_alloc, CHARACTER_TS);
        
        x64->op(MOVQ, R10, length);
        x64->op(CALL, alloc_array);  // clobbers all
        //x64->op(PUSHQ, RAX);
        
        //return Storage(STACK);
        return Storage(REGISTER, RAX);
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
        // Let the pivot be borrowed if possible
        ls = left->compile(x64);
        
        if (ls.where == BREGISTER || ls.where == BSTACK || ls.where == BMEMORY)
            ls = left->ts.store(ls, Storage(BSTACK), x64);
        else
            ls = left->ts.store(ls, Storage(STACK), x64);
        
        x64->unwind->push(this);
        
        right->compile_and_store(x64, Storage(ALISTACK));
        rs = Storage(ALISTACK);
        
        x64->unwind->pop(this);
        
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


Value *make_initialization_by_value(std::string name, Value *v, CodeScope *statement_scope) {
    DeclarationValue *dv = new DeclarationValue(name);
    dv->fix_bare(v->ts, statement_scope->outer_scope);  // save ourselves an escape operation
    
    TypeMatch tm = { dv->ts, dv->ts.unprefix(uninitialized_type) };
    CreateValue *cv = new CreateValue(dv, tm);
    cv->use(v);
    
    Value *vcv = new VoidConversionValue(cv);
    
    CodeScopeValue *csv = new CodeScopeValue(vcv, statement_scope);
    statement_scope->leave();
    
    return csv;
}


Value *interpolate(std::string text, Expr *expr, Scope *scope) {
    std::vector<std::string> fragments = brace_split(text);
    
    // We must scope ourselves
    CodeScope *interpolation_scope = new CodeScope;
    scope->add(interpolation_scope);
    
    InterpolationValue *interpolation = new InterpolationValue;
    
    CodeScope *statement_scope = new CodeScope;
    interpolation_scope->add(statement_scope);
    Value *cv = make_initialization_by_value("<interpolated>", new StringBufferValue(100), statement_scope);
    Variable *interpolated_var = ptr_cast<Variable>(interpolation_scope->contents.back().get());
    interpolation->add_statement(cv);

    bool pseudo_only = (expr->args.size() > 0 || expr->kwargs.size() > 0);
    bool identifier = false;
    int position = 0;
    
    for (auto &fragment : fragments) {
        Value *pivot = NULL;
        
        if (identifier) {
            // NOTE: this assumes that character initializers are uppercase!
            if (fragment.size() > 0 && isupper(fragment[0])) {
                pivot = STRING_TS.lookup_initializer(fragment, interpolation_scope);
                
                if (!pivot) {
                    std::cerr << "Undefined interpolation constant " << fragment << "!\n";
                    throw TYPE_ERROR;
                }
            }
            else if (pseudo_only) {
                if (fragment.size()) {
                    Expr *e = expr->kwargs[fragment].get();
                    
                    if (!e) {
                        std::cerr << "No interpolation argument " << fragment << "!\n";
                        throw TYPE_ERROR;
                    }

                    pivot = typize(e, interpolation_scope, &ANY_TS);

                    if (!pivot) {
                        std::cerr << "Undefined interpolation argument " << fragment << "!\n";
                        throw TYPE_ERROR;
                    }
                }
                else {
                    Expr *e = expr->args[position].get();

                    if (!e) {
                        std::cerr << "No interpolation argument " << position << "!\n";
                        throw TYPE_ERROR;
                    }
                    
                    pivot = typize(e, interpolation_scope, &ANY_TS);

                    if (!pivot) {
                        std::cerr << "Undefined interpolation argument" << position << "!\n";
                        throw TYPE_ERROR;
                    }

                    // NOTE: in C++ 'bool += 1' is legal, and does not even generate a warning
                    position += 1;
                }
            }
            else {
                if (!fragment.size()) {
                    std::cerr << "Invalid positional substring in interpolation!\n";
                    throw TYPE_ERROR;
                }
                
                // For identifiers, we look up outer scopes, but we don't need to look
                // in inner scopes, because that would need a pivot value, which we don't have.
                for (Scope *s = interpolation_scope; s && (s->type == CODE_SCOPE || s->type == FUNCTION_SCOPE); s = s->outer_scope) {
                    pivot = s->lookup(fragment, NULL, interpolation_scope);
        
                    if (pivot)
                        break;
                }
            
                if (!pivot) {
                    std::cerr << "Undefined interpolation argument " << fragment << "!\n";
                    throw TYPE_ERROR;
                }
            }
        }
        else {
            pivot = make<StringLiteralValue>(fragment);
        }

        TypeMatch match;
        Value *streamify = NULL;
        
        if (typematch(STREAMIFIABLE_TS, pivot, match)) {
            streamify = lookup_fake("streamify", pivot, interpolation_scope, expr->token, NULL, interpolated_var);
        }
        else if (pivot->ts.rvalue()[0] == ref_type || pivot->ts.rvalue()[0] == ptr_type) {
            // Complimentary streamification of references and pointers
            streamify = lookup_fake("<streamify>", pivot, interpolation_scope, expr->token, NULL, interpolated_var);
        }

        if (!streamify) {
            std::cerr << "Cannot interpolate unstreamifiable " << pivot->ts << "!\n";
            throw TYPE_ERROR;
        }

        interpolation->add_statement(streamify);
        identifier = !identifier;
    }

    TypeMatch match;  // kinda unnecessary
    Value *ret = make<VariableValue>(interpolated_var, (Value *)NULL, interpolation_scope, match);
    ret = ret->lookup_inner("realloc", interpolation_scope);  // FIXME: missing check, but at least no arguments
    interpolation->add_statement(ret);

    Value *result = make<CodeScopeValue>(interpolation, interpolation_scope);
    interpolation_scope->leave();
    
    return result;
}
