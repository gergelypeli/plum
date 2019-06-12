const int MAX_SIGNED_DWORD = 2147483647;

class IntegerOperationValue: public OptimizedOperationValue {
public:
    int os;
    bool is_unsigned;

    IntegerOperationValue(OperationType o, Value *pivot, TypeMatch &match);
    
    virtual void exponentiation_by_squaring(X64 *x64, Register BASE, Register EXP, Register RES);
    virtual bool fits32(int value);
    virtual Storage unary(X64 *x64, UnaryOp opcode);
    virtual Storage binary_simple(X64 *x64, BinaryOp opcode);
    virtual Storage binary_multiply(X64 *x64);
    virtual Storage binary_divmod(X64 *x64, bool mod);
    virtual Storage binary_shift(X64 *x64, ShiftOp opcode);
    virtual Storage binary_exponent(X64 *x64);
    virtual Storage binary_compare(X64 *x64, ConditionCode cc);
    virtual Storage assign_binary(X64 *x64, BinaryOp opcode);
    virtual Storage assign_multiply(X64 *x64);
    virtual Storage assign_divmod(X64 *x64, bool mod);
    virtual Storage assign_exponent(X64 *x64);
    virtual Storage assign_shift(X64 *x64, ShiftOp opcode);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(X64 *x64);
};
