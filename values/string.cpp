
class StringCharsValue: public Value {
public:
    std::unique_ptr<Value> pivot;

    StringCharsValue(Value *p)
        :Value(CHARACTER_ARRAY_REFERENCE_TS) {
        pivot.reset(p);
    }

    virtual Regs precompile(Regs preferred) {
        return pivot->precompile(preferred);
    }

    virtual Storage compile(X64 *x64) {
        return pivot->compile(x64);
    }
};


class StringWrapperValue: public GenericValue {
public:
    std::unique_ptr<GenericValue> wrap;
    TypeMatch charmatch;

    StringWrapperValue(TypeSpec arg_ts, TypeSpec ret_ts)
        :GenericValue(arg_ts, ret_ts, NULL) {
        charmatch = TypeMatch { CHARACTER_ARRAY_REFERENCE_TS, CHARACTER_TS };
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!GenericValue::check(args, kwargs, scope))
            return false;
            
        if (right) {
            Value *k = right.release();
        
            if (arg_ts == STRING_TS)
                k = new StringCharsValue(k);
                
            wrap->right.reset(k);
        }
        
        return true;
    }
    
    virtual Regs precompile(Regs preferred) {
        return wrap->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        Storage s = wrap->compile(x64);
        
        if (s.where == REGISTER && ts == STRING_TS) {
            x64->op(PUSHQ, s.reg);
            s = Storage(STACK);
        }
        
        return s;
    }
};


class StringLengthValue: public StringWrapperValue {
public:
    StringLengthValue(Value *l, TypeMatch &match)
        :StringWrapperValue(VOID_TS, INTEGER_TS) {
        Value *k = new StringCharsValue(l);
        wrap.reset(new ArrayLengthValue(k, charmatch));
    }
};


class StringItemValue: public StringWrapperValue {
public:
    StringItemValue(Value *l, TypeMatch &match)
        :StringWrapperValue(INTEGER_TS, CHARACTER_TS) {  // NOTE: rvalue returned
        Value *k = new StringCharsValue(l);
        wrap.reset(new ArrayItemValue(TWEAK, k, charmatch));
    }
};


class StringConcatenationValue: public StringWrapperValue {
public:
    StringConcatenationValue(Value *l, TypeMatch &match)
        :StringWrapperValue(STRING_TS, STRING_TS) {
        Value *k = new StringCharsValue(l);
        wrap.reset(new ArrayConcatenationValue(k, charmatch));
    }
};


class StringReallocValue: public StringWrapperValue {
public:
    StringReallocValue(Value *l, TypeMatch &match)
        :StringWrapperValue(INTEGER_OVALUE_TS, STRING_TS) {
        Value *k = new StringCharsValue(l);
        wrap.reset(new ArrayReallocValue(TWEAK, k, charmatch));
    }
};


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
