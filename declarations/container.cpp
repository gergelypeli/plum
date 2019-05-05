
class ContainerType: public Type {
public:
    ContainerType(std::string name, Metatypes param_metatypes)
        :Type(name, param_metatypes, identity_metatype) {
    }

    virtual Allocation measure(TypeMatch tm) {
        std::cerr << "This is probably an error, shouldn't measure a container type!\n";
        throw INTERNAL_ERROR;
    }

    virtual void incref(TypeMatch tm, Register r, X64 *x64) {
        x64->runtime->incref(r);
    }

    virtual void decref(TypeMatch tm, Register r, X64 *x64) {
        x64->runtime->decref(r);
    }

    virtual void streamify(TypeMatch tm, X64 *x64) {
        // We do this for container types that don't implement Streamifiable
        Address value_addr(RSP, ALIAS_SIZE);
        Address alias_addr(RSP, 0);

        x64->op(MOVQ, RDI, value_addr);
        x64->op(MOVQ, RSI, alias_addr);
    
        x64->runtime->call_sysv(x64->runtime->sysv_streamify_pointer_label);
    }

    static unsigned elem_size(TypeSpec elem_ts) {
        return ::elem_size(elem_ts.measure_raw());
    }

    virtual unsigned get_elem_size(TypeSpec elem_ts) {
        return elem_size(elem_ts);
    }

    virtual int get_elems_offset() {
        return 0;
    }

    virtual void type_info(TypeMatch tm, X64 *x64) {
        unsigned elem_ts_index = x64->once->type_info(tm[1]);
        unsigned elem_size = get_elem_size(tm[1]);
        int elems_offset = get_elems_offset();
        Label label;
        unsigned array_ts_index = label.def_index;
        
        x64->dwarf->begin_structure_type_info(tm[0].symbolize(), elems_offset);
        x64->dwarf->member_info("elems", elems_offset, array_ts_index);
        x64->dwarf->end_info();
        
        x64->dwarf->info_def(array_ts_index);
        x64->dwarf->array_type_info(tm[1].symbolize("<Elems>"), elem_ts_index, elem_size);
    }
};


// Linearray based

class LinearrayType: public ContainerType {
public:
    LinearrayType(std::string name)
        :ContainerType(name, Metatypes { value_metatype }) {
    }
    
    virtual Label get_finalizer_label(TypeMatch tm, X64 *x64) {
        return x64->once->compile(compile_finalizer, tm[0]);
    }

    static void compile_finalizer(Label label, TypeSpec ts, X64 *x64) {
        TypeSpec elem_ts = ts.unprefix(linearray_type);
        int elem_size = ContainerType::elem_size(elem_ts);
        Label start, end, loop;

        x64->code_label_local(label, elem_ts.prefix(linearray_type).symbolize("finalizer"));
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
    
    virtual int get_elems_offset() {
        return LINEARRAY_ELEMS_OFFSET;
    }
};


class ArrayType: public RecordType {
public:
    ArrayType(std::string name)
        :RecordType(name, Metatypes { value_metatype }) {
    }
    
    virtual Value *lookup_initializer(TypeMatch tm, std::string name, Scope *scope) {
        TypeSpec ts = tm[0];
        
        if (name == "empty")
            return make<ArrayEmptyValue>(ts);
        else if (name == "reserved")
            return make<ArrayReservedValue>(ts);
        else if (name == "all")
            return make<ArrayAllValue>(ts);
        else if (name == "{")
            return make<ArrayInitializerValue>(ts);

        std::cerr << "No Array initializer called " << name << "!\n";
        return NULL;
    }
    
    virtual void streamify(TypeMatch tm, X64 *x64) {
        TypeSpec elem_ts = tm[1];
        Label label = x64->once->compile(compile_streamification, elem_ts);

        x64->op(CALL, label);  // clobbers all
    }
    
