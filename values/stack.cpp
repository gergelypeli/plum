
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

    virtual void fix_growth(Register reg, int elem_size, X64 *x64) {
    }
    
    virtual void fix_index(Register reg, X64 *x64) {
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
        
        //x64->err("Will grow the queue.");
        x64->op(INCQ, RBX);
        x64->op(MOVQ, RCX, elem_size);
        x64->op(PUSHQ, x64->array_reservation_address(RAX));
        
        x64->grow_array_RAX_RBX_RCX();

        x64->op(POPQ, RBX);  // old reservation
        x64->op(MOVQ, RCX, Address(RSP, stack_size));
        x64->op(MOVQ, Address(RCX, CLASS_MEMBERS_OFFSET), RAX);

        fix_growth(RAX, elem_size, x64);
        x64->op(MOVQ, RBX, x64->array_length_address(RAX));
        
        x64->code_label(ok);
        // length (the index of the new element) is in RBX
        fix_index(RAX, x64);
        
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

    virtual void fix_growth(Register reg, int elem_size, X64 *x64) {
        Label ok;
        x64->op(CMPQ, x64->array_front_address(reg), 0);
        x64->op(JE, ok);
        
        // Since growth is guaranteed to at least double the reservation, we have enough
        // space after the old reservation to unfold the folded elements to.
        x64->op(PUSHQ, RSI);
        x64->op(PUSHQ, RDI);
        
        x64->op(LEA, RSI, x64->array_elems_address(reg));
        x64->op(MOVQ, RDI, RBX);
        x64->op(IMUL3Q, RDI, RDI, elem_size);
        x64->op(ADDQ, RDI, RSI);
        x64->op(MOVQ, RCX, x64->array_front_address(reg));
        x64->op(IMUL3Q, RCX, RCX, elem_size);
        
        x64->op(REPMOVSB);
        
        x64->op(POPQ, RDI);
        x64->op(POPQ, RSI);
                
        x64->code_label(ok);
    }
    
    virtual void fix_index(Register reg, X64 *x64) {
        fix_index_overflow(reg, x64);
    }
};
