
class SimpleRecordValue: public GenericValue {
public:
    SimpleRecordValue(TypeSpec ret_ts, Value *pivot)
        :GenericValue(VOID_TS, ret_ts, pivot) {
    }

    virtual Regs precompile(Regs preferred) {
        return left->precompile(preferred);
    }
};


// Counters

class CountupNextValue: public GenericValue {
public:
    Declaration *dummy;
    Regs clob;
    
    CountupNextValue(Value *l, TypeMatch &match)
        :GenericValue(VOID_TS, INTEGER_TS, l) {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!check_arguments(args, kwargs, {}))
            return false;
            
        dummy = new Declaration;
        scope->add(dummy);

        return true;
    }

    virtual Regs precompile(Regs preferred) {
        clob = left->precompile(preferred);
        
        if (!clob.has_any())
            clob.add(RAX);
        
        return clob;
    }

    virtual void advance(Address addr, X64 *x64) {
        x64->op(INCQ, addr);
    }

    virtual Storage compile(X64 *x64) {
        ls = left->compile(x64);
        Register reg = (clob & ~ls.regs()).get_any();
        Label ok;
        
        switch (ls.where) {
        case MEMORY:
            x64->op(MOVQ, reg, ls.address + INTEGER_SIZE);  // value
            x64->op(CMPQ, reg, ls.address); // limit
            x64->op(JNE, ok);
            x64->op(MOVB, EXCEPTION_ADDRESS, DONE_EXCEPTION);
            x64->unwind->initiate(dummy, x64);
            x64->code_label(ok);
            advance(ls.address + INTEGER_SIZE, x64);
            return Storage(REGISTER, reg);
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class CountdownNextValue: public CountupNextValue {
public:

    CountdownNextValue(Value *l, TypeMatch &match)
        :CountupNextValue(l, match) {
    }
    
    virtual void advance(Address addr, X64 *x64) {
        x64->op(DECQ, addr);
    }
};


class CountupValue: public SimpleRecordValue {
public:
    CountupValue(Value *l, TypeMatch &match)
        :SimpleRecordValue(COUNTUP_TS, l) {
    }

    virtual Storage compile(X64 *x64) {
        ls = left->compile(x64);  // integer limit
        
        x64->op(PUSHQ, 0);  // value
        
        switch (ls.where) {
        case CONSTANT:
            x64->op(PUSHQ, ls.value);
            break;
        case REGISTER:
            x64->op(PUSHQ, ls.reg);
            break;
        case MEMORY:
            x64->op(PUSHQ, ls.address);
            break;
        default:
            throw INTERNAL_ERROR;
        }
        
        return Storage(STACK);
    }
};


class CountdownValue: public SimpleRecordValue {
public:
    CountdownValue(Value *l, TypeMatch &match)
        :SimpleRecordValue(COUNTDOWN_TS, l) {
    }

    virtual Storage compile(X64 *x64) {
        ls = left->compile(x64);
        
        switch (ls.where) {  // value
        case CONSTANT:
            x64->op(PUSHQ, ls.value - 1);
            break;
        case REGISTER:
            x64->op(DECQ, ls.reg);
            x64->op(PUSHQ, ls.reg);
            break;
        case MEMORY:
            x64->op(MOVQ, RBX, ls.address);
            x64->op(DECQ, RBX);
            x64->op(PUSHQ, RBX);
            break;
        default:
            throw INTERNAL_ERROR;
        }
        
        x64->op(PUSHQ, -1);
        
        return Storage(STACK);
    }
};


// Array iterator initializers

class ArrayIterValue: public SimpleRecordValue {
public:
    ArrayIterValue(TypeSpec t, Value *l)
        :SimpleRecordValue(t, l) {
    }

    virtual Storage compile(X64 *x64) {
        x64->op(PUSHQ, 0);

        left->compile_and_store(x64, Storage(STACK));
        return Storage(STACK);
    }
};

class ArrayElemIterValue: public ArrayIterValue {
public:
    ArrayElemIterValue(Value *l, TypeMatch &match)
        :ArrayIterValue(typesubst(SAME_ARRAYELEMITER_TS, match), l) {
    }
};


class ArrayIndexIterValue: public ArrayIterValue {
public:
    ArrayIndexIterValue(Value *l, TypeMatch &match)
        :ArrayIterValue(typesubst(SAME_ARRAYINDEXITER_TS, match), l) {
    }
};


class ArrayItemIterValue: public ArrayIterValue {
public:
    ArrayItemIterValue(Value *l, TypeMatch &match)
        :ArrayIterValue(typesubst(SAME_ARRAYITEMITER_TS, match), l) {
    }
};


class CircularrayElemIterValue: public ArrayIterValue {
public:
    CircularrayElemIterValue(Value *l, TypeMatch &match)
        :ArrayIterValue(typesubst(SAME_CIRCULARRAYELEMITER_TS, match), l) {
    }
};


class CircularrayIndexIterValue: public ArrayIterValue {
public:
    CircularrayIndexIterValue(Value *l, TypeMatch &match)
        :ArrayIterValue(typesubst(SAME_CIRCULARRAYINDEXITER_TS, match), l) {
    }
};


class CircularrayItemIterValue: public ArrayIterValue {
public:
    CircularrayItemIterValue(Value *l, TypeMatch &match)
        :ArrayIterValue(typesubst(SAME_CIRCULARRAYITEMITER_TS, match), l) {
    }
};


// Array iterator next methods

class ArrayNextValue: public GenericValue {
public:
    Declaration *dummy;
    Regs clob;
    bool is_down;
    TypeSpec elem_ts;
    
    ArrayNextValue(TypeSpec t, TypeSpec et, Value *l, bool d)
        :GenericValue(VOID_TS, t, l) {
        is_down = d;
        elem_ts = et;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!check_arguments(args, kwargs, {}))
            return false;

        dummy = new Declaration;
        scope->add(dummy);
        
        return true;
    }

    virtual Regs precompile(Regs preferred) {
        clob = left->precompile(preferred);
        
        if (!clob.has_any())
            clob.add(RAX);
        
        return clob;
    }

    virtual Storage next_compile(int elem_size, Register reg, X64 *x64) {
        // reg contains the (non-refcounted) array reference, RBX the index
        throw INTERNAL_ERROR;
    }

    virtual Storage compile(X64 *x64) {
        ls = left->compile(x64);  // iterator
        Register reg = (clob & ~ls.regs()).get_any();
        int elem_size = ::elem_size(elem_ts.measure(MEMORY));
        Label ok;
        
        switch (ls.where) {
        case MEMORY:
            //std::cerr << "Compiling itemiter with reg=" << reg << " ls=" << ls << "\n";
            x64->op(MOVQ, RBX, ls.address + REFERENCE_SIZE);  // value
            x64->op(MOVQ, reg, ls.address); // array reference without incref
            x64->op(CMPQ, RBX, x64->array_length_address(reg));
            x64->op(JNE, ok);
            
            x64->op(MOVB, EXCEPTION_ADDRESS, DONE_EXCEPTION);
            x64->unwind->initiate(dummy, x64);
            
            x64->code_label(ok);
            x64->op(is_down ? DECQ : INCQ, ls.address + REFERENCE_SIZE);
            //x64->err("NEXT COMPILE");
            return next_compile(elem_size, reg, x64);
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class ArrayNextElemValue: public ArrayNextValue {
public:
    ArrayNextElemValue(Value *l, TypeMatch &match)
        :ArrayNextValue(match[1], match[1], l, false) {
    }

    virtual void fix_index(Register r, X64 *x64) {
        // r contains the array reference, RBX the index
    }
    
    virtual Storage next_compile(int elem_size, Register reg, X64 *x64) {
        fix_index(reg, x64);
        
        x64->op(IMUL3Q, RBX, RBX, elem_size);
        x64->op(ADDQ, reg, RBX);
        return Storage(MEMORY, x64->array_elems_address(reg));
    }
};


class ArrayNextIndexValue: public ArrayNextValue {
public:
    ArrayNextIndexValue(Value *l, TypeMatch &match)
        :ArrayNextValue(INTEGER_TS, match[1], l, false) {
    }
    
    virtual Storage next_compile(int elem_size, Register reg, X64 *x64) {
        x64->op(MOVQ, reg, RBX);
        return Storage(REGISTER, reg);
    }
};


class ArrayNextItemValue: public ArrayNextValue {
public:
    //TypeSpec elem_ts;
    
    ArrayNextItemValue(Value *l, TypeMatch &match)
        :ArrayNextValue(typesubst(SAME_ITEM_TS, match), match[1], l, false) {
        //elem_ts = match[1];
    }

    virtual void fix_index(Register r, X64 *x64) {
        // r contains the array reference, RBX the index
    }

    virtual Storage next_compile(int elem_size, Register reg, X64 *x64) {
        x64->op(SUBQ, RSP, ts.measure(STACK));
        x64->op(MOVQ, Address(RSP, 0), RBX);
        
        fix_index(reg, x64);
        
        x64->op(IMUL3Q, RBX, RBX, elem_size);
        x64->op(ADDQ, reg, RBX);
        
        Storage s = Storage(MEMORY, x64->array_elems_address(reg));
        Storage t = Storage(MEMORY, Address(RSP, INTEGER_SIZE));
        elem_ts.create(s, t, x64);
        
        return Storage(STACK);
    }
};


class CircularrayNextElemValue: public ArrayNextElemValue {
public:
    CircularrayNextElemValue(Value *l, TypeMatch &match)
        :ArrayNextElemValue(l, match) {
    }
    
    virtual void fix_index(Register r, X64 *x64) {
        fix_index_overflow(r, x64);
    }
};


class CircularrayNextIndexValue: public ArrayNextIndexValue {  // for completeness only
public:
    CircularrayNextIndexValue(Value *l, TypeMatch &match)
        :ArrayNextIndexValue(l, match) {
    }
};


class CircularrayNextItemValue: public ArrayNextItemValue {
public:
    CircularrayNextItemValue(Value *l, TypeMatch &match)
        :ArrayNextItemValue(l, match) {
    }
    
    virtual void fix_index(Register r, X64 *x64) {
        fix_index_overflow(r, x64);
    }
};


// Aatree iterator initializers

class AatreeElemIterValue: public SimpleRecordValue {
public:
    AatreeElemIterValue(Value *l, TypeMatch &match)
        :SimpleRecordValue(typesubst(SAME_AATREEELEMITER_TS, match), l) {
    }

    virtual Storage compile(X64 *x64) {
        left->compile_and_store(x64, Storage(STACK));
        
        x64->op(POPQ, RBX);
        x64->op(PUSHQ, Address(RBX, AATREE_FIRST_OFFSET));
        x64->op(PUSHQ, RBX);
        
        return Storage(STACK);
    }
};


class AatreeNextElemValue: public GenericValue {
public:
    Declaration *dummy;
    Regs clob;
    bool is_down;
    TypeSpec elem_ts;

    AatreeNextElemValue(Value *l, TypeMatch &match)
        :GenericValue(VOID_TS, match[1], l) {
        is_down = false;  // TODO: get as argument for backward iteration!
        elem_ts = match[1];
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!check_arguments(args, kwargs, {}))
            return false;

        dummy = new Declaration;
        scope->add(dummy);
        
        return true;
    }

    virtual Regs precompile(Regs preferred) {
        clob = left->precompile(preferred);
        
        if (!clob.has_any())
            clob.add(RAX);
        
        return clob;
    }

    virtual Storage compile(X64 *x64) {
        ls = left->compile(x64);  // iterator
        Register reg = (clob & ~ls.regs()).get_any();
        //int elem_size = ::elem_size(elem_ts.measure(MEMORY));
        Label ok;
        
        switch (ls.where) {
        case MEMORY:
            x64->op(MOVQ, RBX, ls.address + REFERENCE_SIZE);  // offset
            x64->op(MOVQ, reg, ls.address); // tree reference without incref
            x64->op(CMPQ, RBX, AANODE_NIL);
            x64->op(JNE, ok);
            
            x64->op(MOVB, EXCEPTION_ADDRESS, DONE_EXCEPTION);
            x64->unwind->initiate(dummy, x64);
            
            x64->code_label(ok);
            x64->op(ADDQ, reg, RBX);
            x64->op(MOVQ, RBX, Address(reg, is_down? AANODE_PREV_IS_RED_OFFSET : AANODE_NEXT_OFFSET));
            x64->op(MOVQ, ls.address + REFERENCE_SIZE, RBX);
            
            return Storage(MEMORY, Address(reg, AANODE_VALUE_OFFSET));
        default:
            throw INTERNAL_ERROR;
        }
    }
};
