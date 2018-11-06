
// Set
/*
class SetEmptyValue: public RbtreeEmptyValue {
public:
    SetEmptyValue(TypeMatch &tm)
        :RbtreeEmptyValue(tm[1], tm[0]) {
    }
};


class SetReservedValue: public RbtreeReservedValue {
public:
    SetReservedValue(TypeMatch &tm)
        :RbtreeReservedValue(tm[1], tm[0]) {
    }
};


class SetInitializerValue: public RbtreeInitializerValue {
public:
    SetInitializerValue(TypeMatch &tm)
        :RbtreeInitializerValue(tm[1], tm[0]) {
    }
};
*/

class SetLengthValue: public RbtreeLengthValue {
public:
    SetLengthValue(Value *l, TypeMatch &tm)
        :RbtreeLengthValue(l, tm[1]) {
    }
};


class SetAddValue: public RbtreeAddValue {
public:
    SetAddValue(Value *l, TypeMatch &tm)
        :RbtreeAddValue(l, tm[1], tm[1]) {
    }
};


class SetRemoveValue: public RbtreeRemoveValue {
public:
    SetRemoveValue(Value *l, TypeMatch &tm)
        :RbtreeRemoveValue(l, tm[1], tm[1]) {
    }
};


class SetHasValue: public RbtreeHasValue {
public:
    SetHasValue(Value *l, TypeMatch &tm)
        :RbtreeHasValue(l, tm[1], tm[1]) {
    }
};


class SetElemByAgeIterValue: public RbtreeElemByAgeIterValue {
public:
    SetElemByAgeIterValue(Value *l, TypeMatch &tm)
        :RbtreeElemByAgeIterValue(l, tm[1].prefix(setelembyageiter_type)) {
    }
};


class SetElemByOrderIterValue: public RbtreeElemByOrderIterValue {
public:
    SetElemByOrderIterValue(Value *l, TypeMatch &tm)
        :RbtreeElemByOrderIterValue(l, tm[1].prefix(setelembyorderiter_type)) {
    }
};


// Map

TypeSpec map_elem_ts(TypeMatch &tm) {
    return TypeSpec(item_type, tm[1], tm[2]);
}

/*
class MapEmptyValue: public RbtreeEmptyValue {
public:
    MapEmptyValue(TypeMatch &tm)
        :RbtreeEmptyValue(map_elem_ts(tm), tm[0]) {
    }
};


class MapReservedValue: public RbtreeReservedValue {
public:
    MapReservedValue(TypeMatch &tm)
        :RbtreeReservedValue(map_elem_ts(tm), tm[0]) {
    }
};


class MapInitializerValue: public RbtreeInitializerValue {
public:
    MapInitializerValue(TypeMatch &tm)
        :RbtreeInitializerValue(map_elem_ts(tm), tm[0]) {
    }
};
*/

class MapLengthValue: public RbtreeLengthValue {
public:
    MapLengthValue(Value *l, TypeMatch &tm)
        :RbtreeLengthValue(l, map_elem_ts(tm)) {
    }
};


class MapAddValue: public RbtreeAddItemValue {
public:
    MapAddValue(Value *l, TypeMatch &tm)
        :RbtreeAddItemValue(l, tm[1], tm[2], tm[1], tm[2]) {
    }
};


class MapRemoveValue: public RbtreeRemoveValue {
public:
    MapRemoveValue(Value *l, TypeMatch &tm)
        :RbtreeRemoveValue(l, map_elem_ts(tm), tm[1]) {
    }
};


class MapHasValue: public RbtreeHasValue {
public:
    MapHasValue(Value *l, TypeMatch &tm)
        :RbtreeHasValue(l, map_elem_ts(tm), tm[1]) {
    }
};


class MapIndexValue: public RbtreeIndexValue {
public:
    MapIndexValue(Value *l, TypeMatch &tm)
        :RbtreeIndexValue(l, tm[1], map_elem_ts(tm), tm[1], tm[2]) {
    }
};

/*
class MapElemByAgeIterValue: public RbtreeElemByAgeIterValue {
    MapElemByAgeIterValue(Value *l, TypeMatch &tm)
        :RbtreeElemByAgeIterValue(Value *l, tm[1]) {
    }
};

class MapElemByOrderIterValue: public RbtreeElemByOrderIterValue {
    MapElemByOrderIterValue(Value *l, TypeMatch &tm)
        :RbtreeElemByOrderIterValue(Value *l, tm[1]) {
    }
};
*/


// Weak map helpers

