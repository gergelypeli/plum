

class RecordOperationValue: public GenericOperationValue {
public:
    RecordOperationValue(OperationType o, Value *p, TypeMatch &match)
        :GenericOperationValue(o, match[0].rvalue(), is_comparison(o) ? BOOLEAN_TS : match[0], p) {
    }
};


class RecordInitializerValue: public Value {
public:
    RecordType *record_type;
    std::vector<std::unique_ptr<Value>> values;
    std::vector<TypeSpec> member_tss;
    std::vector<std::string> member_names;
    std::vector<Storage> var_storages;
    
    RecordInitializerValue(TypeMatch &match)
        :Value(match[0]) {
        record_type = dynamic_cast<RecordType *>(ts[0]);
        member_tss = record_type->get_member_tss(match);
        member_names = record_type->get_member_names();
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        return check_arguments(args, kwargs, scope, member_tss, member_names, values);
    }

    virtual Regs precompile(Regs preferred) {
        for (auto &v : values)
            if (v)
                v->precompile(preferred);
            
        return Regs::all();  // We're too complex to care
    }
    
    virtual Storage compile(X64 *x64) {
        x64->op(SUBQ, RSP, ts.measure(STACK));

        x64->unwind->push(this);
        
        for (unsigned i = 0; i < values.size(); i++) {
            Variable *var = record_type->member_variables[i];
            TypeSpec var_ts = var->var_ts;
            Value *v = values[i].get();
            Storage s;
            
            if (v)
                s = v->compile(x64);
            
            int offset = 0;
            
            if (s.where == STACK)
                offset = var_ts.measure(STACK);

            Storage t = var->get_storage(Storage(MEMORY, Address(RSP, offset)));
            
            var_ts.create(s, t, x64);
            
            var_storages.push_back(t + (-offset));
        }
        
        x64->unwind->pop(this);

        return Storage(STACK);
    }
    
    virtual Scope *unwind(X64 *x64) {
        for (int i = var_storages.size() - 1; i >= 0; i--)
            unwind_destroy_var(record_type->member_variables[i]->var_ts, var_storages[i], x64);

        x64->op(ADDQ, RSP, ts.measure(STACK));
            
        return NULL;
    }
};


class RecordPreinitializerValue: public Value {
public:
    RecordPreinitializerValue(TypeSpec ts)
        :Value(ts) {
    }

    virtual Regs precompile(Regs preferred) {
        return Regs();
    }
    
    virtual Storage compile(X64 *x64) {
        ts.create(Storage(), Storage(STACK), x64);
        x64->op(PUSHQ, RSP);
        return Storage(ALISTACK);
    }
};


class RecordPostinitializerValue: public Value {
public:
    std::unique_ptr<Value> value;
    
    RecordPostinitializerValue(Value *v)
        :Value(v->ts.rvalue()) {
        value.reset(v);
    }
    
    virtual Regs precompile(Regs preferred) {
        return value->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        Storage s = value->compile(x64);
        
        if (s.where != MEMORY)  // ALIAS pivot is popped into a register based MEMORY
            throw INTERNAL_ERROR;
            
        return Storage(STACK);
    }
};
