
class ClassPreinitializerValue: public Value {
public:
    ClassPreinitializerValue(TypeSpec rts)
        :Value(rts.reprefix(ref_type, weakref_type).prefix(initializable_type)) {
    }
    
    virtual Regs precompile(Regs preferred) {
        return Regs().add(RAX);
    }
    
    virtual Storage compile(X64 *x64) {
        TypeSpec class_ts = ts.unprefix(initializable_type).unprefix(weakref_type);;
        Label finalizer_label = class_ts.get_finalizer_label(x64);
        unsigned heap_size = class_ts.measure_raw();
        
        x64->op(MOVQ, RAX, heap_size);
        //std::cerr << "XXX Allocating " << heap_size << " on the heap.\n";
        x64->op(LEARIP, RBX, finalizer_label);
        x64->alloc_RAX_RBX();

        Label vt_label = class_ts.get_virtual_table_label(x64);
        int virtual_offset = 0;
        Address addr(RAX, 0);
        int data_offset = 0;
        
        class_ts.init_vt(addr, data_offset, vt_label, virtual_offset, x64);
        
        return Storage(REGISTER, RAX);
    }
};


class ClassWrapperInitializerValue: public Value {
public:
    std::unique_ptr<Value> object, value;
    
    ClassWrapperInitializerValue(Value *o, Value *v)
        :Value(o->ts.unprefix(initializable_type).reprefix(weakref_type, ref_type)) {
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


class RoleDefinitionValue: public Value {
public:
    std::unique_ptr<Value> value;
    TypeSpec role_ts, pivot_ts;
    
    RoleDefinitionValue(Value *pivot, TypeMatch &tm)
        :Value(VOID_TS) {
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (args.size() > 1 || kwargs.size() != 0) {
            std::cerr << "Whacky role definition!\n";
            return false;
        }

        // TODO: check for Class definition scope!
        Value *v = typize(args[0].get(), scope, NULL);
    
        if (v->ts[0] != type_type || !dynamic_cast<ClassType *>(v->ts[1])) {
            std::cerr << "Class type name expected!\n";
            return false;
        }

        role_ts = v->ts.unprefix(type_type);
        pivot_ts = scope->pivot_type_hint();
        value.reset(v);
        
        return true;
    }
    
    virtual Regs precompile(Regs preferred) {
        return Regs();
    }
    
    virtual Storage compile(X64 *x64) {
        return Storage();
    }

    virtual Declaration *declare(std::string name, ScopeType st) {
        return new Role(name, pivot_ts, role_ts);
    }
};