static void compile_nosyvalue_callback(Label label, TypeSpec item_ts, X64 *x64) {
    Label remove_label = x64->once->compile(compile_rbtree_remove, item_ts);

    std::stringstream ss;
    ss << item_ts << " Nosyvalue callback";
    x64->code_label_local(label, ss.str());
    x64->runtime->log(ss.str());
    
    // arguments: fcb, payload1, payload2
    // We may clobber all registers

    x64->op(MOVQ, SELFX, Address(RSP, ADDRESS_SIZE * 2));  // payload1 arg (rbtree ref address)
    x64->op(MOVQ, SELFX, Address(SELFX, 0));  // load current rbtree ref
    x64->op(MOVQ, ROOTX, Address(SELFX, RBTREE_ROOT_OFFSET));
    x64->op(MOVQ, KEYX, Address(RSP, ADDRESS_SIZE));  // payload2 arg (elem index)
    x64->op(LEA, KEYX, Address(SELFX, KEYX, RBNODE_VALUE_OFFSET));  // use the elem key

    x64->op(CALL, remove_label);
    x64->op(MOVQ, Address(SELFX, RBTREE_ROOT_OFFSET), R10);
    
    // Removing an element automatically destroys the NosyValue within, which frees
    // the pointed FCB as well.
    
    x64->op(RET);
}


static void ptr_to_nosyvalue(TypeSpec item_ts, TypeSpec heap_ts, Address alias_addr, X64 *x64) {
    // Turn a Ptr into a NosyValue by replacing a pointer with a pointer+fcbaddr pair
    // on the stack. Clobbers all registers, except SELFX and KEYX.
    Label callback_label = x64->once->compile(compile_nosyvalue_callback, item_ts);
    
    x64->op(MOVQ, RAX, Address(RSP, 0));  // referred heap object
    x64->op(LEA, R10, Address(callback_label, 0));  // callback
    x64->op(MOVQ, RCX, alias_addr);  // payload1, the rbtree ref address, RSP based
    x64->op(MOVQ, RDX, KEYX);  // payload2, the rbnode index

    heap_ts.decref(RAX, x64);  // FIXME: use after decref, should unborrow instead
    x64->op(PUSHQ, RAX);  // in NosyValue the object pointer is at the lower address...
    
    x64->op(PUSHQ, SELFX);  // promised to preserve these two
    x64->op(PUSHQ, KEYX);
    
    x64->op(PUSHQ, RAX);
    x64->op(PUSHQ, R10);
    x64->op(PUSHQ, RCX);
    x64->op(PUSHQ, RDX);
    x64->op(CALL, x64->runtime->fcb_alloc_label);  // clobbers all
    x64->op(ADDQ, RSP, 4 * ADDRESS_SIZE);
    
    x64->op(POPQ, KEYX);
    x64->op(POPQ, SELFX);  // preserved
    
    x64->op(MOVQ, Address(RSP, ADDRESS_SIZE), RAX);  // .. and the fcb at the higher address
}


TypeSpec nosy_ts(TypeSpec ts) {
    return ts.prefix(nosyvalue_type);
}


TypeSpec ptr_ts(TypeSpec ts) {
    return ts.prefix(ptr_type);
}


// WeakValueMap

TypeSpec wvm_elem_ts(TypeMatch &tm) {
    return typesubst(SAME_SAMEID2_NOSYVALUE_ITEM_TS, tm);
}


//Value *wvm_rbtree(Value *v, TypeMatch &tm, Scope *s) {
//    TypeSpec rbts = typesubst(SAME_SAMEID2_NOSYVALUE_ITEM_RBTREE_REF_TS, tm);
//    return make<NosyContainerMemberValue>(v, rbts, s);
//}


class WeakValueMapLengthValue: public RbtreeLengthValue {
public:
    WeakValueMapLengthValue(Value *l, TypeMatch &tm)
        :RbtreeLengthValue(l, wvm_elem_ts(tm)) {
    }
};


class WeakValueMapAddValue: public RbtreeAddItemValue {
public:
    TypeSpec value_heap_ts;
    
    WeakValueMapAddValue(Value *l, TypeMatch &tm)
        :RbtreeAddItemValue(l, tm[1], nosy_ts(tm[2]), tm[1], ptr_ts(tm[2])) {
        value_heap_ts = tm[2];
    }

    virtual void prevalue(Address alias_addr, X64 *x64) {
        // Must preserve SELFX and KEYX
        ptr_to_nosyvalue(elem_ts, value_heap_ts, alias_addr, x64);
    }
};


