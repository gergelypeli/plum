
class ReferenceType: public Type {
public:
    ReferenceType(std::string name)
        :Type(name, 1) {
    }
    
    virtual unsigned measure(TypeSpecIter tsi, StorageWhere where) {
        switch (where) {
        case STACK:
            return REFERENCE_SIZE;
        case MEMORY:
            return REFERENCE_SIZE;
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

    virtual DataScope *get_inner_scope(TypeSpecIter tsi) {
        tsi++;
        return (*tsi)->get_inner_scope(tsi);
    }

    virtual std::vector<Function *> get_virtual_table(TypeSpecIter tsi) {
        tsi++;
        return (*tsi)->get_virtual_table(tsi);
    }

    virtual Label get_virtual_table_label(TypeSpecIter tsi, X64 *x64) {
        tsi++;
        return (*tsi)->get_virtual_table_label(tsi, x64);
    }
};


class HeapType: public Type {
public:
    HeapType(std::string name, unsigned pc)
        :Type(name, pc) {
    }

    virtual unsigned measure(TypeSpecIter tsi, StorageWhere where) {
        std::cerr << "This is probably an error, shouldn't measure a heap type!\n";
        throw INTERNAL_ERROR;
    }
    
    virtual void compile_finalizer(TypeSpecIter tsi, Label label, X64 *x64) {
        throw INTERNAL_ERROR;
    }
};


bool is_heap_type(Type *t) {
    return dynamic_cast<HeapType *>(t);
}


std::map<TypeSpec, Label> finalizer_labels;


void compile_finalizers(X64 *x64) {
    for (auto &kv : finalizer_labels) {
        TypeSpec ts = kv.first;
        Label label = kv.second;
        std::cerr << "Compiling finalizer for " << ts << ".\n";
        
        dynamic_cast<HeapType *>(ts[0])->compile_finalizer(ts.begin(), label, x64);
    }
}


Label finalizer_label(TypeSpec ts, X64 *x64) {
    if (!is_heap_type(ts[0]))
        throw INTERNAL_ERROR;
        
    x64->once(compile_finalizers);
    
    return finalizer_labels[ts];
}


class ArrayType: public HeapType {
public:
    ArrayType(std::string name)
        :HeapType(name, 1) {
        make_inner_scope(TypeSpec { reference_type, this, any_type });
    }
    
    virtual Value *lookup_initializer(TypeSpecIter tsi, std::string name, Scope *scope) {
        TypeSpec rts = TypeSpec(tsi).prefix(reference_type);
        
        if (name == "empty")
            return make_array_empty_value(rts);
        else if (name == "null")
            return make_null_reference_value(rts);
        else if (name == "{}")
            return make_array_initializer_value(rts);

        std::cerr << "No " << this->name << " initializer called " << name << "!\n";
        return NULL;
    }

    virtual void compile_finalizer(TypeSpecIter tsi, Label label, X64 *x64) {
        TypeSpec elem_ts = TypeSpec(tsi).unprefix(array_type).varvalue();
        int elem_size = ::elem_size(elem_ts.measure(MEMORY));
        Label start, end, loop;

        x64->code_label(label);
        x64->op(PUSHQ, RCX);

        x64->op(MOVQ, RCX, x64->array_length_address(RAX));
        x64->op(CMPQ, RCX, 0);
        x64->op(JE, end);

        x64->op(IMUL3Q, RBX, RCX, elem_size);
        x64->op(LEA, RAX, x64->array_elems_address(RAX));
        x64->op(ADDQ, RAX, RBX);

        x64->code_label(loop);
        x64->op(SUBQ, RAX, elem_size);
        elem_ts.destroy(Storage(MEMORY, Address(RAX, 0)), x64);
        x64->op(DECQ, RCX);
        x64->op(JNE, loop);

        x64->code_label(end);
        x64->op(POPQ, RCX);
        x64->op(RET);
    }
};


class CircularrayType: public ArrayType {
public:
    CircularrayType(std::string name)
        :ArrayType(name) {
    }
    
    virtual void compile_finalizer(TypeSpecIter tsi, Label label, X64 *x64) {
        TypeSpec elem_ts = TypeSpec(tsi).unprefix(circularray_type).varvalue();
        int elem_size = ::elem_size(elem_ts.measure(MEMORY));
        Label start, end, loop, ok, ok1;
    
        x64->code_label(label);
        x64->op(PUSHQ, RCX);
        x64->op(PUSHQ, RDX);
    
        x64->op(MOVQ, RCX, x64->array_length_address(RAX));
        x64->op(CMPQ, RCX, 0);
        x64->op(JE, end);
    
        x64->op(MOVQ, RDX, x64->array_front_address(RAX));
        x64->op(ADDQ, RDX, RCX);
        x64->op(CMPQ, RDX, x64->array_reservation_address(RAX));
        x64->op(JBE, ok1);
        
        x64->op(SUBQ, RDX, x64->array_reservation_address(RAX));
        
        x64->code_label(ok1);
        x64->op(IMUL3Q, RDX, RDX, elem_size);
    
        x64->code_label(loop);
        x64->op(SUBQ, RDX, elem_size);
        x64->op(CMPQ, RDX, 0);
        x64->op(JGE, ok);
        
        x64->op(MOVQ, RDX, x64->array_reservation_address(RAX));
        x64->op(DECQ, RDX);
        x64->op(IMUL3Q, RDX, RDX, elem_size);
        
        x64->code_label(ok);
        Address elem_addr = x64->array_elems_address(RAX) + RDX;
        elem_ts.destroy(Storage(MEMORY, elem_addr), x64);
        x64->op(DECQ, RCX);
        x64->op(JNE, loop);
    
        x64->code_label(end);
        x64->op(POPQ, RDX);
        x64->op(POPQ, RCX);
        x64->op(RET);
    }
};


class AatreeType: public HeapType {
public:
    AatreeType(std::string name)
        :HeapType(name, 1) {
        make_inner_scope(TypeSpec { reference_type, this, any_type });
    }
    
    virtual Value *lookup_initializer(TypeSpecIter tsi, std::string name, Scope *scope) {
        TypeSpec rts = TypeSpec(tsi).prefix(reference_type);
        
        if (name == "empty")
            return make_aatree_empty_value(rts);
        else if (name == "reserved")
            return make_aatree_reserved_value(rts);
        else if (name == "null")
            return make_null_reference_value(rts);
        //else if (name == "{}")
        //    return make_array_initializer_value(rts);

        std::cerr << "No " << this->name << " initializer called " << name << "!\n";
        return NULL;
    }

    virtual void compile_finalizer(TypeSpecIter tsi, Label label, X64 *x64) {
        TypeSpec elem_ts = TypeSpec(tsi).unprefix(aatree_type).varvalue();
        Label loop, cond;

        x64->code_label(label);
        x64->op(PUSHQ, RCX);

        x64->op(MOVQ, RCX, Address(RAX, AATREE_LAST_OFFSET));
        x64->op(JMP, cond);
        
        x64->code_label(loop);
        elem_ts.destroy(Storage(MEMORY, Address(RAX, RCX, AANODE_VALUE_OFFSET)), x64);
        x64->op(MOVQ, RCX, Address(RAX, RCX, AANODE_PREV_IS_RED_OFFSET));
        
        x64->code_label(cond);
        x64->op(CMPQ, RCX, AANODE_NIL);
        x64->op(JNE, loop);
        
        x64->op(POPQ, RCX);
        x64->op(RET);
    }
};
