
// Set
/*

SetEmptyValue::SetEmptyValue(TypeMatch &tm)
    :RbtreeEmptyValue(tm[1], tm[0]) {
}



SetReservedValue::SetReservedValue(TypeMatch &tm)
    :RbtreeReservedValue(tm[1], tm[0]) {
}



SetInitializerValue::SetInitializerValue(TypeMatch &tm)
    :RbtreeInitializerValue(tm[1], tm[0]) {
}
*/


SetLengthValue::SetLengthValue(Value *l, TypeMatch &tm)
    :RbtreeLengthValue(l, tm[1]) {
}



SetAddValue::SetAddValue(Value *l, TypeMatch &tm)
    :RbtreeAddValue(l, tm[1], tm[1]) {
}



SetRemoveValue::SetRemoveValue(Value *l, TypeMatch &tm)
    :RbtreeRemoveValue(l, tm[1], tm[1]) {
}



SetHasValue::SetHasValue(Value *l, TypeMatch &tm)
    :RbtreeHasValue(l, tm[1], tm[1]) {
}



SetElemByAgeIterValue::SetElemByAgeIterValue(Value *l, TypeMatch &tm)
    :RbtreeElemByAgeIterValue(l, tm[1].prefix(setelembyageiter_type)) {
}



SetElemByOrderIterValue::SetElemByOrderIterValue(Value *l, TypeMatch &tm)
    :RbtreeElemByOrderIterValue(l, tm[1].prefix(setelembyorderiter_type)) {
}



SetNextElemByAgeValue::SetNextElemByAgeValue(Value *l, TypeMatch &tm)
    :RbtreeNextElemByAgeValue(l, tm[1].prefix(tuple1_type)) {
}

Storage SetNextElemByAgeValue::postprocess(Register r, Register i, X64 *x64) {
    return Storage(MEMORY, Address(r, i, RBNODE_VALUE_OFFSET));
}



SetNextElemByOrderValue::SetNextElemByOrderValue(Value *l, TypeMatch &tm)
    :RbtreeNextElemByOrderValue(l, tm[1].prefix(tuple1_type)) {
}

Storage SetNextElemByOrderValue::postprocess(Register r, Register i, X64 *x64) {
    return Storage(MEMORY, Address(r, i, RBNODE_VALUE_OFFSET));
}



// Map

TypeSpec map_elem_ts(TypeMatch &tm) {
    return TypeSpec(item_type, tm[1], tm[2]);
}

/*

MapEmptyValue::MapEmptyValue(TypeMatch &tm)
    :RbtreeEmptyValue(map_elem_ts(tm), tm[0]) {
}



MapReservedValue::MapReservedValue(TypeMatch &tm)
    :RbtreeReservedValue(map_elem_ts(tm), tm[0]) {
}



MapInitializerValue::MapInitializerValue(TypeMatch &tm)
    :RbtreeInitializerValue(map_elem_ts(tm), tm[0]) {
}
*/


MapLengthValue::MapLengthValue(Value *l, TypeMatch &tm)
    :RbtreeLengthValue(l, map_elem_ts(tm)) {
}



MapAddValue::MapAddValue(Value *l, TypeMatch &tm)
    :RbtreeAddItemValue(l, tm[1], tm[2], tm[1], tm[2]) {
}



MapRemoveValue::MapRemoveValue(Value *l, TypeMatch &tm)
    :RbtreeRemoveValue(l, map_elem_ts(tm), tm[1]) {
}



MapHasValue::MapHasValue(Value *l, TypeMatch &tm)
    :RbtreeHasValue(l, map_elem_ts(tm), tm[1]) {
}



MapIndexValue::MapIndexValue(Value *l, TypeMatch &tm)
    :RbtreeIndexValue(l, tm[1], map_elem_ts(tm), tm[1], tm[2]) {
}



MapItemByAgeIterValue::MapItemByAgeIterValue(Value *l, TypeMatch &tm)
    :RbtreeElemByAgeIterValue(l, TypeSpec(mapitembyageiter_type, tm[1], tm[2])) {
}


MapItemByOrderIterValue::MapItemByOrderIterValue(Value *l, TypeMatch &tm)
    :RbtreeElemByOrderIterValue(l, TypeSpec(mapitembyorderiter_type, tm[1], tm[2])) {
}



MapIndexByAgeIterValue::MapIndexByAgeIterValue(Value *l, TypeMatch &tm)
    :RbtreeElemByAgeIterValue(l, TypeSpec(mapindexbyageiter_type, tm[1], tm[2])) {
}


MapIndexByOrderIterValue::MapIndexByOrderIterValue(Value *l, TypeMatch &tm)
    :RbtreeElemByOrderIterValue(l, TypeSpec(mapindexbyorderiter_type, tm[1], tm[2])) {
}




MapNextItemByAgeValue::MapNextItemByAgeValue(Value *l, TypeMatch &tm)
    :RbtreeNextElemByAgeValue(l, typesubst(SAME_SAME2_LVALUE_TUPLE2_TS, tm)) {
    index_ts = tm[1];
    value_ts = tm[2];
}

Regs MapNextItemByAgeValue::precompile(Regs preferred) {
    return RbtreeNextElemByAgeValue::precompile(preferred) | Regs(RAX, RBX);
}

Storage MapNextItemByAgeValue::postprocess(Register r, Register i, X64 *x64) {
    index_ts.store(Storage(MEMORY, Address(r, i, RBNODE_VALUE_OFFSET)), Storage(STACK), x64);

    x64->op(PUSHQ, 0);
    x64->op(LEA, R10, Address(r, i, RBNODE_VALUE_OFFSET + index_ts.measure_stack()));
    x64->op(PUSHQ, R10);

    return Storage(STACK);
}




MapNextItemByOrderValue::MapNextItemByOrderValue(Value *l, TypeMatch &tm)
    :RbtreeNextElemByOrderValue(l, typesubst(SAME_SAME2_LVALUE_TUPLE2_TS, tm)) {
    index_ts = tm[1];
    value_ts = tm[2];
}

Regs MapNextItemByOrderValue::precompile(Regs preferred) {
    return RbtreeNextElemByOrderValue::precompile(preferred) | Regs(RAX, RBX);
}

Storage MapNextItemByOrderValue::postprocess(Register r, Register i, X64 *x64) {
    index_ts.store(Storage(MEMORY, Address(r, i, RBNODE_VALUE_OFFSET)), Storage(STACK), x64);

    x64->op(PUSHQ, 0);
    x64->op(LEA, R10, Address(r, i, RBNODE_VALUE_OFFSET + index_ts.measure_stack()));
    x64->op(PUSHQ, R10);

    return Storage(STACK);
}



MapNextIndexByAgeValue::MapNextIndexByAgeValue(Value *l, TypeMatch &tm)
    :RbtreeNextElemByAgeValue(l, tm[1].prefix(tuple1_type)) {  // NOTE: the index comes first in Item
}

Storage MapNextIndexByAgeValue::postprocess(Register r, Register i, X64 *x64) {
    return Storage(MEMORY, Address(r, i, RBNODE_VALUE_OFFSET));
}



MapNextIndexByOrderValue::MapNextIndexByOrderValue(Value *l, TypeMatch &tm)
    :RbtreeNextElemByOrderValue(l, tm[1].prefix(tuple1_type)) {  // NOTE: the index comes first in Item
}

Storage MapNextIndexByOrderValue::postprocess(Register r, Register i, X64 *x64) {
    return Storage(MEMORY, Address(r, i, RBNODE_VALUE_OFFSET));
}
