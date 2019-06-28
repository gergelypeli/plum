#include "../plum.h"


ContainerType::ContainerType(std::string name, Metatypes param_metatypes)
    :Type(name, param_metatypes, identity_metatype) {
}

Allocation ContainerType::measure(TypeMatch tm) {
    std::cerr << "This is probably an error, shouldn't measure a container type!\n";
    throw INTERNAL_ERROR;
}

void ContainerType::incref(TypeMatch tm, Register r, Cx *cx) {
    cx->runtime->incref(r);
}

void ContainerType::decref(TypeMatch tm, Register r, Cx *cx) {
    cx->runtime->decref(r);
}

void ContainerType::streamify(TypeMatch tm, Cx *cx) {
    // We do this for container types that don't implement Streamifiable
    auto arg_regs = cx->abi_arg_regs();
    Address value_addr(RSP, ALIAS_SIZE);
    Address alias_addr(RSP, 0);

    cx->op(MOVQ, arg_regs[0], value_addr);
    cx->op(MOVQ, arg_regs[1], alias_addr);

    cx->runtime->call_sysv(cx->runtime->sysv_streamify_pointer_label);
}

unsigned ContainerType::get_elem_size(TypeSpec elem_ts) {
    return elem_size(elem_ts.measure_raw());
}



LinearrayType::LinearrayType(std::string name)
    :ContainerType(name, Metatypes { value_metatype }) {
}

Label LinearrayType::get_finalizer_label(TypeMatch tm, Cx *cx) {
    return cx->once->compile(compile_finalizer, tm[0]);
}

void LinearrayType::compile_finalizer(Label label, TypeSpec ts, Cx *cx) {
    TypeSpec elem_ts = ts.unprefix(linearray_type);
    int elem_size = ContainerType::get_elem_size(elem_ts);
    Label start, end, loop;

    cx->code_label_local(label, elem_ts.prefix(linearray_type).symbolize("finalizer"));
    cx->prologue();
    cx->op(MOVQ, RAX, Address(RSP, ADDRESS_SIZE + RIP_SIZE));

    cx->op(MOVQ, RCX, Address(RAX, LINEARRAY_LENGTH_OFFSET));
    cx->op(CMPQ, RCX, 0);
    cx->op(JE, end);

    cx->op(IMUL3Q, R10, RCX, elem_size);
    cx->op(LEA, RAX, Address(RAX, LINEARRAY_ELEMS_OFFSET));
    cx->op(ADDQ, RAX, R10);

    cx->code_label(loop);
    cx->op(SUBQ, RAX, elem_size);
    elem_ts.destroy(Storage(MEMORY, Address(RAX, 0)), cx);
    cx->op(DECQ, RCX);
    cx->op(JNE, loop);

    cx->code_label(end);
    cx->epilogue();
}

