
// Linearray based

class LinearrayType: public HeapType {
public:
    LinearrayType(std::string name)
        :HeapType(name, Metatypes { value_metatype }) {
        //make_inner_scope(TypeSpec { ref_type, this, any_type });
    }
    
    virtual Label get_finalizer_label(TypeMatch tm, X64 *x64) {
        return x64->once->compile(compile_finalizer, tm[0]);
    }

    static void compile_finalizer(Label label, TypeSpec ts, X64 *x64) {
        TypeSpec elem_ts = ts.unprefix(linearray_type);
        int elem_size = elem_ts.measure_elem();
        Label start, end, loop;

        x64->code_label_local(label, elem_ts.symbolize() + "_linearray_finalizer");
        x64->op(MOVQ, RAX, Address(RSP, ADDRESS_SIZE));

        x64->op(MOVQ, RCX, Address(RAX, LINEARRAY_LENGTH_OFFSET));
        x64->op(CMPQ, RCX, 0);
        x64->op(JE, end);

        x64->op(IMUL3Q, R10, RCX, elem_size);
        x64->op(LEA, RAX, Address(RAX, LINEARRAY_ELEMS_OFFSET));
        x64->op(ADDQ, RAX, R10);

        x64->code_label(loop);
        x64->op(SUBQ, RAX, elem_size);
        elem_ts.destroy(Storage(MEMORY, Address(RAX, 0)), x64);
        x64->op(DECQ, RCX);
        x64->op(JNE, loop);

        x64->code_label(end);
        x64->op(RET);
    }
};


class ArrayType: public RecordType {
public:
    ArrayType(std::string name)
        :RecordType(name, Metatypes { value_metatype }) {
        make_inner_scope(TypeSpec { this, any_type });
    }
    
    virtual Value *lookup_initializer(TypeMatch tm, std::string name, Scope *scope) {
        TypeSpec ts = tm[0];
        
        if (name == "empty")
            return make<ArrayEmptyValue>(ts);
        else if (name == "reserved")
            return make<ArrayReservedValue>(ts);
        else if (name == "all")
            return make<ArrayAllValue>(ts);
        else if (name == "{}")
            return make<ArrayInitializerValue>(ts);

        std::cerr << "No Array initializer called " << name << "!\n";
        return NULL;
    }
    
    virtual void streamify(TypeMatch tm, bool alt, X64 *x64) {
        TypeSpec elem_ts = tm[1];
        Label label = x64->once->compile(compile_contents_streamification, elem_ts);
        x64->op(CALL, label);  // clobbers all
    }
    
    static void compile_contents_streamification(Label label, TypeSpec elem_ts, X64 *x64) {
        int elem_size = elem_ts.measure_elem();
        Label loop, elem, end;

        x64->code_label_local(label, elem_ts.symbolize() + "_array_contents_streamify");
        
        // open
        x64->op(PUSHQ, CHARACTER_LEFTBRACE);
        x64->op(PUSHQ, Address(RSP, ADDRESS_SIZE + ALIAS_SIZE));
        CHARACTER_TS.streamify(true, x64);  // clobbers all
        x64->op(ADDQ, RSP, 2 * ADDRESS_SIZE);
        
        x64->op(MOVQ, RAX, Address(RSP, ADDRESS_SIZE + ALIAS_SIZE));  // Array Ref
        x64->op(MOVQ, RCX, Address(RAX, LINEARRAY_LENGTH_OFFSET));
        x64->op(CMPQ, RCX, 0);
        x64->op(JE, end);

        x64->op(LEA, RAX, Address(RAX, LINEARRAY_ELEMS_OFFSET));
        x64->op(JMP, elem);  // skip separator

        x64->code_label(loop);
        
        // separator
        x64->op(PUSHQ, RAX);
        x64->op(PUSHQ, RCX);
        x64->op(PUSHQ, CHARACTER_COMMA);
        x64->op(PUSHQ, Address(RSP, ADDRESS_SIZE + ALIAS_SIZE + 2 * ADDRESS_SIZE));
        CHARACTER_TS.streamify(true, x64);  // clobbers all
        x64->op(ADDQ, RSP, 2 * ADDRESS_SIZE);
        x64->op(POPQ, RCX);
        x64->op(POPQ, RAX);
        
        x64->code_label(elem);
        x64->op(PUSHQ, RAX);
        x64->op(PUSHQ, RCX);
        x64->op(MOVQ, RBX, Address(RSP, ADDRESS_SIZE + 2 * ADDRESS_SIZE));  // stream alias
        elem_ts.store(Storage(MEMORY, Address(RAX, 0)), Storage(STACK), x64);
        x64->op(PUSHQ, RBX);
        
        elem_ts.streamify(false, x64);  // clobbers all
        
        x64->op(POPQ, RBX);
        elem_ts.store(Storage(STACK), Storage(), x64);
        x64->op(POPQ, RCX);
        x64->op(POPQ, RAX);
        
        x64->op(ADDQ, RAX, elem_size);
        x64->op(DECQ, RCX);
        x64->op(JNE, loop);

        x64->code_label(end);
        
        // close
        x64->op(PUSHQ, CHARACTER_RIGHTBRACE);
        x64->op(PUSHQ, Address(RSP, ADDRESS_SIZE + ALIAS_SIZE));
        CHARACTER_TS.streamify(true, x64);  // clobbers all
        x64->op(ADDQ, RSP, 2 * ADDRESS_SIZE);

        x64->op(RET);
    }
};


