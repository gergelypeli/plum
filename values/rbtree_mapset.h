// Set
/*
class SetEmptyValue: public RbtreeEmptyValue {
public:
    SetEmptyValue(TypeMatch &tm)
};
class SetReservedValue: public RbtreeReservedValue {
public:
    SetReservedValue(TypeMatch &tm)
};
class SetInitializerValue: public RbtreeInitializerValue {
public:
    SetInitializerValue(TypeMatch &tm)
};
*/

class SetLengthValue: public RbtreeLengthValue {
public:
    SetLengthValue(Value *l, TypeMatch &tm);
};

class SetAddValue: public RbtreeAddValue {
public:
    SetAddValue(Value *l, TypeMatch &tm);
};

class SetRemoveValue: public RbtreeRemoveValue {
public:
    SetRemoveValue(Value *l, TypeMatch &tm);
};

class SetHasValue: public RbtreeHasValue {
public:
    SetHasValue(Value *l, TypeMatch &tm);
};

class SetElemByAgeIterValue: public RbtreeElemByAgeIterValue {
public:
    SetElemByAgeIterValue(Value *l, TypeMatch &tm);
};

class SetElemByOrderIterValue: public RbtreeElemByOrderIterValue {
public:
    SetElemByOrderIterValue(Value *l, TypeMatch &tm);
};

class SetNextElemByAgeValue: public RbtreeNextElemByAgeValue {
public:
    SetNextElemByAgeValue(Value *l, TypeMatch &tm);
    
    virtual Storage postprocess(Register r, Register i, X64 *x64);
};

class SetNextElemByOrderValue: public RbtreeNextElemByOrderValue {
public:
    SetNextElemByOrderValue(Value *l, TypeMatch &tm);
    
    virtual Storage postprocess(Register r, Register i, X64 *x64);
};

// Map
/*
class MapEmptyValue: public RbtreeEmptyValue {
public:
    MapEmptyValue(TypeMatch &tm)
};
class MapReservedValue: public RbtreeReservedValue {
public:
    MapReservedValue(TypeMatch &tm)
};
class MapInitializerValue: public RbtreeInitializerValue {
public:
    MapInitializerValue(TypeMatch &tm)
};
*/
class MapLengthValue: public RbtreeLengthValue {
public:
    MapLengthValue(Value *l, TypeMatch &tm);
};

class MapAddValue: public RbtreeAddItemValue {
public:
    MapAddValue(Value *l, TypeMatch &tm);
};

class MapRemoveValue: public RbtreeRemoveValue {
public:
    MapRemoveValue(Value *l, TypeMatch &tm);
};

class MapHasValue: public RbtreeHasValue {
public:
    MapHasValue(Value *l, TypeMatch &tm);
};

class MapIndexValue: public RbtreeIndexValue {
public:
    MapIndexValue(Value *l, TypeMatch &tm);
};

class MapItemByAgeIterValue: public RbtreeElemByAgeIterValue {
public:
    MapItemByAgeIterValue(Value *l, TypeMatch &tm);
};

class MapItemByOrderIterValue: public RbtreeElemByOrderIterValue {
public:
    MapItemByOrderIterValue(Value *l, TypeMatch &tm);
};

class MapIndexByAgeIterValue: public RbtreeElemByAgeIterValue {
public:
    MapIndexByAgeIterValue(Value *l, TypeMatch &tm);
};

class MapIndexByOrderIterValue: public RbtreeElemByOrderIterValue {
public:
    MapIndexByOrderIterValue(Value *l, TypeMatch &tm);
};

class MapNextItemByAgeValue: public RbtreeNextElemByAgeValue {
public:
    TypeSpec index_ts;
    TypeSpec value_ts;

    MapNextItemByAgeValue(Value *l, TypeMatch &tm);

    virtual Regs precompile(Regs preferred);
    virtual Storage postprocess(Register r, Register i, X64 *x64);
};

class MapNextItemByOrderValue: public RbtreeNextElemByOrderValue {
public:
    TypeSpec index_ts;
    TypeSpec value_ts;

    MapNextItemByOrderValue(Value *l, TypeMatch &tm);
    
    virtual Regs precompile(Regs preferred);
    virtual Storage postprocess(Register r, Register i, X64 *x64);
};

class MapNextIndexByAgeValue: public RbtreeNextElemByAgeValue {
public:
    MapNextIndexByAgeValue(Value *l, TypeMatch &tm);
    
    virtual Storage postprocess(Register r, Register i, X64 *x64);
};

class MapNextIndexByOrderValue: public RbtreeNextElemByOrderValue {
public:
    MapNextIndexByOrderValue(Value *l, TypeMatch &tm);
    
    virtual Storage postprocess(Register r, Register i, X64 *x64);
};
