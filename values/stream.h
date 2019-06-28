void stream_preappend2(Address alias_addr, Cx *cx);
void streamify_ascii(std::string s, Address alias_addr, Cx *cx);

class InterpolationValue: public Value {
public:
    std::vector<std::ustring> fragments;
    std::vector<std::unique_ptr<Value>> components;

    InterpolationValue(std::vector<std::ustring> f, Token t);
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope);
    virtual Regs precompile(Regs preferred);
    virtual Storage compile(Cx *cx);
};
