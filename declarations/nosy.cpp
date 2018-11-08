
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
            
        Label ok;
            
        x64->op(MOVQ, R10, s.address + NOSYVALUE_FCB_OFFSET);  // s may be RSP based
        x64->op(CMPQ, R10, FCB_NIL);
        x64->op(JE, ok);  // may happen with Weakref
        
        x64->runtime->pusha();
        x64->op(PUSHQ, R10);
        x64->op(CALL, x64->runtime->fcb_free_label);  // clobbers all
        x64->op(ADDQ, RSP, ADDRESS_SIZE);
        x64->runtime->popa();
        
        x64->code_label(ok);
    }
};


class NosyContainerType: public HeapType {
public:
    NosyContainerType(std::string name)
        :HeapType(name, Metatypes { value_metatype }) {
    }

    virtual Label get_finalizer_label(TypeMatch tm, X64 *x64) {
        return x64->once->compile(compile_finalizer, tm[1]);
    }

    static void compile_finalizer(Label label, TypeSpec member_ts, X64 *x64) {
        Label skip;

        x64->code_label_local(label, "x_nosycontainer_finalizer");
        x64->runtime->log("Nosy container finalized.");

        x64->op(MOVQ, RAX, Address(RSP, ADDRESS_SIZE));  // pointer arg
        
        member_ts.destroy(Storage(MEMORY, Address(RAX, NOSYCONTAINER_MEMBER_OFFSET)), x64);
        
        x64->op(RET);
    }
};


class WeakrefType: public RecordType {
public:
    WeakrefType(std::string n)
        :RecordType(n, Metatypes { identity_metatype }) {
    }

    virtual Value *lookup_initializer(TypeMatch tm, std::string n, Scope *s) {
        if (n == "to")
            return make<WeakrefToValue>(tm[1]);

        std::cerr << "No Weakref initializer " << n << "!\n";
        return NULL;
    }
    
    virtual Value *lookup_matcher(TypeMatch tm, std::string n, Value *p, Scope *s) {
        if (n == "dead")
            return make<WeakrefDeadMatcherValue>(p, tm);
        else if (n == "live")
            return make<WeakrefLiveMatcherValue>(p, tm);

        std::cerr << "No Weakref matcher " << n << "!\n";
        return NULL;
    }
};