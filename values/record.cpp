

class RecordOperationValue: public GenericOperationValue {
public:
    RecordOperationValue(OperationType o, Value *p, TypeMatch &match)
        :GenericOperationValue(o, match[0].rvalue(), is_comparison(o) ? BOOLEAN_TS : match[0], p) {
    }
};


class RecordInitializerValue: public Value {
public:
    RecordType *record_type;
    Variable *variable;
    std::vector<std::unique_ptr<Value>> values;
    
    std::vector<Storage> var_storages;
    
    
    RecordInitializerValue(Variable *var)
        :Value(var->var_ts) {
        record_type = dynamic_cast<RecordType *>(ts[0]);
        variable = var;
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        //std::vector<TypeSpec> arg_tss = record_type->member_tss;
        //std::vector<std::string> arg_names = record_type->member_names;
        
        return check_arguments(args, kwargs, scope, record_type->member_tss, record_type->member_names, values);
    }

    virtual Regs precompile(Regs preferred) {
        for (auto &v : values)
            if (v)
                v->precompile(preferred);
            
        return Regs::all();  // We're too complex to care
    }
    
    virtual Storage compile(X64 *x64) {
        Storage fn_storage(MEMORY, Address(RBP, 0));  // this must be a local variable
        Storage rec_storage = variable->get_storage(fn_storage);

        x64->unwind->push(this);
        
        for (unsigned i = 0; i < values.size(); i++) {
            Variable *var = record_type->member_variables[i];
            Storage var_storage = var->get_storage(rec_storage);
            var_storages.push_back(var_storage);
            TypeSpec var_ts = var->var_ts;

            Value *v = values[i].get();
            Storage t = var_storage;
            Storage s;
            
            if (v)
                s = v->compile(x64);
            
            var_ts.create(s, t, x64);
        }
        
        x64->unwind->pop(this);

        return rec_storage;
    }
    
    virtual Scope *unwind(X64 *x64) {
        for (int i = var_storages.size() - 1; i >= 0; i--)
            unwind_destroy_var(record_type->member_variables[i]->var_ts, var_storages[i], x64);
            
        return NULL;
    }
};
