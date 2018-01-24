
TypeSpec element_ts(TypeSpec ts) {
    TypeSpec ets = ts.rvalue().unprefix(reference_type);
    ets = ets.unprefix(ets[0]);
    
    if (is_heap_type(ets[0]))
        ets = ets.prefix(reference_type);
        
    return ets;
}


class StackInitializerValue: public Value {
public:
    std::unique_ptr<Value> stack, array;
    
    StackInitializerValue(Value *s, Value *a)
        :Value(s->ts.unprefix(partial_type)) {
        stack.reset(s);
        array.reset(a);
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        return array->check(args, kwargs, scope);
    }
    
    virtual Regs precompile(Regs preferred) {
        return stack->precompile(preferred) | array->precompile(preferred);
    }

    virtual Storage compile(X64 *x64) {
        stack->compile_and_store(x64, Storage(STACK));
        array->compile_and_store(x64, Storage(STACK));
        
        x64->op(MOVQ, RBX, Address(RSP, REFERENCE_SIZE));
        x64->op(POPQ, Address(RBX, CLASS_MEMBERS_OFFSET));
        
        return Storage(STACK);
    }
};


class StackPushValue: public GenericValue {
public:
    TypeSpec elem_ts;
    
    StackPushValue(Value *l, TypeMatch &match)
        :GenericValue(element_ts(match[0]), match[0], l) {
        elem_ts = element_ts(match[0]);
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = left->precompile(preferred) | right->precompile(preferred);
        return clob.add(RAX).add(RBX).add(RCX);
    }

    virtual void fix_growth(int elem_size, X64 *x64) {
    }
    
    virtual void fix_index(X64 *x64) {
    }

    virtual Storage compile(X64 *x64) {
        compile_and_store_both(x64, Storage(STACK), Storage(STACK));
    
        int elem_size = ::elem_size(elem_ts.measure(MEMORY));
        int stack_size = ::stack_size(elem_ts.measure(MEMORY));
        Label ok;
        
        x64->op(MOVQ, RBX, Address(RSP, stack_size));
        x64->op(MOVQ, RAX, Address(RBX, CLASS_MEMBERS_OFFSET));
        x64->op(MOVQ, RBX, x64->array_length_address(RAX));
        x64->op(CMPQ, RBX, x64->array_reservation_address(RAX));
        x64->op(JB, ok);
        
        //x64->err("Will grow the stack/queue.");
        x64->op(INCQ, RBX);
        x64->op(MOVQ, RCX, elem_size);
        x64->op(PUSHQ, x64->array_reservation_address(RAX));  // good to know it later
        
        x64->grow_array_RAX_RBX_RCX();

        x64->op(POPQ, RBX);  // old reservation
        x64->op(MOVQ, RCX, Address(RSP, stack_size));
        x64->op(MOVQ, Address(RCX, CLASS_MEMBERS_OFFSET), RAX);

        fix_growth(elem_size, x64);
        x64->op(MOVQ, RBX, x64->array_length_address(RAX));
        
        x64->code_label(ok);
        // length (the index of the new element) is in RBX
        fix_index(x64);
        
        x64->op(IMUL3Q, RBX, RBX, elem_size);
        x64->op(INCQ, x64->array_length_address(RAX));

        x64->op(LEA, RAX, x64->array_elems_address(RAX) + RBX);
        
        elem_ts.create(Storage(STACK), Storage(MEMORY, Address(RAX, 0)), x64);
        
        return Storage(STACK);
    }
};


class QueueInitializerValue: public StackInitializerValue {
public:
    QueueInitializerValue(Value *q, Value *c)
        :StackInitializerValue(q, c) {
    }
};


class QueuePushValue: public StackPushValue {
public:
    QueuePushValue(Value *l, TypeMatch &match)
        :StackPushValue(l, match) {
    }

    virtual void fix_growth(int elem_size, X64 *x64) {
        // RAX is the array, RBX is the old reservation size, RCX is available

        x64->op(PUSHQ, RSI);
        x64->op(PUSHQ, RDI);
        
        Label high, end;
        x64->op(MOVQ, RCX, RBX);
        x64->op(SHRQ, RCX, 1);
        x64->op(CMPQ, x64->array_front_address(RAX), RCX);
        x64->op(JAE, high);

        // The front is low, so it's better to unfold the folded part. This requires that
        // the growth rate was at least 1.5 times.
        
        x64->err("Unfolding queue circularray.");
        
        x64->op(LEA, RSI, x64->array_elems_address(RAX));
        
        x64->op(MOVQ, RDI, RBX);
        x64->op(IMUL3Q, RDI, RDI, elem_size);
        x64->op(LEA, RDI, x64->array_elems_address(RAX) + RDI);
        
        x64->op(MOVQ, RCX, x64->array_front_address(RAX));
        x64->op(IMUL3Q, RCX, RCX, elem_size);
        
        x64->op(REPMOVSB);
        x64->op(JMP, end);
        
        x64->code_label(high);
        
        // The front is high, so it's better to move the unfolded part to the end of the
        // new reservation. This also requires 1.5 growth rate so we can copy forward.

        x64->err("Stretching queue circularray.");
        
        x64->op(MOVQ, RSI, x64->array_front_address(RAX));
        x64->op(IMUL3Q, RSI, RSI, elem_size);
        x64->op(LEA, RSI, x64->array_elems_address(RAX) + RSI);

        x64->op(MOVQ, RDI, x64->array_front_address(RAX));
        x64->op(SUBQ, RDI, RBX);
        x64->op(ADDQ, RDI, x64->array_reservation_address(RAX));
        x64->op(MOVQ, x64->array_front_address(RAX), RDI);  // must update front index
        x64->op(IMUL3Q, RDI, RDI, elem_size);
        x64->op(LEA, RDI, x64->array_elems_address(RAX) + RDI);
        
        x64->op(MOVQ, RCX, x64->array_reservation_address(RAX));
        x64->op(SUBQ, RCX, x64->array_front_address(RAX));
        x64->op(IMUL3Q, RCX, RCX, elem_size);
        
        x64->op(REPMOVSB);
        
        x64->code_label(end);
        x64->op(POPQ, RDI);
        x64->op(POPQ, RSI);
    }
    
    virtual void fix_index(X64 *x64) {
        fix_index_overflow(RAX, x64);
    }
};


class QueueUnshiftValue: public QueuePushValue {
public:
    QueueUnshiftValue(Value *l, TypeMatch &match)
        :QueuePushValue(l, match) {
    }

    virtual void fix_index(X64 *x64) {
        // Compute the new front, and use it for the element index
        x64->op(MOVQ, RBX, -1);
        fix_index_underflow(RAX, x64);
        x64->op(MOVQ, x64->array_front_address(RAX), RBX);
    }
};
