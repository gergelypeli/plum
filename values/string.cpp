

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

        right->ts.store(Storage(STACK), Storage(), x64);
        left->ts.store(Storage(STACK), Storage(), x64);
        
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
        x64->op(LEA, R10, Address(x64->runtime->empty_array_label, 0));
        x64->runtime->incref(R10);
        
        x64->op(PUSHQ, 0);  // length
        x64->op(PUSHQ, 0);  // front
        x64->op(PUSHQ, R10);  // ptr
        
        return Storage(STACK);
    }
};


class SliceAllValue: public GenericValue {
public:
    SliceAllValue(TypeMatch &match)
        :GenericValue(match[1].prefix(array_type).prefix(ptr_type), match[1].prefix(slice_type), NULL) {
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
            x64->op(POPQ, R10);
            r = R10;
            break;
        case MEMORY:
            x64->op(MOVQ, R10, rs.address);
            x64->runtime->incref(R10);
            r = R10;
            break;
        default:
            throw INTERNAL_ERROR;
        }
        
        x64->op(PUSHQ, Address(r, ARRAY_LENGTH_OFFSET));  // length
        x64->op(PUSHQ, 0);  // front
        x64->op(PUSHQ, r);  // ptr
        
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
        return array_value->precompile(preferred) | front_value->precompile(preferred) | length_value->precompile(preferred) | Regs(RAX, RCX, RDX);
    }
    
    virtual Storage compile(X64 *x64) {
        array_value->compile_and_store(x64, Storage(STACK));
        front_value->compile_and_store(x64, Storage(STACK));
        length_value->compile_and_store(x64, Storage(STACK));
        
        x64->op(POPQ, RCX);  // length
        x64->op(POPQ, R10);  // front
        x64->op(POPQ, RAX);  // ptr
        x64->op(MOVQ, RDX, Address(RAX, ARRAY_LENGTH_OFFSET));
        
        Label ok, nok;
        x64->op(CMPQ, R10, RDX);
        x64->op(JAE, nok);
        
        x64->op(SUBQ, RDX, R10);
        x64->op(CMPQ, RCX, RDX);
        x64->op(JBE, ok);
        
        x64->code_label(nok);

        // all popped
        x64->runtime->decref(RAX);
        raise("NOT_FOUND", x64);
        
        x64->code_label(ok);
        x64->op(PUSHQ, RCX);
        x64->op(PUSHQ, R10);
        x64->op(PUSHQ, RAX);  // inherit reference
        
        return Storage(STACK);
    }
};


class SliceSliceValue: public Value, public Raiser {
public:
    std::unique_ptr<Value> slice_value;
    std::unique_ptr<Value> front_value;
    std::unique_ptr<Value> length_value;
    Register reg;
    
    SliceSliceValue(Value *pivot, TypeMatch &match)
        :Value(match[0]) {
        
        slice_value.reset(pivot);
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
        return slice_value->precompile(preferred) | front_value->precompile(preferred) | length_value->precompile(preferred) | Regs(RAX, RCX, RDX);
    }
    
    virtual Storage compile(X64 *x64) {
        slice_value->compile_and_store(x64, Storage(STACK));
        front_value->compile_and_store(x64, Storage(STACK));
        length_value->compile_and_store(x64, Storage(STACK));
        
        x64->op(POPQ, RCX);  // length
        x64->op(POPQ, R10);  // front
        x64->op(MOVQ, RDX, Address(RSP, ADDRESS_SIZE + INTEGER_SIZE));  // old length
        
        Label ok, nok;
        x64->op(CMPQ, R10, RDX);
        x64->op(JAE, nok);
        
        x64->op(SUBQ, RDX, R10);
        x64->op(CMPQ, RCX, RDX);
        x64->op(JBE, ok);
        
        x64->code_label(nok);
        ts.store(Storage(STACK), Storage(), x64);  // pop Slice
        raise("NOT_FOUND", x64);
        
        x64->code_label(ok);
        x64->op(ADDQ, Address(RSP, ADDRESS_SIZE), R10);  // adjust front
        x64->op(MOVQ, Address(RSP, ADDRESS_SIZE + INTEGER_SIZE), RCX);  // set length
        
        return Storage(STACK);
    }
};


