
class ReferenceType: public Type {
public:
    ReferenceType(std::string name)
        :Type(name, TTs { IDENTITY_TYPE }, VALUE_TYPE) {
    }
    
    virtual Allocation measure(TypeMatch tm) {
        return Allocation(REFERENCE_SIZE);
    }

    virtual void incref(Register r, X64 *x64) {
        x64->incref(r);
    }

    virtual void decref(Register r, X64 *x64) {
        x64->decref(r);
    }

    virtual void store(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        switch (s.where * t.where) {
        case REGISTER_NOWHERE:
            decref(s.reg, x64);
            return;
        case REGISTER_REGISTER:
            if (s.reg != t.reg)
                x64->op(MOVQ, t.reg, s.reg);
            return;
        case REGISTER_STACK:
            x64->op(PUSHQ, s.reg);
            return;
        case REGISTER_MEMORY:
            x64->op(XCHGQ, t.address, s.reg);
            decref(s.reg, x64);
            return;

        case STACK_NOWHERE:
            x64->op(POPQ, RBX);
            decref(RBX, x64);
            return;
        case STACK_REGISTER:
            x64->op(POPQ, t.reg);
            return;
        case STACK_STACK:
            return;
        case STACK_MEMORY:
            x64->op(POPQ, RBX);
            x64->op(XCHGQ, RBX, t.address);
            decref(RBX, x64);
            return;

        case MEMORY_NOWHERE:
            return;
        case MEMORY_REGISTER:
            x64->op(MOVQ, t.reg, s.address);
            incref(t.reg, x64);
            return;
        case MEMORY_STACK:
            x64->op(MOVQ, RBX, s.address);
            incref(RBX, x64);
            x64->op(PUSHQ, RBX);
            return;
        case MEMORY_MEMORY:
            x64->op(MOVQ, RBX, s.address);
            incref(RBX, x64);
            x64->op(XCHGQ, RBX, t.address);
            decref(RBX, x64);
            return;
        default:
            Type::store(tm, s, t, x64);
        }
    }

