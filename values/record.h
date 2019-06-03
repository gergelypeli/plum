class RecordOperationValue: public GenericOperationValue {
public:
    RecordOperationValue(OperationType o, Value *p, TypeMatch &match);
};

class StringOperationValue: public RecordOperationValue {
public:
    StringOperationValue(OperationType o, Value *p, TypeMatch &match);
    
    virtual void compile_and_stack_both(X64 *x64);
    virtual Storage compare(X64 *x64);
    virtual Storage equal(X64 *x64, bool negate);
};

class RecordInitializerValue: public Value {
public:
    RecordType *record_type;
    std::vector<std::unique_ptr<Value>> values;
    std::vector<TypeSpec> member_tss;
    std::vector<std::string> member_names;
    std::vector<Storage> var_storages;
    TypeMatch match;

    RecordInitializerValue(TypeMatch &tm);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
    virtual CodeScope *unwind(X64 *x64);
};

class RecordPreinitializerValue: public Value {
public:
    RecordPreinitializerValue(TypeSpec ts);
    
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};

class RecordPostinitializerValue: public Value {
public:
    std::unique_ptr<Value> value;
    
    RecordPostinitializerValue(Value *v);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};

class RecordUnwrapValue: public Value, public GenericLvalue {
public:
    std::unique_ptr<Value> pivot;
    
    RecordUnwrapValue(TypeSpec cast_ts, Value *p);
    
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};

class RecordWrapperValue: public Value {
public:
    std::unique_ptr<Value> value;
    Value *operation;
    std::string arg_operation_name;

    RecordWrapperValue(Value *pivot, TypeSpec pcts, TypeSpec rts, std::string on, std::string aon, Scope *scope);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};