// Circularray based

class CircularrayType: public HeapType {
public:
    CircularrayType(std::string name)
        :HeapType(name, Metatypes { value_metatype }) {
        //make_inner_scope(TypeSpec { ref_type, this, any_type });
    }

    virtual Label get_finalizer_label(TypeMatch tm, X64 *x64) {
        return x64->once->compile(compile_finalizer, tm[0]);
    }

    static void compile_finalizer(Label label, TypeSpec ts, X64 *x64) {
        TypeSpec elem_ts = ts.unprefix(circularray_type);
        int elem_size = elem_ts.measure_elem();
        Label start, end, loop, ok, ok1;
    
        x64->code_label_local(label, elem_ts.symbolize() + "_circularray_finalizer");
        x64->op(MOVQ, RAX, Address(RSP, ADDRESS_SIZE));
    
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
        x64->op(RET);
    }
};


class QueueType: public RecordType {
public:
    QueueType(std::string name)
        :RecordType(name, { value_metatype }) {
        make_inner_scope(TypeSpec { this, any_type });
    }

    virtual Value *lookup_initializer(TypeMatch tm, std::string name, Scope *scope) {
        TypeSpec ts = tm[0];

        if (name == "empty")
            return make<QueueEmptyValue>(ts);
        else if (name == "reserved")
            return make<QueueReservedValue>(ts);
        else if (name == "{}")
            return make<QueueInitializerValue>(ts);

        std::cerr << "No Queue initializer called " << name << "!\n";
        return NULL;
    }
};


// Rbtree based

class RbtreeType: public HeapType {
public:
    RbtreeType(std::string name)
        :HeapType(name, Metatypes { value_metatype }) {
        //make_inner_scope(TypeSpec { ref_type, this, any_type });
    }
    
    virtual Label get_finalizer_label(TypeMatch tm, X64 *x64) {
        return x64->once->compile(compile_finalizer, tm[0]);
    }

    static void compile_finalizer(Label label, TypeSpec ts, X64 *x64) {
        TypeSpec elem_ts = ts.unprefix(rbtree_type);
        Label loop, cond;

        x64->code_label(label);
        x64->op(MOVQ, RAX, Address(RSP, ADDRESS_SIZE));

        x64->op(MOVQ, RCX, Address(RAX, RBTREE_LAST_OFFSET));
        x64->op(JMP, cond);
        
        x64->code_label(loop);
        elem_ts.destroy(Storage(MEMORY, Address(RAX, RCX, RBNODE_VALUE_OFFSET)), x64);
        x64->op(MOVQ, RCX, Address(RAX, RCX, RBNODE_PRED_OFFSET));
        x64->op(ANDQ, RCX, ~RBNODE_RED_BIT);
        
        x64->code_label(cond);
        x64->op(CMPQ, RCX, RBNODE_NIL);
        x64->op(JNE, loop);
        
        x64->op(RET);
    }

};


class TreelikeType: public RecordType {
public:
    TypeSpec elem_ts;
    
    TreelikeType(std::string name, Metatypes param_metatypes, TypeSpec ets)
        :RecordType(name, param_metatypes) {
        if (ets == NO_TS)
            throw INTERNAL_ERROR;  // this can be a global initialization issue
            
        elem_ts = ets;
    }

    virtual Value *lookup_initializer(TypeMatch tm, std::string name, Scope *scope) {
        TypeSpec ets = typesubst(elem_ts, tm);
        
        if (name == "empty")
            return make<RbtreeEmptyValue>(ets, tm[0]);
        else if (name == "reserved")
            return make<RbtreeReservedValue>(ets, tm[0]);
        else if (name == "{}")
            return make<RbtreeInitializerValue>(ets, tm[0]);
        else {
            std::cerr << "No " << this->name << " initializer called " << name << "!\n";
            return NULL;
        }
    }

    virtual void streamify(TypeMatch tm, bool alt, X64 *x64) {
        TypeSpec ets = typesubst(elem_ts, tm);
        Label label = x64->once->compile(compile_contents_streamification, ets);
        x64->op(CALL, label);  // clobbers all
    }

