
// NOTE: node indexes stored in root, vacant, *.{left,right,prev,next} are tree-relative
// offsets, so if RAX points to the tree, then RAX + RCX points to the node. The NIL node
// value may be 0, which is an invalid offset, since the tree itself has a nonempty header.

class AatreeEmptyValue: public GenericValue {
public:
    TypeSpec elem_ts;
    
    AatreeEmptyValue(TypeSpec ts)
        :GenericValue(VOID_TS, ts, NULL) {
        elem_ts = element_ts(ts);
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob;
        return clob.add(RAX).add(RBX).add(RCX);
    }

    virtual Storage compile(X64 *x64) {
        TypeSpec ats = ts.unprefix(reference_type);
        Label aatree_finalizer_label = finalizer_label(ats, x64);
        int elem_size = ::stack_size(elem_ts.measure(MEMORY));  // round to 8 bytes!
    
        x64->op(MOVQ, RAX, 0);
        x64->op(MOVQ, RBX, elem_size);
        x64->op(LEARIP, RCX, aatree_finalizer_label);
    
        x64->alloc_aatree_RAX_RBX_RCX();  // reservation, elem size, finalizer
        
        return Storage(REGISTER, RAX);
    }
};


class AatreeReservedValue: public GenericValue {
public:
    TypeSpec elem_ts;
    
    AatreeReservedValue(TypeSpec ts)
        :GenericValue(INTEGER_TS, ts, NULL) {
        elem_ts = element_ts(ts);
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob;
        return clob.add(RAX).add(RBX).add(RCX);
    }

    virtual Storage compile(X64 *x64) {
        TypeSpec ats = ts.unprefix(reference_type);
        Label aatree_finalizer_label = finalizer_label(ats, x64);
        int elem_size = ::stack_size(elem_ts.measure(MEMORY));  // round to 8 bytes!
    
        right->compile_and_store(x64, Storage(STACK));
    
        x64->op(POPQ, RAX);
        x64->op(MOVQ, RBX, elem_size);
        x64->op(LEARIP, RCX, aatree_finalizer_label);
    
        x64->alloc_aatree_RAX_RBX_RCX();  // reservation, elem size, finalizer
        
        return Storage(REGISTER, RAX);
    }
};


class AatreeLengthValue: public GenericValue {
public:
    Register reg;
    
    AatreeLengthValue(Value *l, TypeMatch &match)
        :GenericValue(VOID_TS, INTEGER_TS, l) {
        reg = NOREG;
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = left->precompile(preferred);
        
        if (!clob.has_any())
            clob.add(RAX);
        
        reg = clob.get_any();
            
        return clob;
    }

