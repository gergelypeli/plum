#include "../plum.h"


OptionNoneValue::OptionNoneValue(TypeSpec ts)
    :Value(ts) {
}

Regs OptionNoneValue::precompile(Regs preferred) {
    return Regs();
}

Storage OptionNoneValue::compile(Cx *cx) {
    ts.store(Storage(), Storage(STACK), cx);
    return Storage(STACK);
}




OptionSomeValue::OptionSomeValue(TypeSpec ts)
    :Value(ts) {
    flag_size = OptionType::get_flag_size(ts.unprefix(option_type));
}

bool OptionSomeValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    TypeSpec some_ts = ts.unprefix(option_type);
    return check_arguments(args, kwargs, {{ "some", &some_ts, scope, &some }});
}

Regs OptionSomeValue::precompile(Regs preferred) {
    return some->precompile(preferred);
}

Storage OptionSomeValue::compile(Cx *cx) {
    some->compile_and_store(cx, Storage(STACK));
    
    if (flag_size == ADDRESS_SIZE)
        cx->op(PUSHQ, OPTION_FLAG_NONE + 1);
    else if (flag_size)
        throw INTERNAL_ERROR;
        
    return Storage(STACK);
}




OptionOperationValue::OptionOperationValue(OperationType o, Value *p, TypeMatch &match)
    :GenericOperationValue(o, op_arg_ts(o, match), op_ret_ts(o, match), p) {
}




OptionNoneMatcherValue::OptionNoneMatcherValue(Value *p, TypeMatch &match)
    :GenericValue(NO_TS, VOID_TS, p) {
}

bool OptionNoneMatcherValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    if (!check_raise(match_unmatched_exception_type, scope))
        return false;
    
    return GenericValue::check(args, kwargs, scope);
}

Regs OptionNoneMatcherValue::precompile(Regs preferred) {
    return left->precompile(preferred);
}

Storage OptionNoneMatcherValue::compile(Cx *cx) {
    Storage ls = left->compile(cx);
    Label ok;
        
    switch (ls.where) {
    case STACK:
        left->ts.destroy(Storage(MEMORY, Address(RSP, 0)), cx);
        cx->op(CMPQ, Address(RSP, 0), OPTION_FLAG_NONE);
        cx->op(LEA, RSP, Address(RSP, left->ts.measure_stack()));  // discard STACK, keep flags
        cx->op(JE, ok);
        
        raise("UNMATCHED", cx);

        cx->code_label(ok);
        return Storage();
    case MEMORY:
        cx->op(CMPQ, ls.address, OPTION_FLAG_NONE);
        cx->op(JE, ok);

        raise("UNMATCHED", cx);
        
        cx->code_label(ok);
        return Storage();
    default:
        throw INTERNAL_ERROR;
    }
}




OptionSomeMatcherValue::OptionSomeMatcherValue(Value *p, TypeMatch &match)
    :GenericValue(NO_TS, match[1], p) {
    flag_size = OptionType::get_flag_size(match[1]);
}

bool OptionSomeMatcherValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    if (!check_raise(match_unmatched_exception_type, scope))
        return false;
    
    return GenericValue::check(args, kwargs, scope);
}

Regs OptionSomeMatcherValue::precompile(Regs preferred) {
    return left->precompile(preferred);
}

Storage OptionSomeMatcherValue::compile(Cx *cx) {
    Storage ls = left->compile(cx);
    Label ok;
        
    switch (ls.where) {
    case STACK: {
        cx->op(CMPQ, Address(RSP, 0), OPTION_FLAG_NONE);
        cx->op(JNE, ok);

        drop_and_raise(left->ts, ls, "UNMATCHED", cx);

        cx->code_label(ok);
        if (flag_size == ADDRESS_SIZE)
            cx->op(ADDQ, RSP, ADDRESS_SIZE);
        else if (flag_size)
            throw INTERNAL_ERROR;
            
        return Storage(STACK);
    }
    case MEMORY:
        cx->op(CMPQ, ls.address, OPTION_FLAG_NONE);
        cx->op(JNE, ok);
        
        drop_and_raise(left->ts, ls, "UNMATCHED", cx);
        
        cx->code_label(ok);
        return Storage(MEMORY, ls.address + flag_size);
    default:
        throw INTERNAL_ERROR;
    }
}




UnionValue::UnionValue(TypeSpec ts, TypeSpec ats, int ti)
    :Value(ts) {
    arg_ts = ats;
    tag_index = ti;
}

bool UnionValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    return check_arguments(args, kwargs, {{ "value", &arg_ts, scope, &value }});
}

Regs UnionValue::precompile(Regs preferred) {
    if (value)
        return value->precompile_tail();
    else
        return Regs();
}

Storage UnionValue::compile(Cx *cx) {
    unsigned flag_size = UnionType::get_flag_size();
    unsigned full_size = ts.measure_stack();
    unsigned arg_size = arg_ts.measure_stack();
    unsigned pad_count = (full_size - flag_size - arg_size) / ADDRESS_SIZE;
    
    for (unsigned i = 0; i < pad_count; i++)
        cx->op(PUSHQ, 0);
        
    if (value)
        value->compile_and_store(cx, Storage(STACK));
    
    cx->op(PUSHQ, tag_index);
        
    return Storage(STACK);
}




UnionMatcherValue::UnionMatcherValue(Value *p, TypeSpec uts, TypeSpec rts, int ti)
    :GenericValue(NO_TS, rts, p) {
    union_ts = uts;
    tag_index = ti;
}

bool UnionMatcherValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    if (!check_raise(match_unmatched_exception_type, scope))
        return false;
    
    return GenericValue::check(args, kwargs, scope);
}

Regs UnionMatcherValue::precompile(Regs preferred) {
    return left->precompile(preferred);
}

Storage UnionMatcherValue::compile(Cx *cx) {
    Storage ls = left->compile(cx);
    Label ok;
        
    switch (ls.where) {
    case STACK: {
        cx->op(CMPQ, Address(RSP, 0), tag_index);
        cx->op(JE, ok);

        drop_and_raise(left->ts, ls, "UNMATCHED", cx);

        cx->code_label(ok);
        
        // Since the matched type may be smaller than the maximum of the member sizes,
        // we may need to shift the value up a bit on the stack. Not nice.
        
        int shift_count = (left->ts.measure_stack() - ADDRESS_SIZE - ts.measure_stack()) / ADDRESS_SIZE;
        int copy_count = ts.measure_stack() / ADDRESS_SIZE;
        
        for (int i = 0; i < copy_count; i++) {
            cx->op(MOVQ, R10, Address(RSP, ADDRESS_SIZE * (copy_count - i)));
            cx->op(MOVQ, Address(RSP, ADDRESS_SIZE * (copy_count - i + shift_count)), R10);
        }

        cx->op(ADDQ, RSP, ADDRESS_SIZE * (1 + shift_count));
        
        return Storage(STACK);
    }
    case MEMORY:
        cx->op(CMPQ, ls.address, tag_index);
        cx->op(JE, ok);
        
        drop_and_raise(left->ts, ls, "UNMATCHED", cx);
        
        cx->code_label(ok);
        return Storage(MEMORY, ls.address + ADDRESS_SIZE);
    default:
        throw INTERNAL_ERROR;
    }
}
