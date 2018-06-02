

class StringRegexpMatcherValue: public GenericValue, public Raiser {
public:
    StringRegexpMatcherValue(Value *l, TypeMatch &match)
        :GenericValue(STRING_TS, STRING_ARRAY_REF_TS, l) {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!check_raise(match_unmatched_exception_type, scope))
            return false;
        
        return GenericValue::check(args, kwargs, scope);
    }

    virtual Regs precompile(Regs preferred) {
        left->precompile(preferred);
        right->precompile(preferred);
        return Regs::all();
    }

    virtual Storage compile(X64 *x64) {
        compile_and_store_both(x64, Storage(STACK), Storage(STACK));
        Label ok;

        x64->op(MOVQ, RDI, Address(RSP, ADDRESS_SIZE));
        x64->op(MOVQ, RSI, Address(RSP, 0));
        
        // This uses SSE instructions, so SysV stack alignment must be ensured
        x64->runtime->call_sysv(x64->runtime->sysv_string_regexp_match_label);
        
        x64->op(POPQ, RBX);
        x64->runtime->decref(RBX);
        x64->op(POPQ, RBX);
        x64->runtime->decref(RBX);
        
        x64->op(CMPQ, RAX, 0);
        x64->op(JNE, ok);
        
        raise("UNMATCHED", x64);
        
        x64->code_label(ok);

        return Storage(REGISTER, RAX);
    }
};


class SliceEmptyValue: public GenericValue {
public:
    SliceEmptyValue(TypeMatch &match)
        :GenericValue(NO_TS, match[1].prefix(slice_type), NULL) {
    }

    virtual Regs precompile(Regs preferred) {
        return Regs();
    }

    virtual Storage compile(X64 *x64) {
        x64->op(LEA, RBX, Address(x64->runtime->empty_array_label, 0));
        x64->runtime->incweakref(RBX);
        
        x64->op(PUSHQ, 0);  // length
        x64->op(PUSHQ, 0);  // front
        x64->op(PUSHQ, RBX);  // weakref
        
        return Storage(STACK);
    }
};


class SliceAllValue: public GenericValue {
public:
    SliceAllValue(TypeMatch &match)
        :GenericValue(match[1].prefix(array_type).prefix(weakref_type), match[1].prefix(slice_type), NULL) {
    }

    virtual Regs precompile(Regs preferred) {
        return right->precompile(preferred);
    }

    virtual Storage compile(X64 *x64) {
        Storage rs = right->compile(x64);
        Register r;
        
        switch (rs.where) {
        case REGISTER:
            r = rs.reg;
            break;
        case STACK:
            x64->op(POPQ, RBX);
            r = RBX;
            break;
        case MEMORY:
            x64->op(MOVQ, RBX, rs.address);
            x64->runtime->incweakref(RBX);
            r = RBX;
            break;
        default:
            throw INTERNAL_ERROR;
        }
        
        x64->op(PUSHQ, Address(r, ARRAY_LENGTH_OFFSET));  // length
        x64->op(PUSHQ, 0);  // front
        x64->op(PUSHQ, r);  // weakref
        
        return Storage(STACK);
    }
};


class ArraySliceValue: public Value, public Raiser {
public:
    std::unique_ptr<Value> array_value;
    std::unique_ptr<Value> front_value;
    std::unique_ptr<Value> length_value;
    Register reg;
    
    ArraySliceValue(Value *pivot, TypeMatch &match)
        :Value(match[1].prefix(slice_type)) {
        
        array_value.reset(pivot);
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!check_raise(lookup_exception_type, scope))
            return false;

        ArgInfos infos = {
            { "front", &INTEGER_TS, scope, &front_value },
            { "length", &INTEGER_TS, scope, &length_value }
        };
        
        if (!check_arguments(args, kwargs, infos))
            return false;

        return true;
    }

    virtual Regs precompile(Regs preferred) {
        return array_value->precompile(preferred) | front_value->precompile(preferred) | length_value->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        array_value->compile_and_store(x64, Storage(STACK));
        front_value->compile_and_store(x64, Storage(STACK));
        length_value->compile_and_store(x64, Storage(STACK));
        
        x64->op(POPQ, RCX);  // length
        x64->op(POPQ, RBX);  // front
        x64->op(POPQ, RAX);  // weakref
        x64->op(MOVQ, RDX, Address(RAX, ARRAY_LENGTH_OFFSET));
        
        Label ok, nok;
        x64->op(CMPQ, RBX, RDX);
        x64->op(JAE, nok);
        
        x64->op(SUBQ, RDX, RBX);
        x64->op(CMPQ, RCX, RDX);
        x64->op(JB, ok);
        
        x64->code_label(nok);
        x64->runtime->decweakref(RAX);

        raise("NOT_FOUND", x64);
        
        x64->code_label(ok);
        x64->op(PUSHQ, RCX);
        x64->op(PUSHQ, RBX);
        x64->op(PUSHQ, RAX);
        
        return Storage(STACK);
    }
};


class SliceIndexValue: public GenericValue, public Raiser {
public:
    TypeSpec elem_ts;
    Borrow *borrow;
    
    SliceIndexValue(Value *pivot, TypeMatch &match)
        :GenericValue(INTEGER_TS, match[1].lvalue(), pivot) {
        elem_ts = match[1];
        borrow = NULL;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!check_raise(lookup_exception_type, scope))
            return false;
        
        borrow = new Borrow;
        scope->add(borrow);
        
        return GenericValue::check(args, kwargs, scope);
    }

    virtual Regs precompile(Regs preferred) {
        return left->precompile(preferred) | right->precompile(preferred) | Regs(RAX, RCX, RDX);
    }

    virtual Storage compile(X64 *x64) {
        int elem_size = elem_ts.measure_elem();
    
        // TODO: MEMORY pivot can be much more optimal
        left->compile_and_store(x64, Storage(STACK));
        right->compile_and_store(x64, Storage(STACK));
        
        x64->op(POPQ, RAX);  // index
        x64->op(POPQ, RBX);  // weakref
        x64->op(POPQ, RCX);  // front
        x64->op(POPQ, RDX);  // length
        
        x64->op(MOVQ, borrow->get_address(), RBX);  // defer decweakref
        
        Label ok;
        x64->op(CMPQ, RAX, RDX);
        x64->op(JB, ok);
        
        raise("NOT_FOUND", x64);
        
        x64->code_label(ok);
        x64->op(ADDQ, RAX, RCX);
        x64->op(IMUL3Q, RAX, RAX, elem_size);
        x64->op(LEA, RAX, Address(RAX, RBX, ARRAY_ELEMS_OFFSET));
    
        return Storage(MEMORY, Address(RAX, 0));
    }
};