    virtual void create(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        // Assume the target MEMORY is uninitialized
        
        switch (s.where * t.where) {
        case NOWHERE_MEMORY:
            std::cerr << "Reference must be initialized!\n";
            throw TYPE_ERROR;
        case REGISTER_MEMORY:
            x64->op(MOVQ, t.address, s.reg);
            return;
        case STACK_MEMORY:
            x64->op(POPQ, t.address);
            return;
        case MEMORY_MEMORY:
            x64->op(MOVQ, RBX, s.address);
            incref(RBX, x64);
            x64->op(MOVQ, t.address, RBX);
            return;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual void destroy(TypeMatch tm, Storage s, X64 *x64) {
        if (s.where == MEMORY) {
            x64->op(MOVQ, RBX, s.address);
            decref(RBX, x64);
        }
        else
            throw INTERNAL_ERROR;
    }

    virtual void compare(TypeMatch tm, Storage s, Storage t, X64 *x64, Label less, Label greater) {
        switch (s.where * t.where) {
        case REGISTER_REGISTER:
            decref(s.reg, x64);
            decref(t.reg, x64);
            x64->op(CMPQ, s.reg, t.reg);
            break;
        case REGISTER_STACK:
            x64->op(POPQ, RBX);
            decref(s.reg, x64);
            decref(RBX, x64);
            x64->op(CMPQ, s.reg, RBX);
            break;
        case REGISTER_MEMORY:
            decref(s.reg, x64);
            x64->op(CMPQ, s.reg, t.address);
            break;

        case STACK_REGISTER:
            x64->op(POPQ, RBX);
            decref(RBX, x64);
            decref(t.reg, x64);
            x64->op(CMPQ, RBX, t.reg);
            break;
        case STACK_STACK:
            x64->op(POPQ, RBX);
            decref(RBX, x64);
            x64->op(XCHGQ, RBX, Address(RSP, 0));
            decref(RBX, x64);
            x64->op(CMPQ, RBX, Address(RSP, 0));
            x64->op(POPQ, RBX);
            break;
        case STACK_MEMORY:
            x64->op(POPQ, RBX);
            decref(RBX, x64);
            x64->op(CMPQ, RBX, t.address);
            break;

        case MEMORY_REGISTER:
            decref(t.reg, x64);
            x64->op(CMPQ, s.address, t.reg);
            break;
        case MEMORY_STACK:
            x64->op(POPQ, RBX);
            decref(RBX, x64);
            x64->op(CMPQ, s.address, RBX);
            break;
        case MEMORY_MEMORY:
            x64->op(MOVQ, RBX, s.address);
            x64->op(CMPQ, RBX, t.address);
            break;
        default:
            throw INTERNAL_ERROR;
        }
        
        x64->op(JB, less);
        x64->op(JA, greater);
    }

    virtual StorageWhere where(TypeMatch tm, bool is_arg, bool is_lvalue) {
        return (is_arg ? (is_lvalue ? ALIAS : MEMORY) : (is_lvalue ? MEMORY : REGISTER));
    }

    virtual Storage boolval(TypeMatch tm, Storage s, X64 *x64, bool probe) {
        switch (s.where) {
        case CONSTANT:
            return Storage(CONSTANT, s.value != 0);
        case REGISTER:
            if (!probe)
                decref(s.reg, x64);
                
            x64->op(CMPQ, s.reg, 0);
            return Storage(FLAGS, SETNE);
        case MEMORY:
            x64->op(CMPQ, s.address, 0);
            return Storage(FLAGS, SETNE);
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Value *lookup_initializer(TypeMatch tm, std::string name) {
        return tm[1].lookup_initializer(name);
    }

    virtual Value *lookup_matcher(TypeMatch tm, std::string name, Value *pivot) {
        return tm[1].lookup_matcher(name, pivot);
    }

    virtual DataScope *get_inner_scope(TypeMatch tm) {
        return tm[1].get_inner_scope();
    }

    virtual std::vector<Function *> get_virtual_table(TypeMatch tm) {
        return tm[1].get_virtual_table();
    }

    virtual Label get_virtual_table_label(TypeMatch tm, X64 *x64) {
        return tm[1].get_virtual_table_label(x64);
    }

    virtual Value *autoconv(TypeMatch tm, TypeSpecIter target, Value *orig, TypeSpec &ifts) {
        return tm[1].autoconv(target, orig, ifts);
    }
};


class WeakReferenceType: public ReferenceType {
public:
    WeakReferenceType(std::string name)
        :ReferenceType(name) {
    }
    
    virtual void incref(Register r, X64 *x64) {
        x64->incweakref(r);
    }

    virtual void decref(Register r, X64 *x64) {
        x64->decweakref(r);
    }
};


class HeapType: public Type {
public:
    HeapType(std::string name, TTs param_tts)
        :Type(name, param_tts, IDENTITY_TYPE) {
    }

    virtual Allocation measure(TypeMatch tm) {
        std::cerr << "This is probably an error, shouldn't measure a heap type!\n";
        throw INTERNAL_ERROR;
    }
};


class ArrayType: public HeapType {
public:
    ArrayType(std::string name)
        :HeapType(name, TTs { VALUE_TYPE }) {
        make_inner_scope(TypeSpec { reference_type, this, any_type });
    }
    
    virtual Value *lookup_initializer(TypeMatch tm, std::string name) {
        TypeSpec rts = tm[0].prefix(reference_type);
        
        if (name == "empty")
            return make_array_empty_value(rts);
        else if (name == "{}")
            return make_array_initializer_value(rts);

        std::cerr << "No Array initializer called " << name << "!\n";
        return NULL;
    }

    virtual Label get_finalizer_label(TypeMatch tm, X64 *x64) {
        return x64->once->compile(compile_finalizer, tm[0]);
    }

    static void compile_finalizer(Label label, TypeSpec ts, X64 *x64) {
        TypeSpec elem_ts = ts.unprefix(array_type);
        int elem_size = elem_ts.measure_elem();
        Label start, end, loop;

        x64->code_label_local(label, "x_array_finalizer");
        //x64->log("finalize array");
        x64->op(PUSHQ, RCX);

        x64->op(MOVQ, RCX, Address(RAX, ARRAY_LENGTH_OFFSET));
        x64->op(CMPQ, RCX, 0);
        x64->op(JE, end);

        x64->op(IMUL3Q, RBX, RCX, elem_size);
        x64->op(LEA, RAX, Address(RAX, ARRAY_ELEMS_OFFSET));
        x64->op(ADDQ, RAX, RBX);

        x64->code_label(loop);
        x64->op(SUBQ, RAX, elem_size);
        elem_ts.destroy(Storage(MEMORY, Address(RAX, 0)), x64);
        x64->op(DECQ, RCX);
        x64->op(JNE, loop);

        x64->code_label(end);
        x64->op(POPQ, RCX);
        //x64->log("finalized array");
        x64->op(RET);
    }
};


class CircularrayType: public HeapType {
public:
    CircularrayType(std::string name)
        :HeapType(name, TTs { VALUE_TYPE }) {
        make_inner_scope(TypeSpec { reference_type, this, any_type });
    }

    virtual Value *lookup_initializer(TypeMatch tm, std::string name) {
        TypeSpec rts = tm[0].prefix(reference_type);
        
        if (name == "empty")
            return make_circularray_empty_value(rts);
        else if (name == "{}")
            return make_circularray_initializer_value(rts);

        std::cerr << "No Circularray initializer called " << name << "!\n";
        return NULL;
    }

    virtual Label get_finalizer_label(TypeMatch tm, X64 *x64) {
        return x64->once->compile(compile_finalizer, tm[0]);
    }

    static void compile_finalizer(Label label, TypeSpec ts, X64 *x64) {
        TypeSpec elem_ts = ts.unprefix(circularray_type);
        int elem_size = elem_ts.measure_elem();
        Label start, end, loop, ok, ok1;
    
        x64->code_label_local(label, "x_circularray_finalizer");
        //x64->log("finalize circularray");
        x64->op(PUSHQ, RCX);
        x64->op(PUSHQ, RDX);
    
        x64->op(MOVQ, RCX, Address(RAX, CIRCULARRAY_LENGTH_OFFSET));
        x64->op(CMPQ, RCX, 0);
        x64->op(JE, end);
    
        x64->op(MOVQ, RDX, Address(RAX, CIRCULARRAY_FRONT_OFFSET));
        x64->op(ADDQ, RDX, RCX);
        x64->op(CMPQ, RDX, Address(RAX, CIRCULARRAY_RESERVATION_OFFSET));
        x64->op(JBE, ok1);
        
        x64->op(SUBQ, RDX, Address(RAX, CIRCULARRAY_RESERVATION_OFFSET));
        
        x64->code_label(ok1);
        x64->op(IMUL3Q, RDX, RDX, elem_size);
    
        x64->code_label(loop);
        x64->op(SUBQ, RDX, elem_size);
        x64->op(CMPQ, RDX, 0);
        x64->op(JGE, ok);
        
        x64->op(MOVQ, RDX, Address(RAX, CIRCULARRAY_RESERVATION_OFFSET));
        x64->op(DECQ, RDX);
        x64->op(IMUL3Q, RDX, RDX, elem_size);
        
        x64->code_label(ok);
        Address elem_addr = Address(RAX, RDX, CIRCULARRAY_ELEMS_OFFSET);
        elem_ts.destroy(Storage(MEMORY, elem_addr), x64);
        x64->op(DECQ, RCX);
        x64->op(JNE, loop);
    
        x64->code_label(end);
        x64->op(POPQ, RDX);
        x64->op(POPQ, RCX);
        //x64->log("finalized circularray");
        x64->op(RET);
    }
};


class RbtreeType: public HeapType {
public:
    RbtreeType(std::string name)
        :HeapType(name, TTs { VALUE_TYPE }) {
        make_inner_scope(TypeSpec { reference_type, this, any_type });
    }
    
    virtual Value *lookup_initializer(TypeMatch tm, std::string name) {
        TypeSpec rts = tm[0].prefix(reference_type);
        
        if (name == "empty")
            return make_rbtree_empty_value(rts);
        else if (name == "reserved")
            return make_rbtree_reserved_value(rts);
        else if (name == "{}")
            return make_rbtree_initializer_value(rts);

        std::cerr << "No " << this->name << " initializer called " << name << "!\n";
        return NULL;
    }

    virtual Label get_finalizer_label(TypeMatch tm, X64 *x64) {
        return x64->once->compile(compile_finalizer, tm[0]);
    }

    static void compile_finalizer(Label label, TypeSpec ts, X64 *x64) {
        TypeSpec elem_ts = ts.unprefix(rbtree_type);
        Label loop, cond;

        x64->code_label(label);
        x64->op(PUSHQ, RCX);

        x64->op(MOVQ, RCX, Address(RAX, RBTREE_LAST_OFFSET));
        x64->op(JMP, cond);
        
        x64->code_label(loop);
        elem_ts.destroy(Storage(MEMORY, Address(RAX, RCX, RBNODE_VALUE_OFFSET)), x64);
        x64->op(MOVQ, RCX, Address(RAX, RCX, RBNODE_PREV_IS_RED_OFFSET));
        x64->op(ANDQ, RCX, -2);
        
        x64->code_label(cond);
        x64->op(CMPQ, RCX, RBNODE_NIL);
        x64->op(JNE, loop);
        
        x64->op(POPQ, RCX);
        x64->op(RET);
    }
};
