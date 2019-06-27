#include "../plum.h"


ContainerType::ContainerType(std::string name, Metatypes param_metatypes)
    :Type(name, param_metatypes, identity_metatype) {
}

Allocation ContainerType::measure(TypeMatch tm) {
    std::cerr << "This is probably an error, shouldn't measure a container type!\n";
    throw INTERNAL_ERROR;
}

void ContainerType::incref(TypeMatch tm, Register r, X64 *x64) {
    x64->runtime->incref(r);
}

void ContainerType::decref(TypeMatch tm, Register r, X64 *x64) {
    x64->runtime->decref(r);
}

void ContainerType::streamify(TypeMatch tm, X64 *x64) {
    // We do this for container types that don't implement Streamifiable
    auto arg_regs = x64->abi_arg_regs();
    Address value_addr(RSP, ALIAS_SIZE);
    Address alias_addr(RSP, 0);

    x64->op(MOVQ, arg_regs[0], value_addr);
    x64->op(MOVQ, arg_regs[1], alias_addr);

    x64->runtime->call_sysv(x64->runtime->sysv_streamify_pointer_label);
}

unsigned ContainerType::get_elem_size(TypeSpec elem_ts) {
    return elem_size(elem_ts.measure_raw());
}



LinearrayType::LinearrayType(std::string name)
    :ContainerType(name, Metatypes { value_metatype }) {
}

Label LinearrayType::get_finalizer_label(TypeMatch tm, X64 *x64) {
    return x64->once->compile(compile_finalizer, tm[0]);
}

void LinearrayType::compile_finalizer(Label label, TypeSpec ts, X64 *x64) {
    TypeSpec elem_ts = ts.unprefix(linearray_type);
    int elem_size = ContainerType::get_elem_size(elem_ts);
    Label start, end, loop;

    x64->code_label_local(label, elem_ts.prefix(linearray_type).symbolize("finalizer"));
    x64->prologue();
    x64->op(MOVQ, RAX, Address(RSP, ADDRESS_SIZE + RIP_SIZE));

    x64->op(MOVQ, RCX, Address(RAX, LINEARRAY_LENGTH_OFFSET));
    x64->op(CMPQ, RCX, 0);
    x64->op(JE, end);

    x64->op(IMUL3Q, R10, RCX, elem_size);
    x64->op(LEA, RAX, Address(RAX, LINEARRAY_ELEMS_OFFSET));
    x64->op(ADDQ, RAX, R10);

    x64->code_label(loop);
    x64->op(SUBQ, RAX, elem_size);
    elem_ts.destroy(Storage(MEMORY, Address(RAX, 0)), x64);
    x64->op(DECQ, RCX);
    x64->op(JNE, loop);

    x64->code_label(end);
    x64->epilogue();
}

void LinearrayType::type_info(TypeMatch tm, X64 *x64) {
    TypeSpec elem_ts = tm[1];
    unsigned elem_ts_index = x64->once->type_info(elem_ts);
    unsigned integer_ts_index = x64->once->type_info(INTEGER_TS);
    unsigned elem_size = get_elem_size(elem_ts);
    Label label;
    unsigned array_ts_index = label.def_index;
    
    x64->dwarf->begin_structure_type_info(tm[0].symbolize(), LINEARRAY_ELEMS_OFFSET);
    x64->dwarf->member_info("length", LINEARRAY_LENGTH_OFFSET, integer_ts_index, true);
    x64->dwarf->member_info("reservation", LINEARRAY_RESERVATION_OFFSET, integer_ts_index, true);
    x64->dwarf->member_info("elems", LINEARRAY_ELEMS_OFFSET, array_ts_index, true);
    x64->dwarf->end_info();
    
    x64->dwarf->info_def(array_ts_index);
    x64->dwarf->begin_array_type_info(elem_ts.symbolize("<Elems>"), elem_ts_index, elem_size);
    x64->dwarf->subrange_type_info(LINEARRAY_LENGTH_OFFSET - LINEARRAY_ELEMS_OFFSET);
    x64->dwarf->end_info();
}




ArrayType::ArrayType(std::string name)
    :RecordType(name, Metatypes { value_metatype }) {
}

