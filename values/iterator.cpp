
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

class ContainerNextValue: public GenericValue {
public:
    Declaration *dummy;
    Regs clob;
    bool is_down;
    TypeSpec elem_ts;
    
    ContainerNextValue(TypeSpec ts, TypeSpec ets, Value *l, bool d)
        :GenericValue(VOID_TS, ts, l) {
        is_down = d;
        elem_ts = ets;
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

    virtual Storage subcompile(int length_offset, X64 *x64) {
        ls = left->compile(x64);  // iterator
        Register reg = (clob & ~ls.regs()).get_any();
        Label ok;
        
        switch (ls.where) {
        case MEMORY:
            //std::cerr << "Compiling itemiter with reg=" << reg << " ls=" << ls << "\n";
            x64->op(MOVQ, RBX, ls.address + REFERENCE_SIZE);  // value
            x64->op(MOVQ, reg, ls.address); // array reference without incref
            x64->op(CMPQ, RBX, Address(reg, length_offset));
            x64->op(JNE, ok);
            
            x64->op(MOVB, EXCEPTION_ADDRESS, DONE_EXCEPTION);
            x64->unwind->initiate(dummy, x64);
            
            x64->code_label(ok);
            x64->op(is_down ? DECQ : INCQ, ls.address + REFERENCE_SIZE);
            //x64->err("NEXT COMPILE");
            return Storage(REGISTER, reg);  // non-refcounted reference, with RBX index
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class ArrayNextElemValue: public ContainerNextValue {
public:
    ArrayNextElemValue(Value *l, TypeMatch &match)
        :ContainerNextValue(match[1].varvalue(), match[1].varvalue(), l, false) {
    }

    virtual Storage compile(X64 *x64) {
        int elem_size = ::elem_size(elem_ts.measure(MEMORY));
        
        Storage r = subcompile(ARRAY_LENGTH_OFFSET, x64);
        
        x64->op(IMUL3Q, RBX, RBX, elem_size);
        x64->op(LEA, r.reg, Address(r.reg, RBX, ARRAY_ELEMS_OFFSET));
        
        return Storage(MEMORY, Address(r.reg, 0));
    }
};


class ArrayNextIndexValue: public ContainerNextValue {
public:
    ArrayNextIndexValue(Value *l, TypeMatch &match)
        :ContainerNextValue(INTEGER_TS, match[1].varvalue(), l, false) {
    }
    
    virtual Storage compile(X64 *x64) {
        Storage r = subcompile(ARRAY_LENGTH_OFFSET, x64);
        
        x64->op(MOVQ, r.reg, RBX);
        
        return Storage(REGISTER, r.reg);
    }
};


class ArrayNextItemValue: public ContainerNextValue {
public:
    ArrayNextItemValue(Value *l, TypeMatch &match)
        :ContainerNextValue(typesubst(SAME_ITEM_TS, match), match[1].varvalue(), l, false) {
    }

    virtual Storage compile(X64 *x64) {
        int elem_size = ::elem_size(elem_ts.measure(MEMORY));

        Storage r = subcompile(ARRAY_LENGTH_OFFSET, x64);

        x64->op(SUBQ, RSP, ts.measure(STACK));
        x64->op(MOVQ, Address(RSP, 0), RBX);
        
        x64->op(IMUL3Q, RBX, RBX, elem_size);
        x64->op(LEA, r.reg, Address(r.reg, RBX, ARRAY_ELEMS_OFFSET));
        
        Storage s = Storage(MEMORY, Address(r.reg, 0));
        Storage t = Storage(MEMORY, Address(RSP, INTEGER_SIZE));
        elem_ts.create(s, t, x64);
        
        return Storage(STACK);
    }
};


class CircularrayNextElemValue: public ContainerNextValue {
public:
    CircularrayNextElemValue(Value *l, TypeMatch &match)
        :ContainerNextValue(match[1].varvalue(), match[1].varvalue(), l, false) {
    }

    virtual Storage compile(X64 *x64) {
        int elem_size = ::elem_size(elem_ts.measure(MEMORY));

        Storage r = subcompile(CIRCULARRAY_LENGTH_OFFSET, x64);

        fix_RBX_index_overflow(r.reg, x64);
        
        x64->op(IMUL3Q, RBX, RBX, elem_size);
        x64->op(LEA, r.reg, Address(r.reg, RBX, CIRCULARRAY_ELEMS_OFFSET));
        
        return Storage(MEMORY, Address(r.reg, 0));
    }
};


class CircularrayNextIndexValue: public ContainerNextValue {
public:
    CircularrayNextIndexValue(Value *l, TypeMatch &match)
        :ContainerNextValue(INTEGER_TS, match[1].varvalue(), l, false) {
    }
    
    virtual Storage compile(X64 *x64) {
        Storage r = subcompile(CIRCULARRAY_LENGTH_OFFSET, x64);
        
        x64->op(MOVQ, r.reg, RBX);
        
        return Storage(REGISTER, r.reg);
    }
};


class CircularrayNextItemValue: public ContainerNextValue {
public:
    CircularrayNextItemValue(Value *l, TypeMatch &match)
        :ContainerNextValue(typesubst(SAME_ITEM_TS, match), match[1].varvalue(), l, false) {
    }

    virtual Storage compile(X64 *x64) {
        int elem_size = ::elem_size(elem_ts.measure(MEMORY));

        Storage r = subcompile(CIRCULARRAY_LENGTH_OFFSET, x64);
        
        x64->op(SUBQ, RSP, ts.measure(STACK));
        x64->op(MOVQ, Address(RSP, 0), RBX);

        fix_RBX_index_overflow(r.reg, x64);

        x64->op(IMUL3Q, RBX, RBX, elem_size);
        x64->op(LEA, r.reg, Address(r.reg, RBX, CIRCULARRAY_ELEMS_OFFSET));
        
        Storage s = Storage(MEMORY, Address(r.reg, 0));
        Storage t = Storage(MEMORY, Address(RSP, INTEGER_SIZE));
        elem_ts.create(s, t, x64);
        
        return Storage(STACK);
    }
};


// Rbtree iterator initializers

class RbtreeElemIterValue: public SimpleRecordValue {
public:
    RbtreeElemIterValue(Value *l, TypeMatch &match)
        :SimpleRecordValue(typesubst(SAME_RBTREEELEMITER_TS, match), l) {
    }

    virtual Storage compile(X64 *x64) {
        left->compile_and_store(x64, Storage(STACK));
        
        x64->op(POPQ, RBX);
        x64->op(PUSHQ, Address(RBX, RBTREE_FIRST_OFFSET));
        x64->op(PUSHQ, RBX);
        
        return Storage(STACK);
    }
};


class RbtreeNextElemValue: public GenericValue {
public:
    Declaration *dummy;
    Regs clob;
    bool is_down;
    TypeSpec elem_ts;

    RbtreeNextElemValue(Value *l, TypeMatch &match)
        :GenericValue(VOID_TS, match[1], l) {
        is_down = false;  // TODO: get as argument for backward iteration!
        elem_ts = match[1].varvalue();
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
            x64->op(CMPQ, RBX, RBNODE_NIL);
            x64->op(JNE, ok);
            
            x64->op(MOVB, EXCEPTION_ADDRESS, DONE_EXCEPTION);
            x64->unwind->initiate(dummy, x64);
            
            x64->code_label(ok);
            x64->op(ADDQ, reg, RBX);
            
            if (is_down) {
                x64->op(MOVQ, RBX, Address(reg, RBNODE_PREV_IS_RED_OFFSET));
                x64->op(ANDQ, RBX, -2);  // remove color bit
            }
            else {
                x64->op(MOVQ, RBX, Address(reg, RBNODE_NEXT_OFFSET));
            }
            
            x64->op(MOVQ, ls.address + REFERENCE_SIZE, RBX);
            
            return Storage(MEMORY, Address(reg, RBNODE_VALUE_OFFSET));
        default:
            throw INTERNAL_ERROR;
        }
    }
};
