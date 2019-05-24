class FloatOperationValue: public OptimizedOperationValue {
public:
    FloatOperationValue(OperationType o, Value *p, TypeMatch &match);

    virtual Storage negate(X64 *x64);
    virtual Storage binary(X64 *x64, SseSsememOp opcode);
    virtual Storage compare(X64 *x64, ConditionCode cc);
    virtual Storage assign_binary(X64 *x64, SseSsememOp opcode);
    virtual Storage compile(X64 *x64);
};

class FloatFunctionValue: public GenericValue {
public:
    Regs rclob;
    ImportedFloatFunction *function;

    FloatFunctionValue(ImportedFloatFunction *f, Value *l, TypeMatch &match);
    
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};

class FloatIsnanValue: public GenericValue {
public:
    FloatIsnanValue(Value *p, TypeMatch &tm);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};