    static void compile_contents_streamification(Label label, TypeSpec elem_ts, X64 *x64) {
        // TODO: massive copypaste from Array's
        Label loop, elem, end;

        x64->code_label_local(label, elem_ts.symbolize() + "_rbtree_contents_streamify");
        
        // open
        x64->op(PUSHQ, CHARACTER_LEFTBRACE);
        x64->op(PUSHQ, Address(RSP, ADDRESS_SIZE + ALIAS_SIZE));
        CHARACTER_TS.streamify(true, x64);  // clobbers all
        x64->op(ADDQ, RSP, 2 * ADDRESS_SIZE);
        
        x64->op(MOVQ, RAX, Address(RSP, ADDRESS_SIZE + ALIAS_SIZE));  // Rbtree Ref
        x64->op(MOVQ, RCX, Address(RAX, RBTREE_FIRST_OFFSET));
        x64->op(CMPQ, RCX, RBNODE_NIL);
        x64->op(JE, end);

        x64->op(JMP, elem);  // skip separator

        x64->code_label(loop);
        
        // separator
        x64->op(PUSHQ, RAX);
        x64->op(PUSHQ, RCX);
        x64->op(PUSHQ, CHARACTER_COMMA);
        x64->op(PUSHQ, Address(RSP, ADDRESS_SIZE + ALIAS_SIZE + 2 * ADDRESS_SIZE));
        CHARACTER_TS.streamify(true, x64);  // clobbers all
        x64->op(ADDQ, RSP, 2 * ADDRESS_SIZE);
        x64->op(POPQ, RCX);
        x64->op(POPQ, RAX);
        
        x64->code_label(elem);
        x64->op(PUSHQ, RAX);
        x64->op(PUSHQ, RCX);
        x64->op(MOVQ, RBX, Address(RSP, ADDRESS_SIZE + 2 * ADDRESS_SIZE));  // stream alias
        elem_ts.store(Storage(MEMORY, Address(RAX, RCX, RBNODE_VALUE_OFFSET)), Storage(STACK), x64);
        x64->op(PUSHQ, RBX);
        
        elem_ts.streamify(false, x64);  // clobbers all
        
        x64->op(POPQ, RBX);
        elem_ts.store(Storage(STACK), Storage(), x64);
        x64->op(POPQ, RCX);
        x64->op(POPQ, RAX);
        
        x64->op(MOVQ, RCX, Address(RAX, RCX, RBNODE_NEXT_OFFSET));
        x64->op(CMPQ, RCX, RBNODE_NIL);
        x64->op(JNE, loop);

        x64->code_label(end);
        
        // close
        x64->op(PUSHQ, CHARACTER_RIGHTBRACE);
        x64->op(PUSHQ, Address(RSP, ADDRESS_SIZE + ALIAS_SIZE));
        CHARACTER_TS.streamify(true, x64);  // clobbers all
        x64->op(ADDQ, RSP, 2 * ADDRESS_SIZE);

        x64->op(RET);
    }
};


class SetType: public TreelikeType {
public:
    SetType(std::string name)
        :TreelikeType(name, { value_metatype }, SAME_TS) {
    }
};


class MapType: public TreelikeType {
public:
    MapType(std::string name)
        :TreelikeType(name, { value_metatype, value_metatype }, SAME_SAME2_ITEM_TS) {
    }
};


// Nosy container based

// Base class for Weak* containers, containing a Ref to a Nosycontainer, which contains a
// container wrapper (likely a Set or Map).
class WeakContainerType: public RecordType {
public:
    TypeSpec elem_ts;
    
    WeakContainerType(std::string n, Metatypes param_metatypes, TypeSpec ets)
        :RecordType(n, param_metatypes) {
        elem_ts = ets;
    }

    virtual Value *lookup_initializer(TypeMatch tm, std::string n, Scope *s) {
        TypeSpec ets = typesubst(elem_ts, tm);
        TypeSpec mts = ets.prefix(rbtree_type).prefix(ref_type);
        Value *member = NULL;
        
        if (n == "empty")
            member = make<RbtreeEmptyValue>(ets, mts);
        else if (n == "reserved")
            member = make<RbtreeReservedValue>(ets, mts);
        else if (n == "{}")
            member = make<RbtreeInitializerValue>(ets, mts);
        else {
            std::cerr << "No " << name << " initializer called " << n << "!\n";
            return NULL;
        }
        
        return make<WeakContainerValue>(member, mts, tm[0]);
    }
};


class WeakSetType: public WeakContainerType {
public:
    WeakSetType(std::string name)
        :WeakContainerType(name, { identity_metatype }, SAMEID_NOSYVALUE_TS) {
    }
};


class WeakIndexMapType: public WeakContainerType {
public:
    WeakIndexMapType(std::string name)
        :WeakContainerType(name, { identity_metatype, value_metatype }, SAMEID_NOSYVALUE_SAME2_ITEM_TS) {
    }
};


class WeakValueMapType: public WeakContainerType {
public:
    WeakValueMapType(std::string name)
        :WeakContainerType(name, { value_metatype, identity_metatype }, SAME_SAMEID2_NOSYVALUE_ITEM_TS) {
    }
};
