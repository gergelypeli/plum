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

Storage BasicValue::compile(X64 *x64) {
    if (number < -2147483648 || number > 2147483647) {
        x64->op(MOVABSQ, reg, number);
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

Storage FloatValue::compile(X64 *x64) {
    Label label;
    
    x64->data_label(label);
    x64->data_double(number);
    
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

Storage UnicodeCharacterValue::compile(X64 *x64) {
    Storage s = value->compile(x64);
    
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

Storage StringLiteralValue::compile(X64 *x64) {
    Label l = x64->runtime->data_heap_string(utext);
    Label dl;
    x64->data_label(dl);
    x64->data_reference(l);
    
    return Storage(MEMORY, Address(dl, 0));
}




StringTemplateValue::StringTemplateValue(std::vector<std::ustring> f)
    :Value(STRINGTEMPLATE_TS) {
    fragments = f;
}

Regs StringTemplateValue::precompile(Regs preferred) {
    throw INTERNAL_ERROR;
}

Storage StringTemplateValue::compile(X64 *x64) {
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

Storage TreenumerationMatcherValue::compile(X64 *x64) {
    TreenumerationType *t = ptr_cast<TreenumerationType>(left->ts.rvalue()[0]);
    Label parents_label = t->get_parents_label(x64);
    Label loop, cond, match;
    
    ls = left->compile(x64);
    
    switch (ls.where) {
    case CONSTANT:
        x64->op(MOVQ, R11, ls.value);
        break;
    case REGISTER:
        x64->op(MOVZXBQ, R11, ls.reg);
        break;
    case STACK:
        x64->op(MOVZXBQ, R11, Address(RSP, 0));
        break;
    case MEMORY:
        x64->op(MOVZXBQ, R11, ls.address);
        break;
    default:
        throw INTERNAL_ERROR;
    }
    
    // R11 always contains one byte of nonzero data, so we can use it for addressing
    x64->op(LEA, R10, Address(parents_label, 0));
    
    x64->code_label(loop);
    x64->op(CMPQ, R11, index);
    x64->op(JE, match);
    
    x64->op(MOVB, R11B, Address(R10, R11, 0));
    
    x64->op(CMPQ, R11, 0);
    x64->op(JNE, loop);
    
    raise("UNMATCHED", x64);
    
    x64->code_label(match);
    
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

Storage TreenumerationAnyMatcherValue::compile(X64 *x64) {
    return left->compile(x64);
}
