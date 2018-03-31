
class ClassPreinitializerValue: public Value {
public:
    ClassPreinitializerValue(TypeSpec rts)
        :Value(rts.reprefix(ref_type, weakref_type).prefix(initializable_type)) {
    }
    
    virtual Regs precompile(Regs preferred) {
        return Regs(RAX);
    }
    
    virtual Storage compile(X64 *x64) {
        TypeSpec class_ts = ts.unprefix(initializable_type).unprefix(weakref_type);;
        Label finalizer_label = class_ts.get_finalizer_label(x64);
        unsigned heap_size = class_ts.measure_raw();
        
        x64->op(MOVQ, RAX, heap_size);
        //std::cerr << "XXX Allocating " << heap_size << " on the heap.\n";
        x64->op(LEARIP, RBX, finalizer_label);
        x64->runtime->alloc_RAX_RBX();

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


class ClassMatcherValue: public Value, public Raiser {
public:
    std::string name;
    std::unique_ptr<Value> value;

    ClassMatcherValue(std::string n, Value *v)
        :Value(NO_TS) {
        name = n;
        value.reset(v);
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        // This is ridiculously late, but better than never
        Expr *expr = new Expr(Expr::IDENTIFIER, Token(), name);
        Value *tv = lookup(name, NULL, expr, scope, NULL);
        
        if (!tv)
            return false;
            
        if (!tv->ts.is_meta()) {
            std::cerr << "Class matcher is not a type name!\n";
            return false;
        }
        
        TypeSpec tts = ptr_cast<TypeValue>(tv)->represented_ts;
        ClassType *ct = ptr_cast<ClassType>(tts[0]);
        
        if (!ct) {
            std::cerr << "Class matcher is not a class name!\n";
            return false;
        }
        
        if (args.size() != 0 || kwargs.size() != 0) {
            std::cerr << "Whacky class matcher!\n";
            return false;
        }
        
        TypeSpec vts = value->ts.rvalue();
        ts = tts.prefix(vts[0]);
        std::cerr << "Class matcher " << name << " is " << ts << ".\n";
        
        if (!check_raise(match_unmatched_exception_type, scope))
            return false;

        return true;
    }

    virtual Regs precompile(Regs preferred) {
        return value->precompile(preferred);
    }

    virtual Storage compile(X64 *x64) {
        Label loop, cond, matched;
        
        value->compile_and_store(x64, Storage(STACK));
        
        x64->op(LEARIP, RBX, ts.get_virtual_table_label(x64));
        x64->op(PUSHQ, RBX);
        
        x64->op(MOVQ, RBX, Address(RSP, ADDRESS_SIZE));  // the reference
        x64->op(MOVQ, RBX, Address(RBX, 0));  // the virtual table
        x64->op(JMP, cond);
        
        x64->code_label(loop);
        x64->op(MOVQ, RBX, Address(RBX, 0));  // base virtual table
        
        x64->code_label(cond);
        x64->op(CMPQ, RBX, Address(RSP, 0));
        x64->op(JE, matched);
        x64->op(CMPQ, RBX, 0);
        x64->op(JNE, loop);
        
        // Not matched
        x64->op(ADDQ, RSP, ADDRESS_SIZE);
        ts.store(Storage(STACK), Storage(), x64);
        raise("UNMATCHED", x64);
        
        x64->code_label(matched);
        x64->op(ADDQ, RSP, ADDRESS_SIZE);
        
        return Storage(STACK);
    }
};
