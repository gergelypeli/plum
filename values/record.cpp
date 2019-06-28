#include "../plum.h"


RecordOperationValue::RecordOperationValue(OperationType o, Value *p, TypeMatch &match)
    :GenericOperationValue(o, op_arg_ts(o, match), op_ret_ts(o, match), p) {
    //std::cerr << "XXX Record " << match[0] << " operation " << o << ".\n";
}



StringOperationValue::StringOperationValue(OperationType o, Value *p, TypeMatch &match)
    :RecordOperationValue(o, p, match) {
}

void StringOperationValue::compile_and_stack_both(Cx *cx) {
    // We have a custom comparison functions that takes stack arguments
    ls = left->compile(cx);
    
    switch (ls.where) {
    case REGISTER:
    case MEMORY:
        ls = left->ts.store(ls, Storage(STACK), cx);
        break;
    case STACK:
        break;
    default:
        throw INTERNAL_ERROR;
    }
    
    cx->unwind->push(this);
    rs = right->compile(cx);
    cx->unwind->pop(this);
    
    switch (rs.where) {
    case REGISTER:
    case MEMORY:
        rs = right->ts.store(rs, Storage(STACK), cx);
        break;
    case STACK:
        break;
    default:
        throw INTERNAL_ERROR;
    }
}

Storage StringOperationValue::compare(Cx *cx) {
    compile_and_stack_both(cx);
    
    left->ts.compare(ls, rs, cx);

    Register r = clob.get_gpr();
    cx->op(MOVSXBQ, r, R10B);  // sign extend byte to qword

    right->ts.store(rs, Storage(), cx);
    left->ts.store(ls, Storage(), cx);

    return Storage(REGISTER, r);
}

Storage StringOperationValue::equal(Cx *cx, bool negate) {
    compile_and_stack_both(cx);
    
    left->ts.equal(ls, rs, cx);

    Register r = clob.get_gpr();
    cx->op(negate ? SETNE : SETE, r);

    right->ts.store(rs, Storage(), cx);
    left->ts.store(ls, Storage(), cx);
    
    return Storage(REGISTER, r);
}




RecordInitializerValue::RecordInitializerValue(TypeMatch &tm)
    :Value(tm[0]) {
    record_type = ptr_cast<RecordType>(ts[0]);
    member_tss = record_type->get_member_tss(tm);
    member_names = record_type->get_partial_initializable_names();
    match = tm;
    
    std::cerr << "Record " << record_type->name << " initialization with members: " << member_tss << ".\n";
}

bool RecordInitializerValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    ArgInfos infos;

    // Separate loop, so reallocations won't screw us
    for (unsigned i = 0; i < member_tss.size(); i++) {
        values.push_back(NULL);
        member_tss[i] = member_tss[i].prefix(ovalue_type);  // TODO
    }
    
    for (unsigned i = 0; i < member_tss.size(); i++)
        infos.push_back(ArgInfo { member_names[i].c_str(), &member_tss[i], scope, &values[i] });
    
    return check_arguments(args, kwargs, infos);
}

Regs RecordInitializerValue::precompile(Regs preferred) {
    for (auto &v : values)
        if (v)
            v->precompile_tail();  // All will be pushed
        
    return Regs::all();  // We're too complex to care
}

Storage RecordInitializerValue::compile(Cx *cx) {
    cx->op(SUBQ, RSP, ts.measure_stack());

    cx->unwind->push(this);
    
    for (unsigned i = 0; i < values.size(); i++) {
        Variable *var = record_type->member_variables[i];
        TypeSpec var_ts = member_tss[i];
        Value *v = values[i].get();
        Storage s;
        
        if (v)
            s = v->compile(cx);
        
        int offset = 0;
        
        if (s.where == STACK)
            offset = var_ts.measure_stack();

        Storage t = var->get_storage(match, Storage(MEMORY, Address(RSP, offset)));
        
        var_ts.create(s, t, cx);
        
        var_storages.push_back(t + (-offset));
    }
    
    cx->unwind->pop(this);

    return Storage(STACK);
}

CodeScope *RecordInitializerValue::unwind(Cx *cx) {
    for (int i = var_storages.size() - 1; i >= 0; i--)
        record_type->member_variables[i]->alloc_ts.destroy(var_storages[i], cx);

    cx->op(ADDQ, RSP, ts.measure_stack());
        
    return NULL;
}



RecordPreinitializerValue::RecordPreinitializerValue(TypeSpec ts)
    :Value(ts.prefix(initializable_type)) {
}

Regs RecordPreinitializerValue::precompile(Regs preferred) {
    return Regs();
}

Storage RecordPreinitializerValue::compile(Cx *cx) {
    cx->op(SUBQ, RSP, ts.measure_stack());

    // The initializer function will need a stack-relative-alias fix here, but that's OK.
    return Storage(MEMORY, Address(RSP, 0));
}




RecordPostinitializerValue::RecordPostinitializerValue(Value *v)
    :Value(v->ts.unprefix(initializable_type)) {
    value.reset(v);
}

bool RecordPostinitializerValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    return value->check(args, kwargs, scope);
}

Regs RecordPostinitializerValue::precompile(Regs preferred) {
    return value->precompile(preferred);
}

Storage RecordPostinitializerValue::compile(Cx *cx) {
    Storage s = value->compile(cx);
    
    if (s.where != MEMORY || s.address.base != RSP || s.address.index != NOREG || s.address.offset != 0)
        throw INTERNAL_ERROR;
    
    return Storage(STACK);
}




RecordUnwrapValue::RecordUnwrapValue(TypeSpec cast_ts, Value *p)
    :Value(cast_ts) {
    pivot.reset(p);
}

void RecordUnwrapValue::need_rvalue() {
    GenericLvalue::need_rvalue();

    if (pivot->ts[0] == lvalue_type) {
        GenericLvalue *glv = ptr_cast<GenericLvalue>(pivot.get());
    
        if (glv)
            glv->need_rvalue();
    }
}

Regs RecordUnwrapValue::precompile(Regs preferred) {
    return pivot->precompile(preferred);
}

Storage RecordUnwrapValue::compile(Cx *cx) {
    return pivot->compile(cx);
}




RecordWrapperValue::RecordWrapperValue(Value *pivot, TypeSpec pcts, TypeSpec rts, std::string on, std::string aon, Scope *scope)
    :Value(rts) {
    arg_operation_name = aon;
    
    if (pcts != NO_TS)
        pivot = make<RecordUnwrapValue>(pcts, pivot);
    
    if (on != "") {
        pivot = pivot->lookup_inner(on, scope);
        if (!pivot)
            throw INTERNAL_ERROR;
    }

    operation = pivot;

    // Necessary for casting an Array indexing into String indexing
    if (pivot->ts[0] == lvalue_type && rts[0] != lvalue_type)
        pivot = make<RvalueCastValue>(pivot);
    
    value.reset(pivot);
}

bool RecordWrapperValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    if (arg_operation_name.size()) {       
        if (args.size() != 1 || kwargs.size() != 0)
            return false;

        Expr *expr = new Expr(Expr::IDENTIFIER, Token(), arg_operation_name);
        args[0].reset(expr->set_pivot(args[0].release()));
    }

    return operation->check(args, kwargs, scope);
}

Regs RecordWrapperValue::precompile(Regs preferred) {
    return value->precompile(preferred);
}

Storage RecordWrapperValue::compile(Cx *cx) {
    Storage s = value->compile(cx);
    
    if (s.where == REGISTER) {
        cx->op(PUSHQ, s.reg);
        s = Storage(STACK);
    }
    
    return s;
}