    static void compile_streamification(Label label, TypeSpec elem_ts, X64 *x64) {
        int elem_size = ContainerType::elem_size(elem_ts);
        Label loop, elem, end;
        Address value_addr(RSP, RIP_SIZE + ALIAS_SIZE);
        Address alias_addr(RSP, RIP_SIZE);

        x64->code_label_local(label, elem_ts.prefix(array_type).symbolize("streamify"));
        
        // open
        streamify_ascii("{", alias_addr, x64);  // clobbers all
        
        x64->op(MOVQ, RAX, value_addr);  // Array Ref
        x64->op(MOVQ, RCX, Address(RAX, LINEARRAY_LENGTH_OFFSET));
        x64->op(CMPQ, RCX, 0);
        x64->op(JE, end);

        x64->op(LEA, RAX, Address(RAX, LINEARRAY_ELEMS_OFFSET));
        x64->op(JMP, elem);  // skip separator

        x64->code_label(loop);
        
        // separator
        x64->op(PUSHQ, RAX);
        x64->op(PUSHQ, RCX);
        
        streamify_ascii(",", alias_addr + 2 * ADDRESS_SIZE, x64);

        x64->op(POPQ, RCX);
        x64->op(POPQ, RAX);
        
        x64->code_label(elem);
        x64->op(PUSHQ, RAX);
        x64->op(PUSHQ, RCX);
        x64->op(MOVQ, RBX, alias_addr + 2 * ADDRESS_SIZE);  // stream alias
        elem_ts.store(Storage(MEMORY, Address(RAX, 0)), Storage(STACK), x64);
        x64->op(PUSHQ, RBX);
        
        // Invoking a custom streamification may relocate the stack, so the
        // passed stream alias may be fixed, must propagate it upwards.
        elem_ts.streamify(x64);  // clobbers all
        
        x64->op(POPQ, RBX);
        elem_ts.store(Storage(STACK), Storage(), x64);
        x64->op(MOVQ, alias_addr + 2 * ADDRESS_SIZE, RBX);  // stream alias
        x64->op(POPQ, RCX);
        x64->op(POPQ, RAX);
        
        x64->op(ADDQ, RAX, elem_size);
        x64->op(DECQ, RCX);
        x64->op(JNE, loop);

        x64->code_label(end);
        
        // close
        streamify_ascii("}", alias_addr, x64);  // clobbers all

        x64->op(RET);
    }
};


// Circularray based

class CircularrayType: public ContainerType {
public:
    CircularrayType(std::string name)
        :ContainerType(name, Metatypes { value_metatype }) {
    }

    virtual Label get_finalizer_label(TypeMatch tm, X64 *x64) {
        return x64->once->compile(compile_finalizer, tm[0]);
    }

    static void compile_finalizer(Label label, TypeSpec ts, X64 *x64) {
        TypeSpec elem_ts = ts.unprefix(circularray_type);
        int elem_size = ContainerType::elem_size(elem_ts);
        Label start, end, loop, ok, ok1;
    
        x64->code_label_local(label, elem_ts.prefix(circularray_type).symbolize("finalizer"));
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

    virtual int get_elems_offset() {
        return CIRCULARRAY_ELEMS_OFFSET;
    }
};


class QueueType: public RecordType {
public:
    QueueType(std::string name)
        :RecordType(name, { value_metatype }) {
    }

