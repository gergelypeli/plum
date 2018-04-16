

class RecordOperationValue: public GenericOperationValue {
public:
    RecordOperationValue(OperationType o, Value *p, TypeMatch &match)
        :GenericOperationValue(o, op_arg_ts(o, match), op_ret_ts(o, match), p) {
        //std::cerr << "XXX Record " << match[0] << " operation " << o << ".\n";
    }
};


class RecordInitializerValue: public Value {
public:
    RecordType *record_type;
    std::vector<std::unique_ptr<Value>> values;
    std::vector<TypeSpec> member_tss;
    std::vector<std::string> member_names;
    std::vector<Storage> var_storages;
    TypeMatch match;
    
    RecordInitializerValue(TypeMatch &tm)
        :Value(tm[0]) {
        record_type = ptr_cast<RecordType>(ts[0]);
        member_tss = record_type->get_member_tss(tm);
        member_names = record_type->get_member_names();
        match = tm;
        
        std::cerr << "Record " << record_type->name << " initialization with members: " << member_tss << ".\n";
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        ArgInfos infos;

        // Separate loop, so reallocations won't screw us
        for (unsigned i = 0; i < member_tss.size(); i++) {
            values.push_back(NULL);
            member_tss[i] = member_tss[i].prefix(ovalue_type);  // TODO
        }
        
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
            TypeSpec var_ts = member_tss[i];
            Value *v = values[i].get();
            Storage s;
            
            if (v)
                s = v->compile(x64);
            
            int offset = 0;
            
            if (s.where == STACK)
                offset = var_ts.measure_stack();

            Storage t = var->get_storage(match, Storage(MEMORY, Address(RSP, offset)));
            
            var_ts.create(s, t, x64);
            
            var_storages.push_back(t + (-offset));
        }
        
        x64->unwind->pop(this);

        return Storage(STACK);
    }
    
    virtual Scope *unwind(X64 *x64) {
        for (int i = var_storages.size() - 1; i >= 0; i--)
            unwind_destroy_var(record_type->member_variables[i]->alloc_ts, var_storages[i], x64);

        x64->op(ADDQ, RSP, ts.measure_stack());
            
        return NULL;
    }
};


class RecordPreinitializerValue: public Value {
public:
    RecordPreinitializerValue(TypeSpec ts)
        :Value(ts.prefix(lvalue_type).prefix(initializable_type)) {  // TODO: eiiii...
    }

    virtual Regs precompile(Regs preferred) {
        return Regs();
    }
    
    virtual Storage compile(X64 *x64) {
        //ts.create(Storage(), Storage(STACK), x64);
        x64->op(SUBQ, RSP, ts.measure_stack());
        return Storage(MEMORY, Address(RSP, 0));
    }
};


class RecordPostinitializerValue: public Value {
public:
    std::unique_ptr<Value> value;
    
    RecordPostinitializerValue(Value *v)
        :Value(v->ts.unprefix(initializable_type)) {
        value.reset(v);
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        return value->check(args, kwargs, scope);
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


class RecordWrapperValue: public Value {
public:
    std::unique_ptr<Value> operation;
    std::string arg_operation_name;

    RecordWrapperValue(Value *pivot, TypeSpec pcts, TypeSpec rts, std::string on, std::string aon)
        :Value(rts) {
        arg_operation_name = aon;
        
        if (pcts != NO_TS)
            pivot = make_record_unwrap_value(pcts, pivot);
        
        if (on != "") {
            pivot = pivot->ts.lookup_inner(on, pivot);
            if (!pivot)
                throw INTERNAL_ERROR;
        }
        
        operation.reset(pivot);
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (arg_operation_name.size()) {       
            if (args.size() != 1 || kwargs.size() != 0)
                return false;

            Expr *expr = new Expr(Expr::IDENTIFIER, Token(), arg_operation_name);
            args[0].reset(expr->set_pivot(args[0].release()));
        }

        return operation->check(args, kwargs, scope);
    }
    
    virtual Regs precompile(Regs preferred) {
        return operation->precompile(preferred);
    }
    
    virtual Storage compile(X64 *x64) {
        Storage s = operation->compile(x64);
        
        if (s.where == REGISTER) {
            x64->op(PUSHQ, s.reg);
            s = Storage(STACK);
        }
        
        return s;
    }
};

