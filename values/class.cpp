
class ClassPreinitializerValue: public Value {
public:
    ClassPreinitializerValue(TypeSpec rts)
        :Value(rts.reprefix(ref_type, ptr_type).prefix(initializable_type)) {
    }
    
    virtual Regs precompile(Regs preferred) {
        return Regs::all();
    }
    
    virtual Storage compile(X64 *x64) {
        TypeSpec class_ts = ts.unprefix(initializable_type).unprefix(ptr_type);;
        Label finalizer_label = class_ts.get_finalizer_label(x64);
        unsigned heap_size = class_ts.measure_raw();
        
        x64->op(PUSHQ, heap_size);
        //std::cerr << "XXX Allocating " << heap_size << " on the heap.\n";
        x64->op(LEA, R10, Address(finalizer_label, 0));
        x64->op(PUSHQ, R10);
        
        x64->runtime->heap_alloc();  // clobbers all
        
        x64->op(ADDQ, RSP, 2 * ADDRESS_SIZE);

        Label vt_label = class_ts.get_virtual_table_label(x64);
        class_ts.init_vt(Address(RAX, 0), vt_label, x64);
        
        return Storage(REGISTER, RAX);
    }
};


class ClassPostinitializerValue: public Value {
public:
    std::unique_ptr<Value> pivot;

    ClassPostinitializerValue(Value *p)
        :Value(p->ts.unprefix(initializable_type).reprefix(ptr_type, ref_type)) {
        pivot.reset(p);
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        return pivot->check(args, kwargs, scope);
    }

    virtual Regs precompile(Regs preferred) {
        return pivot->precompile(preferred);
    }

    virtual Storage compile(X64 *x64) {
        return pivot->compile(x64);
    }
};


class ClassWrapperInitializerValue: public Value {
public:
    std::unique_ptr<Value> object, value;
    
    ClassWrapperInitializerValue(Value *o, Value *v)
        :Value(o->ts.unprefix(initializable_type).reprefix(ptr_type, ref_type)) {
        object.reset(o);
        value.reset(v);
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        return value->check(args, kwargs, scope);
    }
    
    virtual Regs precompile(Regs preferred) {
        return object->precompile(preferred) | value->precompile(preferred) | Regs(RAX);
    }

    virtual Storage compile(X64 *x64) {
        object->compile_and_store(x64, Storage(STACK));
        value->compile_and_store(x64, Storage(STACK));
        
        x64->op(MOVQ, RAX, Address(RSP, REFERENCE_SIZE));
        value->ts.create(Storage(STACK), Storage(MEMORY, Address(RAX, CLASS_MEMBERS_OFFSET)), x64);
        //x64->op(POPQ, Address(R10, CLASS_MEMBERS_OFFSET));  // creating ref from STACK to MEMORY

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
        Value *tv = lookup(name, NULL, scope, expr, NULL);
        
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
        
        x64->op(LEA, R10, Address(ts.get_virtual_table_label(x64), 0));
        x64->op(PUSHQ, R10);
        
        x64->op(MOVQ, R10, Address(RSP, ADDRESS_SIZE));  // the reference
        x64->op(MOVQ, R10, Address(R10, CLASS_VT_OFFSET));  // the virtual table
        x64->op(JMP, cond);
        
        x64->code_label(loop);
        x64->op(MOVQ, R10, Address(R10, VT_BASEVT_INDEX * ADDRESS_SIZE));  // base virtual table
        
        x64->code_label(cond);
        x64->op(CMPQ, R10, Address(RSP, 0));
        x64->op(JE, matched);
        x64->op(CMPQ, R10, 0);
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