    virtual Value *lookup_initializer(TypeMatch tm, std::string name, Scope *scope) {
        TypeSpec ts = tm[0];

        if (name == "empty")
            return make<QueueEmptyValue>(ts);
        else if (name == "reserved")
            return make<QueueReservedValue>(ts);
        else if (name == "{")
            return make<QueueInitializerValue>(ts);

        std::cerr << "No Queue initializer called " << name << "!\n";
        return NULL;
    }
};


// Rbtree based

class RbtreeType: public ContainerType {
public:
    RbtreeType(std::string name)
        :ContainerType(name, Metatypes { value_metatype }) {
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

    static unsigned elem_size(TypeSpec elem_ts) {
        return elem_ts.measure_stack() + RBNODE_HEADER_SIZE;
    }

    virtual unsigned get_elem_size(TypeSpec elem_ts) {
        return elem_size(elem_ts);
    }

    virtual int get_elems_offset() {
        return RBTREE_ELEMS_OFFSET;
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
        else if (name == "{")
            return make<RbtreeInitializerValue>(ets, tm[0]);
        else {
            std::cerr << "No " << this->name << " initializer called " << name << "!\n";
            return NULL;
        }
    }

    virtual void streamify(TypeMatch tm, X64 *x64) {
        TypeSpec ets = typesubst(elem_ts, tm);
        Label label = x64->once->compile(compile_streamification, ets);

        x64->op(CALL, label);  // clobbers all
    }

    static void compile_streamification(Label label, TypeSpec elem_ts, X64 *x64) {
        // TODO: massive copypaste from Array's
        Label loop, elem, end;
        Address value_addr(RSP, RIP_SIZE + ALIAS_SIZE);
        Address alias_addr(RSP, RIP_SIZE);

        x64->code_label_local(label, elem_ts.prefix(rbtree_type).symbolize("streamify"));
        
        // open
        streamify_ascii("{", alias_addr, x64);
        
        x64->op(MOVQ, RAX, value_addr);  // Rbtree Ref
        x64->op(MOVQ, RCX, Address(RAX, RBTREE_FIRST_OFFSET));
        x64->op(CMPQ, RCX, RBNODE_NIL);
        x64->op(JE, end);

        x64->op(JMP, elem);  // skip separator

        x64->code_label(loop);
        
        // separator
        x64->op(PUSHQ, RAX);
        x64->op(PUSHQ, RCX);

        streamify_ascii(",", alias_addr + 2 * ADDRESS_SIZE, x64);
        
        x64->op(POPQ, RCX);
        x64->op(POPQ, RAX);
        
        x64->code_label(elem);
        x64->op(PUSHQ, RAX);
        x64->op(PUSHQ, RCX);
        x64->op(MOVQ, RBX, alias_addr + 2 * ADDRESS_SIZE);  // stream alias
        elem_ts.store(Storage(MEMORY, Address(RAX, RCX, RBNODE_VALUE_OFFSET)), Storage(STACK), x64);
        x64->op(PUSHQ, RBX);
        
        // Invoking a custom streamification may relocate the stack, so the
        // passed stream alias may be fixed, must propagate it upwards.
        elem_ts.streamify(x64);  // clobbers all
        
        x64->op(POPQ, RBX);
        elem_ts.store(Storage(STACK), Storage(), x64);
        x64->op(MOVQ, alias_addr + 2 * ADDRESS_SIZE, RBX);  // stream alias
        x64->op(POPQ, RCX);
        x64->op(POPQ, RAX);
        
        x64->op(MOVQ, RCX, Address(RAX, RCX, RBNODE_NEXT_OFFSET));
        x64->op(CMPQ, RCX, RBNODE_NIL);
        x64->op(JNE, loop);

        x64->code_label(end);
        
        // close
        streamify_ascii("}", alias_addr, x64);

        x64->op(RET);
    }
};


class SetType: public TreelikeType {
public:
    SetType(std::string name)  // Must not use TS constants in constructor
        :TreelikeType(name, { value_metatype }, { same_type }) {
    }
};


class MapType: public TreelikeType {
public:
    MapType(std::string name)  // Must not use TS constants in constructor
        :TreelikeType(name, { value_metatype, value_metatype }, { item_type, same_type, same2_type }) {
    }
};


// Nosy container based

// Base class for Weak* containers, containing a Ref to a Nosycontainer, which contains a
// container wrapper (likely a Set or Map).
class WeaktreeType: public RecordType {
public:
    TypeSpec elem_ts;
    
    WeaktreeType(std::string n, Metatypes param_metatypes, TypeSpec ets)
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
        else if (n == "{")
            member = make<RbtreeInitializerValue>(ets, mts);
        else {
            std::cerr << "No " << name << " initializer called " << n << "!\n";
            return NULL;
        }
        
        return make<WeaktreeValue>(member, ets, tm[0]);
    }
};


class WeakSetType: public WeaktreeType {
public:
    WeakSetType(std::string name)  // Must not use TS constants in constructor
        :WeaktreeType(name, { identity_metatype }, { nosyvalue_type, sameid_type }) {
    }
};


class WeakIndexMapType: public WeaktreeType {
public:
    WeakIndexMapType(std::string name)  // Must not use TS constants in constructor
        :WeaktreeType(name, { identity_metatype, value_metatype }, { item_type, nosyvalue_type, sameid_type, same2_type }) {
    }
};


class WeakValueMapType: public WeaktreeType {
public:
    WeakValueMapType(std::string name)  // Must not use TS constants in constructor
        :WeaktreeType(name, { value_metatype, identity_metatype }, { item_type, same_type, nosyvalue_type, sameid2_type }) {
    }
};
