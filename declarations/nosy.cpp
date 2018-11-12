
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
        if (s.where == STACK && t.where == MEMORY) {
            x64->op(POPQ, t.address + NOSYVALUE_RAW_OFFSET);
        }
        else if (s.where == MEMORY && t.where == MEMORY) {
            x64->op(MOVQ, R10, s.address + NOSYVALUE_RAW_OFFSET);
            x64->op(MOVQ, t.address + NOSYVALUE_RAW_OFFSET, R10);
        }
        else
            throw INTERNAL_ERROR;
    }

    virtual void destroy(TypeMatch tm, Storage s, X64 *x64) {
        // Nothing to do
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

        x64->code_label_local(label, member_ts.symbolize() + "_nosycontainer_finalizer");
        x64->runtime->log("Nosy container finalized.");

        x64->op(MOVQ, RAX, Address(RSP, ADDRESS_SIZE));  // pointer arg
        
        // TODO: this case handling is not nice
        if (member_ts[0] == ref_type && member_ts[1] == rbtree_type) {
            TypeSpec elem_ts = member_ts.unprefix(ref_type).unprefix(rbtree_type);
            Label callback_label = x64->once->compile(compile_rbtree_nosy_callback, elem_ts);
            
            x64->op(LEA, R10, Address(callback_label, 0));
            x64->op(LEA, R11, Address(RAX, NOSYCONTAINER_MEMBER_OFFSET));
            rbtree_fcb_action(x64->runtime->fcb_free_label, elem_ts, x64);  // clobbers all
            
            // If an iterator is referring to this rbtree, it must have increased all
            // reference counts to make sure they continue to point to a valid object.
            // Once we destroy the Rbtree Ref, only iterator(s) will be the owner(s)
            // of this rbtree.
        }
        else if (member_ts[0] == nosyvalue_type) {
            Label callback_label = x64->once->compile(compile_weakref_nosy_callback);  // FIXME
            Label ok;

            // This object may have died already
            x64->op(MOVQ, R10, Address(RAX, NOSYCONTAINER_MEMBER_OFFSET + NOSYVALUE_RAW_OFFSET));  // object
            x64->op(CMPQ, R10, 0);
            x64->op(JE, ok);
            
            x64->op(PUSHQ, R10);  // object
            x64->op(LEA, R10, Address(callback_label, 0));
            x64->op(PUSHQ, R10);  // callback
            x64->op(PUSHQ, RAX);  // payload1
            x64->op(PUSHQ, 0);    // payload2
            
            x64->op(CALL, x64->runtime->fcb_free_label);  // clobbers all

            x64->op(ADDQ, RSP, 4 * ADDRESS_SIZE);
            
            x64->code_label(ok);
        }
        else
            throw INTERNAL_ERROR;

        x64->op(MOVQ, RAX, Address(RSP, ADDRESS_SIZE));
        
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
