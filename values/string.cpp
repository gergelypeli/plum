
class StringEqualityValue: public GenericValue {
public:
    StringEqualityValue(Value *l, TypeMatch &match)
        :GenericValue(STRING_TS, BOOLEAN_TS, l) {
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = left->precompile(preferred) | right->precompile(preferred);
        return clob;
    }

    virtual Storage compile(X64 *x64) {
        compile_and_store_both(x64, Storage(STACK), Storage(STACK));
        
        left->ts.equal(Storage(STACK), Storage(STACK), x64);
        
        return Storage(FLAGS, SETE);
    }
};


class StringRegexpMatcherValue: public GenericValue, public Raiser {
public:
    StringRegexpMatcherValue(Value *l, TypeMatch &match)
        :GenericValue(STRING_TS, STRING_ARRAY_REF_TS, l) {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!check_raise(match_unmatched_exception_type, scope))
            return false;
        
        return GenericValue::check(args, kwargs, scope);
    }

    virtual Regs precompile(Regs preferred) {
        left->precompile(preferred);
        right->precompile(preferred);
        return Regs::all();
    }

    virtual Storage compile(X64 *x64) {
        compile_and_store_both(x64, Storage(STACK), Storage(STACK));
        Label ok;

        x64->op(MOVQ, RDI, Address(RSP, ADDRESS_SIZE));
        x64->op(MOVQ, RSI, Address(RSP, 0));
        
        // This uses SSE instructions, so SysV stack alignment must be ensured
        x64->call_sysv(x64->string_regexp_match_label);
        
        x64->op(POPQ, RBX);
        x64->decref(RBX);
        x64->op(POPQ, RBX);
        x64->decref(RBX);
        
        x64->op(CMPQ, RAX, 0);
        x64->op(JNE, ok);
        
        raise("UNMATCHED", x64);
        
        x64->code_label(ok);

        return Storage(REGISTER, RAX);
    }
};
