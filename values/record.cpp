

class RecordOperationValue: public GenericOperationValue {
public:
    RecordOperationValue(OperationType o, Value *p, TypeMatch &match)
        :GenericOperationValue(o, op_arg_ts(o, match), op_ret_ts(o, match), p) {
        std::cerr << "XXX Record " << match[0] << " operation " << o << ".\n";
    }
};


Declaration *make_record_compare() {
    return new TemplateOperation<RecordOperationValue>("compare", ANY_TS, COMPARE);
}


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
        ArgInfos infos;

        // Separate loop, so reallocations won't screw us
        for (unsigned i = 0; i < member_tss.size(); i++)
            values.push_back(NULL);
        
        for (unsigned i = 0; i < member_tss.size(); i++)
            infos.push_back(ArgInfo { member_names[i].c_str(), &member_tss[i], scope, &values[i] });
        
        return check_arguments(args, kwargs, infos);
    }

    virtual Regs precompile(Regs preferred) {
        for (auto &v : values)
            if (v)
                v->precompile(preferred);
            
        return Regs::all();  // We're too complex to care
    }
    
    virtual Storage compile(X64 *x64) {
        x64->op(SUBQ, RSP, ts.measure_stack());

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
                offset = var_ts.measure_stack();

            Storage t = var->get_storage(ts.match(), Storage(MEMORY, Address(RSP, offset)));
            
            var_ts.create(s, t, x64);
            
            var_storages.push_back(t + (-offset));
        }
        
        x64->unwind->pop(this);

        return Storage(STACK);
    }
    
    virtual Scope *unwind(X64 *x64) {
        for (int i = var_storages.size() - 1; i >= 0; i--)
            unwind_destroy_var(record_type->member_variables[i]->var_ts, var_storages[i], x64);

        x64->op(ADDQ, RSP, ts.measure_stack());
            
        return NULL;
    }
};


class RecordPreinitializerValue: public Value {
public:
    RecordPreinitializerValue(TypeSpec ts)
        :Value(ts.prefix(lvalue_type).prefix(partial_type)) {  // TODO: eiiii...
    }

    virtual Regs precompile(Regs preferred) {
        return Regs();
    }
    
    virtual Storage compile(X64 *x64) {
        ts.create(Storage(), Storage(STACK), x64);
        return Storage(MEMORY, Address(RSP, 0));
        //x64->op(PUSHQ, RSP);
        //return Storage(ALISTACK);
    }
};


class RecordPostinitializerValue: public Value {
public:
    std::unique_ptr<Value> value;
    
    RecordPostinitializerValue(Value *v)
        :Value(v->ts.unprefix(partial_type)) {
        value.reset(v);
    }
    
    virtual Regs precompile(Regs preferred) {
        return value->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        Storage s = value->compile(x64);
        
        // ALIAS pivot is popped into a register based MEMORY, we'll ignore that,
        // and just return the record on the stack by value
        if (s.where != MEMORY)
            throw INTERNAL_ERROR;
        
        return Storage(STACK);
    }
};


class NullStringValue: public Value {
public:
    NullStringValue()
        :Value(STRING_TS) {
    }
    
    Regs precompile(Regs preferred) {
        return Regs();
    }
    
    Storage compile(X64 *x64) {
        x64->op(PUSHQ, 0);
        return Storage(STACK);
    }
};


class RecordUnwrapValue: public Value {
public:
    std::unique_ptr<Value> pivot;

    RecordUnwrapValue(TypeSpec cast_ts, Value *p)
        :Value(cast_ts) {
        pivot.reset(p);
    }

    virtual Regs precompile(Regs preferred) {
        return pivot->precompile(preferred);
    }

    virtual Storage compile(X64 *x64) {
        return pivot->compile(x64);
    }
};


class RecordWrapperValue: public GenericValue {
public:
    TypeSpec arg_cast_ts;

    RecordWrapperValue(Value *pivot, TypeSpec pcts, TypeSpec ats, TypeSpec acts, TypeSpec rts, std::string on)
        :GenericValue(ats, rts, NULL) {
        arg_cast_ts = acts;

        if (pcts != NO_TS)
            pivot = make_record_unwrap_value(pcts, pivot);
        
        if (on != "") {
            pivot = pivot->ts.lookup_inner(on, pivot);
            if (!pivot)
                throw INTERNAL_ERROR;
        }
        
        left.reset(pivot);
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!GenericValue::check(args, kwargs, scope))
            return false;
            
        if (right) {
            GenericValue *generic_left = dynamic_cast<GenericValue *>(left.get());
            if (!generic_left)
                throw INTERNAL_ERROR;
            
            Value *k = right.release();
        
            if (arg_cast_ts != NO_TS)
                k = make_record_unwrap_value(arg_cast_ts, k);
                
            generic_left->right.reset(k);
        }
        
        return true;
    }
    
    virtual Regs precompile(Regs preferred) {
        return left->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        Storage s = left->compile(x64);
        
        if (s.where == REGISTER) {
            x64->op(PUSHQ, s.reg);
            s = Storage(STACK);
        }
        
        return s;
    }
};

