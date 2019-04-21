
// This is a hack type to cooperate closely with Weakref and Weak* containers.
// It contains a raw pointer, so it can disguise as a Ptr within Weak*Map, and
// comparisons would work with the input Ptr-s.
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
            // Used when a Weak* container adds an elem. This is always an initialization
            // from a Ptr, so we keep the refcount, which will be decremented by
            // the container after setting up the FCB just in case.
            x64->op(POPQ, t.address + NOSYVALUE_RAW_OFFSET);
        }
        else if (s.where == MEMORY && t.where == MEMORY) {
            // Used when cloning an rbtree. We don't touch reference counters.
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


class NosytreeType: public ContainerType {
public:
    NosytreeType(std::string name)
        :ContainerType(name, Metatypes { value_metatype }) {
    }
};


class NosyrefType: public ContainerType {
public:
    NosyrefType(std::string name)
        :ContainerType(name, Metatypes { value_metatype }) {
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

        return RecordType::lookup_matcher(tm, n, p, s);
    }
};
