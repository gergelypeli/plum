
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

    virtual void compare(TypeSpecIter tsi, Storage s, Storage t, X64 *x64, Label less, Label greater) {
        switch (s.where * t.where) {
        case REGISTER_REGISTER:
            x64->decref(s.reg);
            x64->decref(t.reg);
            x64->op(CMPQ, s.reg, t.reg);
            break;
        case REGISTER_STACK:
            x64->op(POPQ, RBX);
            x64->decref(s.reg);
            x64->decref(RBX);
            x64->op(CMPQ, s.reg, RBX);
            break;
        case REGISTER_MEMORY:
            x64->decref(s.reg);
            x64->op(CMPQ, s.reg, t.address);
            break;

        case STACK_REGISTER:
            x64->op(POPQ, RBX);
            x64->decref(RBX);
            x64->decref(t.reg);
            x64->op(CMPQ, RBX, t.reg);
            break;
        case STACK_STACK:
            x64->op(POPQ, RBX);
            x64->decref(RBX);
            x64->op(XCHGQ, RBX, Address(RSP, 0));
            x64->decref(RBX);
            x64->op(CMPQ, RBX, Address(RSP, 0));
            x64->op(POPQ, RBX);
            break;
        case STACK_MEMORY:
            x64->op(POPQ, RBX);
            x64->decref(RBX);
            x64->op(CMPQ, RBX, t.address);
            break;

        case MEMORY_REGISTER:
            x64->decref(t.reg);
            x64->op(CMPQ, s.address, t.reg);
            break;
        case MEMORY_STACK:
            x64->op(POPQ, RBX);
            x64->decref(RBX);
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
            tsi++;
            return tsi[0]->lookup_initializer(tsi, name, scope);
            //std::cerr << "No reference initializer called " << name << "!\n";
            //return NULL;
        }
    }

    virtual Scope *get_inner_scope(TypeSpecIter tsi) {
        tsi++;
        return (*tsi)->get_inner_scope(tsi);
    }

    virtual std::vector<Function *> get_virtual_table(TypeSpecIter tsi) {
        tsi++;
        return (*tsi)->get_virtual_table(tsi);
    }

    virtual Label get_virtual_table_label(TypeSpecIter tsi) {
        tsi++;
        return (*tsi)->get_virtual_table_label(tsi);
    }
};

/*
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
*/

class HeapType: public Type {
public:
    HeapType(std::string name, unsigned pc)
        :Type(name, pc) {
    }

    virtual unsigned measure(TypeSpecIter tsi, StorageWhere where) {
        std::cerr << "This is probably an error, shouldn't measure a heap type!\n";
        throw INTERNAL_ERROR;
    }
};


bool is_heap_type(Type *t) {
    return dynamic_cast<HeapType *>(t);
}


class ArrayType: public HeapType {
public:
    std::unique_ptr<DataScope> inner_scope;

    ArrayType(std::string name)
        :HeapType(name, 1) {
        inner_scope.reset(new DataScope);
        inner_scope->set_pivot_type_hint(TypeSpec { reference_type, this, any_type });
    }
    
    virtual Scope *get_inner_scope(TypeSpecIter tsi) {
        return inner_scope.get();
    }

    virtual Value *lookup_initializer(TypeSpecIter tsi, std::string name, Scope *scope) {
        TypeSpec rts = TypeSpec(tsi).prefix(reference_type);
        
        if (name == "empty")
            return make_array_empty_value(rts);
        else if (name == "null")
            return make_null_reference_value(rts);
        else if (name == "{}")
            return make_array_initializer_value(rts);

        std::cerr << "No array initializer called " << name << "!\n";
        return NULL;
    }
};
