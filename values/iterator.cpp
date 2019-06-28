#include "../plum.h"


SimpleRecordValue::SimpleRecordValue(TypeSpec ret_ts, Value *pivot)
    :GenericValue(NO_TS, ret_ts, pivot) {
}

Regs SimpleRecordValue::precompile(Regs preferred) {
    return left->precompile(preferred);
}


// Counters

CountupNextValue::CountupNextValue(Value *l, TypeMatch &match)
    :GenericValue(NO_TS, INTEGER_TUPLE1_TS, l) {
}

bool CountupNextValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    if (!check_arguments(args, kwargs, {}))
        return false;
        
    if (!check_raise(iterator_done_exception_type, scope))
        return false;

    return true;
}

Regs CountupNextValue::precompile(Regs preferred) {
    clob = left->precompile(preferred);

    clob.reserve_gpr(3);
    
    return clob;
}

void CountupNextValue::advance(Address addr, Cx *cx) {
    cx->op(INCQ, addr);
}

Storage CountupNextValue::compile(Cx *cx) {
    ls = left->compile(cx);
    Register reg = (clob & ~ls.regs()).get_gpr();
    Label ok;
    
    switch (ls.where) {
    case MEMORY:
        cx->op(MOVQ, reg, ls.address + INTEGER_SIZE);  // value
        cx->op(CMPQ, reg, ls.address); // limit
        cx->op(JNE, ok);
        
        raise("ITERATOR_DONE", cx);
        
        cx->code_label(ok);
        advance(ls.address + INTEGER_SIZE, cx);
        return Storage(REGISTER, reg);
    default:
        throw INTERNAL_ERROR;
    }
}




CountdownNextValue::CountdownNextValue(Value *l, TypeMatch &match)
    :CountupNextValue(l, match) {
}

void CountdownNextValue::advance(Address addr, Cx *cx) {
    cx->op(DECQ, addr);
}



CountupValue::CountupValue(Value *l, TypeMatch &match)
    :SimpleRecordValue(COUNTUP_TS, l) {
}

Storage CountupValue::compile(Cx *cx) {
    ls = left->compile(cx);  // integer limit
    
    cx->op(PUSHQ, 0);  // value
    
    switch (ls.where) {
    case CONSTANT:
        cx->op(PUSHQ, ls.value);
        break;
    case REGISTER:
        cx->op(PUSHQ, ls.reg);
        break;
    case MEMORY:
        cx->op(PUSHQ, ls.address);
        break;
    default:
        throw INTERNAL_ERROR;
    }
    
    return Storage(STACK);
}



CountdownValue::CountdownValue(Value *l, TypeMatch &match)
    :SimpleRecordValue(COUNTDOWN_TS, l) {
}

Storage CountdownValue::compile(Cx *cx) {
    ls = left->compile(cx);
    
    switch (ls.where) {  // value
    case CONSTANT:
        cx->op(PUSHQ, ls.value - 1);
        break;
    case REGISTER:
        cx->op(DECQ, ls.reg);
        cx->op(PUSHQ, ls.reg);
        break;
    case MEMORY:
        cx->op(MOVQ, R10, ls.address);
        cx->op(DECQ, R10);
        cx->op(PUSHQ, R10);
        break;
    default:
        throw INTERNAL_ERROR;
    }
    
    cx->op(PUSHQ, -1);
    
    return Storage(STACK);
}
