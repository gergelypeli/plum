
class InterpolationValue: public Value {
public:
    std::vector<std::ustring> fragments;
    std::unique_ptr<Value> buffer;
    std::vector<std::unique_ptr<Value>> components;

    InterpolationValue(std::vector<std::ustring> f, Token t)
        :Value(STRING_TS) {
        fragments = f;
        set_token(t);
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        bool pseudo_only = (args.size() > 0 || kwargs.size() > 0);
        bool is_identifier = false;
        int position = 0;
    
        for (auto &fragment : fragments) {
            Value *pivot = NULL;
        
            if (is_identifier) {
                std::string kw = encode_ascii(fragment);
                
                if (kw.empty() && !fragment.empty()) {
                    std::cerr << "Interpolation keyword not ASCII: " << token << "\n";
                    return false;
                }

                if (pseudo_only) {
                    if (kw.size()) {
                        Expr *e = kwargs[kw].get();
                    
                        if (!e) {
                            std::cerr << "No interpolation argument " << kw << "!\n";
                            throw TYPE_ERROR;
                        }

                        pivot = typize(e, scope, &ANY_TS);

                        if (!pivot) {
                            std::cerr << "Undefined interpolation argument " << kw << "!\n";
                            throw TYPE_ERROR;
                        }
                    }
                    else {
                        Expr *e = args[position].get();

                        if (!e) {
                            std::cerr << "No interpolation argument " << position << "!\n";
                            throw TYPE_ERROR;
                        }
                    
                        pivot = typize(e, scope, &ANY_TS);

                        if (!pivot) {
                            std::cerr << "Undefined interpolation argument" << position << "!\n";
                            throw TYPE_ERROR;
                        }

                        // NOTE: in C++ 'bool += 1' is legal, and does not even generate a warning
                        position += 1;
                    }
                }
                else {
                    if (!kw.size()) {
                        std::cerr << "Invalid positional substring in interpolation!\n";
                        throw TYPE_ERROR;
                    }
                
                    // For identifiers, we look up outer scopes, but we don't need to look
                    // in inner scopes, because that would need a pivot value, which we don't have.
                    for (Scope *s = scope; s && (s->type == CODE_SCOPE || s->type == FUNCTION_SCOPE); s = s->outer_scope) {
                        pivot = s->lookup(kw, NULL, scope);
        
                        if (pivot)
                            break;
                    }
            
                    if (!pivot) {
                        std::cerr << "Undefined interpolation argument " << kw << "!\n";
                        throw TYPE_ERROR;
                    }
                }
            }
            else {
                pivot = make<StringLiteralValue>(fragment)->lookup_inner("raw", scope);
            }
        
            components.push_back(std::unique_ptr<Value>(pivot));
            
            is_identifier = !is_identifier;
        }
        
        return true;
    }

    virtual Regs precompile(Regs preferred) {
        if (buffer)
            buffer->precompile(preferred);

        for (auto &c : components)
            c->precompile(preferred);
            
        return Regs::all();
    }

    virtual Storage compile(X64 *x64) {
        Label alloc_array = x64->once->compile(compile_array_alloc, CHARACTER_TS);
        Label realloc_array = x64->once->compile(compile_array_realloc, CHARACTER_TS);
        
        x64->op(MOVQ, R10, 100);  // TODO
        x64->op(CALL, alloc_array);  // clobbers all
        x64->op(PUSHQ, RAX);
        
        for (auto &c : components) {
            c->compile_and_store(x64, Storage(STACK));
            
            x64->op(LEA, R10, Address(RSP, c->ts.measure_stack()));
            x64->op(PUSHQ, R10);
            
            c->streamify(x64);
            
            x64->op(ADDQ, RSP, ADDRESS_SIZE);
            c->ts.store(Storage(STACK), Storage(), x64);
        }

        // shrink to fit
        x64->op(POPQ, RAX);
        x64->op(MOVQ, R10, Address(RAX, ARRAY_LENGTH_OFFSET));
        x64->op(CALL, realloc_array);

        return Storage(REGISTER, RAX);
    }
};
