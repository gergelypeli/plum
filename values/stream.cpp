
class InterpolationValue: public Value {
public:
    std::string pattern;
    std::unique_ptr<Value> buffer;
    std::vector<std::unique_ptr<Value>> components;

    InterpolationValue(std::string p)
        :Value(STRING_TS) {
        pattern = p;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        std::vector<std::string> fragments = brace_split(pattern);
        bool pseudo_only = (args.size() > 0 || kwargs.size() > 0);
        bool identifier = false;
        int position = 0;
    
        for (auto &fragment : fragments) {
            Value *pivot = NULL;
        
            if (identifier) {
                // NOTE: this assumes that character initializers are uppercase!
                if (fragment.size() > 0 && isupper(fragment[0])) {
                    pivot = STRING_TS.lookup_initializer(fragment, scope);
                
                    if (!pivot) {
                        std::cerr << "Undefined interpolation constant " << fragment << "!\n";
                        throw TYPE_ERROR;
                    }
                }
                else if (pseudo_only) {
                    if (fragment.size()) {
                        Expr *e = kwargs[fragment].get();
                    
                        if (!e) {
                            std::cerr << "No interpolation argument " << fragment << "!\n";
                            throw TYPE_ERROR;
                        }

                        pivot = typize(e, scope, &ANY_TS);

                        if (!pivot) {
                            std::cerr << "Undefined interpolation argument " << fragment << "!\n";
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
                    if (!fragment.size()) {
                        std::cerr << "Invalid positional substring in interpolation!\n";
                        throw TYPE_ERROR;
                    }
                
                    // For identifiers, we look up outer scopes, but we don't need to look
                    // in inner scopes, because that would need a pivot value, which we don't have.
                    for (Scope *s = scope; s && (s->type == CODE_SCOPE || s->type == FUNCTION_SCOPE); s = s->outer_scope) {
                        pivot = s->lookup(fragment, NULL, scope);
        
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
                pivot = make<StringLiteralValue>(fragment)->lookup_inner("raw", scope);
            }
        
            components.push_back(std::unique_ptr<Value>(pivot));
            
            identifier = !identifier;
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