Value *ArrayType::lookup_initializer(TypeMatch tm, std::string name, Scope *scope) {
    TypeSpec ts = tm[0];
    
    if (name == "empty")
        return make<ArrayEmptyValue>(ts);
    else if (name == "reserved")
        return make<ArrayReservedValue>(ts);
    else if (name == "all")
        return make<ArrayAllValue>(ts);
    else if (name == "{")
        return make<ArrayInitializerValue>(ts);

    std::cerr << "No Array initializer called " << name << "!\n";
    return NULL;
}

void ArrayType::streamify(TypeMatch tm, X64 *x64) {
    TypeSpec elem_ts = tm[1];
    Label label = x64->once->compile(compile_streamification, elem_ts);

    x64->op(CALL, label);  // clobbers all
}

void ArrayType::compile_streamification(Label label, TypeSpec elem_ts, X64 *x64) {
    int elem_size = ContainerType::get_elem_size(elem_ts);
    Label loop, elem, end;
    Address value_addr(RSP, ADDRESS_SIZE + RIP_SIZE + ALIAS_SIZE);
    Address alias_addr(RSP, ADDRESS_SIZE + RIP_SIZE);

    x64->code_label_local(label, elem_ts.prefix(array_type).symbolize("streamify"));
    x64->prologue();
    
    // open
    streamify_ascii("{", alias_addr, x64);  // clobbers all
    
    x64->op(MOVQ, RAX, value_addr);  // Array Ref
    x64->op(MOVQ, RCX, Address(RAX, LINEARRAY_LENGTH_OFFSET));
    x64->op(CMPQ, RCX, 0);
    x64->op(JE, end);

    x64->op(LEA, RAX, Address(RAX, LINEARRAY_ELEMS_OFFSET));
    x64->op(JMP, elem);  // skip separator

    x64->code_label(loop);
    
    // separator
    x64->op(PUSHQ, RAX);
    x64->op(PUSHQ, RCX);
    
    streamify_ascii(",", alias_addr + 2 * ADDRESS_SIZE, x64);

    x64->op(POPQ, RCX);
    x64->op(POPQ, RAX);
    
    x64->code_label(elem);
    x64->op(PUSHQ, RAX);
    x64->op(PUSHQ, RCX);
    x64->op(MOVQ, RBX, alias_addr + 2 * ADDRESS_SIZE);  // stream alias
    elem_ts.store(Storage(MEMORY, Address(RAX, 0)), Storage(STACK), x64);
    x64->op(PUSHQ, 0);
    x64->op(PUSHQ, RBX);
    
    // Invoking a custom streamification may relocate the stack, so the
    // passed stream alias may be fixed, must propagate it upwards.
    elem_ts.streamify(x64);  // clobbers all
    
    x64->op(POPQ, RBX);
    x64->op(POPQ, R10);
    elem_ts.store(Storage(STACK), Storage(), x64);
    x64->op(MOVQ, alias_addr + 2 * ADDRESS_SIZE, RBX);  // stream alias
    x64->op(POPQ, RCX);
    x64->op(POPQ, RAX);
    
    x64->op(ADDQ, RAX, elem_size);
    x64->op(DECQ, RCX);
    x64->op(JNE, loop);

    x64->code_label(end);
    
    // close
    streamify_ascii("}", alias_addr, x64);  // clobbers all

    x64->epilogue();
}




CircularrayType::CircularrayType(std::string name)
    :ContainerType(name, Metatypes { value_metatype }) {
}

Label CircularrayType::get_finalizer_label(TypeMatch tm, X64 *x64) {
    return x64->once->compile(compile_finalizer, tm[0]);
}

