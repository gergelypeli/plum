

class ArrayItemValue: public GenericOperationValue {
public:
    Register mreg;

    ArrayItemValue(TypeSpec t, Value *a)
        :GenericOperationValue(TWEAK, INTEGER_TS, t.rvalue().unprefix(reference_type).unprefix(array_type).lvalue(), a) {
    }

    virtual Register pick_early_register(Regs preferred) {
        // And we need to allocate a special address-only register for the real return value
        return preferred.has_ptr() ? preferred.get_ptr() : Regs::all_ptrs().get_ptr();
    }

    virtual Storage compile(X64 *x64) {
        int size = item_size(ts.measure());
        
        subcompile(x64);
    
        // TODO: probably this is the point where we need to borrow a reference to the
        // array, and unborrow it sometimes later during the stack unwinding.
    
        // NOTE: the reg we selected is a PTR register, and that means that either
        // arguments may be using it to return a MEMORY storage value.
        // So before we overwrite it, we must make sure we already dereferenced it.
    
        switch (ls.where * rs.where) {
        case REGISTER_CONSTANT:
            x64->op(LEA, reg, Address(ls.reg, rs.value * size + ARRAY_ITEMS_OFFSET));
            return Storage(MEMORY, Address(reg, 0));
        case REGISTER_REGISTER:
            x64->op(IMUL3Q, reg, rs.reg, size);
            x64->op(ADDQ, reg, ls.reg);
            return Storage(MEMORY, Address(reg, ARRAY_ITEMS_OFFSET));
        case REGISTER_MEMORY:
            x64->op(IMUL3Q, reg, rs.address, size);  // reg may be the base of rs.address
            x64->op(ADDQ, reg, ls.reg);
            return Storage(MEMORY, Address(reg, ARRAY_ITEMS_OFFSET));
        case MEMORY_CONSTANT:
            x64->op(MOVQ, reg, ls.address);  // reg may be the base of ls.address
            return Storage(MEMORY, Address(reg, rs.value * size + ARRAY_ITEMS_OFFSET));
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
            return Storage(MEMORY, Address(reg, ARRAY_ITEMS_OFFSET));
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
            return Storage(MEMORY, Address(reg, ARRAY_ITEMS_OFFSET));
        default:
            throw INTERNAL_ERROR;
        }
    }    
};


class ArrayConcatenationValue: public GenericOperationValue {
public:
    ArrayConcatenationValue(TypeSpec t, Value *l, Value *other)
        :GenericOperationValue(TWEAK, t, t, l) {
        if (other) {
            // This shortcut is used in string interpolation only
            right.reset(other);
        }
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = GenericOperationValue::precompile(preferred);
        return clob.add(RAX).add(RBX).add(RCX).add(RDX).add(RSI).add(RDI);
    }

    virtual Storage compile(X64 *x64) {
        // TODO: this only works for arrays of basic types now, that can be just copied
        // TODO: don't inline this
        int size = item_size(ts.unprefix(reference_type).unprefix(array_type).measure());
        
        subcompile(x64);
        
        // RAX - result, RBX - first, RCX - counter, RDX - second
        
        left->ts.store(ls, Storage(STACK), x64);
        right->ts.store(rs, Storage(REGISTER, RDX), x64);
        left->ts.store(Storage(STACK), Storage(REGISTER, RBX), x64);
        
        x64->op(MOVQ, RCX, Address(RBX, ARRAY_LENGTH_OFFSET));
        x64->op(ADDQ, RCX, Address(RDX, ARRAY_LENGTH_OFFSET));  // total length in RCX
        
        x64->op(IMUL3Q, RAX, RCX, size);
        x64->op(ADDQ, RAX, ARRAY_HEADER_SIZE);
        
        x64->alloc();  // Allocate this many bytes with a refcount of 1, return in RAX
        
        x64->op(MOVQ, Address(RAX, ARRAY_RESERVATION_OFFSET), RCX);
        x64->op(MOVQ, Address(RAX, ARRAY_LENGTH_OFFSET), RCX);
        
        x64->op(LEA, RDI, Address(RAX, ARRAY_ITEMS_OFFSET));
        
        x64->op(LEA, RSI, Address(RBX, ARRAY_ITEMS_OFFSET));
        x64->op(IMUL3Q, RCX, Address(RBX, ARRAY_LENGTH_OFFSET), size);
        x64->op(REPMOVSB);

        x64->op(LEA, RSI, Address(RDX, ARRAY_ITEMS_OFFSET));
        x64->op(IMUL3Q, RCX, Address(RDX, ARRAY_LENGTH_OFFSET), size);
        x64->op(REPMOVSB);
        
        right->ts.store(Storage(REGISTER, RDX), Storage(), x64);
        left->ts.store(Storage(REGISTER, RBX), Storage(), x64);
        
        return Storage(REGISTER, RAX);
    }
};


class ArrayReallocValue: public GenericOperationValue {
public:
    ArrayReallocValue(TypeSpec t, Value *l)
        :GenericOperationValue(TWEAK, INTEGER_TS, VOID_TS, l) {
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = GenericOperationValue::precompile(preferred);
        return clob.add(RAX).add(RBX).add(RCX);
    }

    virtual Storage compile(X64 *x64) {
        // TODO: this only works for arrays of basic types now, that can be just copied
        // TODO: don't inline this either
        Label end;
        int size = item_size(ts.unprefix(reference_type).unprefix(array_type).measure());
        
        subcompile(x64);
        
        if (ls.where != MEMORY)
            throw INTERNAL_ERROR;
            
        switch (rs.where) {
        case CONSTANT:
            x64->op(MOVQ, RCX, rs.value);
            break;
        case REGISTER:
            x64->op(MOVQ, RCX, rs.reg);
            break;
        case MEMORY:
            x64->op(MOVQ, RCX, rs.address);
            break;
        default:
            throw INTERNAL_ERROR;
        }
            
        x64->op(MOVQ, RAX, ls.address);
        x64->cmpref(RAX, 1);
        x64->op(JNE, end);
        
        x64->op(IMUL3Q, RBX, RCX, size);
        x64->op(ADDQ, RBX, ARRAY_HEADER_SIZE);
        
        x64->realloc();
        
        x64->op(MOVQ, Address(RAX, ARRAY_RESERVATION_OFFSET), RCX);
        x64->op(MOVQ, ls.address, RAX);
        
        return Storage();
    }
};

