

class ArrayItemValue: public GenericOperationValue {
public:
    Register mreg;

    ArrayItemValue(OperationType o, Value *pivot, TypeMatch &match)
        :GenericOperationValue(o, INTEGER_TS, match[1].lvalue(), pivot) {
    }

    virtual Register pick_early_register(Regs preferred) {
        // And we need to allocate a special address-only register for the real return value
        return preferred.has_ptr() ? preferred.get_ptr() : Regs::all_ptrs().get_ptr();
    }

    virtual Storage compile(X64 *x64) {
        int size = item_size(ts.measure(MEMORY));
        
        subcompile(x64);
    
        // TODO: probably this is the point where we need to borrow a reference to the
        // array, and unborrow it sometimes later during the stack unwinding.
    
        // NOTE: the reg we selected is a PTR register, and that means that either
        // arguments may be using it to return a MEMORY storage value.
        // So before we overwrite it, we must make sure we already dereferenced it.
    
        switch (ls.where * rs.where) {
        case REGISTER_CONSTANT:
            x64->op(LEA, reg, x64->array_items_address(ls.reg) + rs.value * size);
            return Storage(MEMORY, Address(reg, 0));
        case REGISTER_REGISTER:
            x64->op(IMUL3Q, reg, rs.reg, size);
            x64->op(ADDQ, reg, ls.reg);
            return Storage(MEMORY, x64->array_items_address(reg));
        case REGISTER_MEMORY:
            x64->op(IMUL3Q, reg, rs.address, size);  // reg may be the base of rs.address
            x64->op(ADDQ, reg, ls.reg);
            return Storage(MEMORY, x64->array_items_address(reg));
        case MEMORY_CONSTANT:
            x64->op(MOVQ, reg, ls.address);  // reg may be the base of ls.address
            return Storage(MEMORY, x64->array_items_address(reg) + rs.value * size);
        case MEMORY_REGISTER:
            if (reg != ls.address.base) {
                x64->op(IMUL3Q, reg, rs.reg, size);  // reg is not the base of ls.address
                x64->op(ADDQ, reg, ls.address);
            }
            else {
                x64->op(IMUL3Q, rs.reg, rs.reg, size);
                x64->op(MOVQ, reg, ls.address);  // reg is the base of ls.address
                x64->op(ADDQ, reg, rs.reg);
            }
            return Storage(MEMORY, x64->array_items_address(reg));
        case MEMORY_MEMORY:
            if (reg != ls.address.base) {
                x64->op(IMUL3Q, reg, rs.address, size);  // reg may be the base of rs.address
                x64->op(ADDQ, reg, ls.address);
            }
            else {
                x64->op(MOVQ, reg, ls.address);  // reg is the base of ls.address
                x64->op(IMUL3Q, RBX, rs.address, size);
                x64->op(ADDQ, reg, RBX);
            }
            return Storage(MEMORY, x64->array_items_address(reg));
        default:
            throw INTERNAL_ERROR;
        }
    }    
};


class ArrayConcatenationValue: public GenericOperationValue {
public:
    ArrayConcatenationValue(OperationType o, Value *l, TypeMatch &match)
        :GenericOperationValue(o, match[0], match[0], l) {
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = GenericOperationValue::precompile(preferred);
        return clob.add(RAX).add(RBX).add(RCX).add(RDX).add(RSI).add(RDI);
    }

    virtual Storage compile(X64 *x64) {
        // TODO: this only works for arrays of basic types now, that can be just copied
        // TODO: don't inline this
        int item_size = ::item_size(ts.unprefix(reference_type).unprefix(array_type).measure(MEMORY));
        
        subcompile(x64);
        
        // RAX - result, RBX - first, RCX - counter, RDX - second
        
        left->ts.store(ls, Storage(STACK), x64);
        right->ts.store(rs, Storage(REGISTER, RDX), x64);
        left->ts.store(Storage(STACK), Storage(REGISTER, RBX), x64);
        
        x64->op(MOVQ, RAX, Address(RBX, ARRAY_LENGTH_OFFSET));
        x64->op(ADDQ, RAX, Address(RDX, ARRAY_LENGTH_OFFSET));  // total length in RAX
        x64->op(PUSHQ, RAX);
        
        x64->alloc_array_RAX(item_size);
        
        x64->op(POPQ, x64->array_length_address(RAX));
        
        x64->op(LEA, RDI, x64->array_items_address(RAX));
        
        x64->op(LEA, RSI, x64->array_items_address(RBX));
        x64->op(IMUL3Q, RCX, x64->array_length_address(RBX), item_size);
        x64->op(REPMOVSB);

        x64->op(LEA, RSI, x64->array_items_address(RDX));
        x64->op(IMUL3Q, RCX, x64->array_length_address(RDX), item_size);
        x64->op(REPMOVSB);
        
        right->ts.store(Storage(REGISTER, RDX), Storage(), x64);
        left->ts.store(Storage(REGISTER, RBX), Storage(), x64);
        
        return Storage(REGISTER, RAX);
    }
};


class ArrayReallocValue: public GenericOperationValue {
public:
    ArrayReallocValue(OperationType o, Value *l, TypeMatch &match)
        :GenericOperationValue(o, INTEGER_OVALUE_TS, l->ts, l) {
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = GenericOperationValue::precompile(preferred);
        return clob.add(RAX).add(RBX);
    }

    virtual Storage compile(X64 *x64) {
        // TODO: this only works for arrays of basic types now, that can be just copied
        // TODO: don't inline this either
        Label end;
        int item_size = ::item_size(ts.rvalue().unprefix(reference_type).unprefix(array_type).measure(MEMORY));
        
        subcompile(x64);
        
        if (ls.where != MEMORY)
            throw INTERNAL_ERROR;

        x64->op(MOVQ, RAX, ls.address);
            
        switch (rs.where) {
        case NOWHERE:
            x64->op(MOVQ, RBX, x64->array_length_address(RAX));  // shrink to fit
            break;
        case CONSTANT:
            x64->op(MOVQ, RBX, rs.value);
            break;
        case REGISTER:
            x64->op(MOVQ, RBX, rs.reg);
            break;
        case MEMORY:
            x64->op(MOVQ, RBX, rs.address);
            break;
        default:
            throw INTERNAL_ERROR;
        }
        
        x64->realloc_array_RAX_RBX(item_size);
        x64->op(MOVQ, ls.address, RAX);
        
        return ls;
    }
};
