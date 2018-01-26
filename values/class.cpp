
class ClassPreinitializerValue: public Value {
public:
    ClassPreinitializerValue(TypeSpec rts)
        :Value(rts.prefix(partial_type)) {
    }
    
    virtual Regs precompile(Regs preferred) {
        return Regs().add(RAX);
    }
    
    virtual Storage compile(X64 *x64) {
        TypeSpec class_ts = ts.unprefix(partial_type).unprefix(reference_type);
        Label finalizer_label = dynamic_cast<ClassType *>(class_ts[0])->get_finalizer_label(class_ts.begin(), x64);
        unsigned heap_size = class_ts.measure(MEMORY);
        
        x64->op(MOVQ, RAX, heap_size);
        //std::cerr << "XXX Allocating " << heap_size << " on the heap.\n";
        x64->op(LEARIP, RBX, finalizer_label);
        x64->alloc_RAX_RBX();

        x64->op(LEARIP, RBX, class_ts.get_virtual_table_label(x64));
        x64->op(MOVQ, Address(RAX, CLASS_VT_OFFSET), RBX);
        
        return Storage(REGISTER, RAX);
    }
};


class ClassUnwrapValue: public Value {
public:
    std::unique_ptr<Value> pivot;

    ClassUnwrapValue(TypeSpec cast_ts, Value *p)
        :Value(cast_ts) {
        pivot.reset(p);
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = pivot->precompile(preferred);
        clob = clob.add(RAX);
        return clob;
    }

    virtual Storage compile(X64 *x64) {
        Storage s = pivot->compile(x64);
        
        switch (s.where) {
        case REGISTER:
            x64->decref(s.reg);
            return Storage(MEMORY, Address(s.reg, CLASS_MEMBERS_OFFSET));
        case STACK:
            x64->op(POPQ, RAX);
            x64->decref(RAX);
            return Storage(MEMORY, Address(RAX, CLASS_MEMBERS_OFFSET));
        case MEMORY:
            x64->op(MOVQ, RAX, s.address);
            return Storage(MEMORY, Address(RAX, CLASS_MEMBERS_OFFSET));
        case ALISTACK:
            x64->op(POPQ, RBX);
            x64->op(MOVQ, RAX, Address(RBX, 0));
            x64->decref(RAX);
            return Storage(MEMORY, Address(RAX, CLASS_MEMBERS_OFFSET));
        case ALIAS:
            x64->op(MOVQ, RBX, s.address);
            x64->op(MOVQ, RAX, Address(RBX, 0));
            x64->decref(RAX);
            return Storage(MEMORY, Address(RAX, CLASS_MEMBERS_OFFSET));
        default:
            throw INTERNAL_ERROR;
        }
    }
};

/*
class ClassWrapperValue: public GenericValue {
public:
    TypeSpec arg_cast_ts;

    ClassWrapperValue(Value *pivot, TypeSpec pcts, TypeSpec ats, TypeSpec acts, TypeSpec rts, std::string on)
        :GenericValue(ats, rts, NULL) {
        arg_cast_ts = acts;

        if (pcts != NO_TS)
            pivot = make_class_unwrap_value(pcts, pivot);
        
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
                k = make_class_unwrap_value(arg_cast_ts, k);
                
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

*/
