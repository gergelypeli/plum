

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
        :GenericValue(match[1].varvalue(), match[0], l) {
        elem_ts = match[1].varvalue();
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
        x64->op(MOVQ, RBX, Address(RAX, ARRAY_LENGTH_OFFSET));
        x64->op(CMPQ, RBX, Address(RAX, ARRAY_RESERVATION_OFFSET));
        x64->op(JB, ok);
        
        //x64->err("Will grow the stack/queue.");
        x64->op(INCQ, RBX);
        x64->op(MOVQ, RCX, elem_size);
        x64->op(PUSHQ, Address(RAX, ARRAY_RESERVATION_OFFSET));  // good to know it later
        
        x64->grow_array_RAX_RBX_RCX();

        x64->op(POPQ, RBX);  // old reservation
        x64->op(MOVQ, RCX, Address(RSP, stack_size));
        x64->op(MOVQ, Address(RCX, CLASS_MEMBERS_OFFSET), RAX);

        fix_growth(elem_size, x64);
        x64->op(MOVQ, RBX, Address(RAX, ARRAY_LENGTH_OFFSET));
        
        x64->code_label(ok);
        // length (the index of the new element) is in RBX
        fix_index(x64);
        
        x64->op(IMUL3Q, RBX, RBX, elem_size);
        x64->op(INCQ, Address(RAX, ARRAY_LENGTH_OFFSET));

        x64->op(LEA, RAX, Address(RAX, RBX, ARRAY_ELEMS_OFFSET));
        
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
        x64->op(CMPQ, Address(RAX, ARRAY_FRONT_OFFSET), RCX);
        x64->op(JAE, high);

        // The front is low, so it's better to unfold the folded part. This requires that
        // the growth rate was at least 1.5 times.
        
        x64->log("Unfolding queue circularray.");
        
        x64->op(LEA, RSI, Address(RAX, ARRAY_ELEMS_OFFSET));
        
        x64->op(MOVQ, RDI, RBX);
        x64->op(IMUL3Q, RDI, RDI, elem_size);
        x64->op(LEA, RDI, Address(RAX, RDI, ARRAY_ELEMS_OFFSET));
        
        x64->op(MOVQ, RCX, Address(RAX, ARRAY_FRONT_OFFSET));
        x64->op(IMUL3Q, RCX, RCX, elem_size);
        
        x64->op(REPMOVSB);
        x64->op(JMP, end);
        
        x64->code_label(high);
        
        // The front is high, so it's better to move the unfolded part to the end of the
        // new reservation. This also requires 1.5 growth rate so we can copy forward.

        x64->log("Stretching queue circularray.");
        
        x64->op(MOVQ, RSI, Address(RAX, ARRAY_FRONT_OFFSET));
        x64->op(IMUL3Q, RSI, RSI, elem_size);
        x64->op(LEA, RSI, Address(RAX, RSI, ARRAY_ELEMS_OFFSET));

        x64->op(MOVQ, RDI, Address(RAX, ARRAY_FRONT_OFFSET));
        x64->op(SUBQ, RDI, RBX);
        x64->op(ADDQ, RDI, Address(RAX, ARRAY_RESERVATION_OFFSET));
        x64->op(MOVQ, Address(RAX, ARRAY_FRONT_OFFSET), RDI);  // must update front index
        x64->op(IMUL3Q, RDI, RDI, elem_size);
        x64->op(LEA, RDI, Address(RAX, RDI, ARRAY_ELEMS_OFFSET));
        
        x64->op(MOVQ, RCX, Address(RAX, ARRAY_RESERVATION_OFFSET));
        x64->op(SUBQ, RCX, Address(RAX, ARRAY_FRONT_OFFSET));
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
        x64->op(MOVQ, Address(RAX, ARRAY_FRONT_OFFSET), RBX);
    }
};