class SliceIndexValue: public GenericValue, public Raiser {
public:
    TypeSpec elem_ts;
    Unborrow *unborrow;
    
    SliceIndexValue(Value *pivot, TypeMatch &match)
        :GenericValue(INTEGER_TS, match[1].lvalue(), pivot) {
        elem_ts = match[1];
        unborrow = NULL;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!check_raise(lookup_exception_type, scope))
            return false;
        
        // Borrow only if not raising
        unborrow = new Unborrow;
        scope->add(unborrow);
        
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
        x64->op(POPQ, R10);  // ptr
        x64->op(POPQ, RCX);  // front
        x64->op(POPQ, RDX);  // length

        Label ok;
        x64->op(CMPQ, RAX, RDX);
        x64->op(JB, ok);

        // all popped
        x64->runtime->decref(R10);
        raise("NOT_FOUND", x64);
        
        x64->code_label(ok);
        x64->op(ADDQ, RAX, RCX);
        x64->op(IMUL3Q, RAX, RAX, elem_size);
        x64->op(LEA, RAX, Address(RAX, R10, ARRAY_ELEMS_OFFSET));

        // Borrow Lvalue container
        x64->op(MOVQ, unborrow->get_address(), R10);
    
        return Storage(MEMORY, Address(RAX, 0));
    }
};


class SliceFindValue: public GenericValue, public Raiser {
public:
    TypeSpec slice_ts;
    TypeSpec elem_ts;
    
    SliceFindValue(Value *l, TypeMatch &match)
        :GenericValue(match[1], INTEGER_TS, l) {
        elem_ts = match[1];
        slice_ts = match[0];
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!check_raise(lookup_exception_type, scope))
            return false;

        return GenericValue::check(args, kwargs, scope);
    }

    virtual Regs precompile(Regs preferred) {
        return left->precompile(preferred) | right->precompile(preferred) | Regs(RAX, RCX, RDX) | COMPARE_CLOB;
    }

    virtual Storage compile(X64 *x64) {
        left->compile_and_store(x64, Storage(STACK));
        right->compile_and_store(x64, Storage(STACK));
        
        Label loop, check, found;
        int elem_size = array_elem_size(elem_ts);
        int stack_size = elem_ts.measure_stack();
    
        x64->op(MOVQ, RAX, Address(RSP, stack_size));

        x64->op(MOVQ, RCX, 0);
        x64->op(MOVQ, RDX, Address(RSP, stack_size + ADDRESS_SIZE));  // front index
        x64->op(IMUL3Q, RDX, RDX, elem_size);
        x64->op(JMP, check);
        
        x64->code_label(loop);
        elem_ts.compare(Storage(MEMORY, Address(RAX, RDX, ARRAY_ELEMS_OFFSET)), Storage(MEMORY, Address(RSP, 0)), x64);
        x64->op(JE, found);

        x64->op(INCQ, RCX);
        x64->op(ADDQ, RDX, elem_size);
        
        x64->code_label(check);
        x64->op(CMPQ, RCX, Address(RSP, stack_size + ADDRESS_SIZE + INTEGER_SIZE));  // length
        x64->op(JB, loop);

        elem_ts.store(Storage(STACK), Storage(), x64);
        slice_ts.store(Storage(STACK), Storage(), x64);
        raise("NOT_FOUND", x64);

        x64->code_label(found);
        elem_ts.store(Storage(STACK), Storage(), x64);
        slice_ts.store(Storage(STACK), Storage(), x64);
        
        return Storage(REGISTER, RCX);
    }
};



// Iteration
// TODO: too many similarities with container iteration!

class SliceIterValue: public SimpleRecordValue {
public:
    SliceIterValue(TypeSpec t, Value *l)
        :SimpleRecordValue(t, l) {
    }

    virtual Storage compile(X64 *x64) {
        x64->op(PUSHQ, 0);

        left->compile_and_store(x64, Storage(STACK));
        
        return Storage(STACK);
    }
};


class SliceElemIterValue: public SliceIterValue {
public:
    SliceElemIterValue(Value *l, TypeMatch &match)
        :SliceIterValue(typesubst(SAME_SLICEELEMITER_TS, match), l) {
    }
};