void LinearrayType::type_info(TypeMatch tm, Cx *cx) {
    TypeSpec elem_ts = tm[1];
    unsigned elem_ts_index = cx->once->type_info(elem_ts);
    unsigned integer_ts_index = cx->once->type_info(INTEGER_TS);
    unsigned elem_size = get_elem_size(elem_ts);
    Label label;
    unsigned array_ts_index = label.def_index;
    
    cx->dwarf->begin_structure_type_info(tm[0].symbolize(), LINEARRAY_ELEMS_OFFSET);
    cx->dwarf->member_info("length", LINEARRAY_LENGTH_OFFSET, integer_ts_index, true);
    cx->dwarf->member_info("reservation", LINEARRAY_RESERVATION_OFFSET, integer_ts_index, true);
    cx->dwarf->member_info("elems", LINEARRAY_ELEMS_OFFSET, array_ts_index, true);
    cx->dwarf->end_info();
    
    cx->dwarf->info_def(array_ts_index);
    cx->dwarf->begin_array_type_info(elem_ts.symbolize("<Elems>"), elem_ts_index, elem_size);
    cx->dwarf->subrange_type_info(LINEARRAY_LENGTH_OFFSET - LINEARRAY_ELEMS_OFFSET);
    cx->dwarf->end_info();
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

void ArrayType::streamify(TypeMatch tm, Cx *cx) {
    TypeSpec elem_ts = tm[1];
    Label label = cx->once->compile(compile_streamification, elem_ts);

    cx->op(CALL, label);  // clobbers all
}

void ArrayType::compile_streamification(Label label, TypeSpec elem_ts, Cx *cx) {
    int elem_size = ContainerType::get_elem_size(elem_ts);
    Label loop, elem, end;
    Address value_addr(RSP, ADDRESS_SIZE + RIP_SIZE + ALIAS_SIZE);
    Address alias_addr(RSP, ADDRESS_SIZE + RIP_SIZE);

    cx->code_label_local(label, elem_ts.prefix(array_type).symbolize("streamify"));
    cx->prologue();
    
    // open
    streamify_ascii("{", alias_addr, cx);  // clobbers all
    
    cx->op(MOVQ, RAX, value_addr);  // Array Ref
    cx->op(MOVQ, RCX, Address(RAX, LINEARRAY_LENGTH_OFFSET));
    cx->op(CMPQ, RCX, 0);
    cx->op(JE, end);

    cx->op(LEA, RAX, Address(RAX, LINEARRAY_ELEMS_OFFSET));
    cx->op(JMP, elem);  // skip separator

    cx->code_label(loop);
    
    // separator
    cx->op(PUSHQ, RAX);
    cx->op(PUSHQ, RCX);
    
    streamify_ascii(",", alias_addr + 2 * ADDRESS_SIZE, cx);

    cx->op(POPQ, RCX);
    cx->op(POPQ, RAX);
    
    cx->code_label(elem);
    cx->op(PUSHQ, RAX);
    cx->op(PUSHQ, RCX);
    cx->op(MOVQ, RBX, alias_addr + 2 * ADDRESS_SIZE);  // stream alias
    elem_ts.store(Storage(MEMORY, Address(RAX, 0)), Storage(STACK), cx);
    cx->op(PUSHQ, 0);
    cx->op(PUSHQ, RBX);
    
    // Invoking a custom streamification may relocate the stack, so the
    // passed stream alias may be fixed, must propagate it upwards.
    elem_ts.streamify(cx);  // clobbers all
    
    cx->op(POPQ, RBX);
    cx->op(POPQ, R10);
    elem_ts.store(Storage(STACK), Storage(), cx);
    cx->op(MOVQ, alias_addr + 2 * ADDRESS_SIZE, RBX);  // stream alias
    cx->op(POPQ, RCX);
    cx->op(POPQ, RAX);
    
    cx->op(ADDQ, RAX, elem_size);
    cx->op(DECQ, RCX);
    cx->op(JNE, loop);

    cx->code_label(end);
    
    // close
    streamify_ascii("}", alias_addr, cx);  // clobbers all

    cx->epilogue();
}




CircularrayType::CircularrayType(std::string name)
    :ContainerType(name, Metatypes { value_metatype }) {
}

Label CircularrayType::get_finalizer_label(TypeMatch tm, Cx *cx) {
    return cx->once->compile(compile_finalizer, tm[0]);
}

void CircularrayType::compile_finalizer(Label label, TypeSpec ts, Cx *cx) {
    TypeSpec elem_ts = ts.unprefix(circularray_type);
    int elem_size = ContainerType::get_elem_size(elem_ts);
    Label start, end, loop, ok, ok1;

    cx->code_label_local(label, elem_ts.prefix(circularray_type).symbolize("finalizer"));
    cx->prologue();
    
    cx->op(MOVQ, RAX, Address(RSP, ADDRESS_SIZE + RIP_SIZE));

    cx->op(MOVQ, RCX, Address(RAX, CIRCULARRAY_LENGTH_OFFSET));
    cx->op(CMPQ, RCX, 0);
    cx->op(JE, end);

    cx->op(MOVQ, RDX, Address(RAX, CIRCULARRAY_FRONT_OFFSET));
    cx->op(ADDQ, RDX, RCX);
    cx->op(CMPQ, RDX, Address(RAX, CIRCULARRAY_RESERVATION_OFFSET));
    cx->op(JBE, ok1);
    
    cx->op(SUBQ, RDX, Address(RAX, CIRCULARRAY_RESERVATION_OFFSET));
    
    cx->code_label(ok1);
    cx->op(IMUL3Q, RDX, RDX, elem_size);

    cx->code_label(loop);
    cx->op(SUBQ, RDX, elem_size);
    cx->op(CMPQ, RDX, 0);
    cx->op(JGE, ok);
    
    cx->op(MOVQ, RDX, Address(RAX, CIRCULARRAY_RESERVATION_OFFSET));
    cx->op(DECQ, RDX);
    cx->op(IMUL3Q, RDX, RDX, elem_size);
    
    cx->code_label(ok);
    Address elem_addr = Address(RAX, RDX, CIRCULARRAY_ELEMS_OFFSET);
    elem_ts.destroy(Storage(MEMORY, elem_addr), cx);
    cx->op(DECQ, RCX);
    cx->op(JNE, loop);

    cx->code_label(end);
    cx->epilogue();
}

void CircularrayType::type_info(TypeMatch tm, Cx *cx) {
    TypeSpec elem_ts = tm[1];
    unsigned elem_ts_index = cx->once->type_info(elem_ts);
    unsigned integer_ts_index = cx->once->type_info(INTEGER_TS);
    unsigned elem_size = get_elem_size(elem_ts);
    Label label;
    unsigned array_ts_index = label.def_index;
    
    cx->dwarf->begin_structure_type_info(tm[0].symbolize(), CIRCULARRAY_ELEMS_OFFSET);
    cx->dwarf->member_info("length", CIRCULARRAY_LENGTH_OFFSET, integer_ts_index, true);
    cx->dwarf->member_info("reservation", CIRCULARRAY_RESERVATION_OFFSET, integer_ts_index, true);
    cx->dwarf->member_info("front", CIRCULARRAY_FRONT_OFFSET, integer_ts_index, true);
    cx->dwarf->member_info("elems", CIRCULARRAY_ELEMS_OFFSET, array_ts_index, true);
    cx->dwarf->end_info();
    
    cx->dwarf->info_def(array_ts_index);
    cx->dwarf->begin_array_type_info(elem_ts.symbolize("<Elems>"), elem_ts_index, elem_size);
    cx->dwarf->subrange_type_info(CIRCULARRAY_LENGTH_OFFSET - CIRCULARRAY_ELEMS_OFFSET);
    cx->dwarf->end_info();
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

Label RbtreeType::get_finalizer_label(TypeMatch tm, Cx *cx) {
    return cx->once->compile(compile_finalizer, tm[0]);
}

void RbtreeType::compile_finalizer(Label label, TypeSpec ts, Cx *cx) {
    TypeSpec elem_ts = ts.unprefix(rbtree_type);
    Label loop, cond;

    cx->code_label(label);
    cx->prologue();
    
    cx->op(MOVQ, RAX, Address(RSP, ADDRESS_SIZE + RIP_SIZE));

    cx->op(MOVQ, RCX, Address(RAX, RBTREE_LAST_OFFSET));
    cx->op(JMP, cond);
    
    cx->code_label(loop);
    elem_ts.destroy(Storage(MEMORY, Address(RAX, RCX, RBNODE_VALUE_OFFSET)), cx);
    cx->op(MOVQ, RCX, Address(RAX, RCX, RBNODE_PRED_OFFSET));
    cx->op(ANDQ, RCX, ~RBNODE_RED_BIT);
    
    cx->code_label(cond);
    cx->op(CMPQ, RCX, RBNODE_NIL);
    cx->op(JNE, loop);
    
    cx->epilogue();
}

unsigned RbtreeType::get_rbnode_size(TypeSpec elem_ts) {
    return elem_ts.measure_stack() + RBNODE_HEADER_SIZE;
}

void RbtreeType::type_info(TypeMatch tm, Cx *cx) {
    TypeSpec elem_ts = tm[1];
    unsigned elem_ts_index = cx->once->type_info(elem_ts);
    unsigned integer_ts_index = cx->once->type_info(INTEGER_TS);
    unsigned rbnode_size = get_rbnode_size(elem_ts);
    Label label;
    unsigned array_ts_index = label.def_index;
    Label label2;
    unsigned rbnode_ts_index = label2.def_index;
    
    cx->dwarf->begin_structure_type_info(tm[0].symbolize(), RBTREE_ELEMS_OFFSET);
    cx->dwarf->member_info("length", RBTREE_LENGTH_OFFSET, integer_ts_index, true);
    cx->dwarf->member_info("reservation", RBTREE_RESERVATION_OFFSET, integer_ts_index, true);
    cx->dwarf->member_info("root", RBTREE_ROOT_OFFSET, integer_ts_index, true);
    cx->dwarf->member_info("first", RBTREE_FIRST_OFFSET, integer_ts_index, true);
    cx->dwarf->member_info("last", RBTREE_LAST_OFFSET, integer_ts_index, true);
    cx->dwarf->member_info("vacant", RBTREE_VACANT_OFFSET, integer_ts_index, true);
    cx->dwarf->member_info("elems", RBTREE_ELEMS_OFFSET, array_ts_index, true);
    cx->dwarf->end_info();
    
    cx->dwarf->info_def(array_ts_index);
    cx->dwarf->begin_array_type_info(elem_ts.symbolize("<Rbnodes>"), rbnode_ts_index, rbnode_size);
    cx->dwarf->subrange_type_info(RBTREE_RESERVATION_OFFSET - RBTREE_ELEMS_OFFSET);
    cx->dwarf->end_info();
    
    cx->dwarf->info_def(rbnode_ts_index);
    cx->dwarf->begin_structure_type_info(elem_ts.symbolize("<Rbnode>"), rbnode_size);
    cx->dwarf->member_info("pred", RBNODE_PRED_OFFSET, integer_ts_index, true);
    cx->dwarf->member_info("next", RBNODE_NEXT_OFFSET, integer_ts_index, true);
    cx->dwarf->member_info("left", RBNODE_LEFT_OFFSET, integer_ts_index, true);
    cx->dwarf->member_info("right", RBNODE_RIGHT_OFFSET, integer_ts_index, true);
    cx->dwarf->member_info("value", RBNODE_VALUE_OFFSET, elem_ts_index, true);
    cx->dwarf->end_info();
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

void TreelikeType::streamify(TypeMatch tm, Cx *cx) {
    TypeSpec ets = typesubst(elem_ts, tm);
    Label label = cx->once->compile(compile_streamification, ets);

    cx->op(CALL, label);  // clobbers all
}

void TreelikeType::compile_streamification(Label label, TypeSpec elem_ts, Cx *cx) {
    // TODO: massive copypaste from Array's
    Label loop, elem, end;
    Address value_addr(RSP, ADDRESS_SIZE + RIP_SIZE + ALIAS_SIZE);
    Address alias_addr(RSP, ADDRESS_SIZE + RIP_SIZE);

    cx->code_label_local(label, elem_ts.prefix(rbtree_type).symbolize("streamify"));
    cx->prologue();
    
    // open
    streamify_ascii("{", alias_addr, cx);
    
    cx->op(MOVQ, RAX, value_addr);  // Rbtree Ref
    cx->op(MOVQ, RCX, Address(RAX, RBTREE_FIRST_OFFSET));
    cx->op(CMPQ, RCX, RBNODE_NIL);
    cx->op(JE, end);

    cx->op(JMP, elem);  // skip separator

    cx->code_label(loop);
    
    // separator
    cx->op(PUSHQ, RAX);
    cx->op(PUSHQ, RCX);

    streamify_ascii(",", alias_addr + 2 * ADDRESS_SIZE, cx);
    
    cx->op(POPQ, RCX);
    cx->op(POPQ, RAX);
    
    cx->code_label(elem);
    cx->op(PUSHQ, RAX);
    cx->op(PUSHQ, RCX);
    cx->op(MOVQ, RBX, alias_addr + 2 * ADDRESS_SIZE);  // stream alias
    elem_ts.store(Storage(MEMORY, Address(RAX, RCX, RBNODE_VALUE_OFFSET)), Storage(STACK), cx);
    cx->op(PUSHQ, 0);
    cx->op(PUSHQ, RBX);
    
    // Invoking a custom streamification may relocate the stack, so the
    // passed stream alias may be fixed, must propagate it upwards.
    elem_ts.streamify(cx);  // clobbers all
    
    cx->op(POPQ, RBX);
    cx->op(POPQ, R10);
    elem_ts.store(Storage(STACK), Storage(), cx);
    cx->op(MOVQ, alias_addr + 2 * ADDRESS_SIZE, RBX);  // stream alias
    cx->op(POPQ, RCX);
    cx->op(POPQ, RAX);
    
    cx->op(MOVQ, RCX, Address(RAX, RCX, RBNODE_NEXT_OFFSET));
    cx->op(CMPQ, RCX, RBNODE_NIL);
    cx->op(JNE, loop);

    cx->code_label(end);
    
    // close
    streamify_ascii("}", alias_addr, cx);

    cx->epilogue();
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