void CircularrayType::compile_finalizer(Label label, TypeSpec ts, X64 *x64) {
    TypeSpec elem_ts = ts.unprefix(circularray_type);
    int elem_size = ContainerType::get_elem_size(elem_ts);
    Label start, end, loop, ok, ok1;

    x64->code_label_local(label, elem_ts.prefix(circularray_type).symbolize("finalizer"));
    x64->prologue();
    
    x64->op(MOVQ, RAX, Address(RSP, ADDRESS_SIZE + RIP_SIZE));

    x64->op(MOVQ, RCX, Address(RAX, CIRCULARRAY_LENGTH_OFFSET));
    x64->op(CMPQ, RCX, 0);
    x64->op(JE, end);

    x64->op(MOVQ, RDX, Address(RAX, CIRCULARRAY_FRONT_OFFSET));
    x64->op(ADDQ, RDX, RCX);
    x64->op(CMPQ, RDX, Address(RAX, CIRCULARRAY_RESERVATION_OFFSET));
    x64->op(JBE, ok1);
    
    x64->op(SUBQ, RDX, Address(RAX, CIRCULARRAY_RESERVATION_OFFSET));
    
    x64->code_label(ok1);
    x64->op(IMUL3Q, RDX, RDX, elem_size);

    x64->code_label(loop);
    x64->op(SUBQ, RDX, elem_size);
    x64->op(CMPQ, RDX, 0);
    x64->op(JGE, ok);
    
    x64->op(MOVQ, RDX, Address(RAX, CIRCULARRAY_RESERVATION_OFFSET));
    x64->op(DECQ, RDX);
    x64->op(IMUL3Q, RDX, RDX, elem_size);
    
    x64->code_label(ok);
    Address elem_addr = Address(RAX, RDX, CIRCULARRAY_ELEMS_OFFSET);
    elem_ts.destroy(Storage(MEMORY, elem_addr), x64);
    x64->op(DECQ, RCX);
    x64->op(JNE, loop);

    x64->code_label(end);
    x64->epilogue();
}

void CircularrayType::type_info(TypeMatch tm, X64 *x64) {
    TypeSpec elem_ts = tm[1];
    unsigned elem_ts_index = x64->once->type_info(elem_ts);
    unsigned integer_ts_index = x64->once->type_info(INTEGER_TS);
    unsigned elem_size = get_elem_size(elem_ts);
    Label label;
    unsigned array_ts_index = label.def_index;
    
    x64->dwarf->begin_structure_type_info(tm[0].symbolize(), CIRCULARRAY_ELEMS_OFFSET);
    x64->dwarf->member_info("length", CIRCULARRAY_LENGTH_OFFSET, integer_ts_index, true);
    x64->dwarf->member_info("reservation", CIRCULARRAY_RESERVATION_OFFSET, integer_ts_index, true);
    x64->dwarf->member_info("front", CIRCULARRAY_FRONT_OFFSET, integer_ts_index, true);
    x64->dwarf->member_info("elems", CIRCULARRAY_ELEMS_OFFSET, array_ts_index, true);
    x64->dwarf->end_info();
    
    x64->dwarf->info_def(array_ts_index);
    x64->dwarf->begin_array_type_info(elem_ts.symbolize("<Elems>"), elem_ts_index, elem_size);
    x64->dwarf->subrange_type_info(CIRCULARRAY_LENGTH_OFFSET - CIRCULARRAY_ELEMS_OFFSET);
    x64->dwarf->end_info();
}




QueueType::QueueType(std::string name)
    :RecordType(name, { value_metatype }) {
}

Value *QueueType::lookup_initializer(TypeMatch tm, std::string name, Scope *scope) {
    TypeSpec ts = tm[0];

    if (name == "empty")
        return make<QueueEmptyValue>(ts);
    else if (name == "reserved")
        return make<QueueReservedValue>(ts);
    else if (name == "{")
        return make<QueueInitializerValue>(ts);

    std::cerr << "No Queue initializer called " << name << "!\n";
    return NULL;
}



RbtreeType::RbtreeType(std::string name)
    :ContainerType(name, Metatypes { value_metatype }) {
}

Label RbtreeType::get_finalizer_label(TypeMatch tm, X64 *x64) {
    return x64->once->compile(compile_finalizer, tm[0]);
}

void RbtreeType::compile_finalizer(Label label, TypeSpec ts, X64 *x64) {
    TypeSpec elem_ts = ts.unprefix(rbtree_type);
    Label loop, cond;

    x64->code_label(label);
    x64->prologue();
    
    x64->op(MOVQ, RAX, Address(RSP, ADDRESS_SIZE + RIP_SIZE));

    x64->op(MOVQ, RCX, Address(RAX, RBTREE_LAST_OFFSET));
    x64->op(JMP, cond);
    
    x64->code_label(loop);
    elem_ts.destroy(Storage(MEMORY, Address(RAX, RCX, RBNODE_VALUE_OFFSET)), x64);
    x64->op(MOVQ, RCX, Address(RAX, RCX, RBNODE_PRED_OFFSET));
    x64->op(ANDQ, RCX, ~RBNODE_RED_BIT);
    
    x64->code_label(cond);
    x64->op(CMPQ, RCX, RBNODE_NIL);
    x64->op(JNE, loop);
    
    x64->epilogue();
}