class WeakValueMapRemoveValue: public RbtreeRemoveValue {
public:
    WeakValueMapRemoveValue(Value *l, TypeMatch &tm)
        :RbtreeRemoveValue(l, wvm_elem_ts(tm), tm[1]) {
    }
};


class WeakValueMapHasValue: public RbtreeHasValue {
public:
    WeakValueMapHasValue(Value *l, TypeMatch &tm)
        :RbtreeHasValue(l, wvm_elem_ts(tm), tm[1]) {
    }
};


class WeakValueMapIndexValue: public RbtreeIndexValue {
public:
    WeakValueMapIndexValue(Value *l, TypeMatch &tm)
        :RbtreeIndexValue(l, tm[1], wvm_elem_ts(tm), tm[1], ptr_ts(tm[2])) {
        // NOTE: We must not return an Lvalue to this NosyValue!
    }
};


// WeakIndexMap

TypeSpec wim_elem_ts(TypeMatch &tm) {
    return typesubst(SAMEID_NOSYVALUE_SAME2_ITEM_TS, tm);
}


//Value *wim_rbtree(Value *v, TypeMatch &tm) {
//    return make<NosyContainerMemberValue>(v, typesubst(SAMEID_NOSYVALUE_SAME2_ITEM_RBTREE_REF_TS, tm));
//}


class WeakIndexMapLengthValue: public RbtreeLengthValue {
public:
    WeakIndexMapLengthValue(Value *l, TypeMatch &tm)
        :RbtreeLengthValue(l, wim_elem_ts(tm)) {
    }
};


class WeakIndexMapAddValue: public RbtreeAddItemValue {
public:
    TypeSpec value_heap_ts;
    
    WeakIndexMapAddValue(Value *l, TypeMatch &tm)
        :RbtreeAddItemValue(l, nosy_ts(tm[1]), tm[2], ptr_ts(tm[1]), tm[2]) {
        value_heap_ts = tm[1];
    }

    virtual void prekey(Address alias_addr, X64 *x64) {
        // Must preserve SELFX and KEYX
        ptr_to_nosyvalue(elem_ts, value_heap_ts, alias_addr, x64);
    }
};


class WeakIndexMapRemoveValue: public RbtreeRemoveValue {
public:
    WeakIndexMapRemoveValue(Value *l, TypeMatch &tm)
        :RbtreeRemoveValue(l, wim_elem_ts(tm), ptr_ts(tm[1])) {
    }
};


class WeakIndexMapHasValue: public RbtreeHasValue {
public:
    WeakIndexMapHasValue(Value *l, TypeMatch &tm)
        :RbtreeHasValue(l, wim_elem_ts(tm), ptr_ts(tm[1])) {
    }
};


class WeakIndexMapIndexValue: public RbtreeIndexValue {
public:
    WeakIndexMapIndexValue(Value *l, TypeMatch &tm)
        :RbtreeIndexValue(l, nosy_ts(tm[1]), wim_elem_ts(tm), ptr_ts(tm[1]), tm[2]) {
    }
};


// WeakSet

TypeSpec ws_elem_ts(TypeMatch &tm) {
    return typesubst(SAMEID_NOSYVALUE_TS, tm);
}


//Value *ws_rbtree(Value *v, TypeMatch &tm) {
//    return make<NosyContainerMemberValue>(v, typesubst(SAMEID_NOSYVALUE_RBTREE_REF_TS, tm));
//}


class WeakSetLengthValue: public RbtreeLengthValue {
public:
    WeakSetLengthValue(Value *l, TypeMatch &tm)
        :RbtreeLengthValue(l, ws_elem_ts(tm)) {
    }
};


class WeakSetAddValue: public RbtreeAddValue {
public:
    TypeSpec elem_heap_ts;
    
    WeakSetAddValue(Value *l, TypeMatch &tm)
        :RbtreeAddValue(l, ws_elem_ts(tm), ptr_ts(tm[1])) {
        elem_heap_ts = tm[1];
    }

    virtual void preelem(Address alias_addr, X64 *x64) {
        // Must preserve SELFX and KEYX
        ptr_to_nosyvalue(elem_ts, elem_heap_ts, alias_addr, x64);
    }
};


class WeakSetRemoveValue: public RbtreeRemoveValue {
public:
    WeakSetRemoveValue(Value *l, TypeMatch &tm)
        :RbtreeRemoveValue(l, ws_elem_ts(tm), ptr_ts(tm[1])) {
    }
};


class WeakSetHasValue: public RbtreeHasValue {
public:
    WeakSetHasValue(Value *l, TypeMatch &tm)
        :RbtreeHasValue(l, ws_elem_ts(tm), ptr_ts(tm[1])) {
    }
};
