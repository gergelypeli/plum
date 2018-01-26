
class Unwindable {
public:
    virtual Scope *unwind(X64 *x64) = 0;
};


class Unwind {
public:
    std::vector<Unwindable *> stack;
    
    virtual void push(Unwindable *v) {
        stack.push_back(v);
    }
    
    virtual void pop(Unwindable *v) {
        if (v != stack.back())
            throw INTERNAL_ERROR;
            
        stack.pop_back();
    }
    
    virtual void initiate(Declaration *last, X64 *x64) {
        for (int i = stack.size() - 1; i >= 0; i--) {
            Scope *s = stack[i]->unwind(x64);
            
            if (s) {
                if (s != last->outer_scope)
                    throw INTERNAL_ERROR;
                    
                last->jump_to_finalization(x64);
                return;
            }
        }
        
        throw INTERNAL_ERROR;
    }
};
