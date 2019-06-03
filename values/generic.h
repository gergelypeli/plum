class GenericValue: public Value {
public:
    TypeSpec arg_ts;
    std::unique_ptr<Value> left, right;
    Storage ls, rs;
    
    GenericValue(TypeSpec at, TypeSpec rt, Value *l);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual void compile_and_store_both(X64 *x64, Storage l, Storage r);
    virtual CodeScope *unwind(X64 *x64);
};

// Unoptimized version, but works with STACK valued types
class GenericOperationValue: public GenericValue, public GenericLvalue {
public:
    OperationType operation;
    bool is_left_lvalue;
    Regs clob, rclob;
    
    GenericOperationValue(OperationType o, TypeSpec at, TypeSpec rt, Value *l);
    
    static TypeSpec op_arg_ts(OperationType o, TypeMatch &match);
    static TypeSpec op_ret_ts(OperationType o, TypeMatch &match);
    virtual void need_rvalue();
    virtual Regs precompile(Regs preferred);
    virtual Storage lmemory(X64 *x64);
    virtual Storage assign_create(X64 *x64);
    virtual Storage compare(X64 *x64);
    virtual Storage equal(X64 *x64, bool negate);
    virtual Storage compile(X64 *x64);
    virtual CodeScope *unwind(X64 *x64);
};

class OptimizedOperationValue: public GenericOperationValue {
public:
    Storage auxls;
    RegSubset lsubset, rsubset;

    OptimizedOperationValue(OperationType o, TypeSpec at, TypeSpec rt, Value *l, RegSubset lss, RegSubset rss);
    
    virtual Storage pick_early_auxls(Regs preferred);
    virtual Storage pick_late_auxls();
    virtual Storage pick_auxrs(RegSubset rss);
    virtual Regs precompile(Regs preferred);
    virtual void subcompile(X64 *x64);
    virtual Storage assign(X64 *x64);
    virtual Storage compare(X64 *x64);
    virtual Storage equal(X64 *x64, bool negate);
    virtual Storage compile(X64 *x64);
    virtual CodeScope *unwind(X64 *x64);
};
