
class ArrayOperationValue: public GenericOperationValue {
public:
    ArrayOperationValue(NumericOperation o, TypeSpec t, Value *l)
        :GenericOperationValue(o, t.rvalue(), is_comparison(o) ? BOOLEAN_TS : t, l) {
    }

    virtual Storage equal(X64 *x64, BitSetOp op) {
        subcompile(x64);
        
        switch (ls.where * rs.where) {
        case REGISTER_REGISTER:
            x64->decref(ls.reg);
            x64->decref(rs.reg);
            x64->op(CMPQ, ls.reg, rs.reg);
            return Storage(FLAGS, op);
        case REGISTER_MEMORY:
            x64->decref(ls.reg);
            x64->op(CMPQ, ls.reg, rs.address);
            return Storage(FLAGS, op);
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Storage assign(X64 *x64) {
        subcompile(x64);

        switch (ls.where * rs.where) {
        case MEMORY_REGISTER:
            x64->incref(rs.reg);
            x64->op(XCHGQ, rs.reg, ls.address);
            x64->decref(rs.reg);
            return ls;
        case MEMORY_MEMORY:
            x64->op(MOVQ, reg, rs.address);
            x64->incref(reg);
            x64->op(XCHGQ, reg, ls.address);
            x64->decref(reg);
            return ls;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Storage compile(X64 *x64) {
        switch (operation) {
        case EQUAL:
            return equal(x64, SETE);
        case NOT_EQUAL:
            return equal(x64, SETNE);
        case ASSIGN:
            return assign(x64);
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class ArrayItemValue: public GenericOperationValue {
public:
    Register mreg;

    ArrayItemValue(TypeSpec t, Value *a)  // FIXME: ADD?
        :GenericOperationValue(ADD, INTEGER_TS, t.rvalue().unprefix(reference_type).unprefix(array_type).lvalue(), a) {
    }

    virtual Register pick_early_register(Regs preferred) {
        // And we need to allocate a special address-only register for the real return value
        return preferred.has_ptr() ? preferred.get_ptr() : Regs::all_ptrs().get_ptr();
    }

    virtual Storage compile(X64 *x64) {
        int size = item_size(ts.measure());
        int offset = 8;
        
        subcompile(x64);
    
        // TODO: probably this is the point where we need to borrow a reference to the
        // array, and unborrow it sometimes later during the stack unwinding.
    
        // NOTE: the reg we selected is a PTR register, and that means that either
        // MEMORY side may be using it. So before we overwrite it, we must make sure
        // we already dereferenced it.
    
        switch (ls.where * rs.where) {
        case REGISTER_CONSTANT:
            x64->op(LEA, reg, Address(ls.reg, rs.value * size + offset));
            return Storage(MEMORY, Address(reg, 0));
        case REGISTER_REGISTER:
            x64->op(IMUL3Q, reg, rs.reg, size);
            x64->op(ADDQ, reg, ls.reg);
            return Storage(MEMORY, Address(reg, offset));
        case REGISTER_MEMORY:
            x64->op(IMUL3Q, reg, rs.address, size);  // reg may be the base of rs.address
            x64->op(ADDQ, reg, ls.reg);
            return Storage(MEMORY, Address(reg, offset));
        case MEMORY_CONSTANT:
            x64->op(MOVQ, reg, ls.address);  // reg may be the base of ls.address
            return Storage(MEMORY, Address(reg, rs.value * size + offset));
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
            return Storage(MEMORY, Address(reg, offset));
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
            return Storage(MEMORY, Address(reg, offset));
        default:
            throw INTERNAL_ERROR;
        }
    }    
};


class ArrayConcatenationValue: public GenericOperationValue {
public:
    ArrayConcatenationValue(TypeSpec t, Value *l)
        :GenericOperationValue(TWEAK, t, t, l) {
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = GenericOperationValue::precompile(preferred);
        return clob.add(RAX).add(RBX).add(RCX).add(RSI).add(RDI);
    }

    virtual Storage compile(X64 *x64) {
        // TODO: this only works for arrays of basic types now, that can be just copied
        // TODO: don't inline this
        int size = item_size(ts.unprefix(reference_type).unprefix(array_type).measure());
        
        subcompile(x64);
        
        left->ts.store(ls, Storage(STACK), x64);
        right->ts.store(rs, Storage(STACK), x64);
        
        x64->op(MOVQ, RAX, Address(RSP, 8));
        x64->op(MOVQ, RBX, Address(RAX, 0));
        
        x64->op(MOVQ, RAX, Address(RSP, 0));
        x64->op(ADDQ, RBX, Address(RAX, 0));  // total size
        
        x64->op(IMUL3Q, RAX, RBX, size);
        x64->op(ADDQ, RAX, 8);
        
        x64->alloc();  // Allocate this many bytes with a refcount of 1, return in RAX
        
        x64->op(MOVQ, Address(RAX, 0), RBX);
        x64->op(LEA, RDI, Address(RAX, 8));
        
        x64->op(MOVQ, RSI, Address(RSP, 8));
        x64->op(IMUL3Q, RCX, Address(RSI, 0), size);
        x64->op(ADDQ, RSI, 8);
        x64->op(REPMOVSB);

        x64->op(MOVQ, RSI, Address(RSP, 0));
        x64->op(IMUL3Q, RCX, Address(RSI, 0), size);
        x64->op(ADDQ, RSI, 8);
        x64->op(REPMOVSB);
        
        right->ts.store(Storage(STACK), Storage(), x64);
        left->ts.store(Storage(STACK), Storage(), x64);
        
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
        return clob.add(RAX).add(RBX);
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
            x64->op(MOVQ, RBX, rs.value * size + 8);
            break;
        case REGISTER:
            x64->op(IMUL3Q, RBX, rs.reg, size);
            x64->op(ADDQ, RBX, 8);
            break;
        case MEMORY:
            x64->op(IMUL3Q, RBX, rs.address, size);
            x64->op(ADDQ, RBX, 8);
            break;
        default:
            throw INTERNAL_ERROR;
        }
            
        x64->op(MOVQ, RAX, ls.address);
        x64->cmpref(RAX, 1);
        x64->op(JNE, end);
        
        x64->realloc();
        
        x64->op(MOVQ, ls.address, RAX);
        
        return Storage();
    }
};

