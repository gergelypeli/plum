#include "../plum.h"


// During the streamification process the stream aliases are on the stack. As the stream
// reference is created on the stack, if the stack is relocated, these aliases would point
// to the old stack. Built-in streamifications that invoke custom functions must make
// sure to keep their stream alias updated from the custom function's fixable argument,
// so the relocated alias is passed on to the callers.

void stream_preappend2(Address alias_addr, Cx *cx) {
    Label grow_label = cx->once->compile(compile_array_grow, CHARACTER_TS);
    Storage ref_storage(ALIAS, alias_addr, 0);

    container_preappend2(LINEARRAY_RESERVATION_OFFSET, LINEARRAY_LENGTH_OFFSET, grow_label, ref_storage, cx);
}


void streamify_ascii(std::string s, Address alias_addr, Cx *cx) {
    unsigned n = s.size();
    
    cx->op(MOVQ, R10, n);

    stream_preappend2(alias_addr, cx);
    
    cx->op(MOVQ, RCX, Address(RAX, LINEARRAY_LENGTH_OFFSET));
    Address tail_address = Address(RAX, RCX, Address::SCALE_2, LINEARRAY_ELEMS_OFFSET);

    for (unsigned i = 0; i + 1 < n; i += 2) {
        int dw = (s[i] | (s[i + 1] << 16));

        cx->op(MOVD, tail_address + i * CHARACTER_SIZE, dw);
    }
    
    if (n % 2) {
        int dw = s[n - 1];
        
        cx->op(MOVW, tail_address + (n - 1) * CHARACTER_SIZE, dw);
    }
    
    cx->op(ADDQ, Address(RAX, LINEARRAY_LENGTH_OFFSET), n);
}



InterpolationValue::InterpolationValue(std::vector<std::ustring> f, Token t)
    :Value(STRING_TS) {
    fragments = f;
    set_token(t);
}

bool InterpolationValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
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
            
            if (pivot->ts[0] == lvalue_type)
                pivot = make<RvalueCastValue>(pivot);
        }
        else {
            pivot = make<StringLiteralValue>(fragment)->lookup_inner("raw", scope);
        }
    
        components.push_back(std::unique_ptr<Value>(pivot));
        
        is_identifier = !is_identifier;
    }
    
    return true;
}

Regs InterpolationValue::precompile(Regs preferred) {
    for (auto &c : components)
        c->precompile_tail();
        
    return Regs::all();
}

Storage InterpolationValue::compile(Cx *cx) {
    Label alloc_array = cx->once->compile(compile_array_alloc, CHARACTER_TS);
    Label realloc_array = cx->once->compile(compile_array_realloc, CHARACTER_TS);
    
    cx->op(MOVQ, R10, 100);  // TODO
    cx->op(CALL, alloc_array);  // clobbers all
    cx->op(PUSHQ, RAX);
    
    for (auto &c : components) {
        c->compile_and_store(cx, Storage(STACK));
        
        cx->op(LEA, R10, Address(RSP, c->ts.measure_stack()));
        cx->op(PUSHQ, 0);
        cx->op(PUSHQ, R10);
        
        c->streamify(cx);
        
        cx->op(ADDQ, RSP, ALIAS_SIZE);
        c->ts.store(Storage(STACK), Storage(), cx);
    }

    // shrink to fit
    cx->op(POPQ, RAX);
    cx->op(MOVQ, R10, Address(RAX, LINEARRAY_LENGTH_OFFSET));
    cx->op(CALL, realloc_array);

    return Storage(REGISTER, RAX);
}