class SliceIndexIterValue: public SliceIterValue {
public:
    SliceIndexIterValue(Value *l, TypeMatch &match)
        :SliceIterValue(typesubst(SAME_SLICEINDEXITER_TS, match), l) {
    }
};


class SliceItemIterValue: public SliceIterValue {
public:
    SliceItemIterValue(Value *l, TypeMatch &match)
        :SliceIterValue(typesubst(SAME_SLICEITEMITER_TS, match), l) {
    }
};


class SliceNextValue: public GenericValue, public Raiser {
public:
    Regs clob;
    bool is_down;
    TypeSpec elem_ts;
    int elem_size;
    
    SliceNextValue(TypeSpec ts, TypeSpec ets, Value *l, bool d)
        :GenericValue(NO_TS, ts, l) {
        is_down = d;
        elem_ts = ets;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!check_arguments(args, kwargs, {}))
            return false;

        if (!check_raise(iterator_done_exception_type, scope))
            return false;
        
        return true;
    }

    virtual Regs precompile(Regs preferred) {
        clob = left->precompile(preferred);

        if (!clob.has_any())
            clob = clob | RAX;

        return clob;
    }

    virtual Storage subcompile(X64 *x64) {
        elem_size = array_elem_size(elem_ts);
        ls = left->compile(x64);  // iterator
        Register reg = (clob & ~ls.regs()).get_any();
        Label ok;

        int LENGTH_OFFSET = REFERENCE_SIZE + INTEGER_SIZE;
        int VALUE_OFFSET = REFERENCE_SIZE + 2 * INTEGER_SIZE;
        
        switch (ls.where) {
        case MEMORY:
            x64->op(MOVQ, R10, ls.address + VALUE_OFFSET);
            x64->op(MOVQ, reg, ls.address); // array ptr
            x64->op(CMPQ, R10, ls.address + LENGTH_OFFSET);
            x64->op(JNE, ok);
            
            raise("ITERATOR_DONE", x64);
            
            x64->code_label(ok);
            x64->op(is_down ? DECQ : INCQ, ls.address + VALUE_OFFSET);
            
            return Storage(REGISTER, reg);
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class SliceNextElemValue: public SliceNextValue {
public:
    SliceNextElemValue(Value *l, TypeMatch &match)
        :SliceNextValue(match[1], match[1], l, false) {
    }

    virtual Storage compile(X64 *x64) {
        int FRONT_OFFSET = REFERENCE_SIZE;
        Storage r = subcompile(x64);
        
        x64->op(ADDQ, R10, ls.address + FRONT_OFFSET);
        x64->op(IMUL3Q, R10, R10, elem_size);
        x64->op(LEA, r.reg, Address(r.reg, R10, ARRAY_ELEMS_OFFSET));
        
        return Storage(MEMORY, Address(r.reg, 0));
    }
};


class SliceNextIndexValue: public SliceNextValue {
public:
    SliceNextIndexValue(Value *l, TypeMatch &match)
        :SliceNextValue(INTEGER_TS, match[1], l, false) {
    }
    
    virtual Storage compile(X64 *x64) {
        Storage r = subcompile(x64);
        
        x64->op(MOVQ, r.reg, R10);
        
        return Storage(REGISTER, r.reg);
    }
};


class SliceNextItemValue: public SliceNextValue {
public:
    SliceNextItemValue(Value *l, TypeMatch &match)
        :SliceNextValue(typesubst(INTEGER_SAME_ITEM_TS, match), match[1], l, false) {
    }

    virtual Storage compile(X64 *x64) {
        int FRONT_OFFSET = REFERENCE_SIZE;
        int item_stack_size = ts.measure_stack();

        Storage r = subcompile(x64);

        x64->op(SUBQ, RSP, item_stack_size);
        x64->op(MOVQ, Address(RSP, 0), R10);
        
        x64->op(ADDQ, R10, ls.address + FRONT_OFFSET);
        x64->op(IMUL3Q, R10, R10, elem_size);
        x64->op(LEA, r.reg, Address(r.reg, R10, ARRAY_ELEMS_OFFSET));
        
        Storage s = Storage(MEMORY, Address(r.reg, 0));
        Storage t = Storage(MEMORY, Address(RSP, INTEGER_SIZE));
        elem_ts.create(s, t, x64);
        
        return Storage(STACK);
    }
};

