#include "../plum.h"


EqualityValue::EqualityValue(bool n, Value *v)
    :Value(BOOLEAN_TS) {
    no = n;
    value.reset(v);
}

bool EqualityValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    return value->check(args, kwargs, scope);
}

Regs EqualityValue::precompile(Regs preferred) {
    return value->precompile(preferred);
}

Storage EqualityValue::compile(Cx *cx) {
    // Returns a boolean if the arguments were equal
    Storage s = value->compile(cx);

    if (!no)
        return s;
    
    switch (s.where) {
    case CONSTANT:
        return Storage(CONSTANT, !s.value);
    case FLAGS:
        return Storage(FLAGS, negated(s.cc));
    case REGISTER:
        cx->op(CMPB, s.reg, 0);
        return Storage(FLAGS, SETE);
    case MEMORY:
        cx->op(CMPB, s.address, 0);
        return Storage(FLAGS, SETE);
    default:
        throw INTERNAL_ERROR;
    }
}




ComparisonValue::ComparisonValue(ConditionCode c, Value *v)
    :Value(BOOLEAN_TS) {
    cc = c;
    value.reset(v);
}

bool ComparisonValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    return value->check(args, kwargs, scope);
}

Regs ComparisonValue::precompile(Regs preferred) {
    return value->precompile(preferred);
}

Storage ComparisonValue::compile(Cx *cx) {
    // Value returns an integer representing the ordering of the arguments
    Storage s = value->compile(cx);

    switch (s.where) {
    case REGISTER:
        cx->op(CMPB, s.reg, 0);
        break;
    case MEMORY:
        cx->op(CMPB, s.address, 0);
        break;
    default:
        throw INTERNAL_ERROR;
    }

    return Storage(FLAGS, cc);
}




BulkEqualityMatcherValue::BulkEqualityMatcherValue(Value *p)
    :Value(p->ts.rvalue()) {
    pivot_value.reset(p);
}

bool BulkEqualityMatcherValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    if (!check_raise(match_unmatched_exception_type, scope))
        return false;

    if (kwargs.size() != 0)
        return false;
        
    TypeSpec arg_ts = pivot_value->ts.rvalue();
        
    for (auto &a : args) {
        Value *v = typize(a.get(), scope, &arg_ts);
        
        values.push_back(std::unique_ptr<Value>(v));
    }

    return true;
}

Regs BulkEqualityMatcherValue::precompile(Regs preferred) {
    Regs clob;
    
    for (auto &v : values)
        clob = clob | v->precompile_tail();
        
    clob = clob | pivot_value->precompile(preferred & ~clob);
        
    return clob | EQUAL_CLOB;
}

Storage BulkEqualityMatcherValue::compile(Cx *cx) {
    Label equal;
    Storage ps = pivot_value->compile(cx);
    
    // The switch variables are read only, no need to push a copy
    if (!am_implicit_matcher) {
        pivot_value->ts.store(ps, Storage(STACK), cx);
        ps = Storage(STACK);
    }

    for (auto &v : values) {
        int pop = 0;
        Storage vs = v->compile(cx);
        Storage xps = ps;
        
        if (vs.where == STACK) {
            // NOTE: we use that destroy does not change values
            vs = Storage(MEMORY, Address(RSP, 0));
            v->ts.destroy(vs, cx);
            pop = v->ts.measure_stack();
            
            if (xps.where == STACK)
                xps = Storage(MEMORY, Address(RSP, pop));
        }
        else {
            if (xps.where == STACK)
                xps = Storage(MEMORY, Address(RSP, 0));
        }
        
        pivot_value->ts.equal(xps, vs, cx);  // sets flags
        
        if (pop)
            cx->op(LEA, RSP, Address(RSP, pop));  // preserve flags
        
        cx->op(JE, equal);
    }

    drop_and_raise(pivot_value->ts, ps, "UNMATCHED", cx);
    
    cx->code_label(equal);
    
    return ps;
}
