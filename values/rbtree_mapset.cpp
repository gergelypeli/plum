
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


class SetNextElemByAgeValue: public RbtreeNextElemByAgeValue {
public:
    SetNextElemByAgeValue(Value *l, TypeMatch &tm)
        :RbtreeNextElemByAgeValue(l, tm[1]) {
    }
};


class SetNextElemByOrderValue: public RbtreeNextElemByOrderValue {
public:
    SetNextElemByOrderValue(Value *l, TypeMatch &tm)
        :RbtreeNextElemByOrderValue(l, tm[1]) {
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


class MapItemByAgeIterValue: public RbtreeElemByAgeIterValue {
public:
    MapItemByAgeIterValue(Value *l, TypeMatch &tm)
        :RbtreeElemByAgeIterValue(l, TypeSpec(mapitembyageiter_type, tm[1], tm[2])) {
    }
};

class MapItemByOrderIterValue: public RbtreeElemByOrderIterValue {
public:
    MapItemByOrderIterValue(Value *l, TypeMatch &tm)
        :RbtreeElemByOrderIterValue(l, TypeSpec(mapitembyorderiter_type, tm[1], tm[2])) {
    }
};


class MapIndexByAgeIterValue: public RbtreeElemByAgeIterValue {
public:
    MapIndexByAgeIterValue(Value *l, TypeMatch &tm)
        :RbtreeElemByAgeIterValue(l, TypeSpec(mapindexbyageiter_type, tm[1], tm[2])) {
    }
};

class MapIndexByOrderIterValue: public RbtreeElemByOrderIterValue {
public:
    MapIndexByOrderIterValue(Value *l, TypeMatch &tm)
        :RbtreeElemByOrderIterValue(l, TypeSpec(mapindexbyorderiter_type, tm[1], tm[2])) {
    }
};


class MapNextItemByAgeValue: public RbtreeNextElemByAgeValue {
public:
    MapNextItemByAgeValue(Value *l, TypeMatch &tm)
        :RbtreeNextElemByAgeValue(l, TypeSpec(item_type, tm[1], tm[2])) {
    }
};


class MapNextItemByOrderValue: public RbtreeNextElemByOrderValue {
public:
    MapNextItemByOrderValue(Value *l, TypeMatch &tm)
        :RbtreeNextElemByOrderValue(l, TypeSpec(item_type, tm[1], tm[2])) {
    }
};


class MapNextIndexByAgeValue: public RbtreeNextElemByAgeValue {
public:
    MapNextIndexByAgeValue(Value *l, TypeMatch &tm)
        :RbtreeNextElemByAgeValue(l, tm[1]) {  // NOTE: the index comes first in Item
    }
};


class MapNextIndexByOrderValue: public RbtreeNextElemByOrderValue {
public:
    MapNextIndexByOrderValue(Value *l, TypeMatch &tm)
        :RbtreeNextElemByOrderValue(l, tm[1]) {  // NOTE: the index comes first in Item
    }
};



// Weak map helpers

void compile_nosytree_callback(Label label, TypeSpec elem_ts, X64 *x64) {
    Label remove_label = x64->once->compile(compile_rbtree_remove, elem_ts);

    x64->code_label_local(label, elem_ts.symbolize() + "_nosytree_callback");
    
    std::stringstream ss;
    ss << elem_ts << " Nosytree callback";
    x64->runtime->log(ss.str());
    
    // arguments: payload1, payload2
    // We may clobber all registers

    x64->op(MOVQ, SELFX, Address(RSP, ADDRESS_SIZE * 2));  // payload1 arg (rbtree ref address)
    x64->op(MOVQ, SELFX, Address(SELFX, 0));  // load current rbtree ref
    x64->op(MOVQ, ROOTX, Address(SELFX, RBTREE_ROOT_OFFSET));
    x64->op(MOVQ, KEYX, Address(RSP, ADDRESS_SIZE));  // payload2 arg (elem index)
    x64->op(LEA, KEYX, Address(SELFX, KEYX, RBNODE_VALUE_OFFSET));  // use the elem key

    x64->op(CALL, remove_label);
    x64->op(MOVQ, Address(SELFX, RBTREE_ROOT_OFFSET), R10);
    
    x64->op(RET);
}


void nosy_postadd(TypeSpec elem_ts, Storage ref_storage, X64 *x64) {
    // Add an FCB to the newly added rbtree elem, and decreases the reference count.
    // ref_storage - the storage of the rbtree ref
    // SELFX/KEYX - points to the newly added elem
    // Clobbers all registers.
    Label callback_label = x64->once->compile(compile_nosytree_callback, elem_ts);
    TypeSpec heap_ts = nosyvalue_heap_ts(elem_ts);
    int noffset = nosyvalue_offset(elem_ts);
    
    // This is the storage of the rbtree ref as the operation's pivot value returned.
    // As it was an Lvalue pivot, it is either MEMORY[RBP+x] or ALIAS[RBP+x].
    // The rbtree ref of a nosy structure is on the heap, so it must be the second one.
    if (ref_storage.where != ALIAS || ref_storage.value != 0)
        throw INTERNAL_ERROR;
        
    x64->op(MOVQ, R11, ref_storage.address);
    
    x64->op(PUSHQ, Address(SELFX, KEYX, RBNODE_VALUE_OFFSET + noffset));  // object
    x64->op(LEA, R10, Address(callback_label, 0));
    x64->op(PUSHQ, R10);  // callback
    x64->op(PUSHQ, R11);  // payload1
    x64->op(PUSHQ, KEYX);  // payload2

    x64->op(CALL, x64->runtime->fcb_alloc_label);  // clobbers all
    x64->op(ADDQ, RSP, 3 * ADDRESS_SIZE);
    x64->op(POPQ, R10);  // object
    
    heap_ts.decref(R10, x64);
}


void nosy_postremove(TypeSpec elem_ts, Storage ref_storage, X64 *x64) {
    // Remove an FCB to the newly removed rbtree elem.
    // ref_storage - the storage of the rbtree ref
    // SELFX/KEYX - points to the just removed elem
    // Clobbers all registers.
    Label callback_label = x64->once->compile(compile_nosytree_callback, elem_ts);
    int noffset = nosyvalue_offset(elem_ts);

    // See above
    if (ref_storage.where != ALIAS || ref_storage.value != 0)
        throw INTERNAL_ERROR;
        
    x64->op(MOVQ, R11, ref_storage.address);
    
    x64->op(PUSHQ, Address(SELFX, KEYX, RBNODE_VALUE_OFFSET + noffset));  // object
    x64->op(LEA, R10, Address(callback_label, 0));
    x64->op(PUSHQ, R10);  // callback
    x64->op(PUSHQ, R11);  // payload1
    x64->op(PUSHQ, KEYX);  // payload2

    x64->op(CALL, x64->runtime->fcb_free_label);  // clobbers all
    x64->op(ADDQ, RSP, 4 * ADDRESS_SIZE);
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
    WeakValueMapAddValue(Value *l, TypeMatch &tm)
        :RbtreeAddItemValue(l, tm[1], nosy_ts(tm[2]), tm[1], ptr_ts(tm[2])) {
    }

    virtual Regs precompile(Regs preferred) {
        return RbtreeAddItemValue::precompile(preferred) | Regs::all();
    }

    virtual Storage compile(X64 *x64) {
        Storage ps = RbtreeAddItemValue::compile(x64);
        
        // Using the ps+SELFX+KEYX
        nosy_postadd(elem_ts, ps, x64);  // clobbers all
        
        return ps;
    }
};


class WeakValueMapRemoveValue: public RbtreeRemoveValue {
public:
    WeakValueMapRemoveValue(Value *l, TypeMatch &tm)
        :RbtreeRemoveValue(l, wvm_elem_ts(tm), tm[1]) {
    }

    virtual Regs precompile(Regs preferred) {
        return RbtreeRemoveValue::precompile(preferred) | Regs::all();
    }

    virtual Storage compile(X64 *x64) {
        Storage ps = RbtreeRemoveValue::compile(x64);
        
        // Using the ps+SELFX+KEYX
        nosy_postremove(elem_ts, ps, x64);  // clobbers all
        
        return ps;
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
    WeakIndexMapAddValue(Value *l, TypeMatch &tm)
        :RbtreeAddItemValue(l, nosy_ts(tm[1]), tm[2], ptr_ts(tm[1]), tm[2]) {
    }

    virtual Regs precompile(Regs preferred) {
        return RbtreeAddItemValue::precompile(preferred) | Regs::all();
    }

    virtual Storage compile(X64 *x64) {
        Storage ps = RbtreeAddItemValue::compile(x64);

        // Using the ps+SELFX+KEYX
        nosy_postadd(elem_ts, ps, x64);  // clobbers all
        
        return ps;
    }
};


class WeakIndexMapRemoveValue: public RbtreeRemoveValue {
public:
    WeakIndexMapRemoveValue(Value *l, TypeMatch &tm)
        :RbtreeRemoveValue(l, wim_elem_ts(tm), ptr_ts(tm[1])) {
    }

    virtual Regs precompile(Regs preferred) {
        return RbtreeRemoveValue::precompile(preferred) | Regs::all();
    }

    virtual Storage compile(X64 *x64) {
        Storage ps = RbtreeRemoveValue::compile(x64);
        
        // Using the ps+SELFX+KEYX
        nosy_postremove(elem_ts, ps, x64);  // clobbers all
        
        return ps;
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
    WeakSetAddValue(Value *l, TypeMatch &tm)
        :RbtreeAddValue(l, ws_elem_ts(tm), ptr_ts(tm[1])) {
    }

    virtual Regs precompile(Regs preferred) {
        return RbtreeAddValue::precompile(preferred) | Regs::all();
    }

    virtual Storage compile(X64 *x64) {
        Storage ps = RbtreeAddValue::compile(x64);
        
        // Using the ps+SELFX+KEYX
        nosy_postadd(elem_ts, ps, x64);  // clobbers all
        
        return ps;
    }
};


class WeakSetRemoveValue: public RbtreeRemoveValue {
public:
    WeakSetRemoveValue(Value *l, TypeMatch &tm)
        :RbtreeRemoveValue(l, ws_elem_ts(tm), ptr_ts(tm[1])) {
    }

    virtual Regs precompile(Regs preferred) {
        return RbtreeRemoveValue::precompile(preferred) | Regs::all();
    }

    virtual Storage compile(X64 *x64) {
        Storage ps = RbtreeRemoveValue::compile(x64);
        
        // Using the ps+SELFX+KEYX
        nosy_postremove(elem_ts, ps, x64);  // clobbers all
        
        return ps;
    }
};


class WeakSetHasValue: public RbtreeHasValue {
public:
    WeakSetHasValue(Value *l, TypeMatch &tm)
        :RbtreeHasValue(l, ws_elem_ts(tm), ptr_ts(tm[1])) {
    }
};
