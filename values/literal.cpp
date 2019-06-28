#include "../plum.h"


BasicValue::BasicValue(TypeSpec ts, int64 n)
    :Value(ts) {
    reg = NOREG;
    number = n;
}

Regs BasicValue::precompile(Regs preferred) {
    if (number < -2147483648 || number > 2147483647) {
        reg = preferred.get_gpr();
        return Regs(reg);
    }
    else
        return Regs();
}

Storage BasicValue::compile(Cx *cx) {
    if (number < -2147483648 || number > 2147483647) {
        cx->op(MOVABSQ, reg, number);
        return Storage(REGISTER, reg);
    }
    else {
        // 32-bit signed integers fit in immediate operands
        return Storage(CONSTANT, (int)number);  
    }
}




FloatValue::FloatValue(TypeSpec ts, double n)
    :Value(ts) {
    number = n;
}

Regs FloatValue::precompile(Regs preferred) {
    return Regs();
}

Storage FloatValue::compile(Cx *cx) {
    Label label;
    
    cx->data_label(label);
    cx->data_double(number);
    
    return Storage(MEMORY, Address(label, 0));
}




UnicodeCharacterValue::UnicodeCharacterValue()
    :Value(CHARACTER_TS) {
}

bool UnicodeCharacterValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    return check_arguments(args, kwargs, { { "code", &INTEGER_TS, scope, &value } });
}

Regs UnicodeCharacterValue::precompile(Regs preferred) {
    return value->precompile(preferred);
}

Storage UnicodeCharacterValue::compile(Cx *cx) {
    Storage s = value->compile(cx);
    
    switch (s.where) {
    case CONSTANT:
        return Storage(CONSTANT, s.value & 0xFFFF);
    case REGISTER:
        return Storage(REGISTER, s.reg);
    case MEMORY:
        return Storage(MEMORY, s.address);
    default:
        throw INTERNAL_ERROR;
    }
}




StringLiteralValue::StringLiteralValue(std::ustring ut)
    :Value(STRING_TS) {
    utext = ut;
}

Regs StringLiteralValue::precompile(Regs preferred) {
    return Regs(RAX);
}

Storage StringLiteralValue::compile(Cx *cx) {
    Label l = cx->runtime->data_heap_string(utext);
    Label dl;
    cx->data_label(dl);
    cx->data_reference(l);
    
    return Storage(MEMORY, Address(dl, 0));
}




StringTemplateValue::StringTemplateValue(std::vector<std::ustring> f)
    :Value(STRINGTEMPLATE_TS) {
    fragments = f;
}

Regs StringTemplateValue::precompile(Regs preferred) {
    throw INTERNAL_ERROR;
}

Storage StringTemplateValue::compile(Cx *cx) {
    throw INTERNAL_ERROR;
}




TreenumerationMatcherValue::TreenumerationMatcherValue(int i, Value *p)
    :GenericValue(NO_TS, p->ts.rvalue(), p) {
    if (i == 0)
        throw INTERNAL_ERROR;
        
    index = i;
}

bool TreenumerationMatcherValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    if (!check_raise(match_unmatched_exception_type, scope))
        return false;
    
    return GenericValue::check(args, kwargs, scope);
}

Regs TreenumerationMatcherValue::precompile(Regs preferred) {
    return left->precompile(preferred);
}

Storage TreenumerationMatcherValue::compile(Cx *cx) {
    TreenumerationType *t = ptr_cast<TreenumerationType>(left->ts.rvalue()[0]);
    Label parents_label = t->get_parents_label(cx);
    Label loop, cond, match;
    
    ls = left->compile(cx);
    
    switch (ls.where) {
    case CONSTANT:
        cx->op(MOVQ, R11, ls.value);
        break;
    case REGISTER:
        cx->op(MOVZXBQ, R11, ls.reg);
        break;
    case STACK:
        cx->op(MOVZXBQ, R11, Address(RSP, 0));
        break;
    case MEMORY:
        cx->op(MOVZXBQ, R11, ls.address);
        break;
    default:
        throw INTERNAL_ERROR;
    }
    
    // R11 always contains one byte of nonzero data, so we can use it for addressing
    cx->op(LEA, R10, Address(parents_label, 0));
    
    cx->code_label(loop);
    cx->op(CMPQ, R11, index);
    cx->op(JE, match);
    
    cx->op(MOVB, R11B, Address(R10, R11, 0));
    
    cx->op(CMPQ, R11, 0);
    cx->op(JNE, loop);
    
    raise("UNMATCHED", cx);
    
    cx->code_label(match);
    
    return ls;
}



TreenumerationAnyMatcherValue::TreenumerationAnyMatcherValue(Value *p)
    :GenericValue(NO_TS, p->ts.rvalue(), p) {
}

bool TreenumerationAnyMatcherValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    return GenericValue::check(args, kwargs, scope);
}

Regs TreenumerationAnyMatcherValue::precompile(Regs preferred) {
    return left->precompile(preferred);
}

Storage TreenumerationAnyMatcherValue::compile(Cx *cx) {
    return left->compile(cx);
}
