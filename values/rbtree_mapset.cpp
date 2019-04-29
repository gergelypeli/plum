
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



