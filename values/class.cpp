#include "../plum.h"


Storage preinitialize_class(TypeSpec class_ts, Cx *cx) {
    Label finalizer_label = class_ts.get_finalizer_label(cx);
    unsigned heap_size = class_ts.measure_identity().concretize();
    
    cx->op(PUSHQ, heap_size);
    //std::cerr << "XXX Allocating " << heap_size << " on the heap.\n";
    cx->op(LEA, R10, Address(finalizer_label, 0));
    cx->op(PUSHQ, R10);
    
    cx->runtime->heap_alloc();  // clobbers all
    
    cx->op(ADDQ, RSP, 2 * ADDRESS_SIZE);

    class_ts.init_vt(Address(RAX, 0), cx);
    
    return Storage(REGISTER, RAX);
}


ClassPreinitializerValue::ClassPreinitializerValue(TypeSpec rts)
    :Value(rts.reprefix(ref_type, ptr_type).prefix(initializable_type)) {
}

Regs ClassPreinitializerValue::precompile(Regs preferred) {
    return Regs::all();
}

Storage ClassPreinitializerValue::compile(Cx *cx) {
    TypeSpec class_ts = ts.unprefix(initializable_type).unprefix(ptr_type);;
    
    // This is necessary, because if variables can't have concrete class types, then
    // there's nothing to trigger the debug info generation for such types.
    cx->once->type_info(class_ts);
    
    return preinitialize_class(class_ts, cx);
}




ClassPostinitializerValue::ClassPostinitializerValue(TypeSpec mts, Value *p)
    :Value(mts) {
    pivot.reset(p);
}

bool ClassPostinitializerValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    return pivot->check(args, kwargs, scope);
}

Regs ClassPostinitializerValue::precompile(Regs preferred) {
    return pivot->precompile(preferred);
}

Storage ClassPostinitializerValue::compile(Cx *cx) {
    return pivot->compile(cx);
}




ClassWrapperInitializerValue::ClassWrapperInitializerValue(Value *o, Value *v)
    :Value(o->ts.unprefix(initializable_type).reprefix(ptr_type, ref_type)) {
    object.reset(o);
    value.reset(v);
}

bool ClassWrapperInitializerValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    return value->check(args, kwargs, scope);
}

Regs ClassWrapperInitializerValue::precompile(Regs preferred) {
    Regs clob = value->precompile_tail();
    clob = clob | object->precompile_tail();
    
    return clob | Regs(RAX);
}

Storage ClassWrapperInitializerValue::compile(Cx *cx) {
    object->compile_and_store(cx, Storage(STACK));
    value->compile_and_store(cx, Storage(STACK));
    
    cx->op(MOVQ, RAX, Address(RSP, REFERENCE_SIZE));
    value->ts.create(Storage(STACK), Storage(MEMORY, Address(RAX, CLASS_MEMBERS_OFFSET)), cx);
    //cx->op(POPQ, Address(R10, CLASS_MEMBERS_OFFSET));  // creating ref from STACK to MEMORY

    return Storage(STACK);
}


// TODO: rename to AbstractMatcher, and validate name sooner!

ClassMatcherValue::ClassMatcherValue(std::string n, Value *v)
    :Value(NO_TS) {
    name = n;
    value.reset(v);
}

bool ClassMatcherValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    // This is ridiculously late, but better than never
    Value *tv = lookup_fake(name, NULL, scope, Token(), NULL, NULL);
    
    if (!tv)
        return false;
        
    if (!tv->ts.is_meta()) {
        std::cerr << "Class matcher is not a type name!\n";
        return false;
    }
    
    TypeSpec tts = ptr_cast<TypeValue>(tv)->represented_ts;
    AbstractType *ct = ptr_cast<AbstractType>(tts[0]);
    
    if (!ct) {
        std::cerr << "Class matcher is not an abstract name!\n";
        return false;
    }
    
    if (args.size() != 0 || kwargs.size() != 0) {
        std::cerr << "Whacky class matcher!\n";
        return false;
    }
    
    TypeSpec vts = value->ts.rvalue();
    ts = tts.prefix(vts[0]);
    std::cerr << "Class matcher " << name << " is " << ts << ".\n";
    
    if (!check_raise(match_unmatched_exception_type, scope))
        return false;

    return true;
}

Regs ClassMatcherValue::precompile(Regs preferred) {
    return value->precompile(preferred);
}

Storage ClassMatcherValue::compile(Cx *cx) {
    Label loop, matched, not_matched;
    
    value->compile_and_store(cx, Storage(STACK));
    
    cx->op(LEA, R11, Address(ts.get_interface_table_label(cx), 0));  // target ACT
    
    cx->op(MOVQ, R10, Address(RSP, 0));  // the borrowed reference
    cx->op(MOVQ, R10, Address(R10, CLASS_VT_OFFSET));  // the virtual table
    cx->op(MOVQ, R10, Address(R10, VT_AUTOCONV_INDEX * ADDRESS_SIZE));  // table start
    
    cx->code_label(loop);
    cx->op(CMPQ, Address(R10, 0), 0);  // check for table end marker
    cx->op(JE, not_matched);
    cx->op(CMPQ, Address(R10, 0), R11);  // check for target ACT
    cx->op(JE, matched);
    
    cx->op(ADDQ, R10, 2 * ADDRESS_SIZE);
    cx->op(JMP, loop);
    
    cx->code_label(not_matched);
    
    drop_and_raise(ts, Storage(STACK), "UNMATCHED", cx);
    
    cx->code_label(matched);
    cx->op(MOVQ, R11, Address(R10, ADDRESS_SIZE));  // matched role offset
    cx->op(ADDQ, Address(RSP, 0), R11);  // adjust result reference
    
    return Storage(STACK);
}
