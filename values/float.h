class FloatOperationValue: public OptimizedOperationValue {
public:
    FloatOperationValue(OperationType o, Value *p, TypeMatch &match);

    virtual Storage negate(Cx *cx);
    virtual Storage binary(Cx *cx, FprFprmemOp opcode);
    virtual Storage compare(Cx *cx, ConditionCode cc);
    virtual Storage assign_binary(Cx *cx, FprFprmemOp opcode);
    virtual Storage compile(Cx *cx);
};

class FloatFunctionValue: public GenericValue {
public:
    Regs rclob;
    ImportedFloatFunction *function;

    FloatFunctionValue(ImportedFloatFunction *f, Value *l, TypeMatch &match);
    
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
};

class FloatIsnanValue: public GenericValue {
public:
    FloatIsnanValue(Value *p, TypeMatch &tm);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
};