    virtual Storage compile(X64 *x64) {
        ls = left->compile(x64);

        switch (ls.where) {
        case REGISTER:
            x64->decref(ls.reg);
            x64->op(MOVQ, ls.reg, Address(ls.reg, AATREE_LENGTH_OFFSET));
            return Storage(REGISTER, ls.reg);
        case MEMORY:
            x64->op(MOVQ, reg, ls.address);
            x64->op(MOVQ, reg, Address(reg, AATREE_LENGTH_OFFSET));
            return Storage(REGISTER, reg);
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class AatreeHasValue: public GenericValue {
public:
    TypeSpec elem_ts;
    
    AatreeHasValue(Value *pivot, TypeMatch &match)
        :GenericValue(match[1], BOOLEAN_TS, pivot) {
        elem_ts = element_ts(match[0]);
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = left->precompile(preferred) | right->precompile(preferred);
        return clob.add(RAX).add(RBX).add(RCX).add(RDX);
    }

    virtual Storage compile(X64 *x64) {
        Label loop, no, less, greater, end, huuu;
        int key_size = ::stack_size(elem_ts.measure(MEMORY));
        
        compile_and_store_both(x64, Storage(STACK), Storage(STACK));
    
        x64->op(MOVQ, RDX, RSP);  // save key address for stack usage
        x64->op(MOVQ, RAX, Address(RSP, key_size));  // Aatree without incref
        x64->op(MOVQ, RCX, Address(RAX, AATREE_ROOT_OFFSET));

        x64->code_label(loop);
        //x64->log("Has loop.");
        x64->op(CMPQ, RCX, AANODE_NIL);
        x64->op(JE, no);

        Storage ks(MEMORY, Address(RDX, 0));  // can't use STACK, that would be popped!
        Storage vs(MEMORY, Address(RAX, RCX, AANODE_VALUE_OFFSET));
        elem_ts.compare(ks, vs, x64, less, greater);
        
        x64->op(MOVQ, RCX, 1);
        x64->op(JMP, end);
        
        x64->code_label(less);
        x64->op(MOVQ, RCX, Address(RAX, RCX, AANODE_LEFT_OFFSET));
        x64->op(JMP, loop);
        
        x64->code_label(greater);
        x64->op(MOVQ, RCX, Address(RAX, RCX, AANODE_RIGHT_OFFSET));
        x64->op(JMP, loop);
        
        x64->code_label(no);
        x64->op(MOVQ, RCX, 0);
        
        x64->code_label(end);
        
        elem_ts.store(Storage(STACK), Storage(), x64);
        x64->op(POPQ, RAX);
        x64->decref(RAX);
        //x64->log("Has end.");
        
        return Storage(REGISTER, RCX);
    }
};


class AatreeAddValue: public GenericValue {
public:
    TypeSpec elem_ts;
    
    AatreeAddValue(Value *pivot, TypeMatch &match)
        :GenericValue(match[1], VOID_TS, pivot) {
        elem_ts = element_ts(match[0]);
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = left->precompile(preferred) | right->precompile(preferred);
        return clob.add(RAX).add(RBX).add(RCX).add(RDX);
    }

    virtual void allocate_aanode(X64 *x64) {
        // In: RAX - tree
        // Out: RCX - node
        // Clob: RBX
        Label no_vacancy, no_reservation, init, no_last, end;
        int key_size = ::stack_size(elem_ts.measure(MEMORY));
        int node_size = key_size + AANODE_HEADER_SIZE;
        std::cerr << "Allocating anode of " << node_size << " bytes.\n";
        
        x64->log("Allocating aanode.");
        x64->op(MOVQ, RCX, Address(RAX, AATREE_VACANT_OFFSET));
        x64->op(CMPQ, RCX, AANODE_NIL);
        x64->op(JE, no_vacancy);
        
        x64->op(MOVQ, RBX, Address(RAX, RCX, AANODE_NEXT_OFFSET));
        x64->op(MOVQ, Address(RAX, AATREE_VACANT_OFFSET), RBX);
        x64->op(JMP, init);
        
        x64->code_label(no_vacancy);
        x64->op(MOVQ, RCX, Address(RAX, AATREE_LENGTH_OFFSET));
        x64->op(CMPQ, RCX, Address(RAX, AATREE_RESERVATION_OFFSET));
        x64->op(JE, no_reservation);
        
        x64->op(INCQ, Address(RAX, AATREE_LENGTH_OFFSET));
        x64->op(IMUL3Q, RCX, RCX, node_size);
        x64->op(ADDQ, RCX, AATREE_HEADER_SIZE);
        x64->op(JMP, init);
        
        x64->code_label(no_reservation);
        x64->die("Aatree full!");
        
        x64->code_label(init);
        x64->op(MOVQ, Address(RAX, RCX, AANODE_LEFT_OFFSET), AANODE_NIL);
        x64->op(MOVQ, Address(RAX, RCX, AANODE_RIGHT_OFFSET), AANODE_NIL);
        x64->op(MOVQ, Address(RAX, RCX, AANODE_NEXT_OFFSET), AANODE_NIL);
        x64->op(MOVQ, RBX, Address(RAX, AATREE_LAST_OFFSET));
        x64->op(MOVQ, Address(RAX, RCX, AANODE_PREV_IS_RED_OFFSET), RBX);
        
        x64->op(CMPQ, RBX, AANODE_NIL);
        x64->op(JE, no_last);
        
        x64->op(MOVQ, Address(RAX, RBX, AANODE_NEXT_OFFSET), RCX);
        x64->op(JMP, end);
        
        x64->code_label(no_last);
        x64->op(MOVQ, Address(RAX, AATREE_FIRST_OFFSET), RCX);
        
        x64->code_label(end);
        x64->op(MOVQ, Address(RAX, AATREE_LAST_OFFSET), RCX);
    }

    virtual Storage compile(X64 *x64) {
        Label loop, no, less, greater, end, fun;
        int key_size = ::stack_size(elem_ts.measure(MEMORY));
        
        compile_and_store_both(x64, Storage(STACK), Storage(STACK));

        x64->op(MOVQ, RDX, RSP);  // save key address for stack usage
        x64->op(MOVQ, RAX, Address(RSP, key_size));  // Aatree without incref
        x64->op(MOVQ, RCX, Address(RAX, AATREE_ROOT_OFFSET));
        x64->op(CALL, fun);
        x64->op(MOVQ, Address(RAX, AATREE_ROOT_OFFSET), RBX);
        x64->op(JMP, end);

        // Expects RAX - tree, RCX - index, RDX - key, all unmodified
        // Returns RBX - new index
        
        x64->code_label_export(fun, "fun", 0, false);
        x64->log("Add fun.");
        x64->op(CMPQ, RCX, AANODE_NIL);
        x64->op(JE, no);
        
        Storage ks(MEMORY, Address(RDX, 0));  // can't use STACK, that would be popped!
        Storage vs(MEMORY, Address(RAX, RCX, AANODE_VALUE_OFFSET));
        elem_ts.compare(ks, vs, x64, less, greater);
        
        // Found the value, nothing to do
        x64->op(MOVQ, RBX, RCX);
        x64->op(RET);
        
        x64->code_label(less);
        x64->op(PUSHQ, RCX);
        x64->op(MOVQ, RCX, Address(RAX, RCX, AANODE_LEFT_OFFSET));
        x64->op(CALL, fun);
        x64->op(POPQ, RCX);
        x64->op(MOVQ, Address(RAX, RCX, AANODE_LEFT_OFFSET), RBX);
        x64->op(MOVQ, RBX, RCX);
        x64->op(RET);
        
        x64->code_label(greater);
        x64->op(PUSHQ, RCX);
        x64->op(MOVQ, RCX, Address(RAX, RCX, AANODE_RIGHT_OFFSET));
        x64->op(CALL, fun);
        x64->op(POPQ, RCX);
        x64->op(MOVQ, Address(RAX, RCX, AANODE_RIGHT_OFFSET), RBX);
        x64->op(MOVQ, RBX, RCX);
        x64->op(RET);
        
        x64->code_label(no);
        allocate_aanode(x64);  // from RAX to RCX
        elem_ts.create(ks, vs, x64);
        //std::cerr << "Creating a " << elem_ts << " aanode.\n";
        x64->op(MOVQ, RBX, RCX);
        x64->op(RET);
        
        x64->code_label(end);
        elem_ts.store(Storage(STACK), Storage(), x64);
        x64->op(POPQ, RAX);
        x64->decref(RAX);
        
        return Storage();
    }
};


class AatreeRemoveValue: public GenericValue {
public:
    TypeSpec elem_ts;
    
    AatreeRemoveValue(Value *pivot, TypeMatch &match)
        :GenericValue(match[1], VOID_TS, pivot) {
        elem_ts = element_ts(match[0]);
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = left->precompile(preferred) | right->precompile(preferred);
        return clob.add(RAX).add(RBX).add(RCX).add(RDX);
    }

    virtual void deallocate_aanode(X64 *x64) {
        // In: RAX - tree
        // Out: RCX - node
        // Clob: RBX, RDX
        Label no_prev, prev_ok, no_next, next_ok;
        
        x64->op(MOVQ, RBX, Address(RAX, RCX, AANODE_PREV_IS_RED_OFFSET));
        x64->op(MOVQ, RDX, Address(RAX, RCX, AANODE_NEXT_OFFSET));

        x64->op(CMPQ, RBX, AANODE_NIL);
        x64->op(JE, no_prev);
        
        x64->op(MOVQ, Address(RAX, RBX, AANODE_NEXT_OFFSET), RDX);
        x64->op(JMP, prev_ok);
        
        x64->code_label(no_prev);
        x64->op(MOVQ, Address(RAX, AATREE_FIRST_OFFSET), RDX);
        
        x64->code_label(prev_ok);
        x64->op(CMPQ, RDX, AANODE_NIL);
        x64->op(JE, no_next);
        
        x64->op(MOVQ, Address(RAX, RDX, AANODE_PREV_IS_RED_OFFSET), RBX);
        x64->op(JMP, next_ok);
        
        x64->code_label(no_next);
        x64->op(MOVQ, Address(RAX, AATREE_LAST_OFFSET), RBX);
        
        x64->code_label(next_ok);
        x64->op(MOVQ, RBX, Address(RAX, AATREE_VACANT_OFFSET));
        x64->op(MOVQ, Address(RAX, RCX, AANODE_NEXT_OFFSET), RBX);
        x64->op(MOVQ, Address(RAX, AATREE_VACANT_OFFSET), RCX);
        
        x64->op(DECQ, Address(RAX, AATREE_LENGTH_OFFSET));
    }

    virtual Storage compile(X64 *x64) {
        Label loop, no, remove_left, remove_right, end, fun;
        int key_size = ::stack_size(elem_ts.measure(MEMORY));
        
        compile_and_store_both(x64, Storage(STACK), Storage(STACK));

        x64->op(MOVQ, RDX, RSP);  // save key address for stack usage
        x64->op(MOVQ, RAX, Address(RSP, key_size));  // Aatree without incref
        x64->op(MOVQ, RCX, Address(RAX, AATREE_ROOT_OFFSET));
        x64->op(CALL, fun);
        x64->op(MOVQ, Address(RAX, AATREE_ROOT_OFFSET), RBX);
        x64->op(JMP, end);

        // Expects RAX - tree, RCX - index, RDX - key (may be modified upon return)
        // Returns RBX - new index
        
        x64->code_label_export(fun, "fun", 0, false);
        x64->log("Remove fun.");
        x64->op(CMPQ, RCX, AANODE_NIL);
        x64->op(JE, no);
        
        Storage ks(MEMORY, Address(RDX, 0));  // can't use STACK, that would be popped!
        Storage vs(MEMORY, Address(RAX, RCX, AANODE_VALUE_OFFSET));
        elem_ts.compare(ks, vs, x64, remove_left, remove_right);
        
        // Found the value, remove it (RDX is no longer needed)
        Label no_left, left_loop, no_right;
        x64->op(MOVQ, RBX, Address(RAX, RCX, AANODE_LEFT_OFFSET));
        x64->op(CMPQ, RBX, AANODE_NIL);
        x64->op(JE, no_left);
        
        // Find the greatest smaller element as replacement
        x64->code_label(left_loop);
        x64->op(MOVQ, RDX, RBX);
        x64->op(MOVQ, RBX, Address(RAX, RDX, AANODE_RIGHT_OFFSET));
        x64->op(CMPQ, RBX, AANODE_NIL);
        x64->op(JNE, left_loop);
        
        Storage rs(MEMORY, Address(RAX, RDX, AANODE_VALUE_OFFSET));
        elem_ts.store(rs, vs, x64);
        
        x64->op(LEA, RDX, Address(RAX, RDX, AANODE_VALUE_OFFSET));
        x64->op(JMP, remove_left);  // Yes, this is a goto :-)
        
        x64->code_label(no_left);
        x64->op(MOVQ, RDX, Address(RAX, RCX, AANODE_RIGHT_OFFSET));
        x64->op(CMPQ, RDX, AANODE_NIL);
        x64->op(JE, no_right);
        
        // A single red right child can be the replacement FIXME: this needs balanced!
        elem_ts.destroy(vs, x64);
        deallocate_aanode(x64);  // At RCX
        x64->op(MOVQ, RBX, RDX);  // return the right child FIXME: recolor!
        x64->op(RET);
        
        // No children, just remove FIXME: black must be immaterialized!
        x64->code_label(no_right);
        elem_ts.destroy(vs, x64);
        deallocate_aanode(x64);
        x64->op(MOVQ, RBX, AANODE_NIL);
        x64->op(RET);
        
        // Descend to the left
        x64->code_label(remove_left);
        x64->op(PUSHQ, RCX);
        x64->op(MOVQ, RCX, Address(RAX, RCX, AANODE_LEFT_OFFSET));
        x64->op(CALL, fun);
        x64->op(POPQ, RCX);
        x64->op(MOVQ, Address(RAX, RCX, AANODE_LEFT_OFFSET), RBX);
        x64->op(MOVQ, RBX, RCX);  // FIXME: balance
        x64->op(RET);
        
        // Descend to the right
        x64->code_label(remove_right);
        x64->op(PUSHQ, RCX);
        x64->op(MOVQ, RCX, Address(RAX, RCX, AANODE_RIGHT_OFFSET));
        x64->op(CALL, fun);
        x64->op(POPQ, RCX);
        x64->op(MOVQ, Address(RAX, RCX, AANODE_RIGHT_OFFSET), RBX);
        x64->op(MOVQ, RBX, RCX);  // FIXME: balance
        x64->op(RET);
        
        // Not found
        x64->code_label(no);
        x64->op(MOVQ, RBX, AANODE_NIL);
        x64->op(RET);
        
        x64->code_label(end);
        elem_ts.store(Storage(STACK), Storage(), x64);
        x64->op(POPQ, RAX);
        x64->decref(RAX);
        
        return Storage();
    }
};


