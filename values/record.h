class RecordOperationValue: public GenericOperationValue {
public:
    RecordOperationValue(OperationType o, Value *p, TypeMatch &match);
};

class StringOperationValue: public RecordOperationValue {
public:
    StringOperationValue(OperationType o, Value *p, TypeMatch &match);
    
    virtual void compile_and_stack_both(Cx *cx);
    virtual Storage compare(Cx *cx);
    virtual Storage equal(Cx *cx, bool negate);
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
    virtual Storage compile(Cx *cx);
    virtual CodeScope *unwind(Cx *cx);
};

class RecordPreinitializerValue: public Value {
public:
    RecordPreinitializerValue(TypeSpec ts);
    
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
};

class RecordPostinitializerValue: public Value {
public:
    std::unique_ptr<Value> value;
    
    RecordPostinitializerValue(Value *v);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
};

class RecordUnwrapValue: public Value, public GenericLvalue {
public:
    std::unique_ptr<Value> pivot;
    
    RecordUnwrapValue(TypeSpec cast_ts, Value *p);

    virtual void need_rvalue();
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
};

class RecordWrapperValue: public Value {
public:
    std::unique_ptr<Value> value;
    Value *operation;
    std::string arg_operation_name;

    RecordWrapperValue(Value *pivot, TypeSpec pcts, TypeSpec rts, std::string on, std::string aon, Scope *scope);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
};
