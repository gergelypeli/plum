// Allocation

struct Allocation {
    int bytes;
    int count1;
    int count2;
    int count3;
    
    Allocation(int b = 0, int c1 = 0, int c2 = 0, int c3 = 0);
    int concretize(TypeMatch tm);
    int concretize();
};

Allocation stack_size(Allocation a);

std::ostream &operator<<(std::ostream &os, const Allocation &a);

Allocation::Allocation(int b, int c1, int c2, int c3) {
    bytes = b;
    count1 = c1;
    count2 = c2;
    count3 = c3;
}


int Allocation::concretize() {
    if (count1 || count2 || count3)
        throw INTERNAL_ERROR;
    else
        return bytes;
}


int Allocation::concretize(TypeMatch tm) {
    int concrete_size = bytes;
    
    if (count1)
        concrete_size += count1 * tm[1].measure_stack();
        
    if (count2)
        concrete_size += count2 * tm[2].measure_stack();
        
    if (count3)
        concrete_size += count3 * tm[3].measure_stack();
    
    //if (count1 || count2 || count3)
    //    std::cerr << "Hohoho, concretized " << *this << " with " << tm << " to " << concrete_size << " bytes.\n";
    
    return concrete_size;
}


Allocation operator+(Allocation a, Allocation b) {
    return Allocation(a.bytes + b.bytes, a.count1 + b.count1, a.count2 + b.count2, a.count3 + b.count3);
}


Allocation operator*(Allocation a, int c) {
    return Allocation(a.bytes * c, a.count1 * c, a.count2 * c, a.count3 * c);
}


std::ostream &operator<<(std::ostream &os, const Allocation &a) {
    if (a.count1 || a.count2 || a.count3)
        os << "A(" << a.bytes << "," << a.count1 << "," << a.count2 << "," << a.count3 << ")";
    else
        os << "A(" << a.bytes << ")";
        
    return os;
}



struct PartialInfo {
    std::set<std::string> uninitialized_member_names;
    std::set<std::string> initialized_member_names;
    
    PartialInfo() {
    }

    virtual void set_member_names(std::vector<std::string> mn) {
        uninitialized_member_names.insert(mn.begin(), mn.end());
    }
    
    virtual void be_initialized(std::string name) {
        initialized_member_names.insert(name);
        uninitialized_member_names.erase(name);
    }
    
    virtual bool is_initialized(std::string name) {
        return initialized_member_names.count(name) == 1;
    }

    virtual bool is_uninitialized(std::string name) {
        return uninitialized_member_names.count(name) == 1;
    }
    
    virtual bool is_complete() {
        return uninitialized_member_names.size() == 0;
    }

    virtual void be_complete() {
        initialized_member_names.insert(uninitialized_member_names.begin(), uninitialized_member_names.end());
        uninitialized_member_names.clear();
    }

    virtual bool is_dirty() {
        return initialized_member_names.size() != 0;
    }
};


struct SelfInfo {
    std::map<std::string, Identifier *> specials;
    
    SelfInfo() {
    }
    
    virtual void add_special(std::string n, Identifier *i) {
        specials[n] = i;
    }
    
    virtual Identifier *get_special(std::string n) {
        return specials.count(n) ? specials[n] : NULL;
    }
};


struct TreenumInput {
    const char *kw;
    unsigned p;
};


struct ArgInfo {
    const char *name;
    TypeSpec *context;
    Scope *scope;
    std::unique_ptr<Value> *target;  // Yes, a pointer to an unique_ptr
};


struct ExprInfo {
    std::string name;
    Expr **target;
};


enum InheritAs {
    AS_ROLE, AS_BASE, AS_MAIN
};


struct AutoconvEntry {
    TypeSpec role_ts;
    int role_offset;
};