unsigned RbtreeType::get_rbnode_size(TypeSpec elem_ts) {
    return elem_ts.measure_stack() + RBNODE_HEADER_SIZE;
}

void RbtreeType::type_info(TypeMatch tm, X64 *x64) {
    TypeSpec elem_ts = tm[1];
    unsigned elem_ts_index = x64->once->type_info(elem_ts);
    unsigned integer_ts_index = x64->once->type_info(INTEGER_TS);
    unsigned rbnode_size = get_rbnode_size(elem_ts);
    Label label;
    unsigned array_ts_index = label.def_index;
    Label label2;
    unsigned rbnode_ts_index = label2.def_index;
    
    x64->dwarf->begin_structure_type_info(tm[0].symbolize(), RBTREE_ELEMS_OFFSET);
    x64->dwarf->member_info("length", RBTREE_LENGTH_OFFSET, integer_ts_index, true);
    x64->dwarf->member_info("reservation", RBTREE_RESERVATION_OFFSET, integer_ts_index, true);
    x64->dwarf->member_info("root", RBTREE_ROOT_OFFSET, integer_ts_index, true);
    x64->dwarf->member_info("first", RBTREE_FIRST_OFFSET, integer_ts_index, true);
    x64->dwarf->member_info("last", RBTREE_LAST_OFFSET, integer_ts_index, true);
    x64->dwarf->member_info("vacant", RBTREE_VACANT_OFFSET, integer_ts_index, true);
    x64->dwarf->member_info("elems", RBTREE_ELEMS_OFFSET, array_ts_index, true);
    x64->dwarf->end_info();
    
    x64->dwarf->info_def(array_ts_index);
    x64->dwarf->begin_array_type_info(elem_ts.symbolize("<Rbnodes>"), rbnode_ts_index, rbnode_size);
    x64->dwarf->subrange_type_info(RBTREE_RESERVATION_OFFSET - RBTREE_ELEMS_OFFSET);
    x64->dwarf->end_info();
    
    x64->dwarf->info_def(rbnode_ts_index);
    x64->dwarf->begin_structure_type_info(elem_ts.symbolize("<Rbnode>"), rbnode_size);
    x64->dwarf->member_info("pred", RBNODE_PRED_OFFSET, integer_ts_index, true);
    x64->dwarf->member_info("next", RBNODE_NEXT_OFFSET, integer_ts_index, true);
    x64->dwarf->member_info("left", RBNODE_LEFT_OFFSET, integer_ts_index, true);
    x64->dwarf->member_info("right", RBNODE_RIGHT_OFFSET, integer_ts_index, true);
    x64->dwarf->member_info("value", RBNODE_VALUE_OFFSET, elem_ts_index, true);
    x64->dwarf->end_info();
}



TreelikeType::TreelikeType(std::string name, Metatypes param_metatypes, TypeSpec ets)
    :RecordType(name, param_metatypes) {
    if (ets == NO_TS)
        throw INTERNAL_ERROR;  // this can be a global initialization issue
        
    elem_ts = ets;
}

Value *TreelikeType::lookup_initializer(TypeMatch tm, std::string name, Scope *scope) {
    TypeSpec ets = typesubst(elem_ts, tm);
    
    if (name == "empty")
        return make<RbtreeEmptyValue>(ets, tm[0]);
    else if (name == "reserved")
        return make<RbtreeReservedValue>(ets, tm[0]);
    else if (name == "{")
        return make<RbtreeInitializerValue>(ets, tm[0]);
    else {
        std::cerr << "No " << this->name << " initializer called " << name << "!\n";
        return NULL;
    }
}

void TreelikeType::streamify(TypeMatch tm, X64 *x64) {
    TypeSpec ets = typesubst(elem_ts, tm);
    Label label = x64->once->compile(compile_streamification, ets);

    x64->op(CALL, label);  // clobbers all
}

