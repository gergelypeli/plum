

class StringRegexpMatcherValue: public GenericValue, public Raiser {
public:
    StringRegexpMatcherValue(Value *l, TypeMatch &match)
        :GenericValue(STRING_TS, STRING_ARRAY_TS, l) {
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
        TypeSpec heap_ts = ts.reprefix(slice_type, linearray_type);
        heap_ts.incref(R10, x64);
        
        x64->op(PUSHQ, 0);  // length
        x64->op(PUSHQ, 0);  // front
        x64->op(PUSHQ, R10);  // ptr
        
        return Storage(STACK);
    }
};


class SliceAllValue: public GenericValue {
public:
    SliceAllValue(TypeMatch &match)
        :GenericValue(match[1].prefix(array_type), match[1].prefix(slice_type), NULL) {
    }

    virtual Regs precompile(Regs preferred) {
        return right->precompile(preferred);
    }

    virtual Storage compile(X64 *x64) {
        TypeSpec heap_ts = ts.reprefix(slice_type, linearray_type);
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
            heap_ts.incref(R10, x64);
            r = R10;
            break;
        default:
            throw INTERNAL_ERROR;
        }
        
        x64->op(PUSHQ, Address(r, LINEARRAY_LENGTH_OFFSET));  // length
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
    TypeSpec heap_ts;
    
    ArraySliceValue(Value *pivot, TypeMatch &match)
        :Value(match[1].prefix(slice_type)) {
        
        array_value.reset(pivot);
        heap_ts = match[1].prefix(linearray_type);
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
        x64->op(MOVQ, RDX, Address(RAX, LINEARRAY_LENGTH_OFFSET));
        
        Label ok, nok;
        x64->op(CMPQ, R10, RDX);
        x64->op(JAE, nok);
        
        x64->op(SUBQ, RDX, R10);
        x64->op(CMPQ, RCX, RDX);
        x64->op(JBE, ok);
        
        x64->code_label(nok);

        // all popped
        heap_ts.decref(RAX, x64);
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
        
        int old_stack_usage = x64->accounting->mark();
        ts.store(Storage(STACK), Storage(), x64);  // pop Slice
        raise("NOT_FOUND", x64);
        x64->accounting->rewind(old_stack_usage);
        
        x64->code_label(ok);
        x64->op(ADDQ, Address(RSP, ADDRESS_SIZE), R10);  // adjust front
        x64->op(MOVQ, Address(RSP, ADDRESS_SIZE + INTEGER_SIZE), RCX);  // set length
        
        return Storage(STACK);
    }
};


class SliceIndexValue: public GenericValue, public Raiser, public ContainedLvalue {
public:
    TypeSpec elem_ts;
    TypeSpec heap_ts;
    Storage value_storage;
    
    SliceIndexValue(Value *pivot, TypeMatch &match)
        :GenericValue(INTEGER_TS, match[1].lvalue(), pivot) {
        elem_ts = match[1];
        heap_ts = elem_ts.prefix(linearray_type);
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!check_raise(lookup_exception_type, scope))
            return false;

        //if (!check_reference(scope))
        //    return false;
        
        return GenericValue::check(args, kwargs, scope);
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = left->precompile(preferred) | right->precompile(preferred) | Regs(RAX, RBX);

        clob = clob | precompile_contained_lvalue(preferred, lvalue_needed, ts);
            
