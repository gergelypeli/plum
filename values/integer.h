const int MAX_SIGNED_DWORD = 2147483647;

class IntegerOperationValue: public OptimizedOperationValue {
public:
    int os;
    bool is_unsigned;

    IntegerOperationValue(OperationType o, Value *pivot, TypeMatch &match);
    
    virtual void exponentiation_by_squaring(Cx *cx, Register BASE, Register EXP, Register RES);
    virtual bool fits32(int value);
    virtual Storage unary(Cx *cx, UnaryOp opcode);
    virtual Storage binary_simple(Cx *cx, BinaryOp opcode);
    virtual Storage binary_multiply(Cx *cx);
    virtual Storage binary_divmod(Cx *cx, bool mod);
    virtual Storage binary_shift(Cx *cx, ShiftOp opcode);
    virtual Storage binary_exponent(Cx *cx);
    virtual Storage binary_compare(Cx *cx, ConditionCode cc);
    virtual Storage assign_binary(Cx *cx, BinaryOp opcode);
    virtual Storage assign_multiply(Cx *cx);
    virtual Storage assign_divmod(Cx *cx, bool mod);
    virtual Storage assign_exponent(Cx *cx);
    virtual Storage assign_shift(Cx *cx, ShiftOp opcode);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
};
