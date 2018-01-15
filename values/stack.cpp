
TypeSpec stack_elem_ts(TypeSpec ts) {
    TypeSpec ets = ts.rvalue().unprefix(stack_type);
    
    if (is_heap_type(ets[0]))
        ets = ets.prefix(reference_type);
        
    return ets;
}


class StackLengthValue: public GenericOperationValue {
public:
    StackLengthValue(Value *l, TypeMatch &match)
        :GenericOperationValue(GENERIC_UNARY, NO_TS, INTEGER_TS, l) {
    }

    virtual Storage compile(X64 *x64) {
        subcompile(x64);
        
        x64->decref(ls.reg);
        x64->op(MOVQ, ls.reg, x64->array_length_address(ls.reg));
        
        return Storage(REGISTER, ls.reg);
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
        compile_and_store_both(x64, Storage(ALISTACK), Storage(STACK));
    
        int elem_size = ::elem_size(elem_ts.measure(MEMORY));
        int stack_size = ::stack_size(elem_ts.measure(MEMORY));
        
        x64->op(MOVQ, RBX, Address(RSP, stack_size));
        x64->op(MOVQ, RAX, Address(RBX, 0));
        x64->op(MOVQ, RBX, 1);
        x64->op(MOVQ, RCX, elem_size);
        
        x64->preappend_array_RAX_RBX_RCX();
        
        x64->op(MOVQ, RBX, Address(RSP, stack_size));
        x64->op(MOVQ, Address(RBX, 0), RAX);
        
        x64->op(MOVQ, RBX, x64->array_length_address(RAX));
        x64->op(IMUL3Q, RBX, RBX, elem_size);
        x64->op(ADDQ, x64->array_length_address(RAX), 1);

        Address addr = x64->array_elems_address(RAX);
        addr.index = RBX;
        addr.scale = 1;
        x64->op(LEA, RAX, addr);
        
        elem_ts.create(Storage(STACK), Storage(MEMORY, Address(RAX, 0)), x64);
        
        return Storage(ALISTACK);
    }
};


class StackPopValue: public GenericValue {
public:
    TypeSpec elem_ts;
    
    StackPopValue(Value *l, TypeMatch &match)
        :GenericValue(VOID_TS, stack_elem_ts(match[0]), l) {
        elem_ts = stack_elem_ts(match[0]);
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = left->precompile(preferred);
        return clob.add(RAX).add(RBX).add(RCX);
    }

    virtual Storage compile(X64 *x64) {
        int elem_size = ::elem_size(elem_ts.measure(MEMORY));

        left->compile_and_store(x64, Storage(STACK));
        
        x64->op(POPQ, RAX);
        x64->op(DECQ, x64->array_length_address(RAX));
        x64->op(MOVQ, RBX, x64->array_length_address(RAX));
        x64->op(IMUL3Q, RBX, RBX, elem_size);

        x64->decref(RAX);  // FIXME: this is too early

        Address addr = x64->array_elems_address(RAX);
        addr.index = RBX;
        addr.scale = 1;
        x64->op(LEA, RAX, addr);
        
        elem_ts.store(Storage(MEMORY, Address(RAX, 0)), Storage(STACK), x64);
        elem_ts.destroy(Storage(MEMORY, Address(RAX, 0)), x64);
        
        return Storage(STACK);
    }
};
