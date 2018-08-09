
class VirtualEntry;

// Declarations
class Declaration;
class Allocable;
class Variable;
class Role;
class BaseRole;
class PartialVariable;
class Evaluable;
class Function;
class ImportedFloatFunction;
class ClassType;

// Types
class Type;
class TreenumerationType;
class ArrayType;
class InterfaceType;
class ImplementationType;
class HeapType;

// Scopes
class Scope;
class CodeScope;
class DataScope;
class ModuleScope;
class SwitchScope;
class TryScope;
class EvalScope;
class FunctionScope;

// Values
class ArrayEmptyValue;
class ArrayInitializerValue;
class ArrayReservedValue;
class ArrayAllValue;
class BasicValue;
class BulkEqualityMatcherValue;
class CastValue;
class CircularrayEmptyValue;
class CircularrayInitializerValue;
class CircularrayReservedValue;
class ClassDefinitionValue;
class ClassMatcherValue;
class ClassPostinitializerValue;
class ClassPreinitializerValue;
class ClassWrapperInitializerValue;
class CreateValue;
class DeclarationValue;
class EnumerationDefinitionValue;
class EvaluableValue;
class FloatFunctionValue;
class FloatValue;
class FunctionCallValue;
class FunctionReturnValue;
class GenericValue;
class ImplementationConversionValue;
class ImplementationDefinitionValue;
class InitializerEqualityMatcherValue;
class IntegerDefinitionValue;
class InterfaceDefinitionValue;
class ModuleValue;
class OptionNoneMatcherValue;
class OptionNoneValue;
class OptionSomeMatcherValue;
class OptionSomeValue;
class PartialVariableValue;
class PartialModuleValue;
class RbtreeEmptyValue;
class RbtreeInitializerValue;
class RbtreeReservedValue;
class RecordDefinitionValue;
class RecordInitializerValue;
class RecordPostinitializerValue;
class RecordPreinitializerValue;
class RecordUnwrapValue;
class RecordWrapperValue;
class ReferenceWeakenValue;
class RoleValue;
class StringLiteralValue;
class StringRegexpMatcherValue;
class SliceEmptyValue;
class SliceAllValue;
class TreenumerationDefinitionValue;
class TreenumerationMatcherValue;
class TypeValue;
class UnicodeCharacterValue;
class Value;
class VariableValue;
class WeakAnchorageDeadMatcherValue;
class WeakAnchorageLiveMatcherValue;
class WeakAnchorageValue;
class YieldValue;
class YieldableValue;

class TypeSpec;
typedef std::array<TypeSpec,4> TypeMatch;
struct Allocation;
class X64;

enum AsWhat {
    AS_VALUE, AS_VARIABLE, AS_ARGUMENT, AS_PIVOT_ARGUMENT, AS_LVALUE_ARGUMENT
};


class TypeSpec: public std::vector<Type *> {
public:
    TypeSpec();
    TypeSpec(iterator tsi);
    TypeSpec(std::initializer_list<Type *> il):std::vector<Type *>(il) {}
    TypeSpec(Type *t, TypeSpec &tm1, TypeSpec &tm2);

    TypeMatch match();    
    Allocation measure();
    int measure_raw();
    int measure_elem();
    int measure_stack();
    int measure_where(StorageWhere where);
    std::vector<VirtualEntry *> get_virtual_table();
    Label get_virtual_table_label(X64 *x64);
    Label get_finalizer_label(X64 *x64);
    Value *autoconv(iterator target, Value *orig, TypeSpec &ifts);
    StorageWhere where(AsWhat as_what);
    TypeSpec prefix(Type *t);
    TypeSpec unprefix(Type *t = NULL);
    TypeSpec reprefix(Type *s, Type *t);
    TypeSpec rvalue();
    TypeSpec lvalue();
    bool has_meta(Type *mt);
    bool is_meta();
    bool is_hyper();
    Storage store(Storage s, Storage t, X64 *x64);
    Storage create(Storage s, Storage t, X64 *x64);
    void destroy(Storage s, X64 *x64);
    void equal(Storage s, Storage t, X64 *x64);
    void compare(Storage s, Storage t, X64 *x64);
    void streamify(bool repr, X64 *x64);
    Value *lookup_initializer(std::string name);
    Value *lookup_partinitializer(std::string name, Value *pivot);
    Value *lookup_matcher(std::string name, Value *pivot);
    Value *lookup_inner(std::string name, Value *pivot);
    DataScope *get_inner_scope();
    void init_vt(Address addr, int data_offset, Label vt_label, int virtual_offset, X64 *x64);
};

typedef TypeSpec::iterator TypeSpecIter;
typedef std::vector<TypeSpec> TSs;
typedef std::vector<std::string> Ss;
std::ostream &operator<<(std::ostream &os, const TypeSpec &ts);
std::ostream &operator<<(std::ostream &os, const TSs &tss);
std::ostream &operator<<(std::ostream &os, const TypeMatch &tm);




struct ArgInfo {
    const char *name;
    TypeSpec *context;
    Scope *scope;
    std::unique_ptr<Value> *target;  // Yes, a pointer to an unique_ptr
};

typedef std::vector<ArgInfo> ArgInfos;



struct Allocation {
    int bytes;
    int count1;
    int count2;
    int count3;
    
    Allocation(int b = 0, int c1 = 0, int c2 = 0, int c3 = 0);
    int concretize(TypeMatch tm);
    int concretize();
};

Allocation stack_size(Allocation a);

std::ostream &operator<<(std::ostream &os, const Allocation &a);

template <class T, class S> T *ptr_cast(S *s) {
    return dynamic_cast<T *>(s);
}


struct TreenumInput {
    const char *kw;
    unsigned p;
};


class PartialInfo {
public:
    std::set<std::string> uninitialized_member_names;
    std::set<std::string> initialized_member_names;
    
    PartialInfo() {
    }

    virtual void set_member_names(std::vector<std::string> mn) {
        uninitialized_member_names.insert(mn.begin(), mn.end());
    }
    
    virtual void be_initialized(std::string name) {
        initialized_member_names.insert(name);
        uninitialized_member_names.erase(name);
    }
    
    virtual bool is_initialized(std::string name) {
        return initialized_member_names.count(name) == 1;
    }

    virtual bool is_uninitialized(std::string name) {
        return uninitialized_member_names.count(name) == 1;
    }
    
    virtual bool is_complete() {
        return uninitialized_member_names.size() == 0;
    }

    virtual void be_complete() {
        initialized_member_names.insert(uninitialized_member_names.begin(), uninitialized_member_names.end());
        uninitialized_member_names.clear();
    }

    virtual bool is_dirty() {
        return initialized_member_names.size() != 0;
    }
};
