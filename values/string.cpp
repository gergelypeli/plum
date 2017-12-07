
class StringEqualityValue: public GenericValue {
public:
    StringEqualityValue(Value *l, TypeMatch &match)
        :GenericValue(STRING_TS, BOOLEAN_TS, l) {
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = left->precompile(preferred) | right->precompile(preferred);
        clob.add(RAX).add(RBX).add(RCX).add(RSI).add(RDI);
        return clob;
    }

    virtual Storage compile(X64 *x64) {
        Label strcmp_label = x64->once(compile_strcmp);

        compile_and_store_both(x64, Storage(STACK), Storage(STACK));
        
        x64->op(POPQ, RBX);
        x64->op(POPQ, RAX);
        x64->op(CALL, strcmp_label);
        
        return Storage(REGISTER, CL);
    }
    
    static void compile_strcmp(X64 *x64) {
        Label sete, done;
        
        x64->op(MOVB, CL, 0);
        x64->op(CMPQ, RAX, RBX);
        x64->op(JE, sete);
        
        x64->op(CMPQ, RAX, 0);
        x64->op(JE, done);
        x64->op(CMPQ, RBX, 0);
        x64->op(JE, done);
        
        x64->op(MOVQ, RCX, x64->array_length_address(RAX));
        x64->op(CMPQ, RCX, x64->array_length_address(RBX));
        x64->op(JNE, sete);
        
        x64->op(LEA, RSI, x64->array_elems_address(RAX));
        x64->op(LEA, RDI, x64->array_elems_address(RBX));
        x64->op(REPECMPSW);
        x64->op(CMPQ, RCX, 0);
        
        x64->code_label(sete);
        x64->op(SETE, CL);
        
        x64->code_label(done);
        x64->decref(RBX);
        x64->decref(RAX);
        
        x64->op(RET);
    }
};
