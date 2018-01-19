
TypeSpec stack_elem_ts(TypeSpec ts) {
    TypeSpec ets = ts.rvalue().unprefix(reference_type).unprefix(stack_type);
    
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
        
        x64->op(MOVQ, RBX, Address(RSP, 8));
        x64->op(POPQ, Address(RBX, CLASS_MEMBERS_OFFSET));
        
        return Storage(STACK);
    }
};


class StackPushValue: public GenericValue {
public:
    TypeSpec elem_ts;
    
    StackPushValue(Value *l, TypeMatch &match)
        :GenericValue(stack_elem_ts(match[0]), match[0], l) {
        elem_ts = stack_elem_ts(match[0]);
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = left->precompile(preferred) | right->precompile(preferred);
        return clob.add(RAX).add(RBX).add(RCX);
    }

    virtual Storage compile(X64 *x64) {
        compile_and_store_both(x64, Storage(STACK), Storage(STACK));
    
        int elem_size = ::elem_size(elem_ts.measure(MEMORY));
        int stack_size = ::stack_size(elem_ts.measure(MEMORY));
        
        x64->op(MOVQ, RBX, Address(RSP, stack_size));
        x64->op(MOVQ, RAX, Address(RBX, CLASS_MEMBERS_OFFSET));
        x64->op(MOVQ, RBX, 1);
        x64->op(MOVQ, RCX, elem_size);
        
        x64->preappend_array_RAX_RBX_RCX();

        x64->op(MOVQ, RBX, Address(RSP, stack_size));
        x64->op(MOVQ, Address(RBX, CLASS_MEMBERS_OFFSET), RAX);
        
        x64->op(MOVQ, RBX, x64->array_length_address(RAX));
        x64->op(IMUL3Q, RBX, RBX, elem_size);
        x64->op(ADDQ, x64->array_length_address(RAX), 1);

        Address addr = x64->array_elems_address(RAX);
        addr.index = RBX;
        addr.scale = 1;
        x64->op(LEA, RAX, addr);
        
        elem_ts.create(Storage(STACK), Storage(MEMORY, Address(RAX, 0)), x64);
        
        return Storage(STACK);
    }
};