        return clob;
    }

    virtual Storage compile(X64 *x64) {
        int elem_size = ContainerType::get_elem_size(elem_ts);
    
        // TODO: MEMORY pivot can be much more optimal
        left->compile_and_store(x64, Storage(STACK));
        right->compile_and_store(x64, Storage(STACK));
        
        x64->op(POPQ, RAX);  // index
        x64->op(POPQ, RBX);  // ptr
        x64->op(POPQ, R10);  // front
        x64->op(POPQ, R11);  // length

        Label ok;
        //defer_decref(RBX, x64);
        
        x64->op(CMPQ, RAX, R11);
        x64->op(JB, ok);

        // all popped
        raise("NOT_FOUND", x64);
        
        x64->code_label(ok);
        x64->op(ADDQ, RAX, R10);
        
        Address addr = x64->runtime->make_address(RBX, RAX, elem_size, LINEARRAY_ELEMS_OFFSET);

        return compile_contained_lvalue(addr, RBX, ts, x64);
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
        int elem_size = ContainerType::get_elem_size(elem_ts);
        int stack_size = elem_ts.measure_stack();
    
        x64->op(MOVQ, RAX, Address(RSP, stack_size));

        x64->op(MOVQ, RCX, 0);
        x64->op(MOVQ, RDX, Address(RSP, stack_size + ADDRESS_SIZE));  // front index
        x64->op(IMUL3Q, RDX, RDX, elem_size);
        x64->op(JMP, check);
        
        x64->code_label(loop);
        elem_ts.compare(Storage(MEMORY, Address(RAX, RDX, LINEARRAY_ELEMS_OFFSET)), Storage(MEMORY, Address(RSP, 0)), x64);
        x64->op(JE, found);

        x64->op(INCQ, RCX);
        x64->op(ADDQ, RDX, elem_size);
        
        x64->code_label(check);
        x64->op(CMPQ, RCX, Address(RSP, stack_size + ADDRESS_SIZE + INTEGER_SIZE));  // length
        x64->op(JB, loop);

        int old_stack_usage = x64->accounting->mark();
        elem_ts.store(Storage(STACK), Storage(), x64);
        slice_ts.store(Storage(STACK), Storage(), x64);
        raise("NOT_FOUND", x64);
        x64->accounting->rewind(old_stack_usage);

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

        clob.reserve(4);

        return clob;
    }

    virtual Storage postprocess(Register r, Register i, X64 *x64) {
        throw INTERNAL_ERROR;
    }

    virtual Storage compile(X64 *x64) {
        elem_size = ContainerType::get_elem_size(elem_ts);
        ls = left->compile(x64);  // iterator
        Register r = (clob & ~ls.regs()).get_any();
        Register i = (clob & ~ls.regs() & ~Regs(r)).get_any();
        Label ok;

        int LENGTH_OFFSET = REFERENCE_SIZE + INTEGER_SIZE;
        int VALUE_OFFSET = REFERENCE_SIZE + 2 * INTEGER_SIZE;
        
        switch (ls.where) {
        case MEMORY:
            x64->op(MOVQ, i, ls.address + VALUE_OFFSET);
            x64->op(MOVQ, r, ls.address); // array ptr
            x64->op(CMPQ, i, ls.address + LENGTH_OFFSET);
            x64->op(JNE, ok);
            
            raise("ITERATOR_DONE", x64);
            
            x64->code_label(ok);
            x64->op(is_down ? DECQ : INCQ, ls.address + VALUE_OFFSET);
            
            return postprocess(r, i, x64);
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class SliceNextElemValue: public SliceNextValue, public ContainedLvalue {
public:
    SliceNextElemValue(Value *l, TypeMatch &match)
        :SliceNextValue(typesubst(SAME_LVALUE_TUPLE1_TS, match), match[1], l, false) {
    }

    virtual Regs precompile(Regs preferred) {
        return SliceNextValue::precompile(preferred) | precompile_contained_lvalue(preferred, lvalue_needed, ts);
    }

    virtual Storage postprocess(Register r, Register i, X64 *x64) {
        int FRONT_OFFSET = REFERENCE_SIZE;
        
        x64->op(ADDQ, i, ls.address + FRONT_OFFSET);
        
        Address addr = x64->runtime->make_address(r, i, elem_size, LINEARRAY_ELEMS_OFFSET);
        
        return compile_contained_lvalue(addr, NOREG, ts, x64);
    }
};


class SliceNextIndexValue: public SliceNextValue {
public:
    SliceNextIndexValue(Value *l, TypeMatch &match)
        :SliceNextValue(INTEGER_TUPLE1_TS, match[1], l, false) {
    }
    
    virtual Storage postprocess(Register r, Register i, X64 *x64) {
        return Storage(REGISTER, i);
    }
};


class SliceNextItemValue: public SliceNextValue {
public:
    SliceNextItemValue(Value *l, TypeMatch &match)
        :SliceNextValue(typesubst(INTEGER_SAME_LVALUE_TUPLE2_TS, match), match[1], l, false) {
    }

    virtual Storage postprocess(Register r, Register i, X64 *x64) {
        int FRONT_OFFSET = REFERENCE_SIZE;
        //int item_stack_size = ts.measure_stack();

        x64->op(PUSHQ, i);

        x64->op(ADDQ, i, ls.address + FRONT_OFFSET);
        Address addr = x64->runtime->make_address(r, i, elem_size, LINEARRAY_ELEMS_OFFSET);

        x64->op(PUSHQ, 0);
        x64->op(LEA, R10, addr);
        x64->op(PUSHQ, R10);
        
        return Storage(STACK);
    }
};
