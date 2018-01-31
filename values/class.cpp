
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


class ClassWrapperInitializerValue: public Value {
public:
    std::unique_ptr<Value> object, value;
    
    ClassWrapperInitializerValue(Value *o, Value *v)
        :Value(o->ts.unprefix(partial_type)) {
        object.reset(o);
        value.reset(v);
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        return value->check(args, kwargs, scope);
    }
    
    virtual Regs precompile(Regs preferred) {
        return object->precompile(preferred) | value->precompile(preferred);
    }

    virtual Storage compile(X64 *x64) {
        object->compile_and_store(x64, Storage(STACK));
        value->compile_and_store(x64, Storage(STACK));
        
        x64->op(MOVQ, RBX, Address(RSP, REFERENCE_SIZE));
        x64->op(POPQ, Address(RBX, CLASS_MEMBERS_OFFSET));  // creating ref from STACK to MEMORY
        
        return Storage(STACK);
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