void TreelikeType::compile_streamification(Label label, TypeSpec elem_ts, X64 *x64) {
    // TODO: massive copypaste from Array's
    Label loop, elem, end;
    Address value_addr(RSP, ADDRESS_SIZE + RIP_SIZE + ALIAS_SIZE);
    Address alias_addr(RSP, ADDRESS_SIZE + RIP_SIZE);

    x64->code_label_local(label, elem_ts.prefix(rbtree_type).symbolize("streamify"));
    x64->prologue();
    
    // open
    streamify_ascii("{", alias_addr, x64);
    
    x64->op(MOVQ, RAX, value_addr);  // Rbtree Ref
    x64->op(MOVQ, RCX, Address(RAX, RBTREE_FIRST_OFFSET));
    x64->op(CMPQ, RCX, RBNODE_NIL);
    x64->op(JE, end);

    x64->op(JMP, elem);  // skip separator

    x64->code_label(loop);
    
    // separator
    x64->op(PUSHQ, RAX);
    x64->op(PUSHQ, RCX);

    streamify_ascii(",", alias_addr + 2 * ADDRESS_SIZE, x64);
    
    x64->op(POPQ, RCX);
    x64->op(POPQ, RAX);
    
    x64->code_label(elem);
    x64->op(PUSHQ, RAX);
    x64->op(PUSHQ, RCX);
    x64->op(MOVQ, RBX, alias_addr + 2 * ADDRESS_SIZE);  // stream alias
    elem_ts.store(Storage(MEMORY, Address(RAX, RCX, RBNODE_VALUE_OFFSET)), Storage(STACK), x64);
    x64->op(PUSHQ, 0);
    x64->op(PUSHQ, RBX);
    
    // Invoking a custom streamification may relocate the stack, so the
    // passed stream alias may be fixed, must propagate it upwards.
    elem_ts.streamify(x64);  // clobbers all
    
    x64->op(POPQ, RBX);
    x64->op(POPQ, R10);
    elem_ts.store(Storage(STACK), Storage(), x64);
    x64->op(MOVQ, alias_addr + 2 * ADDRESS_SIZE, RBX);  // stream alias
    x64->op(POPQ, RCX);
    x64->op(POPQ, RAX);
    
    x64->op(MOVQ, RCX, Address(RAX, RCX, RBNODE_NEXT_OFFSET));
    x64->op(CMPQ, RCX, RBNODE_NIL);
    x64->op(JNE, loop);

    x64->code_label(end);
    
    // close
    streamify_ascii("}", alias_addr, x64);

    x64->epilogue();
}




SetType::SetType(std::string name)  // Must not use TS constants in constructor
    :TreelikeType(name, { value_metatype }, { same_type }) {
}




MapType::MapType(std::string name)  // Must not use TS constants in constructor
    :TreelikeType(name, { value_metatype, value_metatype }, { item_type, same_type, same2_type }) {
}




WeaktreeType::WeaktreeType(std::string n, Metatypes param_metatypes, TypeSpec ets)
    :RecordType(n, param_metatypes) {
    elem_ts = ets;
}

Value *WeaktreeType::lookup_initializer(TypeMatch tm, std::string n, Scope *s) {
    TypeSpec ets = typesubst(elem_ts, tm);
    TypeSpec mts = ets.prefix(rbtree_type).prefix(ref_type);
    Value *member = NULL;
    
    if (n == "empty")
        member = make<RbtreeEmptyValue>(ets, mts);
    else if (n == "reserved")
        member = make<RbtreeReservedValue>(ets, mts);
    else if (n == "{")
        member = make<RbtreeInitializerValue>(ets, mts);
    else {
        std::cerr << "No " << name << " initializer called " << n << "!\n";
        return NULL;
    }
    
    return make<WeaktreeValue>(member, ets, tm[0]);
}




WeakSetType::WeakSetType(std::string name)  // Must not use TS constants in constructor
    :WeaktreeType(name, { identity_metatype }, { nosyvalue_type, sameid_type }) {
}


WeakIndexMapType::WeakIndexMapType(std::string name)  // Must not use TS constants in constructor
    :WeaktreeType(name, { identity_metatype, value_metatype }, { item_type, nosyvalue_type, sameid_type, same2_type }) {
}


WeakValueMapType::WeakValueMapType(std::string name)  // Must not use TS constants in constructor
    :WeaktreeType(name, { value_metatype, identity_metatype }, { item_type, same_type, nosyvalue_type, sameid2_type }) {
}
