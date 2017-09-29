
class ReferenceType: public Type {
public:
    ReferenceType(std::string name)
        :Type(name, 1) {
    }
    
    virtual unsigned measure(TypeSpecIter tsi, StorageWhere where) {
        switch (where) {
        case STACK:
            return 8;
        case MEMORY:
            return 8;
        default:
            return Type::measure(tsi, where);
        }
    }

    virtual void store(TypeSpecIter tsi, Storage s, Storage t, X64 *x64) {
        switch (s.where * t.where) {
        case REGISTER_NOWHERE:
            x64->decref(s.reg);
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
            x64->decref(s.reg);
            return;

        case STACK_NOWHERE:
            x64->op(POPQ, RBX);
            x64->decref(RBX);
            return;
        case STACK_REGISTER:
            x64->op(POPQ, t.reg);
            return;
        case STACK_STACK:
            return;
        case STACK_MEMORY:
            x64->op(POPQ, RBX);
            x64->op(XCHGQ, RBX, t.address);
            x64->decref(RBX);
            return;

        case MEMORY_NOWHERE:
            return;
        case MEMORY_REGISTER:
            x64->op(MOVQ, t.reg, s.address);
            x64->incref(t.reg);
            return;
        case MEMORY_STACK:
            x64->op(MOVQ, RBX, s.address);
            x64->incref(RBX);
            x64->op(PUSHQ, RBX);
            return;
        case MEMORY_MEMORY:
            x64->op(MOVQ, RBX, s.address);
            x64->incref(RBX);
            x64->op(XCHGQ, RBX, t.address);
            x64->decref(RBX);
            return;
        default:
            Type::store(tsi, s, t, x64);
        }
    }

    virtual void create(TypeSpecIter tsi, Storage s, Storage t, X64 *x64) {
        // Assume the target MEMORY is uninitialized
        
        switch (s.where * t.where) {
        case NOWHERE_MEMORY:
            x64->op(MOVQ, t.address, 0);
            return;
        case REGISTER_MEMORY:
            x64->op(MOVQ, t.address, s.reg);
            return;
        case STACK_MEMORY:
            x64->op(POPQ, t.address);
            return;
        case MEMORY_MEMORY:
            x64->op(MOVQ, RBX, s.address);
            x64->incref(RBX);
            x64->op(MOVQ, t.address, RBX);
            return;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual void destroy(TypeSpecIter , Storage s, X64 *x64) {
        if (s.where == MEMORY) {
            x64->op(MOVQ, RBX, s.address);
            x64->decref(RBX);
        }
        else
            throw INTERNAL_ERROR;
    }

    virtual StorageWhere where(TypeSpecIter, bool is_arg, bool is_lvalue) {
        return (is_arg ? (is_lvalue ? ALIAS : MEMORY) : (is_lvalue ? MEMORY : REGISTER));
    }

    virtual Storage boolval(TypeSpecIter , Storage s, X64 *x64, bool probe) {
        switch (s.where) {
        case CONSTANT:
            return Storage(CONSTANT, s.value != 0);
        case REGISTER:
            if (!probe)
                x64->decref(s.reg);
                
            x64->op(CMPQ, s.reg, 0);
            return Storage(FLAGS, SETNE);
        case MEMORY:
            x64->op(CMPQ, s.address, 0);
            return Storage(FLAGS, SETNE);
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Value *lookup_initializer(TypeSpecIter tsi, std::string name, Scope *scope) {
        if (name == "null") {
            return make_null_reference_value(TypeSpec(tsi));
        }
        else {
            std::cerr << "No reference initializer called " << name << "!\n";
            return NULL;
        }
    }

    virtual Scope *get_inner_scope(TypeSpecIter tsi) {
        tsi++;
        return (*tsi)->get_inner_scope(tsi);
    }
};


class BorrowedReferenceType: public Type {
public:
    BorrowedReferenceType(std::string name)
        :Type(name, 1) {
    }
    
    virtual unsigned measure(TypeSpecIter tsi, StorageWhere where) {
        switch (where) {
        case STACK:
            return 16;
        case MEMORY:
            return 16;
        default:
            return Type::measure(tsi, where);
        }
    }

    virtual void store(TypeSpecIter tsi, Storage s, Storage t, X64 *x64) {
        switch (s.where * t.where) {
        case STACK_NOWHERE:
            x64->op(ADDQ, RSP, 16);
            return;
        case STACK_STACK:
            return;
        case STACK_MEMORY:
            x64->op(POPQ, t.address);
            x64->op(POPQ, t.address + 8);
            return;

        case MEMORY_NOWHERE:
            return;
        case MEMORY_STACK:
            x64->op(PUSHQ, s.address + 8);
            x64->op(PUSHQ, s.address);
            return;
        case MEMORY_MEMORY:
            x64->op(MOVQ, RBX, s.address);
            x64->op(MOVQ, t.address, RBX);
            x64->op(MOVQ, RBX, s.address + 8);
            x64->op(MOVQ, t.address + 8, RBX);
            return;
        default:
            Type::store(tsi, s, t, x64);
        }
    }

    virtual void create(TypeSpecIter tsi, Storage s, Storage t, X64 *x64) {
        // Assume the target MEMORY is uninitialized
        
        switch (s.where * t.where) {
        case NOWHERE_MEMORY:
            x64->op(MOVQ, t.address, 0);
            x64->op(MOVQ, t.address + 8, 0);
            return;
        case STACK_MEMORY:
            x64->op(POPQ, t.address);
            x64->op(POPQ, t.address + 8);
            return;
        case MEMORY_MEMORY:
            x64->op(MOVQ, RBX, s.address);
            x64->op(MOVQ, t.address, RBX);
            x64->op(MOVQ, RBX, s.address + 8);
            x64->op(MOVQ, t.address + 8, RBX);
            return;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual void destroy(TypeSpecIter , Storage s, X64 *x64) {
        if (s.where == MEMORY) {
        }
        else
            throw INTERNAL_ERROR;
    }

    virtual StorageWhere where(TypeSpecIter, bool is_arg, bool is_lvalue) {
        return (is_arg ? (is_lvalue ? ALIAS : MEMORY) : (is_lvalue ? MEMORY : STACK));
    }

    virtual Storage boolval(TypeSpecIter , Storage s, X64 *x64, bool probe) {
        switch (s.where) {
        case MEMORY:
            x64->op(CMPQ, s.address, 0);
            return Storage(FLAGS, SETNE);
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual Value *lookup_initializer(TypeSpecIter tsi, std::string name, Scope *scope) {
        std::cerr << "No borrowed reference initializer called " << name << "!\n";
        return NULL;
    }
    
    virtual Scope *get_inner_scope(TypeSpecIter tsi) {
        tsi++;
        return (*tsi)->get_inner_scope(tsi);
    }
};


class HeapType: public Type {
public:
    std::unique_ptr<DataScope> inner_scope;

    HeapType(std::string name, unsigned pc)
        :Type(name, pc) {
        inner_scope.reset(new DataScope);
        inner_scope->set_pivot_type_hint(TypeSpec { reference_type, any_type });
    }
    
    virtual Scope *get_inner_scope(TypeSpecIter tsi) {
        return inner_scope.get();
    }
};


HeapType *heap_type_cast(Type *t) {
    return dynamic_cast<HeapType *>(t);
}

