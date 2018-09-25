
class ReferenceType: public Type {
public:
    ReferenceType(std::string name)
        :Type(name, Metatypes { identity_metatype }, value_metatype) {
    }
    
    virtual Allocation measure(TypeMatch tm) {
        return Allocation(REFERENCE_SIZE);
    }

    virtual void incref(Register r, X64 *x64, bool is_class) {
        x64->runtime->incref(r);
    }

    virtual void decref(Register r, X64 *x64, bool is_class) {
        x64->runtime->decref(r);
    }

    virtual void store(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        bool is_class = tm[1].has_meta(class_metatype);
    
        switch (s.where * t.where) {
        case NOWHERE_REGISTER:
            std::cerr << "Reference must be initialized!\n";
            throw TYPE_ERROR;
        case NOWHERE_STACK:
            std::cerr << "Reference must be initialized!\n";
            throw TYPE_ERROR;
        
        case REGISTER_NOWHERE:
            decref(s.reg, x64, is_class);
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
            decref(s.reg, x64, is_class);
            return;

        case STACK_NOWHERE:
            x64->op(POPQ, RBX);
            decref(RBX, x64, is_class);
            return;
        case STACK_REGISTER:
            x64->op(POPQ, t.reg);
            return;
        case STACK_STACK:
            return;
        case STACK_MEMORY:
            x64->op(POPQ, RBX);
            x64->op(XCHGQ, RBX, t.address);
            decref(RBX, x64, is_class);
            return;

        case MEMORY_NOWHERE:
        case BMEMORY_NOWHERE:
            return;
        case MEMORY_REGISTER:
        case BMEMORY_REGISTER:
            x64->op(MOVQ, t.reg, s.address);
            incref(t.reg, x64, is_class);
            return;
        case MEMORY_STACK:
        case BMEMORY_STACK:
            x64->op(MOVQ, RBX, s.address);
            incref(RBX, x64, is_class);
            x64->op(PUSHQ, RBX);
            return;
        case MEMORY_MEMORY:  // must work with self-assignment
        case BMEMORY_MEMORY:
            x64->op(MOVQ, RBX, s.address);
            incref(RBX, x64, is_class);
            x64->op(XCHGQ, RBX, t.address);
            decref(RBX, x64, is_class);
            return;

        case BREGISTER_NOWHERE:
            return;
            
        case BSTACK_NOWHERE:
            x64->op(ADDQ, RSP, REFERENCE_SIZE);
            return;
        case BSTACK_BSTACK:
            return;
            
        case BMEMORY_BREGISTER:
            x64->op(MOVQ, t.reg, s.address);
            return;
        case BMEMORY_BSTACK:
            x64->op(PUSHQ, s.address);
            return;
            
        default:
            Type::store(tm, s, t, x64);
        }
    }

    virtual void create(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        // Assume the target MEMORY is uninitialized
        bool is_class = tm[1].has_meta(class_metatype);
        
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
            incref(RBX, x64, is_class);
            x64->op(MOVQ, t.address, RBX);
            return;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual void destroy(TypeMatch tm, Storage s, X64 *x64) {
        bool is_class = tm[1].has_meta(class_metatype);

        if (s.where == MEMORY) {
            x64->op(MOVQ, RBX, s.address);
            decref(RBX, x64, is_class);
        }
        else
            throw INTERNAL_ERROR;
    }

    virtual void equal(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        // No need to handle STACK here, GenericOperationValue takes care of it
        bool is_class = tm[1].has_meta(class_metatype);
        
        switch (s.where * t.where) {
        case REGISTER_REGISTER:
            decref(s.reg, x64, is_class);
            decref(t.reg, x64, is_class);
            x64->op(CMPQ, s.reg, t.reg);
            break;
        case REGISTER_MEMORY:
            decref(s.reg, x64, is_class);
            x64->op(CMPQ, s.reg, t.address);
            break;

        case MEMORY_REGISTER:
            decref(t.reg, x64, is_class);
            x64->op(CMPQ, s.address, t.reg);
            break;
        case MEMORY_MEMORY:
            x64->op(MOVQ, RBX, s.address);
            x64->op(CMPQ, RBX, t.address);
            break;
            
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual void compare(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        equal(tm, s, t, x64);
        x64->blcompar(true);
    }
    
    virtual void streamify(TypeMatch tm, bool repr, X64 *x64) {
        // We do this for reference types that don't implement Streamifiable
        Label label;
        x64->code_label_import(label, "streamify_reference");
        
        x64->op(MOVQ, RDI, Address(RSP, ALIAS_SIZE));
        x64->op(MOVQ, RSI, Address(RSP, 0));
        
        x64->runtime->call_sysv(label);
    }
    
    virtual StorageWhere where(TypeMatch tm, AsWhat as_what) {
        return (
            as_what == AS_VALUE ? REGISTER :
            as_what == AS_VARIABLE ? MEMORY :
            as_what == AS_ARGUMENT ? MEMORY :
            as_what == AS_PIVOT_ARGUMENT ? MEMORY :
            as_what == AS_LVALUE_ARGUMENT ? ALIAS :
            throw INTERNAL_ERROR
        );
    }

    virtual Value *lookup_inner(TypeMatch tm, std::string n, Value *v, Scope *s) {
        //std::cerr << "Ref inner lookup " << tm << " " << n << ".\n";
        Value *value = Type::lookup_inner(tm, n, v, s);
        
        if (value)
            return value;

        return tm[1].lookup_inner(n, v, s);
    }

    virtual Value *lookup_initializer(TypeMatch tm, std::string name, Scope *scope) {
        return tm[1].lookup_initializer(name, scope);
    }

    virtual Value *lookup_matcher(TypeMatch tm, std::string name, Value *pivot, Scope *scope) {
        return tm[1].lookup_matcher(name, pivot, scope);
    }

    virtual std::vector<VirtualEntry *> get_virtual_table(TypeMatch tm) {
        return tm[1].get_virtual_table();
    }

    virtual Label get_virtual_table_label(TypeMatch tm, X64 *x64) {
        return tm[1].get_virtual_table_label(x64);
    }

    virtual Value *autoconv(TypeMatch tm, TypeSpecIter target, Value *orig, TypeSpec &ifts) {
        return tm[1].autoconv(target, orig, ifts);
    }
};


class PointerType: public ReferenceType {
public:
    PointerType(std::string name)
        :ReferenceType(name) {
    }

    virtual void borrow(TypeMatch tm, Register reg, Unborrow *unborrow, X64 *x64) {
        bool is_class = tm[1].has_meta(class_metatype);
        
        if (is_class) {
            x64->op(MOVQ, RBX, Address(reg, CLASS_VT_OFFSET));
            x64->op(MOVQ, RBX, Address(RBX, VT_FASTFORWARD_INDEX * ADDRESS_SIZE));
            x64->op(ADDQ, RBX, reg);
            x64->op(MOVQ, unborrow->get_address(), RBX);
        }
        else
            x64->op(MOVQ, unborrow->get_address(), reg);
    }

    virtual void incref(Register r, X64 *x64, bool is_class) {
        // Class Ptr-s may point to roles within the object

        if (is_class) {
            x64->op(PUSHQ, r);
            x64->op(MOVQ, r, Address(r, CLASS_VT_OFFSET));
            x64->op(MOVQ, r, Address(r, VT_FASTFORWARD_INDEX * ADDRESS_SIZE));
            x64->op(ADDQ, r, Address(RSP, 0));
            x64->runtime->incref(r);
            x64->op(POPQ, r);
        }
        else
            x64->runtime->incref(r);
    }

    virtual void decref(Register r, X64 *x64, bool is_class) {
        // Class Ptr-s may point to roles within the object

        if (is_class) {
            x64->op(PUSHQ, r);
            x64->op(MOVQ, r, Address(r, CLASS_VT_OFFSET));
            x64->op(MOVQ, r, Address(r, VT_FASTFORWARD_INDEX * ADDRESS_SIZE));
            x64->op(ADDQ, r, Address(RSP, 0));
            x64->runtime->decref(r);
            x64->op(POPQ, r);
        }
        else
            x64->runtime->decref(r);
    }
};


// This is a hack type to cooperate closely with Weak*Map
// It contains a raw pointer first field, so it can disguise as a Ptr within Weak*Map, and
// comparisons would work with the input Ptr-s. But it actually contains another
// pointer to an FCB that gets triggered when the pointed object is finalized.
class NosyValueType: public PointerType {
public:
    NosyValueType(std::string name)
        :PointerType(name) {
    }

    virtual Allocation measure(TypeMatch tm) {
        return Allocation(NOSYVALUE_SIZE);
    }
    
    virtual void store(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        throw INTERNAL_ERROR;  // for safety, we'll handle everything manually
    }

    virtual void create(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        // The only known usage is storing a STACK argument into a Weak*Map entry
        if (s.where != STACK || t.where != MEMORY)
            throw INTERNAL_ERROR;

        x64->op(POPQ, t.address + NOSYVALUE_RAW_OFFSET);
        x64->op(POPQ, t.address + NOSYVALUE_FCB_OFFSET);
    }

    virtual void destroy(TypeMatch tm, Storage s, X64 *x64) {
        if (s.where != MEMORY)
            throw INTERNAL_ERROR;
            
        x64->op(PUSHQ, RAX);
        x64->op(MOVQ, RAX, s.address + NOSYVALUE_FCB_OFFSET);
        x64->op(CALL, x64->runtime->free_fcb_label);
        x64->op(POPQ, RAX);
    }
};


class HeapType: public Type {
public:
    HeapType(std::string name, Metatypes param_metatypes, Type *mt = NULL)
        :Type(name, param_metatypes, mt ? mt : identity_metatype) {
    }

    virtual Allocation measure(TypeMatch tm) {
        std::cerr << "This is probably an error, shouldn't measure a heap type!\n";
        throw INTERNAL_ERROR;
    }
};



class NosyObjectType: public HeapType {
public:
    NosyObjectType(std::string name)
        :HeapType(name, Metatypes { identity_metatype }) {
    }
    
    virtual Value *lookup_initializer(TypeMatch tm, std::string name, Scope *scope) {
        TypeSpec rts = tm[0].prefix(ref_type);
        
        if (name == "to")
            return make<NosyObjectValue>(rts);

        std::cerr << "No NosyObject initializer called " << name << "!\n";
        return NULL;
    }

    virtual Value *lookup_matcher(TypeMatch tm, std::string n, Value *pivot, Scope *scope) {
        if (n == "dead")
            return make<NosyObjectDeadMatcherValue>(pivot, tm);
        else if (n == "live")
            return make<NosyObjectLiveMatcherValue>(pivot, tm);
            
        std::cerr << "Can't match NosyObject as " << n << "!\n";
        return NULL;
    }

    virtual Label get_finalizer_label(TypeMatch tm, X64 *x64) {
        return x64->once->compile(compile_finalizer, tm[0]);
    }

    static void compile_finalizer(Label label, TypeSpec ts, X64 *x64) {
        Label skip;

        x64->code_label_local(label, "x_nosyobject_finalizer");
        x64->runtime->log("Nosy object finalized.");
        
        x64->op(MOVQ, RAX, Address(RAX, NOSYOBJECT_FCB_OFFSET));
        x64->op(CMPQ, RAX, 0);
        x64->op(JE, skip);
        
        x64->op(CALL, x64->runtime->free_fcb_label);
        
        x64->code_label(skip);
        x64->op(RET);
    }
};


class ArrayType: public HeapType {
public:
    ArrayType(std::string name)
        :HeapType(name, Metatypes { value_metatype }) {
        make_inner_scope(TypeSpec { ref_type, this, any_type });
    }
    
    virtual Value *lookup_initializer(TypeMatch tm, std::string name, Scope *scope) {
        TypeSpec rts = tm[0].prefix(ref_type);
        
        if (name == "empty")
            return make<ArrayEmptyValue>(rts);
        else if (name == "reserved")
            return make<ArrayReservedValue>(rts);
        else if (name == "all")
            return make<ArrayAllValue>(rts);
        else if (name == "{}")
            return make<ArrayInitializerValue>(rts);

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
        :HeapType(name, Metatypes { value_metatype }) {
        make_inner_scope(TypeSpec { ref_type, this, any_type });
    }

    virtual Value *lookup_initializer(TypeMatch tm, std::string name, Scope *scope) {
        TypeSpec rts = tm[0].prefix(ref_type);

        if (name == "empty")
            return make<CircularrayEmptyValue>(rts);
        else if (name == "reserved")
            return make<CircularrayReservedValue>(rts);
        else if (name == "{}")
            return make<CircularrayInitializerValue>(rts);

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
        :HeapType(name, Metatypes { value_metatype }) {
        make_inner_scope(TypeSpec { ref_type, this, any_type });
    }
    
    virtual Value *lookup_initializer(TypeMatch tm, std::string name, Scope *scope) {
        TypeSpec rts = tm[0].prefix(ref_type);
        
        if (name == "empty")
            return make<RbtreeEmptyValue>(rts);
        else if (name == "reserved")
            return make<RbtreeReservedValue>(rts);
        else if (name == "{}")
            return make<RbtreeInitializerValue>(rts);

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
        x64->op(MOVQ, RCX, Address(RAX, RCX, RBNODE_PRED_OFFSET));
        x64->op(ANDQ, RCX, ~RBNODE_RED_BIT);
        
        x64->code_label(cond);
        x64->op(CMPQ, RCX, RBNODE_NIL);
        x64->op(JNE, loop);
        
        x64->op(POPQ, RCX);
        x64->op(RET);
    }
};
